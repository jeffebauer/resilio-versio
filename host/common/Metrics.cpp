#include "Metrics.h"
#include "Fft.h"
#include "Spectral.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace rv::metrics {

namespace {

// Event = sample exceeding -40 dBFS after >= 0.5 s continuously under it
// (docs/m1-contracts.md). Returns event onset sample indices.
std::vector<size_t> findEvents(const std::vector<float>& mono, float sr)
{
    const float thresh = spectral::fromDb(-40.0f);
    const size_t armSamples = size_t(0.5 * double(sr));
    std::vector<size_t> events;
    size_t under = armSamples; // treat the start as already "armed"
    size_t i = 0;
    while (i < mono.size()) {
        if (std::fabs(mono[i]) >= thresh) {
            if (under >= armSamples) events.push_back(i);
            while (i < mono.size() && std::fabs(mono[i]) >= thresh) ++i;
            under = 0;
        } else {
            ++under;
            ++i;
        }
    }
    return events;
}

// Schroeder backward-integration T60: fit the -5 -> -35 dB region of the
// energy decay curve, extrapolate the slope to -60 dB. NaN if the curve
// never reaches -35 dB in [start, end) (not measurable).
double schroederT60(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    if (end <= start + 8 || end > mono.size()) return std::nan("");
    const size_t len = end - start;
    std::vector<double> edc(len);
    double acc = 0.0;
    for (size_t n = len; n-- > 0;) {
        const double x = double(mono[start + n]);
        acc += x * x;
        edc[n] = acc;
    }
    if (edc[0] <= 0.0) return std::nan("");
    long n5 = -1, n35 = -1;
    for (size_t i = 0; i < len; ++i) {
        const double db = 10.0 * std::log10(edc[i] / edc[0] + 1e-300);
        if (n5 < 0 && db <= -5.0) n5 = long(i);
        if (db <= -35.0) { n35 = long(i); break; }
    }
    if (n5 < 0 || n35 < 0 || n35 <= n5) return std::nan("");

    double sumT = 0, sumD = 0, sumTT = 0, sumTD = 0;
    long count = 0;
    for (long i = n5; i <= n35; ++i) {
        const double t = double(i) / double(sr);
        const double db = 10.0 * std::log10(edc[size_t(i)] / edc[0] + 1e-300);
        sumT += t; sumD += db; sumTT += t * t; sumTD += t * db; ++count;
    }
    const double denom = double(count) * sumTT - sumT * sumT;
    if (std::fabs(denom) < 1e-12) return std::nan("");
    const double slope = (double(count) * sumTD - sumT * sumD) / denom; // dB/s, expect negative
    if (slope >= -1e-6) return std::nan(""); // not decaying
    return -60.0 / slope;
}

// Average power spectrum over [start, end), Hann-windowed FFT (8192, or
// the largest power of two <= 8192 that fits the segment, min 256), then
// max over 100 Hz-10 kHz of (bin dB - 1/3-octave-smoothed median dB).
double resonancePeakDb(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    if (end <= start || end > mono.size()) return std::nan("");
    const size_t segLen = end - start;
    size_t fftSize = 8192;
    while (fftSize > 256 && fftSize > segLen) fftSize >>= 1;
    if (segLen < 32) return std::nan("");

    const size_t hop = fftSize / 2;
    std::vector<double> sumPower(fftSize / 2 + 1, 0.0);
    size_t frames = 0;
    for (size_t pos = start; pos + std::min(fftSize, segLen) <= end; pos += hop) {
        const size_t n = std::min(fftSize, end - pos);
        const auto mag = fft::magnitudeSpectrum(mono.data() + pos, n, fftSize);
        for (size_t k = 0; k < mag.size(); ++k) sumPower[k] += double(mag[k]) * double(mag[k]);
        ++frames;
        if (n < fftSize) break; // last partial frame
    }
    if (frames == 0) {
        const auto mag = fft::magnitudeSpectrum(mono.data() + start, segLen, fftSize);
        for (size_t k = 0; k < mag.size(); ++k) sumPower[k] += double(mag[k]) * double(mag[k]);
        frames = 1;
    }

    std::vector<float> magDb(sumPower.size()), freqHz(sumPower.size());
    for (size_t k = 0; k < sumPower.size(); ++k) {
        magDb[k] = spectral::toDb(float(std::sqrt(sumPower[k] / double(frames))));
        freqHz[k] = float(k) * sr / float(fftSize);
    }
    const auto smoothed = spectral::thirdOctaveSmoothedMedian(magDb, freqHz);

    float best = -1e9f;
    for (size_t k = 0; k < magDb.size(); ++k) {
        if (freqHz[k] < 100.0f || freqHz[k] > 10000.0f) continue;
        best = std::max(best, magDb[k] - smoothed[k]);
    }
    return best > -1e8f ? double(best) : std::nan("");
}

// True if a narrowband peak stays > 12 dB above the smoothed spectrum, at
// an unchanged bin (+-1), across consecutive 0.5 s frames covering > 2 s,
// while its estimated level is > -30 dBFS.
bool steadyTone(const std::vector<float>& mono, float sr)
{
    const size_t frameSamples = size_t(0.5 * double(sr));
    if (frameSamples < 64 || mono.size() < frameSamples) return false;

    // A fixed FFT size for the per-frame snapshot: the 1/3-octave
    // smoothing below is O(bins^2), so padding all the way out to match a
    // full 0.5 s frame (tens of thousands of bins) would make a 60 s file
    // take minutes. 8192 (~5.9 Hz/bin at 48 kHz) keeps that cost bounded
    // while still giving the 1/3-octave window enough bins even at low
    // frequencies to smooth around a narrowband peak rather than through
    // it; it analyzes a representative ~170 ms slice of each 0.5 s frame.
    const size_t fftSize = std::min<size_t>(8192, frameSamples);
    const size_t windowLen = std::min(frameSamples, fftSize);

    const size_t numFrames = mono.size() / frameSamples;
    long bestRun = 0, curRun = 0, refBin = -100;
    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * frameSamples;
        const auto mag = fft::magnitudeSpectrum(mono.data() + start, windowLen, fftSize);
        std::vector<float> magDb(mag.size()), freqHz(mag.size());
        for (size_t k = 0; k < mag.size(); ++k) {
            magDb[k] = spectral::toDb(mag[k]);
            freqHz[k] = float(k) * sr / float(fftSize);
        }
        const auto smoothed = spectral::thirdOctaveSmoothedMean(magDb, freqHz);

        long bestK = -1;
        float bestDiff = -1e9f;
        for (size_t k = 1; k < mag.size(); ++k) { // skip DC
            const float diff = magDb[k] - smoothed[k];
            if (diff > bestDiff) { bestDiff = diff; bestK = long(k); }
        }
        long candidate = -1;
        if (bestK >= 0 && bestDiff > 12.0f) {
            // Amplitude of an isolated Hann-windowed tone: |X[k]| ~= A*n/4.
            const double amp = 4.0 * double(mag[size_t(bestK)]) / double(windowLen);
            const double levelDbfs = spectral::toDb(float(amp));
            if (levelDbfs > -30.0) candidate = bestK;
        }
        if (candidate < 0) { curRun = 0; refBin = -100; continue; }
        if (refBin < 0 || std::labs(candidate - refBin) <= 1) ++curRun; else curRun = 1;
        refBin = candidate;
        bestRun = std::max(bestRun, curRun);
    }
    const double runSeconds = double(bestRun) * (double(frameSamples) / double(sr));
    return runSeconds > 2.0;
}

