// Spring / Tank DSP tests for M1 (SPEC §7 M1). Dependency-free: prints
// PASS/FAIL lines, returns nonzero on any failure.
//
// Conventions: "IR" = impulse response (a single 1.0 sample on both inputs,
// MIX 1 = wet only). Spring-only tests drive rv::Spring directly with the
// high path muted, so they see the low-chirp Loop alone.

#include "dsp/Spring.h"
#include "dsp/Tank.h"
#include "params/Mappings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;

struct Settings {
    float decay = 0.5f, tension = 0.5f, tone = 0.5f, mix = 1.0f;
};

void apply(rv::Tank& t, const Settings& s)
{
    t.setParam(rv::ParamId::Decay, s.decay);
    t.setParam(rv::ParamId::Tension, s.tension);
    t.setParam(rv::ParamId::Tone, s.tone);
    t.setParam(rv::ParamId::Mix, s.mix);
}

// Mono input on both sides -> returns (L+R)/2 of the output, optionally L and R.
Buf renderTank(rv::Tank& t, const Buf& in, int block, Buf* outL = nullptr, Buf* outR = nullptr)
{
    const size_t n = in.size();
    Buf l(n), r(n), mono(n);
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int k = int(std::min(size_t(block), n - pos));
        t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, k);
    }
    for (size_t i = 0; i < n; ++i) mono[i] = 0.5f * (l[i] + r[i]);
    if (outL) *outL = l;
    if (outR) *outR = r;
    return mono;
}

Buf impulse(size_t n)
{
    Buf b(n, 0.0f);
    b[0] = 1.0f;
    return b;
}

Buf noise(size_t n, float amp, uint32_t seed)
{
    Buf b(n);
    rv::dsp::Rng rng;
    rng.seed(seed);
    for (auto& x : b) x = amp * rng.bipolar();
    return b;
}

// Tank-level impulse response.
Buf tankIR(float fs, const Settings& s, float seconds)
{
    rv::Tank t;
    t.prepare(fs, 48);
    apply(t, s);
    return renderTank(t, impulse(size_t(seconds * fs)), 48);
}

// Spring alone, high path muted, settings straight from the mappings.
struct SpringRig {
    std::vector<float> pool;
    rv::Spring spring;
    rv::SpringSettings settings;

    SpringRig(float fs, float decay, float tension, float tone)
    {
        pool.assign(rv::Spring::requiredFloats(fs), 0.0f);
        spring.prepare(fs, pool.data(), 1u);
        settings.loopDelaySeconds = rv::map::tensionLoopDelaySeconds(tension);
        settings.t60Seconds       = rv::map::decayT60Seconds(decay);
        settings.transitionHz     = rv::map::tensionTransitionHz(tension);
        settings.allpassCoeff     = rv::map::tensionCoefficient(tension);
        settings.stages           = rv::map::tensionStages(tension);
        settings.dampingHz        = rv::map::toneDampingHz(tone);
        settings.highPathLevel    = 0.0f;
        spring.setSettings(settings, true);
    }
    Buf ir(size_t n)
    {
        Buf in = impulse(n), out(n);
        spring.process(in.data(), out.data(), int(n));
        return out;
    }
};

// RBJ constant-peak band-pass, run twice for a steeper skirt.
Buf bandpass(const Buf& x, float fs, float lo, float hi)
{
    const float f0 = std::sqrt(lo * hi), q = f0 / (hi - lo);
    const float w = 2.0f * rv::map::kPi * f0 / fs, alpha = std::sin(w) / (2.0f * q), a0 = 1.0f + alpha;
    const float b0 = alpha / a0, b2 = -alpha / a0, a1 = -2.0f * std::cos(w) / a0, a2 = (1.0f - alpha) / a0;
    Buf y = x;
    for (int pass = 0; pass < 2; ++pass) {
        double s1 = 0, s2 = 0;
        for (auto& v : y) {
            const double in = v, out = b0 * in + s1;
            s1 = -a1 * out + s2;
            s2 = b2 * in - a2 * out;
            v  = float(out);
        }
    }
    return y;
}

// Energy-weighted arrival time (seconds) of x² in [0, end).
double centroidSeconds(const Buf& x, size_t end, float fs)
{
    double num = 0, den = 0;
    for (size_t i = 0; i < std::min(end, x.size()); ++i) {
        const double e = double(x[i]) * x[i];
        num += e * double(i);
        den += e;
    }
    return den > 0 ? num / den / fs : 0.0;
}

