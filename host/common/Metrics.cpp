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

// End of the T60 fit when another event follows at `end`. The next event
// doesn't start at its -40 dBFS crossing: a spring's output (or a fading-in
// stimulus) builds up quietly for tens of ms before it, and that energy,
// fed into the backward integration, props up the end of the decay curve
// and makes the tail read long (1.23 s vs 1.03 s on one render). So the
// fit stops at the quietest point between the tail's peak and the next
// onset: the lowest-energy 10 ms block after the loudest one (the latest
// on ties, e.g. digital silence). Before it the tail is still falling;
// after it the next event is arriving. Cutting the tail there costs
// nothing measurable: by then it is at its floor, far below the -35 dB
// end of the fit. Returns `end` if the segment is too short to split.
size_t decayEnd(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    const size_t block = std::max<size_t>(1, size_t(0.010 * double(sr)));
    if (end > mono.size() || end <= start + 4 * block) return end;
    const size_t numBlocks = (end - start) / block;
    std::vector<double> e(numBlocks, 0.0);
    for (size_t b = 0; b < numBlocks; ++b) {
        const size_t from = start + b * block;
        for (size_t i = from; i < from + block; ++i) e[b] += double(mono[i]) * double(mono[i]);
    }
    const size_t peak = size_t(std::max_element(e.begin(), e.end()) - e.begin());
    size_t quietest = peak;
    for (size_t b = peak; b < numBlocks; ++b)
        if (e[b] <= e[quietest]) quietest = b;
    if (quietest == peak) return end;
    return start + quietest * block;
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
    // Every bin keeps its own run (M6 fix): the old version followed only
    // the single most prominent bin per frame, so a steady tone whose
    // harmonics are about as prominent (a saturated, self-oscillating loop)
    // flipped between them and never built a run.
    const size_t bins = fftSize / 2 + 1;
    std::vector<long> run(bins, 0), prev(bins, 0);
    long bestRun = 0;
    for (size_t f = 0; f < numFrames; ++f) {
        const size_t start = f * frameSamples;
        const auto mag = fft::magnitudeSpectrum(mono.data() + start, windowLen, fftSize);
        std::vector<float> magDb(mag.size()), freqHz(mag.size());
        for (size_t k = 0; k < mag.size(); ++k) {
            magDb[k] = spectral::toDb(mag[k]);
            freqHz[k] = float(k) * sr / float(fftSize);
        }
        const auto smoothed = spectral::thirdOctaveSmoothedMean(magDb, freqHz);
        prev.swap(run);
        for (size_t k = 0; k < bins; ++k) {
            run[k] = 0;
            if (k == 0 || magDb[k] - smoothed[k] <= 12.0f) continue; // skip DC
            // Amplitude of an isolated Hann-windowed tone: |X[k]| ~= A*n/4.
            const double amp = 4.0 * double(mag[k]) / double(windowLen);
            if (spectral::toDb(float(amp)) <= -30.0) continue;
            long p = prev[k];
            if (k > 0) p = std::max(p, prev[k - 1]);
            if (k + 1 < bins) p = std::max(p, prev[k + 1]);
            run[k] = p + 1;
            bestRun = std::max(bestRun, run[k]);
        }
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

// Pearson correlation of L and R over [start, end) (docs/m4-contracts.md
// Stream E). NaN if the segment is empty/out of range or either channel
// has ~zero variance there (undefined correlation).
double pearsonCorrelation(const std::vector<float>& L, const std::vector<float>& R, size_t start, size_t end)
{
    if (end <= start || end > L.size() || end > R.size()) return std::nan("");
    const size_t n = end - start;
    double sumL = 0.0, sumR = 0.0;
    for (size_t i = start; i < end; ++i) { sumL += double(L[i]); sumR += double(R[i]); }
    const double meanL = sumL / double(n), meanR = sumR / double(n);
    double cov = 0.0, varL = 0.0, varR = 0.0;
    for (size_t i = start; i < end; ++i) {
        const double dl = double(L[i]) - meanL, dr = double(R[i]) - meanR;
        cov += dl * dr; varL += dl * dl; varR += dr * dr;
    }
    if (varL <= 1e-20 || varR <= 1e-20) return std::nan("");
    return cov / std::sqrt(varL * varR);
}

// 10*log10( power(L+R) / (power(L)+power(R)) ) over the whole file
// (docs/m4-contracts.md Stream E). Uncorrelated -> ~0 dB, identical ->
// +3 dB, out of phase -> very negative (floored so the result stays
// finite rather than -inf for exact cancellation).
double monoLossDb(const std::vector<float>& L, const std::vector<float>& R)
{
    const size_t n = std::min(L.size(), R.size());
    if (n == 0) return std::nan("");
    double sumMono = 0.0, sumSep = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double l = double(L[i]), r = double(R[i]);
        const double mixed = l + r;
        sumMono += mixed * mixed;
        sumSep += l * l + r * r;
    }
    if (sumSep <= 1e-300) return std::nan(""); // silence: undefined
    return 10.0 * std::log10(std::max(sumMono, 1e-300) / sumSep);
}

// Deepest dip in 200Hz-5kHz of the mono-sum power spectrum relative to the
// stereo-average power spectrum ((|L|^2+|R|^2)/2), both averaged over
// Hann-windowed frames and 1/3-octave-smoothed (docs/m4-contracts.md
// Stream E). Destructive interference between L and R shows up as a comb
// notch in the mono sum that the stereo-average (phase-blind) spectrum
// doesn't have, so the difference isolates comb-filtering, not just
// spectral shape. Same 8192-point-or-smaller-power-of-two framing as
// resonancePeakDb.
double monoNotchDb(const std::vector<float>& L, const std::vector<float>& R, size_t start, size_t end, float sr)
{
    if (end <= start || end > L.size() || end > R.size()) return std::nan("");
    const size_t segLen = end - start;
    size_t fftSize = 8192;
    while (fftSize > 256 && fftSize > segLen) fftSize >>= 1;
    if (segLen < 32) return std::nan("");

    std::vector<float> monoSum(segLen);
    for (size_t i = 0; i < segLen; ++i) monoSum[i] = L[start + i] + R[start + i];

    const size_t hop = fftSize / 2;
    std::vector<double> sumMonoPow(fftSize / 2 + 1, 0.0), sumAvgPow(fftSize / 2 + 1, 0.0);
    size_t frames = 0;
    for (size_t pos = 0; pos + std::min(fftSize, segLen) <= segLen; pos += hop) {
        const size_t n = std::min(fftSize, segLen - pos);
        const auto magM = fft::magnitudeSpectrum(monoSum.data() + pos, n, fftSize);
        const auto magL = fft::magnitudeSpectrum(L.data() + start + pos, n, fftSize);
        const auto magR = fft::magnitudeSpectrum(R.data() + start + pos, n, fftSize);
        for (size_t k = 0; k < magM.size(); ++k) {
            sumMonoPow[k] += double(magM[k]) * double(magM[k]);
            sumAvgPow[k] += 0.5 * (double(magL[k]) * double(magL[k]) + double(magR[k]) * double(magR[k]));
        }
        ++frames;
        if (n < fftSize) break;
    }
    if (frames == 0) {
        const auto magM = fft::magnitudeSpectrum(monoSum.data(), segLen, fftSize);
        const auto magL = fft::magnitudeSpectrum(L.data() + start, segLen, fftSize);
        const auto magR = fft::magnitudeSpectrum(R.data() + start, segLen, fftSize);
        for (size_t k = 0; k < magM.size(); ++k) {
            sumMonoPow[k] += double(magM[k]) * double(magM[k]);
            sumAvgPow[k] += 0.5 * (double(magL[k]) * double(magL[k]) + double(magR[k]) * double(magR[k]));
        }
        frames = 1;
    }

    std::vector<float> monoDb(sumMonoPow.size()), avgDb(sumMonoPow.size()), freqHz(sumMonoPow.size());
    for (size_t k = 0; k < sumMonoPow.size(); ++k) {
        monoDb[k] = spectral::toDb(float(std::sqrt(sumMonoPow[k] / double(frames))));
        avgDb[k]  = spectral::toDb(float(std::sqrt(sumAvgPow[k] / double(frames))));
        freqHz[k] = float(k) * sr / float(fftSize);
    }
    const auto monoSmoothed = spectral::thirdOctaveSmoothedMedian(monoDb, freqHz);
    const auto avgSmoothed  = spectral::thirdOctaveSmoothedMedian(avgDb, freqHz);

    float worst = 1e9f;
    for (size_t k = 0; k < monoSmoothed.size(); ++k) {
        if (freqHz[k] < 200.0f || freqHz[k] > 5000.0f) continue;
        worst = std::min(worst, monoSmoothed[k] - avgSmoothed[k]);
    }
    return worst < 1e8f ? double(worst) : std::nan("");
}

// Largest absolute change in RMS dB between consecutive, non-overlapping
// 100 ms windows, ignoring windows below -60 dBFS and the first 100 ms
// window after each event onset (docs/m4-contracts.md Stream E; event =
// as in t60, see findEvents above). Runs on the mono downmix regardless
// of channel count, like t60/resonance.
double maxStepDb100ms(const std::vector<float>& mono, float sr, const std::vector<size_t>& events)
{
    const size_t winLen = size_t(0.1 * double(sr));
    if (winLen == 0 || mono.size() < winLen) return std::nan("");
    const size_t numWindows = mono.size() / winLen;
    if (numWindows < 2) return std::nan("");

    std::vector<double> winDb(numWindows);
    std::vector<bool> eligible(numWindows, true);
    for (size_t w = 0; w < numWindows; ++w) {
        const size_t start = w * winLen, end = start + winLen;
        double sumSq = 0.0;
        for (size_t i = start; i < end; ++i) sumSq += double(mono[i]) * double(mono[i]);
        const double rms = std::sqrt(sumSq / double(winLen));
        winDb[w] = rms > 0.0 ? double(spectral::toDb(float(rms))) : -200.0;
        if (winDb[w] < -60.0) eligible[w] = false;
    }
    for (size_t onset : events) {
        const size_t w = onset / winLen;
        if (w < numWindows) eligible[w] = false;
    }

    double maxStep = 0.0;
    bool any = false;
    for (size_t w = 0; w + 1 < numWindows; ++w) {
        if (!eligible[w] || !eligible[w + 1]) continue;
        maxStep = std::max(maxStep, std::fabs(winDb[w + 1] - winDb[w]));
        any = true;
    }
    return any ? maxStep : std::nan("");
}

// ---- M6 Ringing metric (docs/m6-metric-calibration.md) ----------------------
//
// Plain version: a Ringing tone is one narrow frequency that outlives the
// frequencies around it. So instead of asking "is there a peak?" (a spring's
// own mode comb is full of peaks, which is why resonance_peak_db reads
// 10-40 dB on real tanks), ask "does a peak keep *pulling away from* its
// neighbourhood as the tail dies away?".
//
// 1. STFT of the segment (4-term Blackman-Harris, ~170 ms frames = 8192 at
//    48 kHz, hop 1/4). Power pooled over 3 bins and averaged over
//    kRingingSmoothS in time (evens out beating), in dB.
// 2. Tail start: after the frame where the median bin level (100 Hz-10 kHz;
//    a median ignores any single tone) peaks, plus one frame length and the
//    smoothing half-width, so the input has stopped and only the tail is
//    measured. The input's own tonal content (a snare's 185 Hz body) starts
//    loud but then decays at the tank's rate like everything else, so it
//    does not "grow".
// 3. At up to 96 frames over the rest of the segment: each bin's
//    neighbourhood level = median of the bins within 1/3 octave (at least
//    +-10 bins), leaving out the bin's own +-4; prominence = bin - that.
// 4. Growth, per bin: over the frames where the neighbourhood is still a
//    real reference (within kRingingSpanDb of its own start and of the
//    tail's typical start level, 15 dB above its own floor, peak not yet
//    kRingingLeakDb clear), take the late half, and
//    fit a robust (Theil-Sen) line to prominence vs time. Growth = dB gained
//    over that late half. Counted only for narrow peaks (louder than +-3
//    bins) that end >= kRingingEndProminenceDb clear, climb steadily (the
//    late half's thirds rise in order: a bump is not growth) and last (decay
//    no faster than a T60 of kRingingMinT60Ratio x the tail's own T60). The late half only:
//    real tanks have an early phase where their mode peaks emerge from the
//    initial burst (the valleys drain first), then every mode decays
//    together; a Ringing mode keeps pulling away.
// 5. Steady tone, per bin: Ringing's end state (a self-oscillating loop, or
//    a mode so slow it left its neighbourhood behind and stopped "growing"):
//    >= kRingingSteadyProminenceDb clear, within kRingingSteadyRangeDb of
//    the tail's start level, decaying slower than kRingingSteadyDbPerS for
//    >= kRingingSteadyS. Score = its median prominence (capped at
//    kRingingLeakDb, "a bare tone").
// ringingDb = the largest score over 100 Hz-10 kHz; Ringing if >= kRingingGrowthDb.
struct RingingResult {
    double db = std::nan(""), ratio = std::nan(""), hz = std::nan(""), endDb = std::nan(""), spanS = std::nan("");
};

RingingResult ringingGrowth(const std::vector<float>& mono, size_t segStart, size_t segEnd, float sr, double tailT60)
{
    RingingResult r;
    size_t N = 256;
    while (double(N) * 1.5 < 0.171 * double(sr)) N <<= 1; // 8192 at 44.1/48 kHz, 16384 at 96 kHz
    const size_t hop = N / 4;
    const double hopS = double(hop) / double(sr);
    const size_t halfW = std::max<size_t>(1, size_t(std::lround(kRingingSmoothS / 2.0 / hopS)));
    if (segEnd > mono.size() || segEnd <= segStart || segEnd - segStart < N + (4 * halfW + 12) * hop) return r;
    const size_t F = (segEnd - segStart - N) / hop + 1;

    const double binHz = double(sr) / double(N);
    constexpr size_t kMinHalf = 10, kGuard = 4;
    const double kThirdHalf = std::pow(2.0, 1.0 / 6.0);
    const size_t kLo = std::max<size_t>(2, size_t(std::ceil(100.0 / binHz)));
    const size_t kHi = std::min(N / 2 - 2, size_t(std::floor(std::min(10000.0, 0.45 * double(sr)) / binHz)));
    if (kHi <= kLo + 2 * kMinHalf) return r;
    const size_t bLo = std::max<size_t>(2, std::min(kLo - std::min(kLo - 2, kMinHalf), size_t(double(kLo) / 1.13)));
    const size_t bHi = std::min(N / 2 - 2, std::max(kHi + kMinHalf, size_t(double(kHi) * 1.13) + 1));
    const size_t nb = bHi - bLo + 1;

    // Power per frame, pooled over 3 bins (a tone's Hann main lobe), then
    // averaged over ~kRingingSmoothS in time (evens out beating between
    // close modes and the echo pattern), in dB.
    // 4-term Blackman-Harris window (sidelobes -92 dB, main lobe +-4 bins):
    // a strong tone's own leakage stays far below the neighbourhood it is
    // compared with, so its growth can be followed to kRingingLeakDb.
    std::vector<float> win(N), re(N), im(N);
    for (size_t i = 0; i < N; ++i) {
        const double x = 2.0 * M_PI * double(i) / double(N);
        win[i] = float(0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x) - 0.01168 * std::cos(3 * x));
    }
    std::vector<float> pw(F * nb);
    for (size_t f = 0; f < F; ++f) {
        const float* x = mono.data() + segStart + f * hop;
        for (size_t i = 0; i < N; ++i) { re[i] = x[i] * win[i]; im[i] = 0.0f; }
        fft::transform(re, im, false);
        for (size_t j = 0; j < nb; ++j) {
            double p = 0.0;
            for (size_t k = bLo + j - 1; k <= bLo + j + 1; ++k) p += double(re[k]) * re[k] + double(im[k]) * im[k];
            pw[f * nb + j] = float(p);
        }
    }
    std::vector<float> lv(F * nb);
    std::vector<double> acc(nb, 0.0);
    for (size_t f = 0; f < F; ++f) {
        const size_t f0 = f > halfW ? f - halfW : 0, f1 = std::min(F - 1, f + halfW);
        for (size_t j = 0; j < nb; ++j) {
            double p = 0.0;
            for (size_t g = f0; g <= f1; ++g) p += double(pw[g * nb + j]);
            lv[f * nb + j] = float(10.0 * std::log10(std::max(p / double(f1 - f0 + 1), 1e-30)));
        }
    }
    std::vector<float> med(F), scratch;
    for (size_t f = 0; f < F; ++f) {
        scratch.assign(lv.begin() + long(f * nb + (kLo - bLo)), lv.begin() + long(f * nb + (kHi - bLo)) + 1);
        std::nth_element(scratch.begin(), scratch.begin() + long(scratch.size() / 2), scratch.end());
        med[f] = scratch[scratch.size() / 2];
    }
    // Tail start: after the loudest moment, plus one frame length and the
    // smoothing half-width, so no smoothed frame still contains the input.
    const size_t fp = size_t(std::max_element(med.begin(), med.end()) - med.begin());
    const size_t t0 = fp + 4 + halfW;
    if (t0 + 8 >= F) return r;

    // Analysis frames: up to kFrames spread over [t0, F).
    constexpr size_t kFrames = 96;
    const size_t t1 = F - 1;
    const size_t J = std::min<size_t>(kFrames, t1 - t0 + 1);
    const size_t nk = kHi - kLo + 1;
    std::vector<float> freq(nb), row(nb);
    for (size_t j = 0; j < nb; ++j) freq[j] = float(double(bLo + j) * binHz);
    std::vector<float> lev(J * nb), nbh(J * nk);
    std::vector<double> tj(J);
    for (size_t j = 0; j < J; ++j) {
        const size_t f = t0 + (J > 1 ? (j * (t1 - t0) + (J - 1) / 2) / (J - 1) : 0);
        tj[j] = double(f - t0) * hopS;
        std::copy(lv.begin() + long(f * nb), lv.begin() + long((f + 1) * nb), row.begin());
        std::copy(row.begin(), row.end(), lev.begin() + long(j * nb));
        // Neighbourhood = median of the bins within 1/3 octave (at least
        // +-kMinHalf bins), leaving out the peak's own +-kGuard bins (a tone's
        // pooled main lobe), so a strong tone never props up its own reference.
        for (size_t k = 0; k < nk; ++k) {
            const size_t c = k + kLo - bLo;
            const double fc = double(freq[c]);
            size_t lo = c, hi = c;
            while (lo > 0 && (double(freq[lo - 1]) >= fc / kThirdHalf || c - (lo - 1) <= kMinHalf)) --lo;
            while (hi + 1 < nb && (double(freq[hi + 1]) <= fc * kThirdHalf || (hi + 1) - c <= kMinHalf)) ++hi;
            scratch.clear();
            for (size_t i = lo; i <= hi; ++i)
                if (i + kGuard < c || i > c + kGuard) scratch.push_back(row[i]);
            std::nth_element(scratch.begin(), scratch.begin() + long(scratch.size() / 2), scratch.end());
            nbh[j * nk + k] = scratch[scratch.size() / 2];
        }
    }

    // Per bin: the frames where its neighbourhood is still within
    // kRingingSpanDb of where it started and well above its own floor (the
    // quietest it gets in the segment), so hum or spurs in a recording's
    // noise floor can't "grow" out of it. If the neighbourhood never drops
    // much (stationary signal, truncated IR), every frame counts. Then the
    // late half of those frames only: real tanks have an early phase where
    // the valleys between their modes drain fast (the initial broadband
    // burst), so every mode "grows" at first and then stops; a Ringing mode
    // keeps outliving its neighbours.
    // Theil-Sen slope (median of all pairwise slopes): a single bump (a
    // late echo tap, an edit at the end of an IR) can't tilt it the way it
    // tilts a least-squares line. Intercept: median of y - slope * t.
    std::vector<double> pairs;
    auto slopeOf = [&pairs](const std::vector<double>& t, const std::vector<double>& y, double& icpt) {
        pairs.clear();
        for (size_t i = 0; i < t.size(); ++i)
            for (size_t j = i + 1; j < t.size(); ++j)
                if (t[j] > t[i]) pairs.push_back((y[j] - y[i]) / (t[j] - t[i]));
        if (pairs.empty()) { icpt = y.empty() ? 0.0 : y[0]; return 0.0; }
        std::nth_element(pairs.begin(), pairs.begin() + long(pairs.size() / 2), pairs.end());
        const double sl = pairs[pairs.size() / 2];
        pairs.clear();
        for (size_t i = 0; i < t.size(); ++i) pairs.push_back(y[i] - sl * t[i]);
        std::nth_element(pairs.begin(), pairs.begin() + long(pairs.size() / 2), pairs.end());
        icpt = pairs[pairs.size() / 2];
        return sl;
    };
    double best = -1e9, bestLoud = -1e30, maxSpan = 0.0;
    std::vector<double> tt, yb, yn;
    const float audibleFloor = med[t0] - float(kRingingSteadyRangeDb);
    // No band is followed further than kRingingSpanDb below the tail's
    // typical level at its start (the median bin): a fast-dying high band
    // would otherwise be followed down to the float/dither floor, 100+ dB
    // down, where its prominence just wanders.
    const float spanFloor = med[t0] - float(kRingingSpanDb);
    for (size_t k = 0; k < nk; ++k) {
        const size_t jb = k + kLo - bLo; // index into lev rows
        auto promAt = [&](size_t j) { return double(lev[j * nb + jb]) - double(nbh[j * nk + k]); };
        float lo = 1e30f;
        for (size_t j = 0; j < J; ++j) lo = std::min(lo, nbh[j * nk + k]);
        const bool decays = nbh[k] - lo >= 25.0f;
        double score = -1e9, endP = 0.0, ratio = 1.0;

        // (1) Growth. Valid frames: the neighbourhood is still a real
        // reference, i.e. within kRingingSpanDb of where it started, well
        // above its own floor, and not yet down in this peak's own window
        // leakage (prominence < kRingingLeakDb).
        size_t last = 0;
        for (size_t j = 0; j < J; ++j) {
            const float v = nbh[j * nk + k];
            if (decays && (v < lo + 15.0f || v < nbh[k] - float(kRingingSpanDb))) break;
            if (v < spanFloor) break; // below the tail's audible range (see spanFloor)
            if (promAt(j) >= kRingingLeakDb) break;
            last = j;
        }
        const size_t first = last / 2;
        const double span = last >= first + 4 ? tj[last] - tj[first] : 0.0;
        maxSpan = std::max(maxSpan, span);
        bool peak = false;
        if (span >= kRingingMinSpanS) {
            // Narrowband peak: at least as loud as the bins around it (+-3)
            // on average over the late half.
            double own = 0.0, side = -1e30;
            for (int d = -3; d <= 3; ++d) {
                double m = 0.0;
                for (size_t j = first; j <= last; ++j) m += double(lev[j * nb + size_t(long(jb) + d)]);
                if (d == 0) own = m; else side = std::max(side, m);
            }
            peak = own >= side;
        }
        if (peak) {
            tt.clear(); yb.clear(); yn.clear();
            for (size_t j = first; j <= last; ++j) {
                tt.push_back(tj[j]);
                yb.push_back(promAt(j));
                yn.push_back(double(nbh[j * nk + k]));
            }
            double ip = 0.0, in = 0.0;
            const double sp = slopeOf(tt, yb, ip);
            const double e = ip + sp * tt.back();
            // Steady climb, not a bump: the median prominence of the late
            // half's first, middle and last thirds must rise in order (1 dB
            // slack). A Ringing mode keeps pulling away; a tail that merely
            // wobbles (two decay stages handing over, beating) goes up and
            // back down, which a straight line alone could read as growth.
            auto thirdMedian = [&yb](size_t part) {
                const size_t n = yb.size(), a = part * n / 3, b = (part + 1) * n / 3;
                std::vector<double> v(yb.begin() + long(a), yb.begin() + long(std::max(b, a + 1)));
                std::nth_element(v.begin(), v.begin() + long(v.size() / 2), v.end());
                return v[v.size() / 2];
            };
            const double m1 = thirdMedian(0), m2 = thirdMedian(1), m3 = thirdMedian(2);
            const bool climbs = m2 >= m1 - 1.0 && m3 >= m2 - 1.0;
            // ...and lasts: a narrow component that dies faster than half
            // the tail's own T60 can't be heard as Ringing (it is gone while
            // the body of the tail is still sounding). This drops the fast,
            // dark top bands, where decay stages handing over read as growth.
            const double sn = slopeOf(tt, yn, in), sb = sn + sp;
            const bool lasts = std::isnan(tailT60) || tailT60 <= 0.0 || sb >= -60.0 / (kRingingMinT60Ratio * tailT60);
            if (e >= kRingingEndProminenceDb && climbs && lasts) {
                score = sp * span;
                endP = e;
                ratio = sn < -0.5 ? (sb < -1e-3 ? std::min(99.0, sn / sb) : 99.0) : 1.0;
            }
        }

        // (2) Steady tone: Ringing's end state (a self-oscillating loop, a
        // mode so slow it has left its neighbourhood behind): the peak
        // stands >= kRingingSteadyProminenceDb clear of its neighbourhood,
        // within kRingingSteadyRangeDb of the tail's start level, and itself
        // decays slower than kRingingSteadyDbPerS, over >= kRingingSteadyS.
        // Checked on the last such stretch of frames.
        size_t runEnd = J, runStart = J;
        for (size_t j = J; j-- > 0;) {
            const bool on = promAt(j) >= kRingingSteadyProminenceDb && lev[j * nb + jb] >= audibleFloor;
            if (on && runEnd == J) runEnd = j + 1;
            if (on) runStart = j;
            else if (runEnd != J) break;
        }
        if (runEnd != J && tj[runEnd - 1] - tj[runStart] >= kRingingSteadyS) {
            tt.clear(); yb.clear();
            for (size_t j = runStart; j < runEnd; ++j) { tt.push_back(tj[j]); yb.push_back(double(lev[j * nb + jb])); }
            double ib = 0.0;
            if (slopeOf(tt, yb, ib) >= -kRingingSteadyDbPerS) {
                std::vector<double> ps;
                for (size_t j = runStart; j < runEnd; ++j) ps.push_back(promAt(j));
                std::nth_element(ps.begin(), ps.begin() + long(ps.size() / 2), ps.end());
                const double steady = std::min(ps[ps.size() / 2], kRingingLeakDb); // >= 70 dB reads "a bare tone"
                if (steady > score) {
                    score = steady;
                    endP = promAt(runEnd - 1);
                    ratio = 99.0;
                }
            }
        }
        // Ties (several bins of one bare tone all at the kRingingLeakDb cap):
        // report the loudest, i.e. the tone's own bin.
        const double loud = double(lev[(J - 1) * nb + jb]);
        score = std::min(score, kRingingLeakDb); // >= 70 dB reads "a bare tone"
        if (score > -1e8 && (score > best || (score == best && loud > bestLoud))) {
            best = score;
            bestLoud = loud;
            r.db = score;
            r.endDb = endP;
            r.hz = double(kLo + k) * binHz;
            r.ratio = ratio;
        }
    }
    r.spanS = maxSpan;
    if (best < -1e8) {
        if (maxSpan >= kRingingMinSpanS) r.db = 0.0; // measurable, nothing stands out
    }
    return r;
}

