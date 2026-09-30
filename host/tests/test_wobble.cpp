// WOBBLE generator tests (SPEC §4.7, §7 M7; ADR 0034 bipolar WOBBLE, which
// supersedes ADR 0008's one-way zones). Stand-alone: the Wobble component
// alone (the whole-Tank held-tone numbers are in test_m7_tank).
//
// Intended behaviour (ADR 0034, params/WobbleVoicing.h):
//   noon ± the dead zone  exactly 0 (only M6's Micro-mod floor remains);
//   left of noon          smooth random wow + flutter: never repeats, the
//                         wow's rate wanders, a faster flutter layer on top;
//   right of noon         a sine LFO whose rate drifts only slightly;
//   both sides            depth grows at every 0.1 knob step, from a few
//                         cents in the tail next to noon to the old top end
//                         ("clearly out of tune") at the end stops;
//   Springs               share Spring A's movement at low amounts (chords
//                         fade evenly), independent from kShareTo up;
//   always                continuous across the whole knob (no jumps crossing
//                         noon), deterministic, block-size independent,
//                         sample-rate aware, within the Loop delay memory.
//
// Pitch deviation ("cents per pass"): a delay line read with delay m[n]
// plays back at rate 1 − (m[n] − m[n−1]), so one pass through the Loop is
// shifted by cents[n] = 1200 log2(1 − Δm). "peak" = max |cents| over 120 s,
// "p95" = 95th percentile of |cents|.

#include "dsp/Tank.h"
#include "dsp/Wobble.h"
#include "params/Mappings.h"
#include "params/WobbleVoicing.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;
char msg[512];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float    kFs = 48000.0f;
constexpr uint32_t kSeeds[3] = {0xA511E9B3u, 0x63D83595u, 0x1B873593u};
// Knob steps of 0.1, as the sweet-spot check walks them (0 = fully left).
constexpr int   kSteps = 11;
constexpr float kKnob[kSteps] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

Buf generate(float w, int spring, float fs, float seconds, int block = 48)
{
    rv::dsp::Wobble wb;
    wb.prepare(fs, spring, kSeeds[spring]);
    wb.setAmount(w);
    wb.reset();
    Buf out(size_t(seconds * fs));
    for (size_t pos = 0; pos < out.size(); pos += size_t(block)) {
        const int n = int(std::min(size_t(block), out.size() - pos));
        wb.process(out.data() + pos, n);
    }
    return out;
}

// Springs A, B, C as the Tank runs them: B and C via next(leader = A).
struct Three {
    Buf a, b, c;
};
Three generateTank(float w, float seconds)
{
    rv::dsp::Wobble g[3];
    for (int s = 0; s < 3; ++s) {
        g[s].prepare(kFs, s, kSeeds[s]);
        g[s].setAmount(w);
        g[s].reset();
    }
    const size_t n = size_t(seconds * kFs);
    Three t{Buf(n), Buf(n), Buf(n)};
    for (size_t i = 0; i < n; ++i) {
        t.a[i] = g[0].next();
        t.b[i] = g[1].next(t.a[i]);
        t.c[i] = g[2].next(t.a[i]);
    }
    return t;
}

struct Cents {
    double peak = 0, p95 = 0;
};
Cents centsOf(const Buf& m)
{
    std::vector<double> c;
    c.reserve(m.size());
    for (size_t i = 1; i < m.size(); ++i) c.push_back(std::fabs(1200.0 * std::log2(1.0 - double(m[i] - m[i - 1]))));
    Cents r;
    r.peak = *std::max_element(c.begin(), c.end());
    std::nth_element(c.begin(), c.begin() + long(0.95 * double(c.size())), c.end());
    r.p95 = c[size_t(0.95 * double(c.size()))];
    return r;
}

