// M7 criteria through the whole Tank (SPEC §7 M7, docs/m7-integration.md):
// SPLASH on 02_hits, WOBBLE on 08_held_tones, the ATTITUDE Morph of the M7
// tables, and determinism with everything M7 turned up. The Kick criteria
// are in test_kick, MIX in test_mix, the components alone in test_splash,
// test_kick_voice and test_wobble.
//
// "Splash share": the Tank has test hooks (setSplashParts) that keep the
// Splash listening but drop its sound (the Clang, the Bite and the Kick's
// Clatter) and/or its Jolt. The Splash's contribution is then the
// difference of renders that are otherwise identical (same seeds, same
// everything):
//   splash energy  = energy of (on − off) in the 6 s after a hit (Clang/Bite + Jolt)
//   splash dB      = 1–6 kHz energy of (sound only − off) re the off render's
//                    own 1–6 kHz energy, first 150 ms after the hit: how loud the
//                    added splash is against the hit's own bright part
//   crash dB       = 1–6 kHz energy on vs off, first 150 ms (total brightening)
//   settled dB     = the same 1.0–1.5 s after the hit
// M8 adds SPLASH audibility on a rimshot at -18..-3 dBFS (splashAudible)
// and ghost notes judged between louder hits (ghostGroove), for the
// level-adaptive hit detector (SplashVoicing.h). ADR 0032 (SPLASH comes
// from the hit): no noise burst on hits, the Clang (every ATTITUDE) and the
// Bite (DRIVEN / KICKED, short hits) instead, at the owner's round-4 picks;
// ADR 0033 (DRIVE is the INPUT): DRIVE never reduces the splash
// (splashVsDrive) and a quiet mixer send splashes once DRIVE is up
// (splashAtSendLevel).
// Stimuli are read from test_audio/stimulus (tools/make_stimulus.py); a
// missing file skips its section.

#include "Wav.h"
#include "dsp/Filters.h"
#include "dsp/Tank.h"
#include "params/DriveVoicing.h"
#include "params/SplashVoicing.h"

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
    bool  clatterOn = true, joltOn = true; // Tank::setSplashParts (clatterOn: the Splash's sound, Clang + Bite + Clatter)
    bool  sustainOn = true;                // Tank::setSustainTrimEnabled
    int   voicing = 0;                     // Tank::setSplashVoicing (SplashVoicing.h "SPLASH stronger")
};

