// M7 criteria through the whole Tank (SPEC §7 M7, docs/m7-integration.md):
// SPLASH on 02_hits, WOBBLE on 08_held_tones, the ATTITUDE Morph of the M7
// tables, and determinism with everything M7 turned up. The Kick criteria
// are in test_kick, MIX in test_mix, the components alone in test_splash,
// test_kick_voice and test_wobble.
//
// "Splash share": the Tank has test hooks (setSplashParts) that keep the
// Splash listening but drop its Clatter and/or its
// Jolt. The Splash's contribution is then the difference of renders that
// are otherwise identical (same seeds, same everything):
//   splash energy  = energy of (on − off) in the 6 s after a hit (Clatter + Jolt)
//   clatter dB     = 1–6 kHz energy of (Clatter only − off) re the off render's
//                    own 1–6 kHz energy, first 150 ms after the hit: how loud the
//                    crash is against the hit's own bright part
//   crash dB       = 1–6 kHz energy on vs off, first 150 ms (total brightening)
//   settled dB     = the same 1.0–1.5 s after the hit
// M8 adds SPLASH audibility on a rimshot at -18..-3 dBFS (splashAudible)
// and ghost notes judged between louder hits (ghostGroove), for the
// level-adaptive hit detector (SplashVoicing.h), and CLEAN's gentle splash
// (ADR 0025: CLEAN gentle < DRIVEN clear < KICKED unmistakable).
// Stimuli are read from test_audio/stimulus (tools/make_stimulus.py); a
// missing file skips its section.

#include "Wav.h"
#include "dsp/Filters.h"
#include "dsp/Tank.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int  failures = 0;
char msg[512];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float kFs = 48000.0f;
const char* const kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};

struct Settings {
    float decay = 0.5f, drive = 0.5f, splash = 0.3f, wobble = 0.5f, tone = 0.5f, tension = 0.5f; // WOBBLE noon = still
    int   att = 1, springs = 1;
    bool  clatterOn = true, joltOn = true; // Tank::setSplashParts
};

struct Out {
    Buf l, r;
    Buf jolt; // Splash Jolt envelope after each block
    Buf clat; // Splash Clatter burst envelope after each block
};