// Sample-to-sample discontinuities: |x[n]-2x[n-1]+x[n-2]| exceeding 20 dB
// above a local (+-10 ms) RMS of that second-difference signal, and above
// -60 dBFS absolute (docs/m1-contracts.md). Debounced 1 ms so one click
// isn't counted many times.
//
// Tuning note: a bare "20 dB above local RMS" test also fires on ordinary
// hard transients coming out of silence (e.g. 04_skank.wav's un-faded
// chord stabs) -- right at the onset the local RMS of the difference
// signal is itself near zero, so almost any attack clears the bar. A real
// click is a singular glitch: the raw signal returns to its prior level
// right after it. A musical onset instead settles into a new, sustained
// level. So a candidate only counts if the raw signal does NOT show a
// sustained level jump: RMS of the raw signal well before the candidate
// (200-20 ms back, clear of it) is compared against RMS just after it
// (5-50 ms forward, skipping the glitch's own few samples). A >5x jump
// means "new sustained sound", not a click. Verified against all six
// stimulus files (see final report) and a sine with an injected step.
long clickCount(const std::vector<float>& mono, float sr)
{
    const size_t n = mono.size();
    if (n < 3) return 0;
    std::vector<double> dsq(n, 0.0), xsq(n, 0.0);
    for (size_t i = 0; i < n; ++i) xsq[i] = double(mono[i]) * double(mono[i]);
    for (size_t i = 2; i < n; ++i) {
        const double d = double(mono[i]) - 2.0 * double(mono[i - 1]) + double(mono[i - 2]);
        dsq[i] = d * d;
    }
    std::vector<double> dPrefix(n + 1, 0.0), xPrefix(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) { dPrefix[i + 1] = dPrefix[i] + dsq[i]; xPrefix[i + 1] = xPrefix[i] + xsq[i]; }

    auto rmsX = [&](long lo, long hi) -> double {
        lo = std::max<long>(lo, 0);
        hi = std::min<long>(hi, long(n) - 1);
        if (hi < lo) return 0.0;
        const size_t cnt = size_t(hi - lo + 1);
        return std::sqrt((xPrefix[size_t(hi) + 1] - xPrefix[size_t(lo)]) / double(cnt));
    };

    const size_t halfWin = std::max<size_t>(1, size_t(0.010 * double(sr)));
    const double absFloor = spectral::fromDb(-60.0f);
    const size_t debounce = std::max<size_t>(1, size_t(0.001 * double(sr)));
    const long bgLo = long(0.200 * double(sr)), bgHi = long(0.020 * double(sr));
    const long trailLo = long(0.005 * double(sr)), trailHi = long(0.050 * double(sr));
    constexpr double kOnsetFactor = 5.0;

    long count = 0;
    size_t i = 2;
    while (i < n) {
        const size_t lo = i > halfWin ? i - halfWin : 0;
        const size_t hi = std::min(n - 1, i + halfWin);
        const double localRms = std::sqrt(dPrefix[hi + 1] - dPrefix[lo]) / std::sqrt(double(hi - lo + 1));
        const double threshold = std::max(localRms * 10.0, absFloor); // 20 dB above local RMS
        if (std::sqrt(dsq[i]) > threshold) {
            const double bg = rmsX(long(i) - bgLo, long(i) - bgHi);
            const double trail = rmsX(long(i) + trailLo, long(i) + trailHi);
            const bool sustainedOnset = trail > std::max(bg * kOnsetFactor, 1e-3);
            if (!sustainedOnset) {
                ++count;
                i += debounce;
                continue;
            }
        }
        ++i;
    }
    return count;
}

} // namespace