// Schroeder backward integration; line fit from -5 to -35 dB, extrapolated to 60 dB.
double schroederT60(const Buf& x, float fs)
{
    std::vector<double> edc(x.size());
    double acc = 0;
    for (size_t i = x.size(); i-- > 0;) {
        acc += double(x[i]) * x[i];
        edc[i] = acc;
    }
    if (acc <= 0) return 0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (size_t i = 0; i < edc.size(); ++i) {
        const double db = 10.0 * std::log10(edc[i] / edc[0] + 1e-300);
        if (db > -5.0) continue;
        if (db < -35.0) break;
        const double t = double(i) / fs;
        sx += t; sy += db; sxx += t * t; sxy += t * db;
        ++n;
    }
    if (n < 10) return 0;
    const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx); // dB per second
    return slope < 0 ? -60.0 / slope : 0;
}

// Echo repeat time (seconds) in a band: the lag in [lo, hi] s maximising the
// autocorrelation of the smoothed band energy.
double repeatSeconds(const Buf& x, float fs, float bandLo, float bandHi, double lagLo, double lagHi)
{
    const Buf b = bandpass(x, fs, bandLo, bandHi);
    std::vector<double> env(b.size());
    const double c = 1.0 - std::exp(-2.0 * 3.14159265 * 300.0 / fs);
    double e = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        e += c * (double(b[i]) * b[i] - e);
        env[i] = e;
    }
    const int l0 = int(lagLo * fs), l1 = int(lagHi * fs);
    std::vector<double> ac(size_t(l1 + 2), 0.0);
    for (int lag = l0 - 1; lag <= l1 + 1; ++lag) {
        double s = 0;
        for (size_t i = 0; i + size_t(lag) < env.size(); ++i) s += env[i] * env[i + size_t(lag)];
        ac[size_t(lag)] = s;
    }
    int best = l0;
    for (int lag = l0; lag <= l1; ++lag)
        if (ac[size_t(lag)] > ac[size_t(best)]) best = lag;
    // Parabolic refinement.
    const double ym = ac[size_t(best - 1)], y0 = ac[size_t(best)], yp = ac[size_t(best + 1)];
    const double den = ym - 2 * y0 + yp;
    const double off = den != 0 ? 0.5 * (ym - yp) / den : 0.0;
    return (best + off) / fs;
}

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

double energy(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < std::min(to, x.size()); ++i) s += double(x[i]) * x[i];
    return s;
}