// ---- M6 Howl criterion (ADR 0019) ---------------------------------------------
// Only meaningful inside the KICKED Howl zone, where it replaces the Ringing
// test. Region = [start, end) (the sustained part). See Metrics.h.
struct HowlResult {
    double floorDb = std::nan(""), movePct = std::nan(""), moveDb = std::nan("");
    bool   ok = false;
};

HowlResult howlStats(const std::vector<float>& mono, size_t start, size_t end, float sr)
{
    HowlResult h;
    size_t N = 256;
    while (double(N) * 1.5 < 0.341 * double(sr)) N <<= 1; // 16384 at 48 kHz: ~2.9 Hz bins
    if (end > mono.size() || end <= start || end - start < N + size_t(2.0 * double(sr))) return h;
    const double binHz = double(sr) / double(N);

    // Broadband floor: 1/3-octave band energies 200 Hz-5 kHz of the average
    // power spectrum; median band re the strongest band.
    std::vector<double> p(N / 2 + 1, 0.0);
    for (size_t w = start; w + N <= end; w += N / 2) {
        const auto m = fft::magnitudeSpectrum(mono.data() + w, N, N);
        for (size_t k = 0; k < p.size(); ++k) p[k] += double(m[k]) * double(m[k]);
    }
    std::vector<double> bands;
    const double half = std::pow(2.0, 1.0 / 6.0);
    for (double f = 200.0; f <= 5000.0 * 1.001; f *= std::pow(2.0, 1.0 / 3.0)) {
        double e = 0.0;
        for (size_t k = size_t(f / half / binHz); k <= size_t(f * half / binHz) && k < p.size(); ++k) e += p[k];
        bands.push_back(e);
    }
    const double strongest = *std::max_element(bands.begin(), bands.end());
    std::nth_element(bands.begin(), bands.begin() + long(bands.size() / 2), bands.end());
    if (strongest <= 0.0) return h;
    h.floorDb = 10.0 * std::log10(std::max(bands[bands.size() / 2], 1e-300) / strongest);

    // Movement: strongest peak (100 Hz-8 kHz) every 0.25 s, parabolic
    // interpolation for frequency; each 2 s window (9 snapshots) must move
    // >= 0.5 % in frequency or >= 3 dB in level while the peak is audible.
    const size_t step = size_t(0.25 * double(sr));
    std::vector<double> hz, lvl;
    for (size_t w = start; w + N <= end; w += step) {
        const auto q = fft::magnitudeSpectrum(mono.data() + w, N, N);
        size_t b = size_t(100.0 / binHz);
        for (size_t k = b; k < size_t(8000.0 / binHz) && k + 1 < q.size(); ++k)
            if (q[k] > q[b]) b = k;
        const double ym = spectral::toDb(q[b - 1]), y0 = spectral::toDb(q[b]), yp = spectral::toDb(q[b + 1]);
        const double den = ym - 2.0 * y0 + yp;
        const double off = std::fabs(den) > 1e-9 ? 0.5 * (ym - yp) / den : 0.0;
        hz.push_back((double(b) + off) * binHz);
        lvl.push_back(y0 + double(spectral::toDb(4.0f / float(N)))); // ~ tone amplitude, dBFS (Hann)
    }
    const size_t W = 9;
    if (hz.size() < W) return h;
    double worst = 1e9;
    bool moving = true;
    for (size_t i = 0; i + W <= hz.size(); ++i) {
        double fMin = 1e18, fMax = 0, lMin = 1e18, lMax = -1e18;
        for (size_t j = i; j < i + W; ++j) {
            fMin = std::min(fMin, hz[j]); fMax = std::max(fMax, hz[j]);
            lMin = std::min(lMin, lvl[j]); lMax = std::max(lMax, lvl[j]);
        }
        const double fDev = (fMax - fMin) / std::max(fMin, 1.0) * 100.0, lDev = lMax - lMin;
        if (lMax < -30.0) continue; // no audible tone in this window
        const double score = std::max(fDev / kHowlMovePct, lDev / kHowlMoveDb);
        if (score < worst) { worst = score; h.movePct = fDev; h.moveDb = lDev; }
        if (score < 1.0) moving = false;
    }
    if (worst > 1e8) { h.movePct = h.moveDb = std::nan(""); } // never audible: nothing steady
    h.ok = h.floorDb >= kHowlFloorMinDb && moving;
    return h;
}

} // namespace