Out render(const Settings& s, const Buf& in, int block = 48)
{
    rv::Tank t;
    t.prepare(kFs, block);
    t.setParam(rv::ParamId::Mix, 1.0f);
    t.setParam(rv::ParamId::Decay, s.decay);
    t.setParam(rv::ParamId::Drive, s.drive);
    t.setParam(rv::ParamId::Splash, s.splash);
    t.setParam(rv::ParamId::Wobble, s.wobble);
    t.setParam(rv::ParamId::Tone, s.tone);
    t.setParam(rv::ParamId::Tension, s.tension);
    t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(s.springs));
    t.setSplashParts(s.clatterOn, s.joltOn);
    Out o{Buf(in.size()), Buf(in.size()), Buf(in.size()), Buf(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += size_t(block)) {
        const int n = int(std::min(size_t(block), in.size() - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        for (int i = 0; i < n; ++i) {
            o.jolt[pos + size_t(i)] = t.splash().joltEnvelope();
            o.clat[pos + size_t(i)] = s.clatterOn ? t.splash().clatterEnvelope() : 0.0f;
        }
    }
    return o;
}

Buf mono(const Out& o)
{
    Buf m(o.l.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
    return m;
}
Buf band(const Buf& x, float lo, float hi)
{
    rv::dsp::Biquad h1, h2, l1, l2;
    h1.setHighpass(lo, 0.707f, kFs);
    h2.setHighpass(lo, 0.707f, kFs);
    l1.setLowpass(hi, 0.707f, kFs);
    l2.setLowpass(hi, 0.707f, kFs);
    Buf y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = l2.process(l1.process(h2.process(h1.process(x[i]))));
    return y;
}
double energy(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < std::min(to, x.size()); ++i) s += double(x[i]) * x[i];
    return s;
}
double db(double p) { return 10.0 * std::log10(p + 1e-30); }

bool load(const char* name, Buf& x)
{
    rv::wav::Audio a;
    std::string err;
    for (const char* prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (rv::wav::read(std::string(prefix) + name, a, err) && a.sampleRate == 48000 && !a.channels.empty()) {
            x = a.channels[0];
            return true;
        }
    std::printf("SKIP  %s not found (run python3 tools/make_stimulus.py)\n", name);
    return false;
}

// ---- 1. SPLASH on 02_hits -------------------------------------------------------------
// 02_hits: snare at -6 / -12 / -18 dBFS from 1 s, then rim at the same
// levels, 6 s apart. DECAY noon (tails gone long before the next hit),
// DRIVE 0.5, 2 Springs.
struct HitStats {
    double splashE[6], clatterDb[6], crashDb[6], settledDb[6];
    // Hard snare: how far the crash (Clatter-only - off, 1-6 kHz) and the
    // hit's own tail (off, 1-6 kHz) fall from the first 150 ms to 1.0-1.5 s
    // after the hit, power per second, dB.
    double crashFall = 0, tailFall = 0;
    float  joltPeak = 0, joltAt1s = 0; // hard snare
};

HitStats hitStats(const Buf& x, int att, float splash)
{
    Settings s;
    s.att    = att;
    s.splash = splash;
    const Out on = render(s, x);
    s.joltOn = false;
    const Out clat = render(s, x);
    s.clatterOn = false;
    const Out off = render(s, x);
    const Buf mOn = mono(on), mOff = mono(off), mClat = mono(clat);
    Buf d(mOn.size()), dc(mOn.size());
    for (size_t i = 0; i < d.size(); ++i) {
        d[i]  = mOn[i] - mOff[i];
        dc[i] = mClat[i] - mOff[i];
    }
    const Buf hOn = band(mOn, 1000.0f, 6000.0f), hOff = band(mOff, 1000.0f, 6000.0f), hC = band(dc, 1000.0f, 6000.0f);
    HitStats h{};
    for (int k = 0; k < 6; ++k) {
        const size_t at = size_t((1.0f + 6.0f * float(k)) * kFs);
        h.splashE[k]   = energy(d, at, at + size_t(6.0f * kFs));
        h.clatterDb[k] = db(energy(hC, at, at + size_t(0.15f * kFs)) / energy(hOff, at, at + size_t(0.15f * kFs)));
        h.crashDb[k]   = db(energy(hOn, at, at + size_t(0.15f * kFs)) / energy(hOff, at, at + size_t(0.15f * kFs)));
        h.settledDb[k] = db(energy(hOn, at + size_t(kFs), at + size_t(1.5f * kFs))
                            / energy(hOff, at + size_t(kFs), at + size_t(1.5f * kFs)));
    }
    const size_t at = size_t(kFs), e1 = at + size_t(0.15f * kFs), l0 = at + size_t(kFs), l1 = at + size_t(1.5f * kFs);
    h.crashFall = db((energy(hC, at, e1) / 0.15) / (energy(hC, l0, l1) / 0.5));
    h.tailFall  = db((energy(hOff, at, e1) / 0.15) / (energy(hOff, l0, l1) / 0.5));
    for (size_t i = at; i < at + size_t(0.5f * kFs); ++i) h.joltPeak = std::max(h.joltPeak, on.jolt[i]);
    h.joltAt1s = on.jolt[at + size_t(kFs)];
    return h;
}

void splashOnHits()
{
    Buf x;
    if (!load("02_hits.wav", x)) return;
    std::printf("      02_hits through the Tank (DECAY 0.5, DRIVE 0.5, 2 Springs). Splash energy of the -12 / -18 dBFS hit\n"
                "      re the -6 dBFS one (snare; rim); Clatter re the hit's own 1-6 kHz and crash = 1-6 kHz on/off, first\n"
                "      150 ms (snare -6/-12/-18); settled = 1.0-1.5 s after the -6 dBFS snare:\n");
    bool ghostOk = true;
    HitStats k1{}, d0{}, d1{}, c1{};
    for (int att : {1, 2})
        for (float sv : {0.0f, 0.5f, 1.0f}) {
            const HitStats h = hitStats(x, att, sv);
            std::printf("      %-6s SPLASH %.1f: snare %6.1f / %6.1f dB, rim %6.1f / %6.1f dB; Clatter %6.1f / %6.1f / %6.1f dB, "
                        "crash %+4.1f / %+4.1f / %+4.1f dB; settled %+4.1f dB; Jolt peak %.3f, at +1 s %.5f\n",
                        kAttName[att], sv, db(h.splashE[1] / h.splashE[0]), db(h.splashE[2] / h.splashE[0]),
                        db(h.splashE[4] / h.splashE[3]), db(h.splashE[5] / h.splashE[3]), h.clatterDb[0], h.clatterDb[1],
                        h.clatterDb[2], h.crashDb[0], h.crashDb[1], h.crashDb[2], h.settledDb[0], h.joltPeak, h.joltAt1s);
            ghostOk &= h.splashE[0] > h.splashE[1] && h.splashE[1] > h.splashE[2] && h.splashE[2] < 0.25 * h.splashE[0]
                    && h.splashE[3] > h.splashE[4] && h.splashE[4] > h.splashE[5] && h.splashE[5] < 0.25 * h.splashE[3];
            if (att == 2 && sv == 1.0f) k1 = h;
            if (att == 1 && sv == 0.0f) d0 = h;
            if (att == 1 && sv == 1.0f) d1 = h;
        }
    check(ghostOk, "Splash through the Tank falls with hit level; the -18 dBFS ghost gives < 25 % of the -6 dBFS hit's "
                   "Splash energy (DRIVEN, KICKED; SPLASH 0 / 0.5 / 1; snare and rim)");

    // M8: the -18 dBFS hit of 02_hits comes 6 s after the last one, so the
    // level-adaptive detector hears it as an isolated quiet hit, not a ghost
    // note, and gives it a crash in proportion to its size (SplashVoicing.h).
    // "Ghosts barely trigger" is now checked where ghosts live, between
    // louder hits (ghostGroove below); here its energy is still < 25 %.
    // The crash is the springs clanging, in the tank (Clatter mostly into
    // the Loop, SplashVoicing.h), so it rings on with the tail and dies with
    // it, instead of stopping dead on top of it (M8 round 1's direct share:
    // settled +0.1 dB, "a sound played over the top"). So "settled back to
    // the tail within 1.5 dB after 1 s" no longer applies; what must hold:
    //  - it dies with the tail, never slower: from the first 150 ms to
    //    1.0-1.5 s the crash falls at least as far as the hit's own tail
    //    (within 1 dB), so it can never outlast it or build into a wash;
    //  - the tail is still the hit's: settled <= +6 dB (KICKED; the crash
    //    in the 1-6 kHz band at most ~3x the hit's own, dark, tail there),
    //    and never above the crash's own first 150 ms.
    std::snprintf(msg, sizeof msg,
                  "KICKED SPLASH 1, -6 dBFS snare: big bright crash (Clatter %+.1f dB re the hit's own 1-6 kHz, >= -6; crash "
                  "%+.1f dB); dies with the tail (crash falls %.1f dB from the first 150 ms to 1-1.5 s, tail %.1f dB: no "
                  "slower than the tail - 1 dB; settled %+.1f dB, <= +6 and <= the crash); Jolt %.2f %% of its peak (< 2 %%)",
                  k1.clatterDb[0], k1.crashDb[0], k1.crashFall, k1.tailFall, k1.settledDb[0],
                  100.0 * k1.joltAt1s / std::max(1e-9f, k1.joltPeak));
    check(k1.clatterDb[0] >= -6.0 && k1.crashFall >= k1.tailFall - 1.0 && k1.settledDb[0] <= 6.0
              && k1.settledDb[0] <= k1.crashDb[0] && k1.joltAt1s < 0.02f * k1.joltPeak,
          msg);
    std::snprintf(msg, sizeof msg, "DRIVEN SPLASH 1 moderate: Clatter %+.1f dB re the hit (between KICKED's %+.1f and -20)",
                  d1.clatterDb[0], k1.clatterDb[0]);
    check(d1.clatterDb[0] < k1.clatterDb[0] && d1.clatterDb[0] > -20.0, msg);

    // SPLASH 0: no Clatter in any ATTITUDE. DRIVEN / KICKED used to keep a
    // "faint natural splash" floor there, which the owner heard as a click
    // on every hard hit (Clatter peak ~13 dB under the wet peak on a 0 dBFS
    // snare). The small Jolt floor stays: a pitch lurch, no transient.
    std::snprintf(msg, sizeof msg,
                  "DRIVEN SPLASH 0: no Clatter, so no click on hard hits (Clatter %+.1f dB re the hit); the Jolt floor "
                  "stays (Splash energy %s)",
                  d0.clatterDb[0], d0.splashE[0] > 0.0 ? "> 0" : "0");
    check(d0.clatterDb[0] < -200.0 && d0.splashE[0] > 0.0, msg);

    // CLEAN (ADR 0025): a real but gentle splash. Nothing at SPLASH 0; at
    // SPLASH 1 a light crash under DRIVEN's that dies with the tail (as
    // KICKED above; its settled share at most DRIVEN's and <= +3 dB, was
    // "within 1 dB" when the crash sat on top), and a Jolt peak at most a
    // third of DRIVEN's.
    c1 = hitStats(x, 0, 1.0f);
    const HitStats c0 = hitStats(x, 0, 0.0f);
    // The lurch: Jolt envelope peak x the Loop delay offset at j = 1 (% of L).
    const double cLurch = 100.0 * c1.joltPeak * rv::splash::kVoice[0].joltLoopFrac;
    const double dLurch = 100.0 * d1.joltPeak * rv::splash::kVoice[1].joltLoopFrac;
    std::snprintf(msg, sizeof msg,
                  "CLEAN SPLASH 1 gentle: Clatter %+.1f dB re the hit (below DRIVEN's %+.1f), crash %+.1f dB; dies with the "
                  "tail (falls %.1f dB, tail %.1f); settled %+.1f dB (<= +3, <= DRIVEN's %+.1f); Jolt lurch %.3f %% of L "
                  "(DRIVEN %.3f %%, <= 1/3); SPLASH 0 adds nothing",
                  c1.clatterDb[0], d1.clatterDb[0], c1.crashDb[0], c1.crashFall, c1.tailFall, c1.settledDb[0],
                  d1.settledDb[0], cLurch, dLurch);
    check(c1.clatterDb[0] < d1.clatterDb[0] && c1.crashDb[0] > 0.0 && c1.crashDb[0] < d1.crashDb[0]
              && c1.crashFall >= c1.tailFall - 1.0 && c1.settledDb[0] <= 3.0 && c1.settledDb[0] <= d1.settledDb[0]
              && cLurch > 0.0 && cLurch <= dLurch / 3.0 && c0.splashE[0] == 0.0,
          msg);
}

// ---- 1b. SPLASH audible at any sensible level (M8, backlog item 2) -----------------------
// The owner could not hear SPLASH 0 vs 1 on a rimshot at -9 dBTP. A
// rimshot-like one-shot (stick crack + 480 Hz / 1.1 kHz head ring + a short
// buzz, ~150 ms) at -18, -9 and -3 dBFS, and 02_hits' -6 dBFS snare,
// DECAY noon, SPRINGS 2, MIX 1, DRIVE default. "Crash" = 1-6 kHz energy,
// SPLASH 1 vs SPLASH 0, in the first 150 ms after the hit (both renders
// otherwise identical). Why not a null test: the Jolt bends the tail's
// pitch, so SPLASH 0 vs 1 nulls near 0 dB even when the crash is inaudible
// (the M7 build: +3 dB null, 0.0 dB of crash on the rimshot in DRIVEN).
// Thresholds: +3 dB of added brightness in the crash band is clearly
// audible (loudness JND ~0.5-1 dB); +6 dB (the crash as loud as the hit's
// own bright part again) is unmistakable. DRIVEN >= +3, KICKED >= +6.
Buf rimshot(float peakDb, size_t len, float at, uint32_t seed = 9u)
{
    rv::dsp::Rng rng;
    rng.seed(seed);
    const size_t n = size_t(0.15f * kFs);
    rv::dsp::OnePoleLowpass c1, c2, b1, b2;
    c1.setCutoff(1500.0f, kFs);
    c2.setCutoff(1500.0f, kFs);
    b1.setCutoff(6000.0f, kFs);
    b2.setCutoff(900.0f, kFs);
    Buf h(n);
    float peak = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        const float t = float(i) / kFs;
        float c = rng.bipolar();
        c -= c1.process(c);
        c -= c2.process(c); // crack: HP noise
        float b = b1.process(rng.bipolar());
        b -= b2.process(b); // buzz: band noise
        h[i] = 0.8f * std::sin(2.0f * rv::map::kPi * 480.0f * t) * std::exp(-t / 0.035f)
             + 0.5f * std::sin(2.0f * rv::map::kPi * 1130.0f * t + 0.7f) * std::exp(-t / 0.025f)
             + 2.2f * c * std::exp(-t / 0.004f) + 0.5f * b * std::exp(-t / 0.04f);
        peak = std::max(peak, std::fabs(h[i]));
    }
    Buf x(len, 0.0f);
    const float g = std::pow(10.0f, peakDb / 20.0f) / peak;
    for (size_t i = 0; i < n; ++i) x[size_t(at * kFs) + i] = g * h[i];
    return x;
}

double crashDb(const Buf& x, int att, float splash, float at)
{
    Settings s;
    s.att    = att;
    s.drive  = rv::spec(rv::ParamId::Drive).defaultValue;
    s.splash = 0.0f;
    const Buf h0 = band(mono(render(s, x)), 1000.0f, 6000.0f);
    s.splash = splash;
    const Buf h1 = band(mono(render(s, x)), 1000.0f, 6000.0f);
    const size_t a = size_t(at * kFs), b = a + size_t(0.15f * kFs);
    return db(energy(h1, a, b) / energy(h0, a, b));
}

void splashAudible()
{
    Buf hits;
    const bool haveHits = load("02_hits.wav", hits);
    if (haveHits) hits.resize(size_t(3.0f * kFs)); // the -6 dBFS snare at 1 s
    bool ok = true;
    double below[3] = {0, 0, 0}; // CLEAN's crash per level, then checked under DRIVEN's
    for (int att : {0, 1, 2}) {
        const double want = att == 0 ? 1.5 : att == 1 ? 3.0 : 6.0;
        int li = 0;
        std::printf("      %-6s crash SPLASH 1 (0.5) vs 0, 1-6 kHz first 150 ms:", kAttName[att]);
        for (float lvl : {-18.0f, -9.0f, -3.0f}) {
            const Buf x = rimshot(lvl, size_t(2.5f * kFs), 0.5f);
            const double c1 = crashDb(x, att, 1.0f, 0.5f), c5 = crashDb(x, att, 0.5f, 0.5f);
            std::printf("  rim %3.0f dBFS %+5.1f (%+4.1f)", lvl, c1, c5);
            ok &= c1 >= want && (lvl < -12.0f || c5 > (att == 0 ? 0.25 : 0.5)); // at noon a -18 dBFS hit may stay under the threshold
            if (att == 0) below[li] = c1;
            if (att == 1) ok &= below[li] < c1; // CLEAN gentler than DRIVEN at the same hit
            ++li;
        }
        if (haveHits) {
            const double c1 = crashDb(hits, att, 1.0f, 1.0f);
            std::printf("  02_hits snare -6 %+5.1f", c1);
            ok &= c1 >= want;
        }
        std::printf("\n");
    }
    check(ok, "SPLASH 1 audible on a rimshot at -18 / -9 / -3 dBFS and the -6 dBFS snare: crash >= +1.5 dB (CLEAN, below "
              "DRIVEN's at each level), >= +3 dB (DRIVEN), >= +6 dB (KICKED); SPLASH 0.5 already adds from -9 dBFS");
}

// ---- 1c. Ghost notes between louder hits barely trigger (M8) -------------------------------
// A groove: -6 dBFS snare backbeats every 1 s, -18 dBFS rimshot ghost notes
// half way between. The level-adaptive detector judges each hit against the
// program level, so the ghosts barely trigger while the backbeats crash.
// The ghost's Clatter burst < 25 % of a backbeat's (source energy), and its
// crash re its own bright part <= -15 dB (the M7 criterion; derived from the
// backbeat's measured crash, see below). Every ATTITUDE, SPLASH 1.
void ghostGroove()
{
    const size_t len = size_t(5.0f * kFs);
    Buf x(len, 0.0f);
    rv::dsp::Rng rng;
    rng.seed(3u);
    rv::dsp::OnePoleLowpass lp, hp;
    lp.setCutoff(7000.0f, kFs);
    hp.setCutoff(800.0f, kFs);
    Buf sn(size_t(0.25f * kFs));
    float pk = 0.0f;
    for (size_t i = 0; i < sn.size(); ++i) {
        const float t = float(i) / kFs;
        float nz = lp.process(rng.bipolar());
        nz -= hp.process(nz);
        sn[i] = 0.6f * std::sin(2.0f * rv::map::kPi * 185.0f * t) * std::exp(-t / 0.03f) + 1.2f * nz * std::exp(-t / 0.06f);
        pk = std::max(pk, std::fabs(sn[i]));
    }
    const Buf ghost = rimshot(-18.0f, size_t(0.2f * kFs), 0.0f);
    Buf beats = x; // backbeats only
    for (int k = 0; k < 4; ++k) {
        const size_t b = size_t((0.5f + float(k)) * kFs), g = size_t((1.0f + float(k)) * kFs);
        for (size_t i = 0; i < sn.size(); ++i) x[b + i] += 0.5012f / pk * sn[i];
        for (size_t i = 0; i < sn.size(); ++i) beats[b + i] += 0.5012f / pk * sn[i];
        for (size_t i = 0; i < ghost.size(); ++i) x[g + i] += ghost[i];
    }
    bool ok = true;
    for (int att : {0, 1, 2}) {
        Settings s;
        s.att    = att;
        s.splash = 1.0f;
        s.drive  = rv::spec(rv::ParamId::Drive).defaultValue;
        s.joltOn = false;
        const Out on = render(s, x);
        const Buf bc = mono(render(s, beats));
        s.clatterOn = false;
        const Buf mo = mono(render(s, x)), bo = mono(render(s, beats));
        // The crash rings in the tank (Clatter into the Loop), so a
        // backbeat's crash is still sounding half a second later, under the
        // ghost, and a render difference cannot isolate the ghost's own crash
        // (the ghosts nudge the detector's program level, which changes the
        // backbeat's ringing crash by a fraction of a dB). So the ghost's
        // crash is judged at its source: its Clatter burst energy re a
        // backbeat's (the Splash's burst envelope, squared and summed; the
        // Tank carries every burst the same way). The backbeat's crash re
        // its own bright part is measured (1-6 kHz, first 150 ms, Clatter-
        // only render - no-Clatter render, backbeats only), and the ghost's
        // follows: backbeat crash + burst ratio + (backbeat's own bright
        // part re the ghost's own: no-Clatter render with - without ghosts).
        Buf dbt(bc.size()), dg(bc.size());
        for (size_t i = 0; i < dbt.size(); ++i) {
            dbt[i] = bc[i] - bo[i];
            dg[i]  = mo[i] - bo[i];
        }
        const Buf hb = band(dbt, 1000.0f, 6000.0f), hob = band(bo, 1000.0f, 6000.0f), hog = band(dg, 1000.0f, 6000.0f);
        double worstShare = -300, beatOwn = 300, worstOwn = -300;
        const size_t w = size_t(0.15f * kFs);
        for (int k = 1; k < 4; ++k) { // from the second bar: the program level has settled
            const size_t b = size_t((0.5f + float(k)) * kFs), g = size_t((1.0f + float(k)) * kFs);
            const double burstG = energy(on.clat, g, g + w), burstB = energy(on.clat, b, b + w);
            const double share = db(burstG / burstB);
            const double own   = db(energy(hb, b, b + w) / energy(hob, b, b + w));
            worstShare = std::max(worstShare, share);
            beatOwn    = std::min(beatOwn, own);
            worstOwn   = std::max(worstOwn, own + share + db(energy(hob, b, b + w) / energy(hog, g, g + w)));
        }
        std::snprintf(msg, sizeof msg,
                      "%s SPLASH 1 groove: ghost notes (-18 dBFS between -6 dBFS backbeats) barely trigger: Clatter burst "
                      "%.1f dB re a backbeat's (< -6), so its crash %+.1f dB re its own 1-6 kHz (<= -15); backbeats %+.1f dB",
                      kAttName[att], worstShare, worstOwn, beatOwn);
        const bool good = worstOwn <= -15.0 && worstShare < -6.0 && beatOwn > worstOwn + 15.0;
        check(good, msg);
        ok &= good;
    }
    (void)ok;
}

// ---- 2. WOBBLE on 08_held_tones ---------------------------------------------------------
// Bipolar WOBBLE (ADR 0034): noon still, left = random wow + flutter, right =
// sine LFO. Walked in 0.1 knob steps, as the sweet-spot check does.
// 1 kHz at -12 dBFS from 1 s to 9 s. DRIVEN at the default DRIVE, SPRINGS 1
// (one Spring: a clean pitch to track), SPLASH 0, MIX 1. The wet is the
// Tank's tail building on the held tone (as test_wobble's "tail": the Loop
// multiplies the per-pass shift). Pitch = 10-cycle-averaged zero-crossing
// frequency of the 700-1400 Hz band, 3-8.9 s, in cents re its median; p95
// (peak) of |cents|. (After the tone stops the tail is several Loop modes
// near 1 kHz beating, so a single pitch is not defined there.)
struct Pitch {
    double p95 = 0, peak = 0;
};
Pitch pitchCents(const Buf& y, size_t from, size_t to)
{
    std::vector<double> f;
    double lastCross = -1;
    int cycles = 0;
    for (size_t n = from + 1; n < std::min(to, y.size()); ++n) {
        if (y[n - 1] < 0 && y[n] >= 0) {
            const double cross = double(n - 1) + double(y[n - 1]) / double(y[n - 1] - y[n]);
            if (++cycles == 10) {
                if (lastCross >= 0) f.push_back(10.0 * double(kFs) / (cross - lastCross));
                lastCross = cross;
                cycles = 0;
            }
        }
    }
    Pitch p;
    if (f.size() < 10) return p;
    std::vector<double> s = f;
    std::nth_element(s.begin(), s.begin() + long(s.size() / 2), s.end());
    const double med = s[s.size() / 2];
    std::vector<double> c;
    for (double v : f) c.push_back(std::fabs(1200.0 * std::log2(v / med)));
    p.peak = *std::max_element(c.begin(), c.end());
    std::nth_element(c.begin(), c.begin() + long(0.95 * double(c.size())), c.end());
    p.p95 = c[size_t(0.95 * double(c.size()))];
    return p;
}

void wobbleOnHeldTones()
{
    Buf x;
    if (!load("08_held_tones.wav", x)) return;
    x.resize(size_t(9.0f * kFs));
    std::printf("      08_held_tones, 1 kHz held, DRIVEN (DRIVE default), 1 Spring: wet p95 (peak) |cents| re median\n");
    constexpr int kN = 11; // knob 0 (fully left) .. 1 (fully right), noon at [5]
    const float ds[] = {0.0f, 0.5f, 1.0f};
    double c[3][kN], dflt[3];
    auto measure = [&](float w, float d) {
        Settings s;
        s.drive   = rv::spec(rv::ParamId::Drive).defaultValue;
        s.decay   = d;
        s.springs = 0;
        s.splash  = 0.0f;
        s.wobble  = w;
        return pitchCents(band(render(s, x).l, 700.0f, 1400.0f), size_t(3.0f * kFs), size_t(8.9f * kFs));
    };
    for (int di = 0; di < 3; ++di) {
        std::printf("      DECAY %.1f:", ds[di]);
        for (int i = 0; i < kN; ++i) {
            const Pitch p = measure(0.1f * float(i), ds[di]);
            c[di][i] = p.p95;
            std::printf(" W%.1f %4.1f (%4.1f)", 0.1f * float(i), p.p95, p.peak);
        }
        dflt[di] = measure(rv::spec(rv::ParamId::Wobble).defaultValue, ds[di]).p95;
        std::printf("  default %.1f\n", dflt[di]);
    }
    bool floorOk = true, dfltOk = true, rising = true, ends = true, even = true;
    for (int di = 0; di < 3; ++di) {
        floorOk &= c[di][5] < 3.0;
        dfltOk &= dflt[di] < 3.0;
        // At DECAY max the tail is several Loop modes beating and the 6 s
        // window holds few slow wow cycles, so the reading is noisy there:
        // allow 10 % (it must still never clearly fall).
        const double tol = di == 2 ? 0.9 : 1.0;
        for (int i = 4; i > 0; --i) rising &= c[di][i - 1] > tol * c[di][i];
        for (int i = 6; i < kN - 1; ++i) rising &= c[di][i + 1] > tol * c[di][i];
        ends &= std::min(c[di][0], c[di][10]) >= (di == 0 ? 10.0 : 25.0);
        even &= c[di][0] >= 0.6 * c[di][10] && c[di][0] <= 1.6 * c[di][10];
    }
    const bool first = c[1][4] >= 1.5 && c[1][6] >= 1.5;
    std::snprintf(msg, sizeof msg, "WOBBLE noon = Micro-mod floor only: p95 %.1f / %.1f / %.1f cents at DECAY 0 / 0.5 / 1 (< 3)",
                  c[0][5], c[1][5], c[2][5]);
    check(floorOk, msg);
    std::snprintf(msg, sizeof msg,
                  "default WOBBLE %.2f (a touch of shared Drift): p95 %.1f / %.1f / %.1f cents (< 3: held chords in tune)",
                  double(rv::spec(rv::ParamId::Wobble).defaultValue), dflt[0], dflt[1], dflt[2]);
    check(dfltOk, msg);
    std::snprintf(msg, sizeof msg,
                  "first step off noon is heard (the old 9 o'clock ~ noon complaint): p95 %.1f cents at 0.4 (random), %.1f at 0.6 "
                  "(LFO), DECAY noon (>= 1.5)",
                  c[1][4], c[1][6]);
    check(first, msg);
    check(rising, "every 0.1 step away from noon moves the held tone more, both sides, at DECAY 0 / 0.5 (and never clearly less at 1)");
    std::snprintf(msg, sizeof msg,
                  "both end stops clearly out of tune: fully left %.1f / %.1f / %.1f, fully right %.1f / %.1f / %.1f cents "
                  "(>= 10 at DECAY 0, >= 25 at noon and max; old WOBBLE 1: 39.0 / 48.6 / 55.3)",
                  c[0][0], c[1][0], c[2][0], c[0][10], c[1][10], c[2][10]);
    check(ends, msg);
    check(even, "fully left roughly as wild as fully right (0.6..1.6x at every DECAY)");
}

// ---- 3. ATTITUDE Morph blends the Splash and Kick tables ---------------------------------
// A DRIVEN -> KICKED flip: the Splash voicing (clatterMax) must glide over
// the Morph (drive::kMorphSeconds), never step.
void morph()
{
    rv::Tank t;
    t.prepare(kFs, 32);
    t.setParam(rv::ParamId::Attitude, 0.5f);
    Buf in(32, 0.0f), l(32), r(32);
    for (int i = 0; i < 50; ++i) t.process(in.data(), in.data(), l.data(), r.data(), 32);
    const float from = t.splash().voice().clatterMax;
    t.setParam(rv::ParamId::Attitude, 1.0f);
    float prev = from, maxStep = 0.0f;
    int steps = 0;
    for (int i = 0; i < 200; ++i) {
        t.process(in.data(), in.data(), l.data(), r.data(), 32);
        const float v = t.splash().voice().clatterMax;
        maxStep = std::max(maxStep, std::fabs(v - prev));
        steps += v != prev;
        prev = v;
    }
    const float total = std::fabs(prev - from);
    std::snprintf(msg, sizeof msg,
                  "ATTITUDE DRIVEN -> KICKED: Splash voicing glides (Clatter max %.2f -> %.2f over %d ticks, largest step %.1f %% "
                  "of the change)",
                  from, prev, steps, 100.0 * maxStep / std::max(1e-9f, total));
    check(steps >= 10 && maxStep <= 0.1f * total && std::fabs(prev - rv::splash::kVoice[2].clatterMax) < 1e-6f, msg);
}

// ---- 4. Determinism with everything M7 up ------------------------------------------------
void determinism()
{
    Buf x;
    if (!load("02_hits.wav", x)) return;
    x.resize(size_t(8.0f * kFs));
    for (float wob : {0.0f, 1.0f}) { // both ends of the bipolar WOBBLE
    auto run = [&](int block) {
        rv::Tank t;
        t.prepare(kFs, block);
        t.setParam(rv::ParamId::Mix, 1.0f);
        t.setParam(rv::ParamId::Attitude, 1.0f);
        t.setParam(rv::ParamId::Splash, 1.0f);
        t.setParam(rv::ParamId::Wobble, wob);
        t.setParam(rv::ParamId::Springs, 1.0f);
        Out o{Buf(x.size()), Buf(x.size()), {}, {}};
        const long kickAt = long(3.3f * kFs);
        for (size_t pos = 0; pos < x.size(); pos += size_t(block)) {
            const int n = int(std::min(size_t(block), x.size() - pos));
            if (kickAt >= long(pos) && kickAt < long(pos) + n) t.kick(int(kickAt - long(pos)));
            t.process(x.data() + pos, x.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        }
        return o;
    };
    const Out ref = run(48);
    bool same = true;
    for (int b : {1, 7, 333, 1024}) {
        const Out o = run(b);
        same &= o.l == ref.l && o.r == ref.r;
    }
    const Out again = run(48);
    same &= again.l == ref.l && again.r == ref.r;
    std::snprintf(msg, sizeof msg,
                  "KICKED, SPLASH 1, WOBBLE %.0f (fully %s), 3 Springs, hits + Kick: bit-identical for blocks 1, 7, 48, 333, 1024 "
                  "and on a re-run",
                  double(wob), wob < 0.5f ? "left" : "right");
    check(same, msg);
    }
}

// ---- 5. CPU (INFO) ---------------------------------------------------------------------------
// SPEC §5 worst case (3 Springs, KICKED, TONE/DRIVE max, TENSION loosest) with every M7
// part busy: a hard noise hit and a Kick 12 times a second each (Clatter,
// Jolt, rattle and Kick voices never idle), SPLASH 1, WOBBLE at the costlier
// end stop; against the same with SPLASH 0 / WOBBLE noon, no hits, no Kicks
// (steady noise). Daisy
// estimate as test_tank: 15-25x this desktop per sample, at 480 MHz.
void performance()
{
    const size_t n = size_t(10.0f * kFs);
    rv::dsp::Rng rng;
    rng.seed(77u);
    Buf hits(n, 0.0f), steady(n);
    for (size_t i = 0; i < n; ++i) {
        const size_t k = i % 4000;
        if (k < 480) hits[i] = 0.5f * std::exp(-float(k) / 96.0f) * rng.bipolar();
        steady[i] = 0.3f * rng.bipolar();
    }
    auto bench = [&](const Buf& in, bool m7, float wob) {
        rv::Tank t;
        t.prepare(kFs, 48);
        t.setParam(rv::ParamId::Mix, 0.5f);
        t.setParam(rv::ParamId::Decay, 0.85f);
        t.setParam(rv::ParamId::Tension, 0.0f); // loosest tank: most stages
        t.setParam(rv::ParamId::Tone, 1.0f);
        t.setParam(rv::ParamId::Drive, 1.0f);
        t.setParam(rv::ParamId::Attitude, 1.0f);
        t.setParam(rv::ParamId::Springs, 1.0f);
        t.setParam(rv::ParamId::Splash, m7 ? 1.0f : 0.0f);
        t.setParam(rv::ParamId::Wobble, m7 ? wob : 0.5f);
        Buf l(n), r(n);
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos < n; pos += 48) {
            const size_t toGate = (4000 - (pos + 2000) % 4000) % 4000; // gates at 2000 + 4000 k
            if (m7 && toGate < 48) t.kick(int(toGate));
            t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
        }
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n);
    };
    double busyL = 1e30, busyR = 1e30, base = 1e30;
    for (int rep = 0; rep < 3; ++rep) { // best of 3: least disturbed by the OS
        busyL = std::min(busyL, bench(hits, true, 0.0f));
        busyR = std::min(busyR, bench(hits, true, 1.0f));
        base  = std::min(base, bench(steady, false, 0.5f));
    }
    std::printf("INFO  WOBBLE sides, M7 busy: fully left (wow + flutter) %.1f ns/sample, fully right (LFO) %.1f ns/sample\n",
                busyL, busyR);
    const double busy = std::max(busyL, busyR);
    std::printf("INFO  CPU worst case, 3 Springs KICKED TONE/DRIVE 1 TENSION 0: M7 busy (SPLASH 1, WOBBLE end stop, hits + Kicks "
                "12/s) %.1f ns/sample, est. Daisy %.0f-%.0f cycles/sample (%.0f-%.0f%% of 10k); M7 quiet (SPLASH 0, "
                "WOBBLE noon, steady noise) %.1f ns/sample (%.0f-%.0f%%); M7 share %.0f-%.0f cycles/sample\n",
                busy, busy * 15 * 0.48, busy * 25 * 0.48, busy * 15 * 0.48 / 100, busy * 25 * 0.48 / 100, base,
                base * 15 * 0.48 / 100, base * 25 * 0.48 / 100, (busy - base) * 15 * 0.48, (busy - base) * 25 * 0.48);
}

} // namespace

int main()
{
    splashOnHits();
    splashAudible();
    ghostGroove();
    wobbleOnHeldTones();
    morph();
    determinism();
    performance();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