// 1 kHz sine read through a delay line of (base + m[n]) samples. Returns
// peak |cents| of per-cycle frequency (upward zero crossings, sub-sample
// interpolated), skipping the first second.
double measuredPeakCents(const Buf& m, float fs)
{
    const double f0 = 1000.0, base = 400.0;
    double peak = 0, lastCross = -1;
    float prev = 0;
    for (size_t n = 0; n < m.size(); ++n) {
        const double t = double(n) - (base + double(m[n]));
        const float y = float(std::sin(2.0 * M_PI * f0 * t / double(fs)));
        if (n > 0 && prev < 0 && y >= 0) {
            const double cross = double(n - 1) + double(prev) / double(prev - y);
            if (lastCross >= 0 && n > size_t(fs)) {
                const double f = double(fs) / (cross - lastCross);
                peak = std::max(peak, std::fabs(1200.0 * std::log2(f / f0)));
            }
            lastCross = cross;
        }
        prev = y;
    }
    return peak;
}

double correlation(const Buf& a, const Buf& b)
{
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
    const double n = double(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        sa += a[i]; sb += b[i]; saa += double(a[i]) * a[i]; sbb += double(b[i]) * b[i]; sab += double(a[i]) * b[i];
    }
    const double cov = sab / n - sa * sb / (n * n);
    const double va = saa / n - sa * sa / (n * n), vb = sbb / n - sb * sb / (n * n);
    return va > 0 && vb > 0 ? cov / std::sqrt(va * vb) : 0.0;
}
Buf diff(const Buf& a)
{
    Buf d(a.size(), 0.0f);
    for (size_t i = 1; i < a.size(); ++i) d[i] = a[i] - a[i - 1];
    return d;
}
Buf decimate(const Buf& a, int by)
{
    Buf d;
    for (size_t i = 0; i < a.size(); i += size_t(by)) d.push_back(a[i]);
    return d;
}

// Largest normalised autocorrelation of x over lags [lo, hi] samples: ~1
// for anything that repeats (a sine), low for a line that never does.
double maxAutocorr(const Buf& x, size_t lo, size_t hi)
{
    double best = -1;
    for (size_t lag = lo; lag <= hi; ++lag)
        best = std::max(best, correlation(Buf(x.begin(), x.end() - long(lag)), Buf(x.begin() + long(lag), x.end())));
    return best;
}

// Share of the pitch movement's power (the slope, diff(m)) in a band, from
// a plain DFT of the control-rate slope (decimated by 32).
double bandShare(const Buf& m, double lo, double hi)
{
    const Buf s = diff(decimate(m, 32));
    const double fs = kFs / 32.0;
    const size_t n = s.size();
    double in = 0, all = 0;
    for (double f = 0.05; f < 20.0; f += 0.05) {
        std::complex<double> acc = 0;
        for (size_t i = 0; i < n; ++i) acc += double(s[i]) * std::polar(1.0, -2.0 * M_PI * f * double(i) / fs);
        const double p = std::norm(acc);
        all += p;
        if (f >= lo && f < hi) in += p;
    }
    return all > 0 ? in / all : 0.0;
}

// Upward zero-crossing intervals (seconds).
std::vector<double> crossingIntervals(const Buf& a)
{
    std::vector<double> out;
    double last = -1;
    for (size_t i = 1; i < a.size(); ++i)
        if (a[i - 1] < 0 && a[i] >= 0) {
            const double t = double(i) / double(kFs);
            if (last >= 0) out.push_back(t - last);
            last = t;
        }
    return out;
}
double quantile(std::vector<double> v, double q)
{
    if (v.empty()) return 0;
    std::nth_element(v.begin(), v.begin() + long(q * double(v.size() - 1)), v.end());
    return v[size_t(q * double(v.size() - 1))];
}

