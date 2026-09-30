// MIX tests against the current Tank (SPEC §3 K6, §4.8, §7 M7; ADR 0015).
// Uses the Tank API as-is (since M7 with SPLASH, KICK and WOBBLE inside; MIX
// itself was already in place and needed no integration).
//
// Trick used throughout: the Tank sums its input to mono (SPEC §4.3), so an
// antiphase input (L = x, R = −x) gives a silent tank: the output is then
// the dry path alone, and dry leakage / the dry gain can be read exactly.
// Loudness: K-weighted (BS.1770 pre-filter at 48 kHz) mean square of both
// channels over the whole render (ungated), in dB.

#include "dsp/Filters.h"
#include "dsp/Tank.h"
#include "params/Mappings.h"
#include "params/ParamSpec.h"

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
constexpr float kFs = 48000.0f;

struct Stereo {
    Buf l, r;
};

// mixAt(n) gives MIX for the block starting at sample n (called per block).
template <class MixFn>
// splash < 0 leaves the ParamSpec default (SPLASH 0.3).
Stereo render(const Stereo& in, int block, MixFn mixAt, float splash = -1.0f)
{
    rv::Tank t;
    t.prepare(kFs, block);
    t.setParam(rv::ParamId::Mix, mixAt(size_t(0)));
    if (splash >= 0.0f) t.setParam(rv::ParamId::Splash, splash);
    const size_t n = in.l.size();
    Stereo o{Buf(n), Buf(n)};
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int k = int(std::min(size_t(block), n - pos));
        t.setParam(rv::ParamId::Mix, mixAt(pos));
        t.process(in.l.data() + pos, in.r.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
    }
    return o;
}
Stereo renderAt(const Stereo& in, float mix, int block = 48, float splash = -1.0f)
{
    return render(in, block, [mix](size_t) { return mix; }, splash);
}

Buf noise(size_t n, float amp, uint32_t seed)
{
    Buf b(n);
    rv::dsp::Rng rng;
    rng.seed(seed);
    for (auto& x : b) x = amp * rng.bipolar();
    return b;
}

// Snare hits as test_drive / 02_hits (185 Hz body + 800 Hz–7 kHz noise), peak -6 dBFS.
Buf snares(size_t n)
{
    Buf out(n, 0.0f);
    rv::dsp::Rng rng;
    rng.seed(1u);
    rv::dsp::OnePoleLowpass lp, hp;
    lp.setCutoff(7000.0f, kFs);
    hp.setCutoff(800.0f, kFs);
    for (int h = 0; h < 4; ++h) {
        const size_t at = size_t(0.5f * kFs) + size_t(float(h) * 1.5f * kFs), len = size_t(0.25f * kFs);
        Buf hit(len);
        float peak = 0;
        for (size_t i = 0; i < len; ++i) {
            const float t = float(i) / kFs, nz = lp.process(rng.bipolar());
            const float band = nz - hp.process(nz);
            hit[i] = 0.6f * std::sin(2.0f * rv::map::kPi * 185.0f * t) * std::exp(-t / 0.03f) + 1.2f * band * std::exp(-t / 0.06f);
            peak = std::max(peak, std::fabs(hit[i]));
        }
        for (size_t i = 0; i < len && at + i < n; ++i) out[at + i] = 0.5f * hit[i] / peak;
    }
    return out;
}

// Pink noise (Paul Kellet's refined filter on seeded white noise), peak-scaled.
Buf pink(size_t n, float peak, uint32_t seed)
{
    const Buf w = noise(n, 1.0f, seed);
    Buf p(n);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    float mx = 0;
    for (size_t i = 0; i < n; ++i) {
        const double x = w[i];
        b0 = 0.99886 * b0 + x * 0.0555179; b1 = 0.99332 * b1 + x * 0.0750759; b2 = 0.96900 * b2 + x * 0.1538520;
        b3 = 0.86650 * b3 + x * 0.3104856; b4 = 0.55000 * b4 + x * 0.5329522; b5 = -0.7616 * b5 - x * 0.0168980;
        p[i] = float(b0 + b1 + b2 + b3 + b4 + b5 + b6 + x * 0.5362);
        b6 = x * 0.115926;
        mx = std::max(mx, std::fabs(p[i]));
    }
    for (auto& v : p) v *= peak / mx;
    return p;
}

double rms(const Buf& x, size_t from = 0, size_t to = ~size_t(0))
{
    double s = 0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(to - from));
}
double db(double a) { return 20.0 * std::log10(a + 1e-30); }

