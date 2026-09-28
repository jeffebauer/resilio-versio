// Multi-spring Tank tests for M4 (SPEC §7 M4, docs/m4-contracts.md Stream D).
// Dependency-free: prints PASS/FAIL lines, returns nonzero on any failure.
//
// Stereo measurements are implemented here (not taken from host/common),
// following the Stream E definitions in docs/m4-contracts.md:
//   correlation   Pearson correlation of wet L and R over the tail
//   mono_loss_db  10·log10( power(L+R) / (power(L) + power(R)) ), whole file.
//                 Independent L/R -> 0 dB, identical -> +3 dB, cancelling -> very negative
//   mono notch    deepest dip, 200 Hz–5 kHz, of the mono-sum power spectrum
//                 relative to the stereo power spectrum (P_L + P_R), both
//                 1/3-octave smoothed, 8192-point Hann average over the tail.
//                 0 dB = "as loud as independent channels"; a comb notch shows
//                 up as a deep negative dip at its frequency.
//   max step      largest change in RMS dB between consecutive 100 ms
//                 windows, ignoring windows below -60 dBFS.

#include "dsp/Tank.h"
#include "params/Mappings.h"
#include "params/SpringModes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;
char msg[320];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;

constexpr float kFs = 48000.0f;
const char* const kModeName[3] = {"1 Spring", "2 Springs", "3 Springs"};

float springsValue(int mode) { return rv::switchToNormalised(mode); }

struct Settings {
    float decay = 0.6f, boing = 0.5f, tone = 0.5f, mix = 1.0f;
    int   mode  = 0;
};

void apply(rv::Tank& t, const Settings& s)
{
    t.setParam(rv::ParamId::Decay, s.decay);
    t.setParam(rv::ParamId::Boing, s.boing);
    t.setParam(rv::ParamId::Tone, s.tone);
    t.setParam(rv::ParamId::Mix, s.mix);
    t.setParam(rv::ParamId::Springs, springsValue(s.mode));
}

struct Stereo {
    Buf l, r;
};

// Mono input on both sides, fixed block size.
Stereo render(rv::Tank& t, const Buf& in, int block)
{
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int k = int(std::min(size_t(block), n - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
    }
    return o;
}

Stereo renderWith(const Settings& s, const Buf& in)
{
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, s);
    return render(t, in, 48);
}

Buf noise(size_t n, float amp, uint32_t seed)
{
    Buf b(n);
    rv::dsp::Rng rng;
    rng.seed(seed);
    for (auto& x : b) x = amp * rng.bipolar();
    return b;
}

// "Hits": four 30 ms noise bursts with a fast exponential decay (a snare-ish
// transient), 1 s apart, then silence. Returns the input; tailFrom gets the
// sample where the last burst has ended.
Buf hits(size_t n, size_t* tailFrom)
{
    Buf b(n, 0.0f);
    rv::dsp::Rng rng;
    rng.seed(4242u);
    const size_t len = size_t(0.030f * kFs);
    for (int h = 0; h < 4; ++h) {
        const size_t at = size_t(0.1f * kFs) + size_t(h) * size_t(kFs);
        for (size_t i = 0; i < len && at + i < n; ++i)
            b[at + i] = 0.5f * std::exp(-float(i) / (0.008f * kFs)) * rng.bipolar();
        if (tailFrom) *tailFrom = at + len;
    }
    return b;
}

double power(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return to > from ? s / double(to - from) : 0.0;
}

double db(double p) { return 10.0 * std::log10(p + 1e-30); }

bool allFinite(const Buf& x)
{
    for (float v : x)
        if (!std::isfinite(v)) return false;
    return true;
}

float peakAbs(const Buf& x)
{
    float p = 0;
    for (float v : x) p = std::max(p, std::fabs(v));
    return p;
}