// Click detector (same idea as the Renderer metric, docs/m1-contracts.md):
// sample-to-sample discontinuity |x[n] - 2x[n-1] + x[n-2]| more than 20 dB
// (x10) above the local ±10 ms RMS of that same second difference, and above
// -60 dBFS. Returns the count; maxRatio gets the worst ratio seen.
int countClicks(const Buf& x, float fs, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = size_t(0.010 * fs);
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

char msg[256];

// ---- 1. Chirp present, in the direction map::kChirpDirection asks for ---------
// LowsLater: in the first echo the high band (0.5-0.85 fC) arrives before
// 200-500 Hz. HighsLater: the high band (0.8-0.97 fC: a rising Chirp's delay
// piles up toward fC, and at TENSION 0 the whole ~3 ms Chirp lives there)
// arrives after it. `lag` is how much later the late band
// arrives, so both directions share one check.
void chirpHighsBeforeLows()
{
    const float fs = 48000.0f, decay = 0.5f;
    const double dir = rv::map::kHighsLater ? -1.0 : 1.0; // +1: lows later
    const char* late = rv::map::kHighsLater ? "highs" : "lows";
    const float hiLo = rv::map::kHighsLater ? 0.8f : 0.5f, hiHi = rv::map::kHighsLater ? 0.97f : 0.85f;
    const float hiRef = rv::map::kHighsLater ? 0.88f : 0.67f; // prediction's reference in the high band
    for (float tension : {0.0f, 0.5f, 1.0f}) {
        SpringRig rig(fs, decay, tension, 0.5f);
        const float L   = rig.spring.loopDelaySamples();
        const float fC  = rv::map::tensionTransitionHz(tension);
        const Buf   ir  = rig.ir(size_t(0.5f * fs));
        // First echo: from the pickup tap (L/2) until the second echo's
        // fastest part could arrive (L/2 + L).
        const size_t end = size_t(1.5f * L);
        const double tHi = centroidSeconds(bandpass(ir, fs, hiLo * fC, hiHi * fC), end, fs);
        const double tLo = centroidSeconds(bandpass(ir, fs, 200.0f, 500.0f), end, fs);
        const double predicted = dir * (rig.spring.chainGroupDelaySamples(316.0f)
                                        - rig.spring.chainGroupDelaySamples(hiRef * fC)) / fs;
        const double lag = dir * (tLo - tHi);
        std::snprintf(msg, sizeof msg,
                      "Chirp TENSION %.1f (Spring): high band %.0f-%.0f Hz at %.1f ms, 200-500 Hz at %.1f ms "
                      "(%s later by %.1f ms, mapping predicts %.1f ms)",
                      tension, hiLo * fC, hiHi * fC, tHi * 1e3, tLo * 1e3, late, lag * 1e3, predicted * 1e3);
        check(lag > 0.001 && lag > 0.4 * predicted, msg);

        // Same check through the whole Tank (high path, decorrelator, output stage).
        Settings s;
        s.decay = decay;
        s.tension = tension;
        const Buf t = tankIR(fs, s, 0.5f);
        const double tHiT = centroidSeconds(bandpass(t, fs, hiLo * fC, hiHi * fC), end, fs);
        const double tLoT = centroidSeconds(bandpass(t, fs, 200.0f, 500.0f), end, fs);
        std::snprintf(msg, sizeof msg, "Chirp TENSION %.1f (Tank): highs at %.1f ms, lows at %.1f ms", tension,
                      tHiT * 1e3, tLoT * 1e3);
        check(dir * (tLoT - tHiT) > 0.001, msg);
    }
}

// ---- 2. Regular repeat at the round-trip time --------------------------------
// "Expected" = L + chain group delay + filter delay at the band centre, from
// the mapping (Spring::roundTripSamples). Band: 800–1250 Hz (centre 1 kHz).
double measureRepeat(float fs, float decay, float tension, double* expected)
{
    SpringRig rig(fs, decay, tension, 0.5f);
    const double rt = rig.spring.roundTripSamples(1000.0f) / fs;
    const Buf ir    = rig.ir(size_t((0.3 + 8 * rt) * fs));
    if (expected) *expected = rt;
    return repeatSeconds(ir, fs, 800.0f, 1250.0f, 0.6 * rt, 1.4 * rt);
}

// DECAY sets tail length only (ADR 0026): the repeat is TENSION's alone, the
// same at every DECAY.
void repeatMatchesRoundTrip()
{
    const float fs = 48000.0f;
    double byDecay[2][3] = {};
    for (int di = 0; di < 3; ++di) {
        const float decay = 0.5f * float(di);
        for (int ti = 0; ti < 2; ++ti) {
            const float tension = float(ti);
            double expected = 0;
            const double got = measureRepeat(fs, decay, tension, &expected);
            byDecay[ti][di] = got;
            const double L = rv::map::tensionLoopDelaySeconds(tension);
            std::snprintf(msg, sizeof msg,
                          "Repeat DECAY %.1f TENSION %.1f: %.2f ms measured, %.2f ms expected (L %.1f ms + chain/filters "
                          "%.2f ms @1 kHz)",
                          decay, tension, got * 1e3, expected * 1e3, L * 1e3, (expected - L) * 1e3);
            check(std::fabs(got / expected - 1.0) < 0.05, msg);
        }
    }
    for (int ti = 0; ti < 2; ++ti) {
        const double lo = std::min({byDecay[ti][0], byDecay[ti][1], byDecay[ti][2]});
        const double hi = std::max({byDecay[ti][0], byDecay[ti][1], byDecay[ti][2]});
        std::snprintf(msg, sizeof msg, "DECAY leaves the tank alone (ADR 0026): TENSION %d repeat %.2f .. %.2f ms over DECAY 0 / 0.5 / 1 "
                      "(within 1 %%)", ti, lo * 1e3, hi * 1e3);
        check(hi / lo - 1.0 < 0.01, msg);
    }
}

// ---- 3. T60 ------------------------------------------------------------------
double tankT60(float fs, float decay)
{
    Settings s;
    s.decay = decay;
    const float target = rv::map::decayT60Seconds(decay);
    return schroederT60(tankIR(fs, s, std::max(2.0f, 1.6f * target + 1.0f)), fs);
}

void t60MatchesDecay()
{
    const float fs = 48000.0f;
    const double t0 = tankT60(fs, 0.0f), t5 = tankT60(fs, 0.5f), t1 = tankT60(fs, 1.0f);
    std::snprintf(msg, sizeof msg, "T60 DECAY 0 = %.3f s (target %.2f, ADR 0006 0.3-0.5 s)", t0,
                  rv::map::decayT60Seconds(0.0f));
    check(t0 >= 0.3 && t0 <= 0.5, msg);
    std::snprintf(msg, sizeof msg, "T60 DECAY 1 = %.2f s (target %.1f, ADR 0001 8-10 s)", t1,
                  rv::map::decayT60Seconds(1.0f));
    check(t1 >= 8.0 && t1 <= 10.0, msg);
    const double target5 = rv::map::decayT60Seconds(0.5f);
    std::snprintf(msg, sizeof msg, "T60 DECAY 0.5 = %.2f s (target %.2f, within 15%%)", t5, target5);
    check(t5 > t0 && t5 < t1 && std::fabs(t5 / target5 - 1.0) < 0.15, msg);
}

// ---- 4. Stability grid ---------------------------------------------------------
void stabilityGrid()
{
    const float fs = 48000.0f;
    const size_t sec = size_t(fs);
    bool ok = true;
    int bad = 0;
    float worstPeak = 0;
    for (float d : {0.0f, 0.5f, 1.0f})
        for (float b : {0.0f, 0.5f, 1.0f})
            for (float t : {0.0f, 0.5f, 1.0f})
                for (int input = 0; input < 2; ++input) {
                    Settings s{d, b, t, 1.0f};
                    rv::Tank tank;
                    tank.prepare(fs, 48);
                    apply(tank, s);
                    Buf in = input == 0 ? impulse(6 * sec) : noise(6 * sec, 1.0f, 99u);
                    if (input == 1) std::fill(in.begin() + long(sec), in.end(), 0.0f); // 1 s full-scale noise
                    Buf l, r;
                    renderTank(tank, in, 48, &l, &r);
                    const float pk = std::max(peakAbs(l), peakAbs(r));
                    worstPeak = std::max(worstPeak, pk);
                    // Energy after the input stops, in half-second windows, must
                    // fall (allow +1 dB: the Chirp's lows arrive after its highs).
                    const size_t start = input == 0 ? sec / 2 : sec + sec / 2;
                    bool falls = true;
                    double prev = energy(l, start, start + sec / 2);
                    for (size_t w = start + sec / 2; w + sec / 2 <= l.size(); w += sec / 2) {
                        const double e = energy(l, w, w + sec / 2);
                        if (e > prev * 1.26 && e > 1e-12) falls = false;
                        prev = e;
                    }
                    const bool tailLower = energy(l, 5 * sec, 6 * sec) < energy(l, start, start + sec);
                    const bool good = allFinite(l) && allFinite(r) && pk < 1.0f && falls && tailLower;
                    if (!good) {
                        ++bad;
                        std::printf("      grid fail: decay %.1f tension %.1f tone %.1f %s peak %.3f falls %d lower %d\n",
                                    d, b, t, input ? "noise" : "impulse", pk, falls, tailLower);
                    }
                    ok &= good;
                }
    std::snprintf(msg, sizeof msg,
                  "Stability DECAY x TENSION x TONE {0,.5,1}^3, impulse + 1 s noise: finite, peak < 1 (worst %.3f), "
                  "decaying (%d bad)",
                  worstPeak, bad);
    check(ok, msg);
}

// ---- 5. Determinism ----------------------------------------------------------
Buf deterministicRender(rv::Tank& tank, const Buf& in, int block)
{
    // Params: set, then change once at a fixed sample (block split there).
    const size_t change = 36000;
    Settings a{0.7f, 0.3f, 0.6f, 0.8f}, b{0.2f, 0.9f, 0.3f, 0.6f};
    apply(tank, a);
    const size_t n = in.size();
    Buf l(n), r(n), out(2 * n);
    size_t pos = 0;
    while (pos < n) {
        if (pos == change) apply(tank, b);
        size_t k = std::min(size_t(block), n - pos);
        if (pos < change && pos + k > change) k = change - pos;
        tank.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, int(k));
        pos += k;
    }
    std::copy(l.begin(), l.end(), out.begin());
    std::copy(r.begin(), r.end(), out.begin() + long(n));
    return out;
}