struct Out {
    Buf l, r;
    Buf jolt; // Splash Jolt envelope after each block
    Buf clat; // Splash Clatter burst envelope after each block
    Buf env;  // Splash hit envelope e (ADR 0032) after each block
    float limitMinGain = 1.0f; // the output limiter's deepest pull (linear gain)
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
    t.setSustainTrimEnabled(s.sustainOn);
    t.setSplashVoicing(s.voicing);
    Out o{Buf(in.size()), Buf(in.size()), Buf(in.size()), Buf(in.size()), Buf(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += size_t(block)) {
        const int n = int(std::min(size_t(block), in.size() - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        for (int i = 0; i < n; ++i) {
            o.jolt[pos + size_t(i)] = t.splash().joltEnvelope();
            o.clat[pos + size_t(i)] = s.clatterOn ? t.splash().clatterEnvelope() : 0.0f;
            o.env[pos + size_t(i)]  = t.splash().hitEnvelope();
        }
        o.limitMinGain = std::min(o.limitMinGain, t.limiterGain());
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
    double splashE[6], splashDb[6], crashDb[6], settledDb[6], tailDb[6];
    // Hard snare: how far the splash (sound only - off, 1-6 kHz) and the
    // hit's own tail (off, 1-6 kHz) fall from the first 150 ms to 1.0-1.5 s
    // after the hit, power per second, dB.
    double splashFall = 0, tailFall = 0;
    float  joltPeak = 0, joltAt1s = 0; // hard snare
};

HitStats hitStats(const Buf& x, int att, float splash)
{
    Settings s;
    s.att    = att;
    s.splash = splash;
    const Out on = render(s, x);
    s.joltOn = false;
    const Out snd = render(s, x);
    s.clatterOn = false;
    const Out off = render(s, x);
    const Buf mOn = mono(on), mOff = mono(off), mSnd = mono(snd);
    Buf d(mOn.size()), dc(mOn.size());
    for (size_t i = 0; i < d.size(); ++i) {
        d[i]  = mOn[i] - mOff[i];
        dc[i] = mSnd[i] - mOff[i];
    }
    const Buf hOn = band(mOn, 1000.0f, 6000.0f), hOff = band(mOff, 1000.0f, 6000.0f), hC = band(dc, 1000.0f, 6000.0f);
    HitStats h{};
    for (int k = 0; k < 6; ++k) {
        const size_t at = size_t((1.0f + 6.0f * float(k)) * kFs);
        h.splashE[k]   = energy(d, at, at + size_t(6.0f * kFs));
        h.splashDb[k]  = db(energy(hC, at, at + size_t(0.15f * kFs)) / energy(hOff, at, at + size_t(0.15f * kFs)));
        h.crashDb[k]   = db(energy(hOn, at, at + size_t(0.15f * kFs)) / energy(hOff, at, at + size_t(0.15f * kFs)));
        h.settledDb[k] = db(energy(hOn, at + size_t(kFs), at + size_t(1.5f * kFs))
                            / energy(hOff, at + size_t(kFs), at + size_t(1.5f * kFs)));
        h.tailDb[k] = db(energy(mOn, at + size_t(kFs), at + size_t(1.5f * kFs))
                         / energy(mOff, at + size_t(kFs), at + size_t(1.5f * kFs)));
    }
    const size_t at = size_t(kFs), e1 = at + size_t(0.15f * kFs), l0 = at + size_t(kFs), l1 = at + size_t(1.5f * kFs);
    h.splashFall = db((energy(hC, at, e1) / 0.15) / (energy(hC, l0, l1) / 0.5));
    h.tailFall   = db((energy(hOff, at, e1) / 0.15) / (energy(hOff, l0, l1) / 0.5));
    for (size_t i = at; i < at + size_t(0.5f * kFs); ++i) h.joltPeak = std::max(h.joltPeak, on.jolt[i]);
    h.joltAt1s = on.jolt[at + size_t(kFs)];
    return h;
}

void splashOnHits()
{
    Buf x;
    if (!load("02_hits.wav", x)) return;
    std::printf("      02_hits through the Tank (DECAY 0.5, DRIVE 0.5, 2 Springs). Splash energy of the -12 / -18 dBFS hit\n"
                "      re the -6 dBFS one (snare; rim); splash (Clang + Bite) re the hit's own 1-6 kHz and crash = 1-6 kHz\n"
                "      on/off, first 150 ms (snare -6/-12/-18); settled = 1.0-1.5 s after the -6 dBFS snare:\n");
    bool ghostOk = true;
    HitStats h1[3]{}, h0[3]{};
    for (int att : {0, 1, 2})
        for (float sv : {0.0f, 0.5f, 1.0f}) {
            const HitStats h = hitStats(x, att, sv);
            std::printf("      %-6s SPLASH %.1f: snare %6.1f / %6.1f dB, rim %6.1f / %6.1f dB; splash %6.1f / %6.1f / %6.1f dB, "
                        "crash %+4.1f / %+4.1f / %+4.1f dB; settled %+4.1f dB (broadband %+4.1f); Jolt peak %.3f, at +1 s %.5f\n",
                        kAttName[att], sv, db(h.splashE[1] / h.splashE[0]), db(h.splashE[2] / h.splashE[0]),
                        db(h.splashE[4] / h.splashE[3]), db(h.splashE[5] / h.splashE[3]), h.splashDb[0], h.splashDb[1],
                        h.splashDb[2], h.crashDb[0], h.crashDb[1], h.crashDb[2], h.settledDb[0], h.tailDb[0], h.joltPeak, h.joltAt1s);
            if (att > 0 && sv > 0.0f)
                ghostOk &= h.splashE[0] > h.splashE[1] && h.splashE[1] > h.splashE[2] && h.splashE[2] < 0.25 * h.splashE[0]
                        && h.splashE[3] > h.splashE[4] && h.splashE[4] > h.splashE[5] && h.splashE[5] < 0.25 * h.splashE[3];
            if (sv == 1.0f) h1[att] = h;
            if (sv == 0.0f) h0[att] = h;
        }
    check(ghostOk, "Splash through the Tank falls with hit level; the -18 dBFS hit gives < 25 % of the -6 dBFS hit's "
                   "Splash energy (DRIVEN, KICKED; SPLASH 0.5 / 1; snare and rim)");

    // ADR 0032: the splash is the hit's own sound: its highs fed harder into
    // the springs (the Clang, every ATTITUDE) and, in DRIVEN / KICKED, a
    // short hit pushed harder into the transducer (the Bite), at the owner's
    // round-4 "clear" strength (C2 / T2). What must hold, SPLASH 1, the -6
    // dBFS snare, every ATTITUDE:
    //  - clearly there: the added splash >= -6 dB re the hit's own 1-6 kHz
    //    (the old "big bright crash" bar) and the crash (total 1-6 kHz
    //    brightening) >= +3 dB ("clearly audible", below);
    //  - the Jolt settles (< 2 % of its peak after 1 s).
    // CLEAN (the Clang alone: added brightness, nothing else) as the noise
    // burst had to: it dies with the tail, never slower (from the first
    // 150 ms to 1.0-1.5 s the splash falls at least as far as the hit's own
    // tail, within 1 dB), and the tail is still the hit's (settled <= +6 dB,
    // never above the crash). DRIVEN / KICKED: the Bite hits the tank harder
    // on purpose, so the whole tail after the hit is louder (a harder hit on
    // a real tank), and it squashes the hit's start more than its tail, so
    // "falls as far" and "settled <= the crash" no longer describe it. What
    // holds instead: the tail (broadband, 1.0-1.5 s) is louder by no more
    // than the Bite's largest push at full SPLASH at this DRIVE (the Tank
    // takes half of it back; DriveIn's automatic makeup can give back what
    // its saturators squashed, never more than the push) plus the Clang's own
    // (CLEAN's, measured here).
    // The ATTITUDEs still step up in the Jolt (ADR 0025: CLEAN's lurch at
    // most a third of DRIVEN's; KICKED's the largest), not in the splash
    // itself (the owner picked the same "clear" strength for all three).
    {
        const HitStats& h = h1[0];
        std::snprintf(msg, sizeof msg,
                      "CLEAN SPLASH 1, -6 dBFS snare: splash %+.1f dB re the hit's own 1-6 kHz (>= -6), crash %+.1f dB (>= +3); "
                      "dies with the tail (splash falls %.1f dB from the first 150 ms to 1-1.5 s, tail %.1f dB: no slower "
                      "than the tail - 1 dB); settled %+.1f dB (<= +6, <= the crash)",
                      h.splashDb[0], h.crashDb[0], h.splashFall, h.tailFall, h.settledDb[0]);
        check(h.splashDb[0] >= -6.0 && h.crashDb[0] >= 3.0 && h.splashFall >= h.tailFall - 1.0 && h.settledDb[0] <= 6.0
                  && h.settledDb[0] <= h.crashDb[0],
              msg);
    }
    const float dcTest = rv::drive::driveCurve(Settings{}.drive);
    const double biteHeard = 20.0 * std::log10(1.0 + rv::splash::kBiteGain
                                                          * rv::splash::splashDriveGain(dcTest, rv::drive::driveCurve(0.5f),
                                                                                        rv::drive::driveCurve(rv::splash::kSplashRefDrive)));
    for (int att : {1, 2}) {
        const HitStats& h = h1[att];
        std::snprintf(msg, sizeof msg,
                      "%s SPLASH 1, -6 dBFS snare: splash %+.1f dB re the hit's own 1-6 kHz (>= -6), crash %+.1f dB (>= +3); "
                      "the tail 1-1.5 s after it %+.1f dB louder (<= the Bite's full push %.1f + the Clang's %.1f); Jolt %.2f %% "
                      "of its peak at 1 s (< 2 %%)",
                      kAttName[att], h.splashDb[0], h.crashDb[0], h.tailDb[0], biteHeard, h1[0].tailDb[0],
                      100.0 * h.joltAt1s / std::max(1e-9f, h.joltPeak));
        check(h.splashDb[0] >= -6.0 && h.crashDb[0] >= 3.0 && h.tailDb[0] <= biteHeard + h1[0].tailDb[0]
                  && h.joltAt1s < 0.02f * h.joltPeak,
              msg);
    }
    // SPLASH 0: no Clang, no Bite, no Clatter in any ATTITUDE (no click on
    // hard hits, M8 round 2). DRIVEN / KICKED keep the small Jolt floor (a
    // pitch lurch, no transient); CLEAN has nothing at all (hi-fi unless asked).
    std::snprintf(msg, sizeof msg,
                  "SPLASH 0: no splash sound on hard hits (splash %+.0f / %+.0f / %+.0f dB re the hit, CLEAN / DRIVEN / "
                  "KICKED); the Jolt floor stays in DRIVEN / KICKED, CLEAN adds nothing",
                  h0[0].splashDb[0], h0[1].splashDb[0], h0[2].splashDb[0]);
    check(h0[0].splashDb[0] < -200.0 && h0[1].splashDb[0] < -200.0 && h0[2].splashDb[0] < -200.0 && h0[0].splashE[0] == 0.0
              && h0[1].splashE[0] > 0.0 && h0[2].splashE[0] > 0.0,
          msg);
    // The Jolt steps up with ATTITUDE (ADR 0025): lurch = Jolt envelope peak
    // x the Loop delay offset at j = 1 (% of L).
    double lurch[3];
    for (int att : {0, 1, 2}) lurch[att] = 100.0 * h1[att].joltPeak * rv::splash::kVoice[size_t(att)].joltLoopFrac;
    std::snprintf(msg, sizeof msg, "Jolt lurch at SPLASH 1: CLEAN %.3f %% of L (<= a third of DRIVEN's %.3f %%), KICKED %.3f %% (the largest)",
                  lurch[0], lurch[1], lurch[2]);
    check(lurch[0] > 0.0 && lurch[0] <= lurch[1] / 3.0 && lurch[2] > lurch[1], msg);
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
// audible (loudness JND ~0.5-1 dB). Since ADR 0032 the ATTITUDEs share the
// owner's "clear" Clang, and DRIVEN / KICKED add the Bite on short hits:
// DRIVEN and KICKED >= +3 dB, CLEAN (the Clang alone) >= +1.5 (ADR 0025's
// "gentle" bar; was CLEAN +1.5 below DRIVEN's, DRIVEN +3, KICKED +6 with the
// noise burst). At the default DRIVE (0.25: +4 dB of INPUT) for the -9 /
// -3 dBFS rimshots and the snare. The -18 dBFS rimshot is a quiet send
// (ADR 0033): judged at DRIVE 0.6 (+12.5 dB of INPUT), where it hits the
// springs like a -6 dBFS one at DRIVE 0. SPLASH 0.5 already adds from
// -9 dBFS.
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

double crashDb(const Buf& x, int att, float splash, float at, float drive = rv::spec(rv::ParamId::Drive).defaultValue)
{
    Settings s;
    s.att    = att;
    s.drive  = drive;
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
    for (int att : {0, 1, 2}) {
        const double want = att == 0 ? 1.5 : 3.0;
        std::printf("      %-6s crash SPLASH 1 (0.5) vs 0, 1-6 kHz first 150 ms:", kAttName[att]);
        for (float lvl : {-18.0f, -9.0f, -3.0f}) {
            const Buf x = rimshot(lvl, size_t(2.5f * kFs), 0.5f);
            const float drive = lvl < -12.0f ? 0.6f : rv::spec(rv::ParamId::Drive).defaultValue;
            const double c1 = crashDb(x, att, 1.0f, 0.5f, drive), c5 = crashDb(x, att, 0.5f, 0.5f, drive);
            std::printf("  rim %3.0f dBFS%s %+5.1f (%+4.1f)", lvl, lvl < -12.0f ? " (DRIVE .6)" : "", c1, c5);
            ok &= c1 >= want && c5 > (att == 0 ? 0.25 : 0.5);
        }
        if (haveHits) {
            const double c1 = crashDb(hits, att, 1.0f, 1.0f);
            std::printf("  02_hits snare -6 %+5.1f", c1);
            ok &= c1 >= want;
        }
        std::printf("\n");
    }
    check(ok, "SPLASH 1 audible on a rimshot at -9 / -3 dBFS and the -6 dBFS snare (default DRIVE) and at -18 dBFS (DRIVE "
              "0.6): crash >= +3 dB (DRIVEN, KICKED), >= +1.5 (CLEAN); SPLASH 0.5 already adds");
}

// ---- 1c. Ghost notes between louder hits barely trigger (M8, ADR 0032) -------------------
// A groove: -6 dBFS snare backbeats every 1 s, -18 dBFS rimshot ghost notes
// half way between. The hit envelope e (SplashVoicing.h) judges each hit's
// loudness against the program level in a groove (kLoudRel), so the ghosts
// barely trigger while the backbeats splash, at any DRIVE (an INPUT that
// lifts the whole groove lifts the program level with it). Judged at the
// source, the Splash's hit envelope (the Clang and the Bite are e times a
// fixed amount): a ghost's e peaks under a quarter of a backbeat's, and its
// e energy is >= 12 dB under a backbeat's. (The splash itself rings in the
// tank, so a backbeat's is still sounding under the ghost and a render
// difference cannot isolate the ghost's.) Every ATTITUDE, SPLASH 1, at the
// default DRIVE and at DRIVE 1; the DRIVE-free SPLASH voicings 2 and 3
// ("SPLASH stronger", which judge levels as DRIVE 0.8 does) at DRIVE 0 too.
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
    for (int k = 0; k < 4; ++k) {
        const size_t b = size_t((0.5f + float(k)) * kFs), g = size_t((1.0f + float(k)) * kFs);
        for (size_t i = 0; i < sn.size(); ++i) x[b + i] += 0.5012f / pk * sn[i];
        for (size_t i = 0; i < ghost.size(); ++i) x[g + i] += ghost[i];
    }
    for (int voicing : {0, 2, 3})
    for (float drive : {0.0f, rv::spec(rv::ParamId::Drive).defaultValue, 1.0f})
        for (int att : {0, 1, 2}) {
            if (voicing == 0 && drive == 0.0f) continue;
            Settings s;
            s.att    = att;
            s.splash = 1.0f;
            s.drive  = drive;
            s.voicing = voicing;
            const Out on = render(s, x, 8);
            double worstShare = -300, worstPeak = 0;
            const size_t w = size_t(0.15f * kFs);
            for (int k = 1; k < 4; ++k) { // from the second bar: the program level has settled
                const size_t b = size_t((0.5f + float(k)) * kFs), g = size_t((1.0f + float(k)) * kFs);
                double eb = 0, eg = 0;
                float pb = 0, pg = 0;
                for (size_t i = 0; i < w; ++i) {
                    eb += double(on.env[b + i]) * on.env[b + i];
                    eg += double(on.env[g + i]) * on.env[g + i];
                    pb = std::max(pb, on.env[b + i]);
                    pg = std::max(pg, on.env[g + i]);
                }
                worstShare = std::max(worstShare, db(eg / eb));
                worstPeak  = std::max(worstPeak, double(pg) / double(pb));
            }
            std::snprintf(msg, sizeof msg,
                          "voicing %d %s SPLASH 1 groove, DRIVE %.2f: ghost notes (-18 dBFS between -6 dBFS backbeats) barely trigger: "
                          "hit envelope energy %.1f dB re a backbeat's (<= -12), peak %.2f of a backbeat's (< 0.25)",
                          voicing, kAttName[att], drive, worstShare, worstPeak);
            check(worstShare <= -12.0 && worstPeak < 0.25, msg);
        }
}

// ---- 1d. DRIVE never reduces the splash (ADR 0033) ------------------------------------------
// Owner (30 Sep 2026, Ableton, KICKED, SPLASH 0.83): "as I increase drive, it
// seems to dampen the splash". `main` before ADR 0032 / 0033: KICKED skank
// +13.2 dB at DRIVE 0 -> +5.6 at DRIVE 1. Now the Splash hears the input after
// the INPUT gain, and above noon the Clang / Bite grow with DRIVE to stay on
// top of DRIVE's own squash (SplashVoicing.h "DRIVE's top half"). Splash =
// 2-8 kHz energy 20-400 ms after the hit, SPLASH 0.7 vs SPLASH 0 (the
// round-4 measure), on 02_hits' -6 dBFS rimshot (isolated) and the first 8 s
// of 04_skank (DAW level, as the owner's session), 2 Springs, DECAY 0.6,
// TENSION / TONE noon, WOBBLE 0.45 (default), MIX 1, DRIVE 0 / 0.25 / 0.5 / 0.75 / 1.
// Gate: no step lower than the step before by more than 0.5 dB, and DRIVE 1
// not under DRIVE 0, every ATTITUDE.
double splash28(const Buf& x, int att, float drive, float splash, const std::vector<double>& at, int voicing = 0)
{
    Settings s;
    s.voicing = voicing;
    s.att = att;
    s.drive = drive;
    s.decay = 0.6f;
    s.wobble = 0.45f; // the default (ADR 0034)
    s.splash = 0.0f;
    const Buf h0 = band(mono(render(s, x)), 2000.0f, 8000.0f);
    s.splash = splash;
    const Buf h1 = band(mono(render(s, x)), 2000.0f, 8000.0f);
    double r = 0;
    for (double t : at) {
        const size_t a = size_t((t + 0.02) * kFs), b = size_t((t + 0.4) * kFs);
        r += energy(h1, a, b) / energy(h0, a, b);
    }
    return db(r / double(at.size()));
}

void splashVsDrive()
{
    Buf hits, skank;
    if (!load("02_hits.wav", hits) || !load("04_skank.wav", skank)) return;
    const Buf rim(hits.begin() + long(18.0f * kFs), hits.begin() + long(22.0f * kFs)); // the -6 dBFS rim at 1.0 s
    skank.resize(size_t(8.0f * kFs));
    std::vector<double> chords;
    for (int k = 0; k < 8; ++k) chords.push_back(1.4 + 0.8 * k);
    const float drives[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (int att : {0, 1, 2})
        for (int which = 0; which < 2; ++which) {
            double v[5];
            bool ok = true;
            for (int d = 0; d < 5; ++d) {
                v[d] = which == 0 ? splash28(rim, att, drives[d], 0.7f, {1.0}) : splash28(skank, att, drives[d], 0.7f, chords);
                if (d > 0) ok &= v[d] >= v[d - 1] - 0.5;
            }
            ok &= v[4] >= v[0];
            std::snprintf(msg, sizeof msg,
                          "DRIVE never reduces the splash, %s %s (SPLASH 0.7): %+.1f / %+.1f / %+.1f / %+.1f / %+.1f dB at "
                          "DRIVE 0 / .25 / .5 / .75 / 1 (each >= the last - 0.5; DRIVE 1 >= DRIVE 0)",
                          kAttName[att], which == 0 ? "-6 dBFS rim" : "04_skank", v[0], v[1], v[2], v[3], v[4]);
            check(ok, msg);
        }
}

// ---- 1e. A quiet mixer send splashes once DRIVE is up (ADR 0033) -----------------------------
// The owner's hardware: drums from a mixer's FX send, peaking below -18 dBFS,
// where SPLASH was barely there (send-level study: rim +0.3 .. +1.6 dB at
// -24 dBFS, whatever DRIVE). DRIVE is the INPUT now: the same rimshot at
// -24 dBFS peak with DRIVE 1 (+24 dB) must splash at least as much as the
// -6 dBFS DAW-level rimshot at DRIVE 0, and clearly (>= +3 dB). Same measure
// and settings as 1d.
void splashAtSendLevel()
{
    Buf hits;
    if (!load("02_hits.wav", hits)) return;
    Buf rim(hits.begin() + long(18.0f * kFs), hits.begin() + long(22.0f * kFs));
    float pk = 0.0f;
    for (float v : rim) pk = std::max(pk, std::fabs(v));
    Buf send = rim;
    for (auto& v : send) v *= std::pow(10.0f, -24.0f / 20.0f) / pk;
    for (int att : {0, 1, 2}) {
        const double daw = splash28(rim, att, 0.0f, 0.7f, {1.0}), s1 = splash28(send, att, 1.0f, 0.7f, {1.0});
        const double s0 = splash28(send, att, 0.0f, 0.7f, {1.0});
        std::snprintf(msg, sizeof msg,
                      "%s: a -24 dBFS send splashes at DRIVE 1: %+.1f dB (DRIVE 0: %+.1f; the -6 dBFS rim at DRIVE 0: %+.1f; "
                      ">= that and >= +3)",
                      kAttName[att], s1, s0, daw);
        check(s1 >= daw && s1 >= 3.0, msg);
    }
}

// ---- 1f. SPLASH stronger: the voicings (ADR 0032 amendment, Proposed) ----------------------
// Owner, 1 Oct 2026: "the splash knob feels very subtle"; wanted stronger at
// the top and less tied to DRIVE (SplashVoicing.h "SPLASH stronger"). On the
// -6 dBFS rim (as 1d), SPLASH 1, CLEAN and KICKED:
//  - voicing 1 renders bit for bit as today at SPLASH 0.75;
//  - every voicing's top (SPLASH 1, DRIVE 0.8) splashes at least as much as
//    today's, KICKED's >= +2 dB more (CLEAN's extra is mostly held back by
//    the Clang's ceiling on this lone loud rim: the limiter's headroom);
//  - voicings 2 and 3 splash at DRIVE 0 within 3 dB of DRIVE 0.8 at SPLASH
//    0.75, and at DRIVE 0 at least as much as today's DRIVE 0;
//  - the output limiter's deepest pull (MIX 1) stays <= 9 dB (printed with
//    today's; the Clang's ceiling, SplashVoicing.h).
// The ghost-note guard for 2 and 3 is in ghostGroove.
double splashLim(const Buf& x, int att, float drive, float splash, int voicing, double& limDb)
{
    Settings s;
    s.att = att;
    s.drive = drive;
    s.decay = 0.6f;
    s.wobble = 0.45f;
    s.voicing = voicing;
    s.splash = 0.0f;
    const Buf h0 = band(mono(render(s, x)), 2000.0f, 8000.0f);
    s.splash = splash;
    const Out on = render(s, x);
    limDb = -20.0 * std::log10(double(on.limitMinGain));
    const Buf h1 = band(mono(on), 2000.0f, 8000.0f);
    const size_t a = size_t(1.02 * kFs), b = size_t(1.4 * kFs);
    return db(energy(h1, a, b) / energy(h0, a, b));
}

void splashStronger()
{
    Buf hits;
    if (!load("02_hits.wav", hits)) return;
    const Buf rim(hits.begin() + long(18.0f * kFs), hits.begin() + long(22.0f * kFs)); // the -6 dBFS rim at 1.0 s
    for (int att : {0, 2}) {
        // Voicing 1 below the top: bit for bit today.
        {
            Settings s;
            s.att = att;
            s.drive = 0.8f;
            s.splash = 0.75f;
            const Out a = render(s, rim);
            s.voicing = 1;
            const Out b = render(s, rim);
            std::snprintf(msg, sizeof msg, "%s voicing 1 at SPLASH 0.75, DRIVE 0.8: bit for bit today's", kAttName[att]);
            check(a.l == b.l && a.r == b.r, msg);
        }
        double lim0[2], top0[2], mid0, unused0;
        for (int d = 0; d < 2; ++d) top0[d] = splashLim(rim, att, d == 0 ? 0.0f : 0.8f, 1.0f, 0, lim0[d]);
        mid0 = splashLim(rim, att, 0.0f, 0.75f, 0, unused0);
        for (int v : {1, 2, 3}) {
            double lim[2], top[2], mid[2], unused;
            for (int d = 0; d < 2; ++d) {
                top[d] = splashLim(rim, att, d == 0 ? 0.0f : 0.8f, 1.0f, v, lim[d]);
                mid[d] = splashLim(rim, att, d == 0 ? 0.0f : 0.8f, 0.75f, v, unused);
            }
            std::snprintf(msg, sizeof msg,
                          "%s voicing %d, -6 dBFS rim: splash at SPLASH 0.75 / 1, DRIVE 0 %+.1f / %+.1f, DRIVE 0.8 %+.1f / %+.1f dB "
                          "(today's: DRIVE 0 %+.1f / %+.1f, DRIVE 0.8 SPLASH 1 %+.1f); limiter <= 9 dB: %.1f / %.1f (today %.1f / %.1f)",
                          kAttName[att], v, mid[0], top[0], mid[1], top[1], mid0, top0[0], top0[1], lim[0], lim[1], lim0[0], lim0[1]);
            bool ok = lim[0] <= 9.0 && lim[1] <= 9.0 && top[1] >= top0[1] + (att == 2 ? 2.0 : 0.0);
            if (v >= 2) ok &= std::fabs(mid[0] - mid[1]) <= 3.0 && mid[0] >= mid0 && top[0] >= top0[0];
            check(ok, msg);
        }
    }
}

// ---- 2. WOBBLE on 08_held_tones ---------------------------------------------------------
// Bipolar WOBBLE (ADR 0034, round 2 voicing B): noon still, left = random wow
// + flutter (+ its tremolo), right = a sine vibrato. Walked in 0.1 knob steps,
// as the sweet-spot check does.
// 1 kHz at -12 dBFS from 1 s to 9 s. DRIVEN at the default DRIVE, SPRINGS 1
// (one Spring: a clean pitch to track), SPLASH 0, MIX 1. The wet is the
// Tank's tail building on the held tone (as test_wobble's "tail": the Loop
// multiplies the per-pass shift). Pitch = 10-cycle-averaged zero-crossing
// frequency of the 700-1400 Hz band, 3-8.9 s, in cents re its median; p95
// (peak) of |cents|. (After the tone stops the tail is several Loop modes
// near 1 kHz beating, so a single pitch is not defined there.)
// Measured with the Sustain trim off (M8, ADR 0035): this reads WOBBLE, and
// the reading depends on the tank's level. The trim eases this held tone
// 1-3 dB down at DECAY 1, and a quieter tank reads more cents here with or
// without it: main (b3e5ac3) reads 4.2 cents at DECAY 1, WOBBLE 0.5 on this
// tone and 5.5 on the same tone 6 dB quieter (the LoopSat's quiet-tail fade,
// AntiRes.h, lets the Loop ring more freely). With the trim on it reads
// 5.5-6 (printed as INFO below).
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
    // "max" is 0.9: above it DRIVEN is the Hold (ADR 0040), which freezes
    // the tank (nothing new gets in; test_throw_hold).
    const float ds[] = {0.0f, 0.5f, 0.9f};
    double c[3][kN], dflt[3];
    auto measure = [&](float w, float d) {
        Settings s;
        s.drive   = rv::spec(rv::ParamId::Drive).defaultValue;
        s.decay   = d;
        s.springs = 0;
        s.splash  = 0.0f;
        s.wobble  = w;
        s.sustainOn = false; // WOBBLE's depth, not the trim's level
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
        // allow 20 % (it must still never clearly fall). Was 10 %: voicing D
        // (owner's pick, 1 Oct 2026) reads 21.7 at 0.1 vs 25.6 at 0.2 there
        // (B: 24 vs 26), the random path landing on the Loop modes' beating;
        // DECAY 0 and noon rise at every step.
        const double tol = di == 2 ? 0.8 : 1.0;
        for (int i = 4; i > 0; --i) rising &= c[di][i - 1] > tol * c[di][i];
        for (int i = 6; i < kN - 1; ++i) rising &= c[di][i + 1] > tol * c[di][i];
        // Round 2 toned the end stops down on purpose (owner, 1 Oct 2026):
        // round 1 asked >= 25 here; B's vibrato top reads ~22-25.
        ends &= std::min(c[di][0], c[di][10]) >= (di == 0 ? 10.0 : 15.0);
        even &= c[di][0] >= 0.6 * c[di][10] && c[di][0] <= 1.6 * c[di][10];
    }
    const bool first = c[1][4] >= 1.5 && c[1][6] >= 1.5;
    std::snprintf(msg, sizeof msg, "WOBBLE noon = Micro-mod floor only: p95 %.1f / %.1f / %.1f cents at DECAY 0 / 0.5 / 0.9 (< 3)",
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
                  "(>= 10 at DECAY 0, >= 15 at noon and max: toned down in round 2; old WOBBLE 1: 39.0 / 48.6 / 55.3)",
                  c[0][0], c[1][0], c[2][0], c[0][10], c[1][10], c[2][10]);
    check(ends, msg);
    check(even, "fully left roughly as wild as fully right (0.6..1.6x at every DECAY)");
}

// ---- 3. ATTITUDE Morph blends the Splash and Kick tables ---------------------------------
// A DRIVEN -> KICKED flip: the Splash voicing (the Kick's crash, clatterMax)
// must glide over the Morph (drive::kMorphSeconds), never step.
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
    splashVsDrive();
    splashAtSendLevel();
    splashStronger();
    wobbleOnHeldTones();
    morph();
    determinism();
    performance();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