// ---- Stereo metrics ------------------------------------------------------------
double correlation(const Buf& l, const Buf& r, size_t from, size_t to)
{
    double sl = 0, sr = 0, sll = 0, srr = 0, slr = 0;
    to = std::min(to, l.size());
    const double n = double(to - from);
    for (size_t i = from; i < to; ++i) {
        sl += l[i]; sr += r[i];
        sll += double(l[i]) * l[i]; srr += double(r[i]) * r[i]; slr += double(l[i]) * r[i];
    }
    const double cov = slr / n - (sl / n) * (sr / n);
    const double vl = sll / n - (sl / n) * (sl / n), vr = srr / n - (sr / n) * (sr / n);
    return vl > 0 && vr > 0 ? cov / std::sqrt(vl * vr) : 1.0;
}

double monoLossDb(const Buf& l, const Buf& r)
{
    double pm = 0, ps = 0;
    for (size_t i = 0; i < l.size(); ++i) {
        const double m = double(l[i]) + r[i];
        pm += m * m;
        ps += double(l[i]) * l[i] + double(r[i]) * r[i];
    }
    return db(pm / ps);
}

// In-place radix-2 FFT.
void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * 3.14159265358979 / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k]           = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Average power spectrum (8192-point Hann, 50 % overlap) over [from, to).
std::vector<double> powerSpectrum(const Buf& x, size_t from, size_t to)
{
    constexpr size_t N = 8192;
    std::vector<double> acc(N / 2 + 1, 0.0), win(N);
    for (size_t i = 0; i < N; ++i) win[i] = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * double(i) / double(N));
    std::vector<std::complex<double>> buf(N);
    for (size_t start = from; start + N <= std::min(to, x.size()); start += N / 2) {
        for (size_t i = 0; i < N; ++i) buf[i] = win[i] * double(x[start + i]);
        fft(buf);
        for (size_t k = 0; k <= N / 2; ++k) acc[k] += std::norm(buf[k]);
    }
    return acc;
}

// 1/3-octave smoothing: each bin becomes the mean power over f/2^(1/6) .. f·2^(1/6).
std::vector<double> thirdOctave(const std::vector<double>& p)
{
    const size_t n = p.size();
    std::vector<double> pre(n + 1, 0.0), out(n, 0.0);
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + p[i];
    const double r = std::pow(2.0, 1.0 / 6.0);
    for (size_t k = 1; k < n; ++k) {
        const size_t lo = std::max<size_t>(1, size_t(double(k) / r));
        const size_t hi = std::min(n - 1, size_t(std::ceil(double(k) * r)));
        out[k] = (pre[hi + 1] - pre[lo]) / double(hi + 1 - lo);
    }
    return out;
}

// Deepest dip (dB, negative = notch) of smoothed P(L+R) / (P(L) + P(R)) in
// 200 Hz–5 kHz. atHz gets its frequency.
double monoNotchDb(const Buf& l, const Buf& r, size_t from, size_t to, double* atHz)
{
    Buf m(l.size());
    for (size_t i = 0; i < l.size(); ++i) m[i] = l[i] + r[i];
    const auto pm = thirdOctave(powerSpectrum(m, from, to));
    const auto pl = thirdOctave(powerSpectrum(l, from, to));
    const auto pr = thirdOctave(powerSpectrum(r, from, to));
    double worst = 1e9;
    const double binHz = kFs / 8192.0;
    for (size_t k = size_t(200.0 / binHz); k <= size_t(5000.0 / binHz); ++k) {
        const double d = db(pm[k] / (pl[k] + pr[k]));
        if (d < worst) {
            worst = d;
            if (atHz) *atHz = double(k) * binHz;
        }
    }
    return worst;
}

// Largest |ΔRMS dB| between consecutive 100 ms windows (stereo power),
// ignoring windows below -60 dBFS.
double maxStepDb100ms(const Stereo& o, size_t from, size_t to)
{
    const size_t w = size_t(0.1f * kFs);
    double worst = 0, prev = 0;
    bool havePrev = false;
    for (size_t s = from; s + w <= std::min(to, o.l.size()); s += w) {
        const double p = 0.5 * (power(o.l, s, s + w) + power(o.r, s, s + w));
        const double d = db(p);
        if (d < -60.0) { havePrev = false; continue; }
        if (havePrev) worst = std::max(worst, std::fabs(d - prev));
        prev = d;
        havePrev = true;
    }
    return worst;
}