// Held tone through a Loop (feedback comb, delay L + m[n], gain g for T60):
// the tail sums copies that went round 1, 2, 3 ... times, each shifted
// again. Returns p95 |cents| of the wet output's 10-cycle pitch.
double tailP95Cents(const Buf& m, float fs, float loopSeconds, float t60)
{
    const double f0 = 1000.0;
    const int L = int(loopSeconds * fs), size = 1 << 15;
    const double g = std::pow(10.0, -3.0 * loopSeconds / t60);
    std::vector<float> buf(size, 0.0f);
    std::vector<double> c;
    double lastCross = -1;
    int cycles = 0;
    float prev = 0;
    for (size_t n = 0; n < m.size(); ++n) {
        const float x = float(0.1 * std::sin(2.0 * M_PI * f0 * double(n) / fs));
        const double d = double(L) + double(m[n]);
        const double rp = double(n) - d;
        const long i0 = long(std::floor(rp));
        const float fr = float(rp - double(i0));
        const float a = buf[size_t(i0 & (size - 1))], b = buf[size_t((i0 + 1) & (size - 1))];
        const float y = rp < 0 ? 0.0f : a + fr * (b - a);
        buf[n & (size - 1)] = x + float(g) * y;
        if (prev < 0 && y >= 0) {
            const double cross = double(n - 1) + double(prev) / double(prev - y);
            if (++cycles == 10) {
                if (lastCross >= 0 && n > size_t(3.0f * fs))
                    c.push_back(std::fabs(1200.0 * std::log2(10.0 * double(fs) / (cross - lastCross) / f0)));
                lastCross = cross;
                cycles = 0;
            }
        }
        prev = y;
    }
    std::nth_element(c.begin(), c.begin() + long(0.95 * double(c.size())), c.end());
    return c[size_t(0.95 * double(c.size()))];
}

} // namespace