Metrics compute(const std::vector<std::vector<float>>& channels, float sampleRate,
                 const std::vector<std::vector<float>>* stereoSource)
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
    // T60 stops before the next event's build-up (decayEnd above); the
    // other segment metrics keep the full [first event, second event).
    const size_t t60End = events.size() > 1 ? decayEnd(mono, segStart, segEnd, sampleRate) : segEnd;
    m.t60S = schroederT60(mono, segStart, t60End, sampleRate);

    // Resonance segment starts 1 s after the first event (or 1 s into the
    // file when no event is found, e.g. analyzing a sustained reference
    // recording with --analyze).
    size_t resStart = events.empty() ? std::min(frames, size_t(1.0 * double(sampleRate)))
                                      : std::min(segEnd, events[0] + size_t(1.0 * double(sampleRate)));
    resStart = std::min(resStart, segEnd);
    m.resonancePeakDb = resonancePeakDb(mono, resStart, segEnd, sampleRate);

    m.steadyTone = steadyTone(mono, sampleRate);

    const RingingResult ring = ringingGrowth(mono, segStart, segEnd, sampleRate, m.t60S);
    m.ringingDb    = ring.db;
    m.ringingHz    = ring.hz;
    m.ringingRatio = ring.ratio;
    m.ringingEndDb = ring.endDb;
    m.ringingSpanS = ring.spanS;
    m.ringing      = !std::isnan(ring.db) && ring.db >= kRingingGrowthDb;

    // The Howl never really stops, so its region runs to the end of the file
    // (a dip under -40 dBFS mid-Howl would otherwise end the segment).
    const HowlResult howl = howlStats(mono, resStart, frames, sampleRate);
    m.howlFloorDb = howl.floorDb;
    m.howlMovePct = howl.movePct;
    m.howlMoveDb  = howl.moveDb;
    m.howlOk      = howl.ok;
    m.clickCount = clickCount(mono, sampleRate);
    m.maxStepDb100ms = maxStepDb100ms(mono, sampleRate, events);

    const auto& stereoCh = stereoSource ? *stereoSource : channels;
    if (stereoCh.size() >= 2 && !stereoCh[0].empty() && !stereoCh[1].empty()) {
        const auto& L = stereoCh[0];
        const auto& R = stereoCh[1];
        const size_t sFrames = std::min(L.size(), R.size());
        const size_t corrStart = std::min(segStart, sFrames), corrEnd = std::min(segEnd, sFrames);
        m.stereoCorrelation = pearsonCorrelation(L, R, corrStart, corrEnd);
        m.monoLossDb = monoLossDb(L, R);
        m.monoNotchDb = monoNotchDb(L, R, corrStart, corrEnd, sampleRate);
    }
    return m;
}