// Click detector as in test_spring (docs/m1-contracts.md): |second
// difference| > 20 dB above its local ±10 ms RMS and above 1e-3.
int countClicks(const Buf& x, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = size_t(0.010 * kFs);
    int clicks = 0;
    double worst = 0;
    for (size_t i = std::max<size_t>(from, half); i + half < n; ++i) {
        const double rms = std::sqrt((pre[i + half] - pre[i - half]) / double(2 * half));
        if (rms <= 0) continue;
        const double ratio = std::fabs(d2[i]) / rms;
        if (std::fabs(d2[i]) > 1e-3) worst = std::max(worst, ratio);
        if (ratio > 10.0 && std::fabs(d2[i]) > 1e-3) ++clicks;
    }
    if (maxRatio) *maxRatio = worst;
    return clicks;
}

// ---- 1. Detuning ---------------------------------------------------------------
void detuning()
{
    using rv::modes::kDetune;
    bool inRange = true, distinct = true;
    for (size_t i = 0; i < 3; ++i) {
        for (float f : {kDetune[i].loopDelay, kDetune[i].transition, kDetune[i].allpassCoeff}) {
            const float off = std::fabs(f - 1.0f);
            inRange &= off >= 0.029f && off <= 0.081f;
        }
        for (size_t j = i + 1; j < 3; ++j) {
            distinct &= std::fabs(kDetune[i].loopDelay / kDetune[j].loopDelay - 1.0f) > 0.03f;
            distinct &= std::fabs(kDetune[i].transition / kDetune[j].transition - 1.0f) > 0.03f;
            distinct &= std::fabs(kDetune[i].allpassCoeff / kDetune[j].allpassCoeff - 1.0f) > 0.03f;
        }
    }
    check(inRange, "Detune: every L, fC and a offset is within 3-8 % (SPEC §4.3)");
    check(distinct, "Detune: every pair of Springs differs by > 3 % in L, fC and a");

    // The Tank really applies them: running Springs have different L and K.
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, Settings{0.5f, 0.5f, 0.5f, 1.0f, 2});
    Buf in(480, 0.0f);
    render(t, in, 48);
    const float la = t.spring(0).loopDelaySamples(), lb = t.spring(1).loopDelaySamples(),
                lc = t.spring(2).loopDelaySamples();
    std::snprintf(msg, sizeof msg, "Detune applied: L = %.0f / %.0f / %.0f samples, K = %.2f / %.2f / %.2f", la, lb, lc,
                  t.spring(0).stretchK(), t.spring(1).stretchK(), t.spring(2).stretchK());
    check(la != lb && lb != lc && la != lc && t.spring(0).stretchK() != t.spring(1).stretchK(), msg);

    // Modes don't line up: the Loop's resonant frequencies (round-trip phase
    // = whole cycles) of two Springs rarely coincide. The phase in cycles is
    // the integral of the round-trip (group) delay over frequency. Count,
    // 100 Hz–2 kHz, modes of B and C that sit within 1 Hz of a mode of A.
    // With identical Springs it would be all of them.
    auto modeFreqs = [](const rv::Spring& s) {
        std::vector<double> f;
        double cycles = 0;
        const double dHz = 0.05;
        for (double hz = dHz; hz < 2000.0; hz += dHz) {
            const double next = cycles + double(s.roundTripSamples(float(hz))) / kFs * dHz;
            if (hz > 100.0 && std::floor(next) != std::floor(cycles)) f.push_back(hz);
            cycles = next;
        }
        return f;
    };
    const auto fa = modeFreqs(t.spring(0));
    int shared = 0, total = 0;
    for (int other = 1; other < 3; ++other) {
        for (double f : modeFreqs(t.spring(other))) {
            ++total;
            for (double g : fa)
                if (std::fabs(f - g) < 1.0) { ++shared; break; }
        }
    }
    // Random placement would share ~2 Hz / mode spacing of them (~20-30 %).
    std::snprintf(msg, sizeof msg, "Detune: %d of %d Loop modes of B, C within 1 Hz of a mode of A (<= 35 %%)", shared,
                  total);
    check(total > 0 && shared * 100 <= 35 * total, msg);
}