int main()
{
    using namespace rv;

    // ---- Knob layout: dead zone, sides, depth per step --------------------------
    std::printf("      knob   side    peak    p95  (cents per pass)   LFO/wow Hz   depth samples (lfo / wow / flutter)\n");
    double peaks[kSteps] = {}, p95s[kSteps] = {};
    for (int i = 0; i < kSteps; ++i) {
        const Buf m = generate(kKnob[i], 1, kFs, 120.0f);
        const Cents c = centsOf(m);
        peaks[i] = c.peak;
        p95s[i]  = c.p95;
        rv::dsp::Wobble probe;
        probe.prepare(kFs, 1, kSeeds[1]);
        probe.setAmount(kKnob[i]);
        const auto& d = probe.depths();
        std::printf("      %4.2f  %-6s  %6.2f  %6.2f                    %5.2f / %4.2f   %6.1f / %6.1f / %5.1f\n", kKnob[i],
                    kKnob[i] < 0.47f ? "random" : (kKnob[i] > 0.53f ? "LFO" : "still"), c.peak, c.p95, d.lfoHz, d.wowHz,
                    d.lfo, d.wow, d.flutter);
    }
    {
        bool still = true;
        for (float w : {0.5f, 0.475f, 0.525f, 0.4701f, 0.5299f}) {
            const Buf m = generate(w, 1, kFs, 10.0f);
            still &= std::all_of(m.begin(), m.end(), [](float v) { return v == 0.0f; });
        }
        check(still, "noon and the dead zone (0.47..0.53): modulation exactly 0 (only M6's Micro-mod floor remains)");
        bool moving = true;
        for (float w : {0.46f, 0.54f}) moving &= centsOf(generate(w, 1, kFs, 30.0f)).peak > 0.0;
        check(moving, "just outside the dead zone (0.46, 0.54): it moves");
    }
    {
        // Every 0.1 step away from noon deepens the per-pass movement, on
        // both sides, by a clear factor (>= 1.3x in p95).
        bool rising = true;
        double worst = 1e9;
        for (int i = 4; i > 0; --i) { // left: 0.4 -> 0
            rising &= peaks[i - 1] > peaks[i] && p95s[i - 1] >= 1.3 * p95s[i];
            worst = std::min(worst, p95s[i - 1] / p95s[i]);
        }
        for (int i = 6; i < kSteps - 1; ++i) { // right: 0.6 -> 1
            rising &= peaks[i + 1] > peaks[i] && p95s[i + 1] >= 1.3 * p95s[i];
            worst = std::min(worst, p95s[i + 1] / p95s[i]);
        }
        std::snprintf(msg, sizeof msg,
                      "each 0.1 step away from noon deepens the per-pass pitch movement on both sides (p95 >= 1.3x per step; "
                      "smallest step x%.2f)",
                      worst);
        check(rising, msg);
    }
    std::snprintf(msg, sizeof msg,
                  "fully right = the old top end: per-pass peak %.1f cents (8..13; old WOBBLE 1: 12.3 with a 10 %% random share)",
                  peaks[10]);
    check(peaks[10] >= 8.0 && peaks[10] <= 13.0, msg);
    // Random lines have rare steep moments, so their peak runs ~2x their
    // p95 (a sine's ~1.05x): "as wild" compares the p95.
    std::snprintf(msg, sizeof msg,
                  "fully left roughly as wild: per-pass p95 %.1f cents vs fully right %.1f (within 0.7..1.5x; peaks %.1f / %.1f)",
                  p95s[0], p95s[10], peaks[0], peaks[10]);
    check(p95s[0] >= 0.7 * p95s[10] && p95s[0] <= 1.5 * p95s[10], msg);

    // ---- Real pitch measurement agrees with the slope estimate -----------------
    for (float w : {0.0f, 1.0f}) {
        const Buf m = generate(w, 1, kFs, 60.0f);
        const double est = centsOf(Buf(m.begin() + long(kFs), m.end())).peak;
        const double meas = measuredPeakCents(m, kFs);
        std::snprintf(msg, sizeof msg,
                      "1 kHz sine through the modulated delay, fully %s: measured peak %.1f cents vs slope estimate %.1f (within 15 %%)",
                      w < 0.5f ? "left" : "right", meas, est);
        check(std::fabs(meas - est) < 0.15 * est, msg);
    }

    // ---- Character: left never repeats, has flutter; right is a steady sine ------
    {
        // At 187.5 Hz (every 256 samples): plenty for movement below 20 Hz.
        const Buf left = decimate(generate(0.0f, 1, kFs, 120.0f), 256), right = decimate(generate(1.0f, 1, kFs, 120.0f), 256);
        // Left: over 2-10 s lags (past its own smoothness). Right: within a
        // few LFO periods (its slow rate drift loosens it over longer spans).
        const double acL = maxAutocorr(left, size_t(2.0 * kFs / 256.0), size_t(10.0 * kFs / 256.0));
        const double acR = maxAutocorr(right, size_t(0.4 * kFs / 256.0), size_t(3.0 * kFs / 256.0));
        std::snprintf(msg, sizeof msg,
                      "left never repeats: largest self-similarity over 2-10 s lags %.2f (< 0.5); right is a steady sine: "
                      "%.2f over 0.4-3 s (> 0.9)",
                      acL, acR);
        check(acL < 0.5 && acR > 0.9, msg);

        const Buf mL = generate(0.0f, 1, kFs, 60.0f), mR = generate(1.0f, 1, kFs, 60.0f), mL3 = generate(0.3f, 1, kFs, 60.0f);
        const double fL = bandShare(mL, 4.0, 14.0), fL3 = bandShare(mL3, 4.0, 14.0), fR = bandShare(mR, 4.0, 14.0);
        std::snprintf(msg, sizeof msg,
                      "flutter on the random side: %.0f %% of the pitch movement's power at 4-14 Hz fully left, %.0f %% at 0.3 "
                      "(3..50 %%: there, smaller than the wow); LFO side %.1f %% (< 1 %%)",
                      100 * fL, 100 * fL3, 100 * fR);
        check(fL > 0.03 && fL < 0.5 && fL3 > 0.03 && fL3 < 0.5 && fR < 0.01, msg);

        // Wow rate wanders: the spread of the slow line's zero-crossing
        // intervals (wow alone: flutter is a small ripple on it, so cross
        // the 0.5 s-smoothed line). The LFO's period drifts only slightly.
        Buf smooth(mL.size());
        {
            float y = 0;
            const float c = 1.0f - std::exp(-1.0f / (0.08f * kFs));
            for (size_t i = 0; i < mL.size(); ++i) smooth[i] = (y += c * (mL[i] - y));
        }
        const auto iL = crossingIntervals(smooth), iR = crossingIntervals(mR);
        const double spreadL = quantile(iL, 0.9) / quantile(iL, 0.1);
        const double spreadR = quantile(iR, 0.95) / quantile(iR, 0.05);
        std::snprintf(msg, sizeof msg,
                      "wow's rate wanders (10-90 %% of its cycle lengths span x%.1f, > 2.5); the LFO's period drifts only "
                      "slightly (5-95 %% span x%.3f: > 1.01, < 1.2; a pure sine would be 1.000)",
                      spreadL, spreadR);
        check(spreadL > 2.5 && spreadR > 1.01 && spreadR < 1.2, msg);
    }

    // ---- Springs: shared at low amounts, independent above ----------------------
    {
        bool shared = true, indep = true;
        for (float w : {0.40f, 0.60f}) {
            const Three t = generateTank(w, 60.0f);
            const double rab = correlation(t.a, t.b), rac = correlation(t.a, t.c);
            std::printf("      WOBBLE %.2f (low amount): delay corr AB %.2f AC %.2f\n", w, rab, rac);
            shared &= rab > 0.95 && rac > 0.95;
        }
        for (float w : {0.0f, 0.2f, 0.8f, 1.0f}) {
            const Three t = generateTank(w, 120.0f);
            const double rab = correlation(t.a, t.b), rac = correlation(t.a, t.c), rbc = correlation(t.b, t.c);
            const double pab = correlation(diff(t.a), diff(t.b)), pbc = correlation(diff(t.b), diff(t.c));
            std::printf("      WOBBLE %.1f: delay corr AB %.2f AC %.2f BC %.2f, pitch corr AB %.2f BC %.2f\n", w, rab, rac, rbc,
                        pab, pbc);
            for (double r : {rab, rac, rbc, pab, pbc}) indep &= std::fabs(r) < 0.3;
        }
        check(shared, "low amounts (0.4, 0.6): Springs B and C follow Spring A (delay correlation > 0.95): chords fade evenly");
        check(indep, "Springs A/B/C independent from the share blend up (WOBBLE 0, 0.2, 0.8, 1): |correlation| < 0.3");
    }

    // ---- Continuity: sweeping the knob across noon never jumps -------------------
    {
        rv::dsp::Wobble wb;
        wb.prepare(kFs, 1, kSeeds[1]);
        wb.setAmount(0.0f);
        wb.reset(); // start moving (a Tank starts from its own first value too)
        const size_t n = size_t(20.0f * kFs);
        double biggest = 0;
        float prev = 0;
        for (size_t i = 0; i < n; ++i) {
            if (i % 32 == 0) wb.setAmount(float(i) / float(n)); // 0 -> 1 over 20 s, through noon
            const float y = wb.next();
            if (i > 0) biggest = std::max(biggest, double(std::fabs(y - prev)));
            prev = y;
        }
        const double c = std::fabs(1200.0 * std::log2(1.0 + biggest));
        std::snprintf(msg, sizeof msg,
                      "knob swept fully left -> fully right over 20 s: largest one-sample step %.4f samples (%.1f cents, "
                      "<= 1.5x the steady per-pass peak %.1f): no jumps crossing noon",
                      biggest, c, std::max(peaks[0], peaks[10]));
        check(c <= 1.5 * std::max(peaks[0], peaks[10]), msg);
    }

    // ---- Determinism and block-size independence -------------------------------
    for (float w : {0.1f, 0.8f}) {
        const Buf ref = generate(w, 2, kFs, 10.0f, 48);
        bool same = generate(w, 2, kFs, 10.0f, 48) == ref;
        for (int b : {1, 7, 32, 333, 1024}) same &= generate(w, 2, kFs, 10.0f, b) == ref;
        rv::dsp::Wobble wb;
        wb.prepare(kFs, 2, kSeeds[2]);
        wb.setAmount(w);
        wb.reset();
        Buf x(ref.size());
        wb.process(x.data(), int(x.size()));
        wb.reset();
        Buf y(ref.size());
        wb.process(y.data(), int(y.size()));
        std::snprintf(msg, sizeof msg,
                      "WOBBLE %.1f: deterministic, bit-identical for blocks 1, 7, 32, 48, 333, 1024, and reset() replays it", w);
        check(same && x == y && x == ref, msg);
    }

    // ---- Sample-rate aware -------------------------------------------------------
    {
        bool ok = true;
        for (float w : {0.2f, 0.8f, 1.0f}) {
            const double c48 = centsOf(generate(w, 1, 48000.0f, 60.0f)).p95;
            const double c96 = centsOf(generate(w, 1, 96000.0f, 60.0f)).p95;
            const double c44 = centsOf(generate(w, 1, 44100.0f, 60.0f)).p95;
            std::printf("      WOBBLE %.1f p95 cents: 44.1k %.2f, 48k %.2f, 96k %.2f\n", w, c44, c48, c96);
            ok &= std::fabs(c96 - c48) < 0.05 * c48 && std::fabs(c44 - c48) < 0.05 * c48;
        }
        check(ok, "same pitch deviation at 44.1, 48 and 96 kHz (within 5 %)");
    }

    // ---- Delay memory ------------------------------------------------------------
    // The random side moves the Loop further than the old sine did (a slow
    // wow needs more delay swing for the same pitch). The bound the Spring
    // sizes its memory with must hold for every generator, and the whole
    // Tank must still fit the firmware's DTCM pool (firmware/main.cpp
    // kTankPoolFloats = 30000).
    {
        double worst = 0;
        for (int i = 0; i <= 20; ++i) {
            const Three t = generateTank(0.05f * float(i), 60.0f);
            for (const Buf* b : {&t.a, &t.b, &t.c})
                for (float v : *b) worst = std::max(worst, double(std::fabs(v)));
        }
        const double bound = wobble::maxDepthSamples(kFs);
        const size_t pool = Tank::requiredPoolFloats(kFs);
        std::snprintf(msg, sizeof msg,
                      "largest |modulation| %.0f samples (%.2f ms) over the knob, within the memory bound %.0f; Tank needs %zu "
                      "floats at 48 kHz (firmware pool 30000)",
                      worst, 1000.0 * worst / kFs, bound, pool);
        check(worst <= bound && pool <= 30000, msg);
    }

    // ---- In the tail: what a held chord actually hears -------------------------
    // Held 1 kHz tone through a Loop at DECAY 0 / noon / max (T60 from
    // Mappings.h; L = TENSION noon's 69 ms): p95 of the 10-cycle-averaged
    // pitch of the wet tail, every 0.1 step. (The whole Tank, with the
    // first-echo Transport on top: test_m7_tank.)
    {
        bool stillOk = true, rising = true, ends = true;
        for (float d : {0.0f, 0.5f, 1.0f}) {
            const float L = map::tensionLoopDelaySeconds(0.5f), t60 = map::decayT60Seconds(d);
            double c[kSteps];
            std::printf("      tail, DECAY %.1f (T60 %.1f s) p95 cents:", d, t60);
            for (int i = 0; i < kSteps; ++i) {
                c[i] = tailP95Cents(generate(kKnob[i], 1, kFs, 30.0f), kFs, L, t60);
                std::printf(" %.1f", c[i]);
            }
            std::printf("  (knob 0 .. 1)\n");
            stillOk &= c[5] < 0.01;
            for (int i = 4; i > 0; --i) rising &= c[i - 1] > c[i];
            for (int i = 6; i < kSteps - 1; ++i) rising &= c[i + 1] > c[i];
            // DECAY max is checked on the whole Tank (test_m7_tank, >= 25 at
            // both ends), not here: in this Loop-only model a pure sine's
            // shifts cancel over the many LFO periods a 9 s tail spans (the
            // old top's 10 % random share was what accumulated there), and
            // the heard number also carries the first-echo Transport.
            if (d < 1.0f) ends &= std::min(c[0], c[10]) >= (d == 0.0f ? 10.0 : 25.0);
        }
        check(stillOk, "tail at noon: no pitch movement");
        check(rising, "tail: every 0.1 step away from noon moves the held tone more, both sides, at every DECAY");
        check(ends, "tail at both end stops (Loop only): p95 >= 25 cents at DECAY noon, >= 10 at DECAY 0: clearly out of tune");
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