void determinism()
{
    const float fs = 48000.0f;
    Buf in = noise(size_t(2.0f * fs), 0.5f, 7u);
    std::fill(in.begin() + 12000, in.end(), 0.0f);
    in[30000] = 1.0f;

    rv::Tank tank;
    tank.prepare(fs, 512);
    const Buf first = deterministicRender(tank, in, 48);
    tank.reset();
    const Buf second = deterministicRender(tank, in, 48);
    check(first == second, "Determinism: same input twice (reset() between) is bit-identical");

    bool same = true;
    for (int block : {1, 7, 48, 512}) {
        rv::Tank t;
        t.prepare(fs, 512);
        same &= deterministicRender(t, in, block) == first;
    }
    check(same, "Determinism: block sizes 1, 7, 48, 512 are bit-identical");
}

// ---- 6. Sample rates -----------------------------------------------------------
void sampleRates()
{
    const double t60Ref = tankT60(48000.0f, 0.5f);
    const double repRef = measureRepeat(48000.0f, 0.5f, 0.5f, nullptr);
    for (float fs : {44100.0f, 96000.0f}) {
        const double t60 = tankT60(fs, 0.5f), rep = measureRepeat(fs, 0.5f, 0.5f, nullptr);
        std::snprintf(msg, sizeof msg, "Sample rate %.1f kHz: T60 %.3f s (48k %.3f), repeat %.2f ms (48k %.2f), within 5%%",
                      fs / 1000.0f, t60, t60Ref, rep * 1e3, repRef * 1e3);
        check(std::fabs(t60 / t60Ref - 1.0) < 0.05 && std::fabs(rep / repRef - 1.0) < 0.05, msg);
    }
}