// ---- 2. Level match ----------------------------------------------------------------
void levelMatch()
{
    size_t tailFrom = 0;
    const Buf in = hits(size_t(6.0f * kFs), &tailFrom);
    bool ok = true;
    for (float decay : {0.1f, 0.6f, 1.0f})
        for (float boing : {0.0f, 1.0f}) {
            double lev[3];
            for (int m = 0; m < 3; ++m) {
                const Stereo o = renderWith(Settings{decay, boing, 0.5f, 1.0f, m}, in);
                lev[m] = db(0.5 * (power(o.l, 0, o.l.size()) + power(o.r, 0, o.r.size())));
            }
            const double lo = std::min({lev[0], lev[1], lev[2]}), hi = std::max({lev[0], lev[1], lev[2]});
            std::snprintf(msg, sizeof msg,
                          "Level DECAY %.1f BOING %.0f: 1/2/3 Springs %.2f / %.2f / %.2f dB (2 and 3 vs 1: %+.2f, %+.2f; "
                          "spread %.2f dB, limit +-1.5)",
                          decay, boing, lev[0], lev[1], lev[2], lev[1] - lev[0], lev[2] - lev[0], hi - lo);
            const bool good = std::fabs(lev[1] - lev[0]) <= 1.5 && std::fabs(lev[2] - lev[0]) <= 1.5
                           && std::fabs(lev[2] - lev[1]) <= 1.5;
            check(good, msg);
            ok &= good;
        }
    (void)ok;
}

// ---- 3. Width and mono safety ---------------------------------------------------
void stereoWidthAndMono()
{
    size_t tailFrom = 0;
    const Buf in = hits(size_t(6.0f * kFs), &tailFrom);
    for (float decay : {0.2f, 0.6f, 1.0f})
        for (int m = 0; m < 3; ++m) {
            const Stereo o = renderWith(Settings{decay, 0.5f, 0.5f, 1.0f, m}, in);
            // Tail segment: from the end of the last hit until the tail has
            // dropped 40 dB (or the file ends), like the T60 segment.
            const size_t w = size_t(0.05f * kFs);
            const double ref = power(o.l, tailFrom, tailFrom + w) + power(o.r, tailFrom, tailFrom + w);
            size_t tailTo = tailFrom + w;
            while (tailTo + w <= o.l.size()
                   && power(o.l, tailTo, tailTo + w) + power(o.r, tailTo, tailTo + w) > ref * 1e-4)
                tailTo += w;
            // Also include the earlier hits' tails: the whole file from the first hit.
            const size_t from = size_t(0.1f * kFs);
            const double corrTail = correlation(o.l, o.r, tailFrom, tailTo);
            const double corrAll  = correlation(o.l, o.r, from, tailTo);
            const double loss     = monoLossDb(o.l, o.r);
            double notchHz        = 0;
            const double notch    = monoNotchDb(o.l, o.r, from, tailTo, &notchHz);
            std::snprintf(msg, sizeof msg,
                          "Stereo %s DECAY %.1f: tail corr %.2f (from first hit %.2f), mono_loss %+.2f dB, deepest mono "
                          "dip %+.1f dB at %.0f Hz",
                          kModeName[m], decay, corrTail, corrAll, loss, notch, notchHz);
            const bool wide = m == 0 || corrTail < 0.5; // width target is for 2 and 3 Springs
            check(wide && loss >= -1.5 && notch >= -6.0, msg);
        }
}

