// WOBBLE generator tests (SPEC §4.7, §7 M7; ADR 0008). Stand-alone: the
// Wobble component alone, before it is wired into the Spring (M6 hook).
//
// Pitch deviation ("cents per pass"): a delay line read with delay m[n]
// plays back at rate 1 − (m[n] − m[n−1]), so one pass through the Loop is
// shifted by cents[n] = 1200 log2(1 − Δm). "peak" = max |cents| over 120 s,
// "p95" = 95th percentile of |cents|. Checked against a real measurement:
// a 1 kHz sine through a modulated fractional delay, frequency from
// zero-crossing intervals.

#include "dsp/Wobble.h"
#include "params/Mappings.h"
#include "params/SplashVoicing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float    kFs = 48000.0f;
constexpr uint32_t kSeeds[3] = {0xA511E9B3u, 0x63D83595u, 0x1B873593u};

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

// 1 kHz sine read through a delay line of (base + m[n]) samples, linear
// interpolation. Returns peak |cents| of per-cycle frequency (upward zero
// crossings, sub-sample interpolated), skipping the first second.
double measuredPeakCents(const Buf& m, float fs)
{
    const double f0 = 1000.0, base = 400.0;
    double peak = 0, lastCross = -1;
    float prev = 0;
    for (size_t n = 0; n < m.size(); ++n) {
        const double t = double(n) - (base + double(m[n])); // read time in samples
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
int zeroCrossings(const Buf& a)
{
    int z = 0;
    for (size_t i = 1; i < a.size(); ++i) z += (a[i - 1] < 0) != (a[i] < 0);
    return z;
}

// Held tone through a Loop (feedback comb, delay L + m[n], gain g for T60):
// the tail sums copies that went round 1, 2, 3 ... times, each shifted
// again. Returns p95 |cents| of the wet output's per-cycle frequency.
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
        const float y = rp < 0 ? 0.0f : a + fr * (b - a); // wet = Loop output
        buf[n & (size - 1)] = x + float(g) * y;
        if (prev < 0 && y >= 0) {
            // Frequency averaged over 10 cycles (10 ms): close to what the
            // ear calls pitch, ignores single-cycle jitter at beating dips.
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

    // ---- Depth curve and zones -------------------------------------------------
    std::printf("      WOBBLE   design   peak    p95  (cents per pass)   rate Hz   D samples\n");
    const float ws[] = {0.0f, 0.25f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f};
    double peaks[7] = {};
    bool zeroAtZero = false, ratio = true;
    for (int i = 0; i < 7; ++i) {
        const Buf m = generate(ws[i], 1, kFs, 120.0f);
        const Cents c = centsOf(m);
        peaks[i] = c.peak;
        rv::dsp::Wobble probe;
        probe.prepare(kFs, 1, kSeeds[1]);
        probe.setAmount(ws[i]);
        std::printf("      %5.3f   %6.2f  %6.2f  %6.2f                     %5.2f   %7.1f\n", ws[i],
                    splash::wobbleCents(ws[i]), c.peak, c.p95, probe.rateHz(), probe.depthSamples());
        if (i == 0) zeroAtZero = std::all_of(m.begin(), m.end(), [](float v) { return v == 0.0f; });
        if (ws[i] >= 0.5f) ratio &= c.peak > 0.7 * splash::wobbleCents(ws[i]) && c.peak < 1.4 * splash::wobbleCents(ws[i]);
    }
    check(zeroAtZero, "WOBBLE 0: modulation exactly 0 (only M6's Micro-mod floor remains)");
    bool mono = true;
    for (int i = 1; i < 7; ++i) mono &= peaks[i] > peaks[i - 1];
    check(mono, "peak pitch deviation rises monotonically with WOBBLE");
    // Per pass (the tail multiplies these, see the tail section below).
    std::snprintf(msg, sizeof msg, "Drift per pass (WOBBLE <= 0.5): peak %.2f cents <= 1, > 0.3 at 0.5", peaks[2]);
    check(peaks[1] <= 1.0 && peaks[2] <= 1.0 && peaks[2] > 0.3, msg);
    std::snprintf(msg, sizeof msg, "transition per pass (0.625): peak %.2f cents (1..3)", peaks[3]);
    check(peaks[3] > 1.0 && peaks[3] < 3.0, msg);
    std::snprintf(msg, sizeof msg, "Warble per pass: peak %.1f cents at 0.75 (>= 2.5), %.1f at 1 (10..16)", peaks[4], peaks[6]);
    check(peaks[4] >= 2.5 && peaks[6] >= 10.0 && peaks[6] <= 16.0, msg);
    check(ratio, "measured peak within 0.7..1.4 x the design curve for WOBBLE >= 0.5");

    // ---- Real pitch measurement agrees with the slope estimate -----------------
    {
        const Buf m = generate(1.0f, 1, kFs, 60.0f);
        const double est = centsOf(Buf(m.begin() + long(kFs), m.end())).peak;
        const double meas = measuredPeakCents(m, kFs);
        std::snprintf(msg, sizeof msg, "1 kHz sine through the modulated delay: measured peak %.1f cents vs slope estimate %.1f (within 15 %%)",
                      meas, est);
        check(std::fabs(meas - est) < 0.15 * est, msg);
    }

    // ---- Rate rises with depth -------------------------------------------------
    {
        const int z25 = zeroCrossings(generate(0.25f, 1, kFs, 120.0f));
        const int z50 = zeroCrossings(generate(0.5f, 1, kFs, 120.0f));
        const int z100 = zeroCrossings(generate(1.0f, 1, kFs, 120.0f));
        std::snprintf(msg, sizeof msg, "rate rises gently with depth: zero crossings in 120 s %d / %d / %d at WOBBLE 0.25 / 0.5 / 1",
                      z25, z50, z100);
        check(z25 < z50 && z50 < z100, msg);
    }

    // ---- Independent per Spring ------------------------------------------------
    {
        bool indep = true;
        for (float w : {0.5f, 1.0f}) {
            const Buf a = generate(w, 0, kFs, 120.0f), b = generate(w, 1, kFs, 120.0f), c = generate(w, 2, kFs, 120.0f);
            const double rab = correlation(a, b), rac = correlation(a, c), rbc = correlation(b, c);
            const double pab = correlation(diff(a), diff(b)), pbc = correlation(diff(b), diff(c));
            std::printf("      WOBBLE %.1f: delay corr AB %.2f AC %.2f BC %.2f, pitch corr AB %.2f BC %.2f\n", w, rab,
                        rac, rbc, pab, pbc);
            for (double r : {rab, rac, rbc, pab, pbc}) indep &= std::fabs(r) < 0.3;
        }
        check(indep, "Springs A/B/C independent: |correlation| < 0.3 of delay and pitch at WOBBLE 0.5 and 1");
    }

    // ---- Determinism and block-size independence -------------------------------
    {
        const Buf ref = generate(0.8f, 2, kFs, 10.0f, 48);
        check(generate(0.8f, 2, kFs, 10.0f, 48) == ref, "deterministic: same seed, same output");
        bool blocks = true;
        for (int b : {1, 7, 32, 333, 1024}) blocks &= generate(0.8f, 2, kFs, 10.0f, b) == ref;
        check(blocks, "block-size independent: bit-identical for blocks 1, 7, 32, 333, 1024");
        rv::dsp::Wobble wb;
        wb.prepare(kFs, 2, kSeeds[2]);
        wb.setAmount(0.8f);
        wb.reset();
        Buf x(ref.size());
        wb.process(x.data(), int(x.size()));
        wb.reset();
        Buf y(ref.size());
        wb.process(y.data(), int(y.size()));
        check(x == y && x == ref, "reset() replays the identical modulation");
    }

    // ---- Sample-rate aware -------------------------------------------------------
    {
        bool ok = true;
        for (float w : {0.5f, 1.0f}) {
            const double c48 = centsOf(generate(w, 1, 48000.0f, 60.0f)).p95;
            const double c96 = centsOf(generate(w, 1, 96000.0f, 60.0f)).p95;
            const double c44 = centsOf(generate(w, 1, 44100.0f, 60.0f)).p95;
            std::printf("      WOBBLE %.1f p95 cents: 44.1k %.2f, 48k %.2f, 96k %.2f\n", w, c44, c48, c96);
            ok &= std::fabs(c96 - c48) < 0.05 * c48 && std::fabs(c44 - c48) < 0.05 * c48;
        }
        check(ok, "same pitch deviation at 44.1, 48 and 96 kHz (within 5 %)");
        std::snprintf(msg, sizeof msg, "max |modulation| %.0f samples at 48 kHz (%.2f ms): extra Loop delay memory needed",
                      splash::wobbleMaxDepthSamples(48000.0f), 1000.0f * splash::wobbleMaxDepthSamples(48000.0f) / 48000.0f);
        check(splash::wobbleMaxDepthSamples(48000.0f) < 0.005f * 48000.0f, msg);
    }

    // ---- In the tail: what a held chord actually hears -------------------------
    // Held 1 kHz tone through a Loop at DECAY 0 / noon / max (L and T60 from
    // Mappings.h): p95 of the 10-cycle-averaged pitch of the wet tail.
    {
        bool drift = true, warble = true, trans = true;
        for (float d : {0.0f, 0.5f, 1.0f}) {
            const float L = map::decayLoopDelaySeconds(d), t60 = map::decayT60Seconds(d);
            double c[4];
            const float wv[4] = {0.5f, 0.625f, 0.75f, 1.0f};
            for (int i = 0; i < 4; ++i) c[i] = tailP95Cents(generate(wv[i], 1, kFs, 30.0f), kFs, L, t60);
            std::printf("      tail, DECAY %.1f (L %3.0f ms, T60 %.1f s): p95 cents %.1f / %.1f / %.1f / %.1f at WOBBLE 0.5 / 0.625 / 0.75 / 1\n",
                        d, 1000.0f * L, t60, c[0], c[1], c[2], c[3]);
            drift &= c[0] < 5.0;
            trans &= c[1] > c[0] && c[1] < c[2];
            warble &= c[3] >= (d == 0.0f ? 15.0 : 25.0);
        }
        check(drift, "tail Drift (WOBBLE 0.5): p95 < 5 cents at every DECAY: held chords stay in tune");
        check(trans, "tail transition (0.625) sits between Drift and Warble at every DECAY");
        check(warble, "tail Warble (WOBBLE 1): p95 >= 25 cents at DECAY noon/max, >= 15 at DECAY 0: clearly out of tune");
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