// ---- 7. Parameter sweeps don't click -------------------------------------------
// A running tail (continuous -20 dB noise in) while one knob sweeps 0 -> 1 in
// 4 s, automated every 16 samples like the Renderer. Pass: the click detector
// above finds nothing during the sweep. The same render without the sweep is
// printed as the steady-state reference for the worst ratio.
void sweepsDontClick()
{
    const float fs = 48000.0f;
    const size_t n = size_t(5.0f * fs), sweepFrom = size_t(0.5f * fs), sweepLen = size_t(4.0f * fs);
    const Buf in = noise(n, 0.1f, 3u);
    for (int which = 0; which < 3; ++which) {
        const rv::ParamId id = which == 0 ? rv::ParamId::Decay : which == 1 ? rv::ParamId::Tension : rv::ParamId::Tone;
        const char* name     = which == 0 ? "DECAY" : which == 1 ? "TENSION" : "TONE";
        double ratio[2]      = {0, 0};
        int clicks[2]        = {0, 0};
        for (int sweep = 0; sweep < 2; ++sweep) {
            rv::Tank t;
            t.prepare(fs, 16);
            apply(t, Settings{});
            t.setParam(id, 0.0f);
            Buf l(n), r(n);
            for (size_t pos = 0; pos < n; pos += 16) {
                if (sweep && pos >= sweepFrom)
                    t.setParam(id, std::min(1.0f, float(pos - sweepFrom) / float(sweepLen)));
                t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 16);
            }
            clicks[sweep] = countClicks(l, fs, sweepFrom, &ratio[sweep]);
        }
        std::snprintf(msg, sizeof msg, "%s sweep 0->1 in 4 s: %d clicks (worst d2/localRMS %.1f, steady %.1f, limit 10)",
                      name, clicks[1], ratio[1], ratio[0]);
        check(clicks[1] == 0, msg);
    }

    // ADR 0015 "testable": a full-range DECAY step is a bend, not a click.
    for (float from : {0.0f, 1.0f}) {
        rv::Tank t;
        t.prepare(fs, 16);
        apply(t, Settings{from, 0.5f, 0.5f, 1.0f});
        Buf l(n), r(n);
        for (size_t pos = 0; pos < n; pos += 16) {
            if (pos == size_t(fs)) t.setParam(rv::ParamId::Decay, 1.0f - from);
            t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 16);
        }
        double ratio = 0;
        const int clicks = countClicks(l, fs, size_t(0.5f * fs), &ratio);
        std::snprintf(msg, sizeof msg, "DECAY step %.0f->%.0f: %d clicks (worst ratio %.1f)", from, 1.0f - from, clicks,
                      ratio);
        check(clicks == 0, msg);
    }
}

