// Tank voicings (Wellspring fit round 3, core/params/TankVoicing.h, ADR 0038
// Proposed). Dependency-free: prints PASS/FAIL / INFO lines, returns nonzero
// on any failure.
//
// The owner picked 7 (ADR 0038 Decision, 2 Oct 2026): the whole suite runs on
// it; 0-6 stay as Renderer-only references. What a voicing does to everything else (mono
// sum, SPRINGS switching, levels, the M6 grids...) is checked by running the
// full suite with that voicing as the default (RV_TANK_DEFAULT_VOICING,
// docs/prototypes/wellspring-fit-3/). Here, what each voicing promises:
//   1. The default is the owner's pick (7), and the firmware's pool for any voicing
//      stays within its 30,000 floats (firmware/main.cpp kTankPoolFloats).
//   2. Stability at the corners (KICKED, DECAY / DRIVE / SPLASH 1, 3
//      Springs, TENSION 0 and 1, TONE 0 and 1): an impulse, 1 s of
//      full-scale noise and the tail stay finite and under full scale.
//   3. The first echo keeps its time (ADR 0029): its body (below ~1.2 kHz)
//      within 1.5 ms of today's at TENSION 0 / 0.5 / 1 (2 Springs).
//   4. Voicing 2+: the two sides move together (L/R envelope correlation >=
//      0.85 on a click's first 0.5 s, 10 ms windows; today's 2 Springs ~0.5)
//      and stay wide (fine-structure L/R correlation <= 0.35, 200 ms-1.5 s).
//   5. Voicing 2+: no Spring is panned, so the mono sum is the Springs' sum:
//      voicing 2's (L + R) is voicing 1's times one gain (correlation >=
//      0.9999; not bit-exact: the output pickups, DriveOut, bend each side a
//      little even in CLEAN, and the limiter is linked).
//   6. Voicing 3+: echoes blur sooner: more echo density 300-500 ms after a
//      click than voicing 2 (normalised echo density, Abel & Huang).
// Round 4 (docs/m8-tuning-backlog.md "Wellspring fit round 4"):
//   7. Voicing 5+ (transducers): the first 60 ms after a click (energy above
//      4 kHz vs 200 Hz-4 kHz) at least 12 dB darker than voicing 3's (the
//      Wellspring sits ~16 dB under 3 at the closest settings).
//   8. Voicing 6+ (wide): fine-structure L/R correlation <= 0.1 (2 and 3
//      Springs; the Wellspring -0.04), and the mono sum is still the
//      Springs' sum: voicing 6's (L + R) is voicing 5's times one gain.
//   9. Voicing 7: the low cut's makeup keeps low material's level: a low,
//      held chord within 2 dB of voicing 6's (round 3's 4 lost 3-4 dB on the
//      skank; the makeup reads power going in, which under-reads the Springs'
//      louder lowest notes, so it gives back most of it, not all).

#include "dsp/Tank.h"
#include "params/ParamSpec.h"
#include "params/TankVoicing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int  failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

constexpr float kFs = 48000.0f;
using Buf = std::vector<float>;

struct Knobs {
    float decay = 0.6f, tension = 0.875f, tone = 0.8f, drive = 0.25f, splash = 0.0f, wobble = 0.5f;
    int   springs = 1, attitude = 0;
};

struct Out {
    Buf l, r;
};