// ---- 4. SPRINGS switching is click-free ------------------------------------------
void switchingClickFree()
{
    // Two tails: a held one (continuous -20 dB noise) and a decaying one
    // (hits, switch 400 ms into the last tail). All 6 transitions, both
    // channels, click detector on everything after the switch.
    const size_t n = size_t(5.0f * kFs);
    const Buf held = noise(n, 0.1f, 3u);
    size_t tailFrom = 0;
    const Buf hit = hits(n, &tailFrom);
    for (int from = 0; from < 3; ++from)
        for (int to = 0; to < 3; ++to) {
            if (from == to) continue;
            int clicks = 0;
            double worst = 0, gapDb = 0;
            for (int input = 0; input < 2; ++input) {
                const Buf& in = input == 0 ? held : hit;
                const size_t at = input == 0 ? size_t(2.0f * kFs) : tailFrom + size_t(0.4f * kFs);
                rv::Tank t;
                t.prepare(kFs, 16);
                apply(t, Settings{0.6f, 0.5f, 0.5f, 1.0f, from});
                Stereo o{Buf(n), Buf(n)};
                for (size_t pos = 0; pos < n; pos += 16) {
                    if (pos == (at / 16) * 16) t.setParam(rv::ParamId::Springs, springsValue(to));
                    t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
                }
                for (const Buf* ch : {&o.l, &o.r}) {
                    double r = 0;
                    clicks += countClicks(*ch, at - size_t(0.1f * kFs), &r);
                    worst = std::max(worst, r);
                }
                if (input == 0) {
                    // Held tail: no side drops out or jumps across the fade
                    // (50 ms before vs the 50 ms window centred on the fade).
                    const size_t w = size_t(0.05f * kFs);
                    for (const Buf* ch : {&o.l, &o.r})
                        gapDb = std::max(gapDb, std::fabs(db(power(*ch, at, at + w) / power(*ch, at - w, at))));
                }
            }
            std::snprintf(msg, sizeof msg,
                          "SPRINGS %d -> %d mid-tail (held + decaying): %d clicks (worst ratio %.1f, limit 10), held "
                          "level change across fade %.1f dB (limit 2)",
                          from + 1, to + 1, clicks, worst, gapDb);
            check(clicks == 0 && gapDb < 2.0, msg);
        }

    // Rapid flipping (every 5 ms, faster than the fade) stays click-free.
    rv::Tank t;
    t.prepare(kFs, 16);
    apply(t, Settings{0.6f, 0.5f, 0.5f, 1.0f, 0});
    Stereo o{Buf(n), Buf(n)};
    int flip = 0;
    for (size_t pos = 0; pos < n; pos += 16) {
        if (pos >= size_t(kFs) && pos < size_t(2.0f * kFs) && pos % 240 == 0)
            t.setParam(rv::ParamId::Springs, springsValue(++flip % 3));
        t.process(held.data() + pos, held.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
    }
    double r1 = 0, r2 = 0;
    const int c = countClicks(o.l, size_t(0.9f * kFs), &r1) + countClicks(o.r, size_t(0.9f * kFs), &r2);
    std::snprintf(msg, sizeof msg, "SPRINGS flipped every 5 ms for 1 s: %d clicks (worst ratio %.1f)", c,
                  std::max(r1, r2));
    check(c == 0, msg);
}

// ---- 5. DECAY sweep with 1/2/3 Springs ---------------------------------------------
void decaySweep()
{
    const size_t n = size_t(5.0f * kFs), sweepFrom = size_t(0.5f * kFs), sweepLen = size_t(4.0f * kFs);
    const Buf in = noise(n, 0.1f, 11u);
    for (int m = 0; m < 3; ++m) {
        rv::Tank t;
        t.prepare(kFs, 16);
        apply(t, Settings{0.0f, 0.5f, 0.5f, 1.0f, m});
        Stereo o{Buf(n), Buf(n)};
        float lPrev = 0, maxJump = 0;
        for (size_t pos = 0; pos < n; pos += 16) {
            if (pos >= sweepFrom) t.setParam(rv::ParamId::Decay, std::min(1.0f, float(pos - sweepFrom) / float(sweepLen)));
            t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
            // Pitch bend smoothness: L moves by at most the slew limit per sample.
            const float l = t.spring(0).loopDelaySamples();
            if (pos > 0) maxJump = std::max(maxJump, std::fabs(l - lPrev) / 16.0f);
            lPrev = l;
        }
        double r1 = 0, r2 = 0;
        const int clicks = countClicks(o.l, sweepFrom, &r1) + countClicks(o.r, sweepFrom, &r2);
        const double step = maxStepDb100ms(o, sweepFrom, n);
        std::snprintf(msg, sizeof msg,
                      "DECAY 0->1 in 4 s, %s: %d clicks (worst ratio %.1f), max 100 ms step %.2f dB (limit 3), L slew "
                      "%.3f samples/sample (limit %.2f)",
                      kModeName[m], clicks, std::max(r1, r2), step, maxJump, rv::Spring::kLoopSlewPerSample);
        check(clicks == 0 && step <= 3.0 && maxJump <= rv::Spring::kLoopSlewPerSample + 1e-4f, msg);
    }
}

// ---- 6. Stability grid with SPRINGS ------------------------------------------------
void stabilityGrid()
{
    const size_t sec = size_t(kFs);
    int bad = 0, cells = 0;
    float worstPeak = 0;
    for (int m = 0; m < 3; ++m)
        for (float d : {0.0f, 0.5f, 1.0f})
            for (float b : {0.0f, 0.5f, 1.0f})
                for (float tn : {0.0f, 0.5f, 1.0f})
                    for (int input = 0; input < 2; ++input) {
                        Buf in;
                        if (input == 0) {
                            in.assign(6 * sec, 0.0f);
                            in[0] = 1.0f;
                        } else {
                            in = noise(6 * sec, 1.0f, 99u);
                            std::fill(in.begin() + long(sec), in.end(), 0.0f);
                        }
                        const Stereo o = renderWith(Settings{d, b, tn, 1.0f, m}, in);
                        const float pk = std::max(peakAbs(o.l), peakAbs(o.r));
                        worstPeak = std::max(worstPeak, pk);
                        const size_t start = input == 0 ? sec / 2 : sec + sec / 2;
                        bool falls = true;
                        for (const Buf* ch : {&o.l, &o.r}) {
                            double prev = power(*ch, start, start + sec / 2);
                            for (size_t w = start + sec / 2; w + sec / 2 <= ch->size(); w += sec / 2) {
                                const double e = power(*ch, w, w + sec / 2);
                                if (e > prev * 1.26 && e > 1e-16) falls = false;
                                prev = e;
                            }
                        }
                        const bool lower = power(o.l, 5 * sec, 6 * sec) < power(o.l, start, start + sec)
                                        && power(o.r, 5 * sec, 6 * sec) < power(o.r, start, start + sec);
                        const bool good = allFinite(o.l) && allFinite(o.r) && pk < 1.0f && falls && lower;
                        ++cells;
                        if (!good) {
                            ++bad;
                            std::printf("      grid fail: %s decay %.1f boing %.1f tone %.1f %s peak %.3f falls %d "
                                        "lower %d\n",
                                        kModeName[m], d, b, tn, input ? "noise" : "impulse", pk, falls, lower);
                        }
                    }
    std::snprintf(msg, sizeof msg,
                  "Stability SPRINGS x DECAY x BOING x TONE (%d cells, impulse + 1 s full-scale noise): finite, peak < 1 "
                  "(worst %.3f), decaying (%d bad)",
                  cells, worstPeak, bad);
    check(bad == 0, msg);
}

// ---- 7. Determinism (with a SPRINGS change mid-render) -----------------------------
Buf deterministicRender(rv::Tank& tank, const Buf& in, int block)
{
    const size_t change1 = 30000, change2 = 60000;
    apply(tank, Settings{0.7f, 0.3f, 0.6f, 0.8f, 0});
    const size_t n = in.size();
    Buf l(n), r(n), out(2 * n);
    size_t pos = 0;
    while (pos < n) {
        if (pos == change1) apply(tank, Settings{0.2f, 0.9f, 0.3f, 0.6f, 2});
        if (pos == change2) tank.setParam(rv::ParamId::Springs, springsValue(1));
        size_t k = std::min(size_t(block), n - pos);
        for (size_t c : {change1, change2})
            if (pos < c && pos + k > c) k = c - pos;
        if (pos <= 40000 && pos + k > 40000) tank.kick(int(40000 - pos));
        tank.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, int(k));
        pos += k;
    }
    std::copy(l.begin(), l.end(), out.begin());
    std::copy(r.begin(), r.end(), out.begin() + long(n));
    return out;
}