std::string summaryLine(const Metrics& m)
{
    char t60Buf[32], resBuf[32], corrBuf[32], lossBuf[32], notchBuf[32], stepBuf[32], ringBuf[64], howlBuf[64], buf[1024];
    if (std::isnan(m.ringingDb)) std::snprintf(ringBuf, sizeof ringBuf, "null");
    else std::snprintf(ringBuf, sizeof ringBuf, "%.1fdB@%.0fHz,x%.2f", m.ringingDb, m.ringingHz, m.ringingRatio);
    if (std::isnan(m.howlFloorDb)) std::snprintf(howlBuf, sizeof howlBuf, "null");
    else std::snprintf(howlBuf, sizeof howlBuf, "floor%.0fdB/move%.2f%%,%.1fdB", m.howlFloorDb, m.howlMovePct, m.howlMoveDb);
    if (std::isnan(m.t60S)) std::snprintf(t60Buf, sizeof t60Buf, "null");
    else std::snprintf(t60Buf, sizeof t60Buf, "%.2fs", m.t60S);
    if (std::isnan(m.resonancePeakDb)) std::snprintf(resBuf, sizeof resBuf, "null");
    else std::snprintf(resBuf, sizeof resBuf, "%.1fdB", m.resonancePeakDb);
    if (std::isnan(m.stereoCorrelation)) std::snprintf(corrBuf, sizeof corrBuf, "mono");
    else std::snprintf(corrBuf, sizeof corrBuf, "%.2f", m.stereoCorrelation);
    if (std::isnan(m.monoLossDb)) std::snprintf(lossBuf, sizeof lossBuf, "mono");
    else std::snprintf(lossBuf, sizeof lossBuf, "%.1fdB", m.monoLossDb);
    if (std::isnan(m.monoNotchDb)) std::snprintf(notchBuf, sizeof notchBuf, "mono");
    else std::snprintf(notchBuf, sizeof notchBuf, "%.1fdB", m.monoNotchDb);
    if (std::isnan(m.maxStepDb100ms)) std::snprintf(stepBuf, sizeof stepBuf, "null");
    else std::snprintf(stepBuf, sizeof stepBuf, "%.1fdB", m.maxStepDb100ms);
    std::snprintf(buf, sizeof buf,
        "peak=%.1fdBFS rms=%.1fdBFS t60=%s res=%s steady=%s nan/inf=%ld clip=%ld click=%ld "
        "corr=%s monoloss=%s notch=%s maxstep=%s ringing=%s%s howl=%s",
        m.peakDbfs, m.rmsDbfs, t60Buf, resBuf,
        m.steadyTone ? "true" : "false", m.nanInfCount, m.clipCount, m.clickCount,
        corrBuf, lossBuf, notchBuf, stepBuf, ringBuf, m.ringing ? "(RINGING)" : "", howlBuf);
    return std::string(buf);
}

} // namespace rv::metrics