Out render(int voicing, const Knobs& k, const Buf& in)
{
    rv::Tank t;
    t.prepare(kFs, 48);
    t.setTankVoicing(voicing);
    // SPRINGS position 3 is echo mode since ADR 0041; these checks are about
    // the tank's own voicings, so position 3 runs the three-Spring reference
    // (as test_tank and the other tank suites do).
    t.setEchoMode(false);
    t.setParam(rv::ParamId::Decay, k.decay);
    t.setParam(rv::ParamId::Tension, k.tension);
    t.setParam(rv::ParamId::Tone, k.tone);
    t.setParam(rv::ParamId::Drive, k.drive);
    t.setParam(rv::ParamId::Splash, k.splash);
    t.setParam(rv::ParamId::Wobble, k.wobble);
    t.setParam(rv::ParamId::Mix, 1.0f);
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(k.springs));
    t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(k.attitude));
    Out o{Buf(in.size()), Buf(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += 48) {
        const int n = int(std::min<size_t>(48, in.size() - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
    }
    return o;
}

Buf click(float seconds)
{
    Buf x(size_t(seconds * kFs), 0.0f);
    x[size_t(0.05f * kFs)] = 0.9f;
    return x;
}

// ---- 1. Default and pool -----------------------------------------------------------
void defaultAndPool()
{
    rv::Tank t;
    t.prepare(kFs, 48);
#ifdef RV_TANK_DEFAULT_VOICING // a scratch build running the suite as if voicing N shipped
    std::printf("INFO  this build's default voicing is %d (RV_TANK_DEFAULT_VOICING)\n", t.tankVoicing());
#else
    // The owner's pick (ADR 0038 Round 5, 5 Oct 2026): 8, round 5's B (7,
    // "plus gentler", was the pick of 2 Oct and stays as the reference).
    std::snprintf(msg, sizeof msg, "a new Tank plays the default voicing (%d), the owner's pick, 8", t.tankVoicing());
    check(t.tankVoicing() == rv::tankv::kDefaultVoicing && rv::tankv::kDefaultVoicing == rv::tankv::kR5, msg);
#endif
    for (int v = 0; v < rv::tankv::kNumVoicings; ++v) {
        const size_t need = rv::Tank::poolFloatsForVoicing(kFs, v);
        std::snprintf(msg, sizeof msg, "voicing %d alone needs %zu pool floats at 48 kHz (firmware pool 30,000)", v, need);
        check(need <= 30000, msg);
    }
    std::printf("INFO  desktop pool (every voicing) %zu floats, Tank object %zu bytes\n", rv::Tank::requiredPoolFloats(kFs),
                sizeof(rv::Tank));
}

// ---- 2. Stability -------------------------------------------------------------------
void stability()
{
    Buf in(size_t(8.0f * kFs), 0.0f);
    in[100] = 1.0f;
    uint32_t s = 7;
    for (size_t i = size_t(0.5f * kFs); i < size_t(1.5f * kFs); ++i) {
        s = s * 1664525u + 1013904223u;
        in[i] = float(int32_t(s)) / 2147483648.0f;
    }
    for (int v = 1; v < rv::tankv::kNumVoicings; ++v) {
        int   cells = 0, bad = 0;
        float worst = 0.0f;
        for (float tension : {0.0f, 1.0f})
            for (float tone : {0.0f, 1.0f}) {
                Knobs k;
                k.decay = k.drive = k.splash = 1.0f;
                k.tension = tension;
                k.tone    = tone;
                k.springs = 2;
                k.attitude = 2;
                const Out o = render(v, k, in);
                float pk = 0.0f;
                bool  finite = true;
                for (size_t i = 0; i < in.size(); ++i) {
                    finite = finite && std::isfinite(o.l[i]) && std::isfinite(o.r[i]);
                    pk = std::max({pk, std::fabs(o.l[i]), std::fabs(o.r[i])});
                }
                ++cells;
                if (!finite || pk >= 1.0f) ++bad;
                worst = std::max(worst, pk);
            }
        std::snprintf(msg, sizeof msg,
                      "voicing %d: KICKED, DECAY / DRIVE / SPLASH 1, 3 Springs, TENSION x TONE corners (%d cells): finite, "
                      "peak < 1 (worst %.3f)",
                      v, cells, worst);
        check(bad == 0, msg);
    }
}

// ---- 3. First echo ------------------------------------------------------------------
// The body of the first echo: onset (10 % of its peak) of the mono sum low-passed
// below ~1.2 kHz (two one-poles), where the Loop carries it. The first sound of
// any kind (full band) is printed: today the high path's thin tick comes a few ms
// before the body (its pickup sits at 0.30 L, the Loop's at 0.36 L); voicing 1
// lines the high path up on the Loop's echo (proto/wellspring-fit's B), so that
// tick now arrives with the body.
float onsetMs(const Out& o, bool lowOnly)
{
    const size_t a = size_t(0.05f * kFs) + 24, b = a + size_t(0.2f * kFs);
    const float  c = 1.0f - std::exp(-2.0f * 3.14159265f * 1200.0f / kFs);
    Buf m(b, 0.0f);
    float y1 = 0.0f, y2 = 0.0f;
    for (size_t i = 0; i < b; ++i) {
        const float x = o.l[i] + o.r[i];
        y1 += c * (x - y1);
        y2 += c * (y1 - y2);
        m[i] = lowOnly ? y2 : x;
    }
    float pk = 0.0f;
    for (size_t i = a; i < b; ++i) pk = std::max(pk, std::fabs(m[i]));
    for (size_t i = a; i < b; ++i)
        if (std::fabs(m[i]) > 0.1f * pk) return float(i - size_t(0.05f * kFs)) * 1000.0f / kFs;
    return -1.0f;
}

void firstEcho()
{
    const Buf in = click(0.4f);
    for (float tension : {0.0f, 0.5f, 1.0f}) {
        Knobs k;
        k.tension = tension;
        k.decay = 0.5f;
        k.tone = 0.5f;
        const Out   o0 = render(0, k, in);
        const float ref = onsetMs(o0, true), refAny = onsetMs(o0, false);
        for (int v = 1; v < rv::tankv::kNumVoicings; ++v) {
            const Out   o  = render(v, k, in);
            const float ms = onsetMs(o, true);
            std::snprintf(msg, sizeof msg,
                          "voicing %d, TENSION %.1f: the first echo's body %.1f ms after the click (today %.1f; within 1.5); "
                          "first sound %.1f ms (today %.1f)",
                          v, tension, ms, ref, onsetMs(o, false), refAny);
            check(std::fabs(ms - ref) <= 1.5f, msg);
        }
    }
}

// ---- 4. Together and wide -----------------------------------------------------------
void together()
{
    const Buf in = click(2.0f);
    for (int springs : {1, 2}) {
        Knobs k; // the closest settings (2 or 3 Springs)
        k.decay   = 0.68f;
        k.springs = springs;
        for (int v = 0; v < rv::tankv::kNumVoicings; ++v) {
            const Out o = render(v, k, in);
            // 10 ms envelopes over the first 0.5 s after the click.
            const size_t a = size_t(0.05f * kFs), h = size_t(0.01f * kFs);
            std::vector<double> el, er;
            for (int w = 0; w < 50; ++w) {
                double sl = 0.0, sr = 0.0;
                for (size_t i = a + size_t(w) * h; i < a + size_t(w + 1) * h; ++i) {
                    sl += double(o.l[i]) * o.l[i];
                    sr += double(o.r[i]) * o.r[i];
                }
                el.push_back(std::sqrt(sl / double(h)));
                er.push_back(std::sqrt(sr / double(h)));
            }
            auto corr = [](const std::vector<double>& x, const std::vector<double>& y) {
                double mx = 0, my = 0;
                for (size_t i = 0; i < x.size(); ++i) mx += x[i], my += y[i];
                mx /= double(x.size());
                my /= double(y.size());
                double sxy = 0, sxx = 0, syy = 0;
                for (size_t i = 0; i < x.size(); ++i) {
                    sxy += (x[i] - mx) * (y[i] - my);
                    sxx += (x[i] - mx) * (x[i] - mx);
                    syy += (y[i] - my) * (y[i] - my);
                }
                return sxy / std::sqrt(sxx * syy + 1e-30);
            };
            const double env = corr(el, er);
            std::vector<double> fl(o.l.begin() + long(0.25f * kFs), o.l.begin() + long(1.55f * kFs));
            std::vector<double> fr(o.r.begin() + long(0.25f * kFs), o.r.begin() + long(1.55f * kFs));
            const double fine = corr(fl, fr);
            std::snprintf(msg, sizeof msg, "voicing %d, %d Springs: L/R envelope correlation %.2f (>= 0.85), fine structure %.2f (<= 0.35)",
                          v, springs + 1, env, fine);
            if (rv::tankv::hasTogether(v)) check(env >= 0.85 && fine <= 0.35, msg);
            else std::printf("INFO  %s\n", msg);
        }
    }
}

// ---- 5. Mono sum --------------------------------------------------------------------
void monoSum()
{
    const Buf in = click(1.5f);
    for (int springs : {1, 2}) {
        Knobs k;
        k.springs = springs;
        const Out o1 = render(1, k, in), o2 = render(2, k, in);
        double s12 = 0, s11 = 0, s22 = 0;
        for (size_t i = 0; i < in.size(); ++i) {
            const double m1 = double(o1.l[i]) + o1.r[i], m2 = double(o2.l[i]) + o2.r[i];
            s12 += m1 * m2;
            s11 += m1 * m1;
            s22 += m2 * m2;
        }
        const double c = s12 / std::sqrt(s11 * s22 + 1e-30);
        std::snprintf(msg, sizeof msg, "%d Springs: voicing 2's mono sum is voicing 1's times one gain (correlation %.6f >= 0.9999, gain %+.2f dB)",
                      springs + 1, c, 10.0 * std::log10(s22 / s11));
        check(c >= 0.9999, msg);
    }
}

// ---- 6. Echo density ----------------------------------------------------------------
double ned(const Out& o, float t0, float t1)
{
    const size_t w = size_t(0.02f * kFs);
    double acc = 0.0;
    int    n = 0;
    for (size_t s = size_t((0.05f + t0) * kFs); s + w <= size_t((0.05f + t1) * kFs); s += w / 2) {
        double m = 0.0, v = 0.0;
        for (size_t i = s; i < s + w; ++i) m += 0.5 * (o.l[i] + o.r[i]);
        m /= double(w);
        for (size_t i = s; i < s + w; ++i) v += (0.5 * (o.l[i] + o.r[i]) - m) * (0.5 * (o.l[i] + o.r[i]) - m);
        const double sd = std::sqrt(v / double(w)) + 1e-20;
        int above = 0;
        for (size_t i = s; i < s + w; ++i) above += std::fabs(0.5 * (o.l[i] + o.r[i]) - m) > sd;
        acc += double(above) / double(w) / 0.3173105; // erfc(1/sqrt 2)
        ++n;
    }
    return acc / n;
}

void density()
{
    const Buf in = click(1.0f);
    Knobs k;
    k.decay = 0.68f;
    double prev = 0.0;
    for (int v = 0; v < rv::tankv::kNumVoicings; ++v) {
        const Out    o = render(v, k, in);
        const double d1 = ned(o, 0.1f, 0.2f), d3 = ned(o, 0.3f, 0.5f);
        std::snprintf(msg, sizeof msg, "voicing %d, closest settings: echo density %.2f at 100-200 ms, %.2f at 300-500 ms", v, d1, d3);
        if (v == rv::tankv::kDiffuse) {
            char m2[480];
            std::snprintf(m2, sizeof m2, "%s (> voicing 2's %.2f, and >= 0.85)", msg, prev);
            check(d3 > prev && d3 >= 0.85, m2);
        } else {
            std::printf("INFO  %s\n", msg);
        }
        prev = d3;
    }
}

// ---- 7. Onset brightness (voicing 5+) -------------------------------------------------
double onsetHfDb(const Out& o)
{
    const size_t a = size_t(0.05f * kFs), n = size_t(0.06f * kFs);
    double hi = 0.0, mid = 0.0;
    for (size_t b = 1; b < n / 2; ++b) {
        const double f = double(b) * kFs / double(n);
        if (f < 200.0) continue;
        double re = 0.0, im = 0.0;
        const double w = 2.0 * 3.14159265358979 * double(b) / double(n);
        for (size_t i = 0; i < n; ++i) {
            const double x = 0.5 * (double(o.l[a + i]) + o.r[a + i]);
            re += x * std::cos(w * double(i));
            im -= x * std::sin(w * double(i));
        }
        (f > 4000.0 ? hi : mid) += re * re + im * im;
    }
    return 10.0 * std::log10(hi / (mid + 1e-30) + 1e-30);
}

Knobs closest4()
{
    Knobs k; // round 4's closest settings (tools/wellspring_settings_fit.py, 2 Oct 2026)
    k.tension = 0.5f;
    k.tone    = 0.7f;
    k.decay   = 0.664f;
    k.springs = 1;
    return k;
}

void onsetBrightness()
{
    const Buf   in = click(0.4f);
    const Knobs k  = closest4();
    const double ref = onsetHfDb(render(rv::tankv::kDiffuse, k, in));
    for (int v = rv::tankv::kTransducers; v < rv::tankv::kNumVoicings; ++v) {
        const double d = onsetHfDb(render(v, k, in));
        std::snprintf(msg, sizeof msg, "voicing %d: onset brightness %.1f dB (voicing 3 %.1f; at least 12 dB darker)", v, d, ref);
        check(d <= ref - 12.0, msg);
    }
}

// ---- 8. Wide, and still mono-safe (voicing 6+) -----------------------------------------
void wide()
{
    const Buf in = click(1.6f);
    for (int springs : {1, 2}) {
        Knobs k = closest4();
        k.springs = springs;
        const Out o5 = render(rv::tankv::kTransducers, k, in);
        for (int v = rv::tankv::kWide; v < rv::tankv::kNumVoicings; ++v) {
            const Out o = render(v, k, in);
            double sl = 0, sr = 0, slr = 0;
            for (size_t i = size_t(0.25f * kFs); i < size_t(1.55f * kFs); ++i) {
                sl += double(o.l[i]) * o.l[i];
                sr += double(o.r[i]) * o.r[i];
                slr += double(o.l[i]) * o.r[i];
            }
            const double fine = slr / std::sqrt(sl * sr + 1e-30);
            std::snprintf(msg, sizeof msg, "voicing %d, %d Springs: fine-structure L/R correlation %.2f (<= 0.1)", v, springs + 1, fine);
            check(fine <= 0.1, msg);
            if (v != rv::tankv::kWide) continue;
            double s12 = 0, s11 = 0, s22 = 0;
            for (size_t i = 0; i < in.size(); ++i) {
                const double m1 = double(o5.l[i]) + o5.r[i], m2 = double(o.l[i]) + o.r[i];
                s12 += m1 * m2;
                s11 += m1 * m1;
                s22 += m2 * m2;
            }
            const double c = s12 / std::sqrt(s11 * s22 + 1e-30);
            std::snprintf(msg, sizeof msg, "%d Springs: voicing 6's mono sum is voicing 5's times one gain (correlation %.6f >= 0.9999, gain %+.2f dB)",
                          springs + 1, c, 10.0 * std::log10(s22 / s11));
            check(c >= 0.9999, msg);
        }
    }
}

// ---- 9. The low cut's makeup (voicing 7) ---------------------------------------------
void lowCutMakeup()
{
    // A low, held chord: 2 s of 110 + 165 + 220 Hz (saw-like: 1/n harmonics), then silence.
    Buf in(size_t(3.0f * kFs), 0.0f);
    for (size_t i = 0; i < size_t(2.0f * kFs); ++i)
        for (float f0 : {110.0f, 165.0f, 220.0f})
            for (int h = 1; h <= 8; ++h)
                in[i] += 0.06f / float(h) * std::sin(2.0f * 3.14159265f * f0 * float(h) * float(i) / kFs);
    const Knobs k = closest4();
    auto power = [](const Out& o) {
        double p = 0.0;
        for (size_t i = 0; i < o.l.size(); ++i) p += double(o.l[i]) * o.l[i] + double(o.r[i]) * o.r[i];
        return 10.0 * std::log10(p + 1e-30);
    };
    const double p6 = power(render(rv::tankv::kWide, k, in)), p7 = power(render(rv::tankv::kGentleWide, k, in));
    std::snprintf(msg, sizeof msg, "voicing 7: a low held chord %+.2f dB re voicing 6 (within 2 dB)", p7 - p6);
    check(std::fabs(p7 - p6) <= 2.0, msg);
}

} // namespace

int main()
{
    defaultAndPool();
    stability();
    firstEcho();
    together();
    monoSum();
    density();
    onsetBrightness();
    wide();
    lowCutMakeup();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