Metrics compute(const std::vector<std::vector<float>>& channels, float sampleRate)
{
    Metrics m;
    const size_t numCh = channels.size();
    const size_t frames = numCh ? channels[0].size() : 0;

    for (const auto& ch : channels) {
        for (float x : ch) {
            if (!std::isfinite(x)) ++m.nanInfCount;
            else if (std::fabs(x) >= 0.999f) ++m.clipCount;
        }
    }

    std::vector<float> mono(frames, 0.0f);
    for (size_t i = 0; i < frames; ++i) {
        double sum = 0.0;
        for (const auto& ch : channels) {
            const float v = ch[i];
            if (std::isfinite(v)) sum += double(v);
        }
        mono[i] = numCh ? float(sum / double(numCh)) : 0.0f;
    }

    double peak = 0.0, sumSq = 0.0;
    for (float x : mono) {
        const double a = std::fabs(double(x));
        if (a > peak) peak = a;
        sumSq += double(x) * double(x);
    }
    m.peakDbfs = peak > 0.0 ? double(spectral::toDb(float(peak))) : -200.0;
    const double rms = frames ? std::sqrt(sumSq / double(frames)) : 0.0;
    m.rmsDbfs = rms > 0.0 ? double(spectral::toDb(float(rms))) : -200.0;

    const auto events = findEvents(mono, sampleRate);
    size_t segStart = 0, segEnd = frames;
    if (!events.empty()) {
        segStart = events[0];
        segEnd = events.size() > 1 ? events[1] : frames;
    }
    m.t60S = schroederT60(mono, segStart, segEnd, sampleRate);

    // Resonance segment starts 1 s after the first event (or 1 s into the
    // file when no event is found, e.g. analyzing a sustained reference
    // recording with --analyze).
    size_t resStart = events.empty() ? std::min(frames, size_t(1.0 * double(sampleRate)))
                                      : std::min(segEnd, events[0] + size_t(1.0 * double(sampleRate)));
    resStart = std::min(resStart, segEnd);
    m.resonancePeakDb = resonancePeakDb(mono, resStart, segEnd, sampleRate);

    m.steadyTone = steadyTone(mono, sampleRate);
    m.clickCount = clickCount(mono, sampleRate);
    return m;
}

std::string summaryLine(const Metrics& m)
{
    char t60Buf[32], resBuf[32], buf[512];
    if (std::isnan(m.t60S)) std::snprintf(t60Buf, sizeof t60Buf, "null");
    else std::snprintf(t60Buf, sizeof t60Buf, "%.2fs", m.t60S);
    if (std::isnan(m.resonancePeakDb)) std::snprintf(resBuf, sizeof resBuf, "null");
    else std::snprintf(resBuf, sizeof resBuf, "%.1fdB", m.resonancePeakDb);
    std::snprintf(buf, sizeof buf,
        "peak=%.1fdBFS rms=%.1fdBFS t60=%s res=%s steady=%s nan/inf=%ld clip=%ld click=%ld",
        m.peakDbfs, m.rmsDbfs, t60Buf, resBuf,
        m.steadyTone ? "true" : "false", m.nanInfCount, m.clipCount, m.clickCount);
    return std::string(buf);
}

} // namespace rv::metrics