void determinism()
{
    Buf in = noise(size_t(2.0f * kFs), 0.5f, 7u);
    std::fill(in.begin() + 12000, in.end(), 0.0f);
    in[20000] = 1.0f;

    rv::Tank tank;
    tank.prepare(kFs, 512);
    const Buf first = deterministicRender(tank, in, 48);
    tank.reset();
    const Buf second = deterministicRender(tank, in, 48);
    check(first == second, "Determinism: SPRINGS 1 -> 3 -> 2 + Kick, same input twice (reset() between) is bit-identical");

    bool same = true;
    for (int block : {1, 7, 48, 512}) {
        rv::Tank t;
        t.prepare(kFs, 512);
        same &= deterministicRender(t, in, block) == first;
    }
    check(same, "Determinism: SPRINGS changes + Kick, block sizes 1, 7, 48, 512 are bit-identical");
}

// ---- 8. Kick reaches every Spring -------------------------------------------------
void kickReachesAllSprings()
{
    for (int m = 0; m < 3; ++m) {
        const size_t n = size_t(kFs);
        Buf silence(n, 0.0f), imp(n, 0.0f);
        imp[1000] = rv::Tank::kKickPlaceholder;
        rv::Tank a, b;
        a.prepare(kFs, 64);
        b.prepare(kFs, 64);
        apply(a, Settings{0.6f, 0.5f, 0.5f, 1.0f, m});
        apply(b, Settings{0.6f, 0.5f, 0.5f, 1.0f, m});
        Stereo ok{Buf(n), Buf(n)};
        for (size_t pos = 0; pos < n; pos += 64) {
            if (pos <= 1000 && pos + 64 > 1000) a.kick(int(1000 - pos));
            a.process(silence.data() + pos, silence.data() + pos, ok.l.data() + pos, ok.r.data() + pos, 64);
        }
        const Stereo ref = render(b, imp, 64);
        const bool both = power(ok.l, 0, n) > 0 && power(ok.r, 0, n) > 0;
        std::snprintf(msg, sizeof msg, "Kick, %s: == input impulse (bit-identical), heard on L and R", kModeName[m]);
        check(ok.l == ref.l && ok.r == ref.r && both, msg);
    }
}