// ---- 8. Performance proxy ------------------------------------------------------
void performance()
{
    const float fs = 48000.0f;
    const size_t n = size_t(10.0f * fs);
    const Buf in = noise(n, 0.3f, 5u);
    Buf l(n), r(n);
    rv::Tank t;
    t.prepare(fs, 48);
    apply(t, Settings{1.0f, 1.0f, 1.0f, 0.5f});
    t.setParam(rv::ParamId::Springs, 0.0f); // 1-Spring mode (M4: 2 idle Springs still run; test_tank has all modes)
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < n; pos += 48)
        t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
    const auto t1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n);
    // Daisy estimate: M7 @ 480 MHz assumed 15–25x slower per sample than this desktop.
    std::printf("INFO  process() worst case (DECAY/TENSION/TONE 1): %.1f ns/sample desktop, "
                "est. Daisy %.0f-%.0f cycles/sample (%.0f-%.0f%% of 10k budget, 1-Spring mode)\n",
                ns, ns * 15 * 0.48, ns * 25 * 0.48, ns * 15 * 0.48 / 100, ns * 25 * 0.48 / 100);
    std::printf("INFO  Tank memory: %zu bytes at 48 kHz (object %zu + pool), %zu bytes at 96 kHz\n",
                t.memoryBytes(), sizeof(rv::Tank),
                sizeof(rv::Tank) + rv::Tank::requiredPoolFloats(96000.0f) * sizeof(float));
}

// Mapping sanity: ranges from the ADRs.
void mappings()
{
    using namespace rv::map;
    bool ok = decayT60Seconds(0) >= 0.3f && decayT60Seconds(0) <= 0.5f && decayT60Seconds(1) >= 8.0f
           && decayT60Seconds(1) <= 10.0f;
    // TENSION anchors (ADR 0026): tight 33 ms / 4.6 kHz / 0.40 / 24, noon
    // 69 ms / 3.3 kHz / 0.47 / 40, loose 110 ms / 2.7 kHz / 0.55 / 64; each
    // rises (fC falls) steadily from tight to loose.
    ok &= std::fabs(tensionLoopDelaySeconds(0) - 0.033f) < 1e-4f && std::fabs(tensionLoopDelaySeconds(0.5f) - 0.069f) < 1e-4f
          && std::fabs(tensionLoopDelaySeconds(1) - 0.110f) < 1e-4f;
    ok &= std::fabs(tensionTransitionHz(0) - 4600.0f) < 1.0f && std::fabs(tensionTransitionHz(1) - 2700.0f) < 1.0f;
    ok &= tensionStages(0) == kMinStages && tensionStages(0.5f) == 40 && tensionStages(1) == kMaxStages;
    ok &= kChirpSign * tensionCoefficient(0) > 0.3f;
    for (int i = 0; i < 20; ++i) {
        const float v0 = 0.05f * float(i), v1 = v0 + 0.05f;
        ok &= tensionLoopDelaySeconds(v1) > tensionLoopDelaySeconds(v0)
              && stretchK(tensionTransitionHz(v1), 48000) > stretchK(tensionTransitionHz(v0), 48000)
              && kChirpSign * tensionCoefficient(v1) > kChirpSign * tensionCoefficient(v0) && tensionStages(v1) >= tensionStages(v0);
    }
    ok &= mixGains(0).dry == 1.0f && mixGains(0).wet == 0.0f && mixGains(1).dry == 0.0f && mixGains(1).wet == 1.0f;
    check(ok, "Mappings: T60 0.4-9 s; TENSION anchors (L 33/69/110 ms, fC 4.6-2.7 kHz, M 24/40/64), L, K, |a|, M rise steadily; exact MIX ends");

    // Loop gain < 1 everywhere (ADR 0001).
    bool below = true;
    for (float d : {0.0f, 0.5f, 1.0f})
        for (float b : {0.0f, 1.0f}) {
            SpringRig rig(48000.0f, d, b, 1.0f);
            below &= rig.spring.feedbackGain() < 1.0f && rig.spring.highFeedbackGain() < 1.0f;
            for (float hz = 20.0f; hz < 20000.0f; hz *= 1.1f)
                below &= rig.spring.feedbackGain() * rig.spring.loopMagnitude(hz) < 1.0f;
        }
    check(below, "Loop gain < 1 at every frequency (ADR 0001)");
}

} // namespace

int main()
{
    mappings();
    chirpHighsBeforeLows();
    repeatMatchesRoundTrip();
    t60MatchesDecay();
    stabilityGrid();
    determinism();
    sampleRates();
    sweepsDontClick();
    performance();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