// BS.1770 K-weighting at 48 kHz (shelf + RLB high-pass), then mean square.
double kLoudnessDb(const Stereo& s)
{
    double sum = 0;
    for (const Buf* ch : {&s.l, &s.r}) {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0, z1 = 0, z2 = 0, w1 = 0, w2 = 0;
        for (float xv : *ch) {
            const double x = xv;
            const double y = 1.53512485958697 * x - 2.69169618940638 * x1 + 1.19839281085285 * x2 + 1.69065929318241 * y1 - 0.73248077421585 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            const double w = y - 2.0 * z1 + z2 + 1.99004745483398 * w1 - 0.99007225036621 * w2;
            z2 = z1; z1 = y; w2 = w1; w1 = w;
            sum += w * w;
        }
    }
    return 10.0 * std::log10(sum / double(s.l.size()) + 1e-30);
}

} // namespace

int main()
{
    const size_t n = size_t(4.0f * kFs);
    const Stereo stereoIn{noise(n, 0.3f, 1u), noise(n, 0.3f, 2u)}; // L != R: dry must stay stereo
    Stereo anti{noise(n, 0.3f, 3u), Buf(n)};
    for (size_t i = 0; i < n; ++i) anti.r[i] = -anti.l[i];

    // ---- CCW: dry only, bit-identical null --------------------------------------------
    {
        bool nullOk = true;
        for (int b : {1, 48, 512}) {
            const Stereo o = renderAt(stereoIn, 0.0f, b);
            nullOk &= o.l == stereoIn.l && o.r == stereoIn.r;
        }
        check(nullOk, "MIX 0 (CCW): output bit-identical to the stereo input (blocks 1, 48, 512)");
        // Returning to 0 from full wet: the smoother lands exactly, the null comes back.
        const Stereo o = render(stereoIn, 48, [](size_t p) { return p < size_t(kFs) ? 1.0f : 0.0f; });
        const bool back = std::equal(o.l.begin() + long(1.2f * kFs), o.l.end(), stereoIn.l.begin() + long(1.2f * kFs)) &&
                          std::equal(o.r.begin() + long(1.2f * kFs), o.r.end(), stereoIn.r.begin() + long(1.2f * kFs));
        check(back, "MIX 1 -> 0: bit-identical null again 200 ms after the move (smoother lands exactly)");
    }

    // ---- CW: wet only, no dry leakage ---------------------------------------------------
    {
        const Stereo o = renderAt(anti, 1.0f);
        const double leak = db(std::max(rms(o.l), rms(o.r)) / rms(anti.l));
        std::snprintf(msg, sizeof msg, "MIX 1 (CW): dry leakage %.0f dB re input (antiphase input, silent tank; limit -80 dB)", leak);
        check(leak < -80.0, msg);
        const Stereo half = renderAt(anti, 0.5f);
        std::snprintf(msg, sizeof msg, "  (same input at MIX 0.5: dry at %.2f dB, so the method sees the dry path)", db(rms(half.l) / rms(anti.l)));
        check(std::fabs(db(rms(half.l) / rms(anti.l)) + 3.01) < 0.02, msg);
    }

    // ---- Equal power at every setting -----------------------------------------------------
    {
        const Stereo mono{stereoIn.l, stereoIn.l};
        const Stereo wet1 = renderAt(mono, 1.0f);
        double worst = 0, noonDry = 0, noonWet = 0;
        for (int s = 0; s <= 10; ++s) {
            const float v = 0.1f * float(s);
            const double gd = rms(renderAt(anti, v).l) / rms(anti.l); // dry gain (tank silent)
            const Stereo o = renderAt(mono, v);
            Buf wetPart(n);
            for (size_t i = 0; i < n; ++i) wetPart[i] = o.l[i] - float(gd) * mono.l[i];
            const double gw = rms(wetPart) / rms(wet1.l);
            worst = std::max(worst, std::fabs(gd * gd + gw * gw - 1.0));
            if (s == 5) { noonDry = gd; noonWet = gw; }
        }
        std::snprintf(msg, sizeof msg, "equal power: dry² + wet² = 1 within %.4f for MIX 0..1 (step 0.1); noon dry %.2f dB, wet %.2f dB",
                      worst, db(noonDry), db(noonWet));
        check(worst < 0.002 && std::fabs(db(noonDry) + 3.01) < 0.02 && std::fabs(db(noonWet) + 3.01) < 0.05, msg);
    }

    // ---- Sweep loudness within ±1.5 dB -------------------------------------------------------
    {
        const Buf sn = snares(size_t(7.0f * kFs));
        const Stereo snareIn{sn, sn};
        const Buf pk = pink(size_t(4.0f * kFs), 0.5f, 9u), wh = noise(size_t(4.0f * kFs), 0.5f, 9u);
        const Stereo pinkIn{pk, pk}, whiteIn{wh, wh};
        // Criterion on the SPEC's typical material (-6 dBFS snare hits, as
        // test_drive / ADR 0022). The MIX law is exact (above), so the sweep
        // spread is set by the wet level against the dry, which depends on
        // the material: noise is printed for M8's kWetGain tuning.
        // SPLASH 0 for the criterion (the Tank's own wet level): since ADR
        // 0032 SPLASH's Bite hits the tank harder on drum hits on purpose,
        // so at the default SPLASH 0.3 the snare's wet comes back ~2-3 dB
        // louder (printed below, INFO); that is SPLASH's level, not the MIX
        // law's, and an owner call (docs/m8-tuning-backlog.md).
        const char* const name[4] = {"-6 dBFS snare hits, SPLASH 0", "pink noise (info only)", "white noise (info only)",
                                     "-6 dBFS snare hits, default SPLASH (info only)"};
        for (int m = 0; m < 4; ++m) {
            const Stereo& in = m == 0 || m == 3 ? snareIn : (m == 1 ? pinkIn : whiteIn);
            double lo = 1e9, hi = -1e9, l[11];
            for (int s = 0; s <= 10; ++s) {
                l[s] = kLoudnessDb(renderAt(in, 0.1f * float(s), 48, m == 0 ? 0.0f : -1.0f));
                lo = std::min(lo, l[s]);
                hi = std::max(hi, l[s]);
            }
            std::printf("      %s: K-weighted level re MIX 0, MIX 0 / 0.25 / 0.5 / 0.75 / 1 (approx): %+.2f %+.2f %+.2f %+.2f %+.2f dB\n",
                        name[m], 0.0, 0.5 * (l[2] + l[3]) - l[0], l[5] - l[0],
                        0.5 * (l[7] + l[8]) - l[0], l[10] - l[0]);
            if (m > 0) {
                // Pink: the wet tail builds up louder than the dry. White: most
                // of its energy is above the transducer band-limit (6.5 kHz in
                // DRIVEN), so the dark wet is quieter. Printed, not a criterion.
                std::printf("      %s: +-%.2f dB around the midpoint (wet vs dry level %+.2f dB)\n", name[m], 0.5 * (hi - lo), l[10] - l[0]);
                continue;
            }
            std::snprintf(msg, sizeof msg, "MIX sweep 0..1, %s: loudness within ±%.2f dB of its midpoint (limit ±1.5)", name[m], 0.5 * (hi - lo));
            check(0.5 * (hi - lo) <= 1.5, msg);
        }
    }

    // ---- MIX envelope (5 ms attack) lands without lag ---------------------------------------------
    {
        // DC antiphase input: out L = 0.5·sqrt(1 − m), so m(t) = 1 − (outL/0.5)² exactly.
        const size_t len = size_t(0.1f * kFs), start = size_t(0.02f * kFs), attack = size_t(0.005f * kFs);
        Stereo dc{Buf(len, 0.5f), Buf(len, -0.5f)};
        auto env = [=](size_t p) {
            return p < start ? 0.0f : (p < start + attack ? float(p - start) / float(attack) : 1.0f);
        };
        std::printf("      ParamSpec MIX smoothing: %s tier, %.1f ms\n",
                    rv::spec(rv::ParamId::Mix).smoothing == rv::Smoothing::Snappy ? "Snappy" : "other", rv::spec(rv::ParamId::Mix).smoothingMs);
        for (int block : {1, 48}) {
            const Stereo o = render(dc, block, env);
            double t50 = -1, t90 = -1;
            for (size_t i = 0; i < len; ++i) {
                const double m = 1.0 - double(o.l[i] / 0.5f) * double(o.l[i] / 0.5f);
                if (t50 < 0 && m >= 0.5) t50 = double(i);
                if (t90 < 0 && m >= 0.9) t90 = double(i);
            }
            const double envHalf = double(start) + 0.5 * double(attack), envEnd = double(start + attack);
            const double lag = 1000.0 * (t50 - envHalf) / kFs, settle = 1000.0 * (t90 - envEnd) / kFs;
            const double limit = block == 1 ? 5.0 : 5.0 + 0.5 * 1000.0 * block / kFs;
            std::snprintf(msg, sizeof msg,
                          "5 ms MIX envelope, host block %d: 50 %% point lags the envelope by %.2f ms (<= %.1f), 90 %% %.1f ms after the attack ends",
                          block, lag, limit, settle);
            check(lag <= limit && rv::spec(rv::ParamId::Mix).smoothingMs <= 5.0f, msg);
        }
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