// ---- 9. CPU and memory report ------------------------------------------------------
void performance()
{
    const size_t n = size_t(10.0f * kFs);
    const Buf in = noise(n, 0.3f, 5u);
    Buf l(n), r(n);
    // Daisy estimate, same method as M1 (test_spring): Cortex-M7 @ 480 MHz
    // assumed 15–25x slower per sample than this desktop.
    for (int m = 0; m < 3; ++m) {
        rv::Tank t;
        t.prepare(kFs, 48);
        apply(t, Settings{1.0f, 1.0f, 1.0f, 0.5f, m});
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos < n; pos += 48)
            t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n);
        int stages = 0;
        for (int s = 0; s < 3; ++s) stages += int(t.spring(s).activeStages());
        std::printf("INFO  %s, DECAY/BOING/TONE 1: %d stages total (cap %d/Spring, idle %d), %.1f ns/sample desktop, "
                    "est. Daisy %.0f-%.0f cycles/sample (%.0f-%.0f%% of 10k budget)\n",
                    kModeName[m], stages, rv::modes::kStageCap[size_t(m)], rv::modes::kIdleStages, ns, ns * 15 * 0.48,
                    ns * 25 * 0.48, ns * 15 * 0.48 / 100, ns * 25 * 0.48 / 100);
    }
    rv::Tank t;
    t.prepare(kFs, 48);
    std::printf("INFO  Tank memory: %zu bytes at 48 kHz (object %zu + pool), %zu bytes at 96 kHz\n", t.memoryBytes(),
                sizeof(rv::Tank), sizeof(rv::Tank) + rv::Tank::requiredPoolFloats(96000.0f) * sizeof(float));
}

} // namespace

int main()
{
    detuning();
    levelMatch();
    stereoWidthAndMono();
    switchingClickFree();
    decaySweep();
    kickReachesAllSprings();
    determinism();
    stabilityGrid();
    performance();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
