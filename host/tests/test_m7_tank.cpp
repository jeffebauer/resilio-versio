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
    float decay = 0.5f, drive = 0.5f, splash = 0.3f, wobble = 0.0f, tone = 0.5f, tension = 0.5f;
    int   att = 1, springs = 1;
    bool  clatterOn = true, joltOn = true; // Tank::setSplashParts
};

struct Out {
    Buf l, r;
    Buf jolt; // Splash Jolt envelope after each block
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
    Out o{Buf(in.size()), Buf(in.size()), Buf(in.size())};
    for (size_t pos = 0; pos < in.size(); pos += size_t(block)) {
        const int n = int(std::min(size_t(block), in.size() - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        for (int i = 0; i < n; ++i) o.jolt[pos + size_t(i)] = t.splash().joltEnvelope();
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
    const size_t at = size_t(kFs);
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
    std::snprintf(msg, sizeof msg,
                  "KICKED SPLASH 1, -6 dBFS snare: big bright crash (Clatter %+.1f dB re the hit's own 1-6 kHz, >= -6; crash "
                  "%+.1f dB); settled into the tail after 1 s (%+.1f dB, within 1.5 dB; Jolt %.2f %% of its peak, < 2 %%)",
                  k1.clatterDb[0], k1.crashDb[0], k1.settledDb[0], 100.0 * k1.joltAt1s / std::max(1e-9f, k1.joltPeak));
    check(k1.clatterDb[0] >= -6.0 && std::fabs(k1.settledDb[0]) <= 1.5 && k1.joltAt1s < 0.02f * k1.joltPeak, msg);
    std::snprintf(msg, sizeof msg, "DRIVEN SPLASH 1 moderate: Clatter %+.1f dB re the hit (between KICKED's %+.1f and -20)",
                  d1.clatterDb[0], k1.clatterDb[0]);
    check(d1.clatterDb[0] < k1.clatterDb[0] && d1.clatterDb[0] > -20.0, msg);

    std::snprintf(msg, sizeof msg,
                  "DRIVEN SPLASH 0: faint natural splash on the hard hit, not zero (Clatter %+.1f dB re the hit, %.1f dB "
                  "below SPLASH 1; plan: ~27 dB below)",
                  d0.clatterDb[0], d1.clatterDb[0] - d0.clatterDb[0]);
    check(d0.clatterDb[0] > -60.0 && d0.clatterDb[0] < d1.clatterDb[0] - 15.0 && d0.splashE[0] > 0.0, msg);

    // CLEAN (ADR 0025): a real but gentle splash. Nothing at SPLASH 0; at
    // SPLASH 1 a light crash under DRIVEN's that settles into the tail, and
    // a Jolt peak at most a third of DRIVEN's.
    c1 = hitStats(x, 0, 1.0f);
    const HitStats c0 = hitStats(x, 0, 0.0f);
    // The lurch: Jolt envelope peak x the Loop delay offset at j = 1 (% of L).
    const double cLurch = 100.0 * c1.joltPeak * rv::splash::kVoice[0].joltLoopFrac;
    const double dLurch = 100.0 * d1.joltPeak * rv::splash::kVoice[1].joltLoopFrac;
    std::snprintf(msg, sizeof msg,
                  "CLEAN SPLASH 1 gentle: Clatter %+.1f dB re the hit (below DRIVEN's %+.1f), crash %+.1f dB, settled %+.1f dB "
                  "(within 1 dB); Jolt lurch %.3f %% of L (DRIVEN %.3f %%, <= 1/3); SPLASH 0 adds nothing",
                  c1.clatterDb[0], d1.clatterDb[0], c1.crashDb[0], c1.settledDb[0], cLurch, dLurch);
    check(c1.clatterDb[0] < d1.clatterDb[0] && c1.crashDb[0] > 0.0 && c1.crashDb[0] < d1.crashDb[0]
              && std::fabs(c1.settledDb[0]) <= 1.0 && cLurch > 0.0 && cLurch <= dLurch / 3.0 && c0.splashE[0] == 0.0,
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
// Clatter (Clatter-only render - no-Splash render, Jolt off) in 1-6 kHz,
// first 150 ms: ghost re its own bright part <= -15 dB (the M7 criterion),
// and < 25 % of a backbeat's Clatter energy. Every ATTITUDE, SPLASH 1.
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
    bool ok = true;
    for (int att : {0, 1, 2}) {
        Settings s;
        s.att    = att;
        s.splash = 1.0f;
        s.drive  = rv::spec(rv::ParamId::Drive).defaultValue;
        s.joltOn = false;
        const Buf mc = mono(render(s, x));
        s.clatterOn = false;
        const Buf mo = mono(render(s, x));
        Buf dc(mc.size());
        for (size_t i = 0; i < dc.size(); ++i) dc[i] = mc[i] - mo[i];
        const Buf hc = band(dc, 1000.0f, 6000.0f), ho = band(mo, 1000.0f, 6000.0f);
        double worstOwn = -300, worstShare = -300, beatOwn = 300;
        const size_t w = size_t(0.15f * kFs);
        for (int k = 1; k < 4; ++k) { // from the second bar: the program level has settled
            const size_t b = size_t((0.5f + float(k)) * kFs), g = size_t((1.0f + float(k)) * kFs);
            worstOwn   = std::max(worstOwn, db(energy(hc, g, g + w) / energy(ho, g, g + w)));
            worstShare = std::max(worstShare, db(energy(hc, g, g + w) / energy(hc, b, b + w)));
            beatOwn    = std::min(beatOwn, db(energy(hc, b, b + w) / energy(ho, b, b + w)));
        }
        std::snprintf(msg, sizeof msg,
                      "%s SPLASH 1 groove: ghost notes (-18 dBFS between -6 dBFS backbeats) barely trigger: Clatter %+.1f dB "
                      "re the ghost's own 1-6 kHz (<= -15), %.1f dB re a backbeat's Clatter (< -6); backbeats %+.1f dB",
                      kAttName[att], worstOwn, worstShare, beatOwn);
        const bool good = worstOwn <= -15.0 && worstShare < -6.0 && beatOwn > worstOwn + 15.0;
        check(good, msg);
        ok &= good;
    }
    (void)ok;
}

// ---- 2. WOBBLE on 08_held_tones ---------------------------------------------------------
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
    const float ws[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    const float ds[] = {0.0f, 0.5f, 1.0f};
    double c[3][5];
    for (int di = 0; di < 3; ++di) {
        std::printf("      DECAY %.1f:", ds[di]);
        for (int i = 0; i < 5; ++i) {
            Settings s;
            s.drive   = rv::spec(rv::ParamId::Drive).defaultValue;
            s.decay   = ds[di];
            s.springs = 0;
            s.splash  = 0.0f;
            s.wobble  = ws[i];
            const Pitch p = pitchCents(band(render(s, x).l, 700.0f, 1400.0f), size_t(3.0f * kFs), size_t(8.9f * kFs));
            c[di][i] = p.p95;
            std::printf("  W%.2f %5.1f (%5.1f)", ws[i], p.p95, p.peak);
        }
        std::printf("\n");
    }
    bool floorOk = true, driftOk = true, rising = true;
    for (int di = 0; di < 3; ++di) {
        floorOk &= c[di][0] < 3.0;
        driftOk &= c[di][2] < 5.0;
        rising &= c[di][4] > c[di][3] && c[di][3] > c[di][2];
    }
    std::snprintf(msg, sizeof msg, "WOBBLE 0 = Micro-mod floor only: p95 %.1f / %.1f / %.1f cents at DECAY 0 / 0.5 / 1 (< 3)",
                  c[0][0], c[1][0], c[2][0]);
    check(floorOk, msg);
    std::snprintf(msg, sizeof msg, "Drift (WOBBLE 0.5): p95 %.1f / %.1f / %.1f cents (< 5: held chords in tune)", c[0][2],
                  c[1][2], c[2][2]);
    check(driftOk, msg);
    std::snprintf(msg, sizeof msg,
                  "Warble (WOBBLE 1): p95 %.1f / %.1f / %.1f cents (>= 10 at DECAY 0, >= 25 at noon and max: clearly out of "
                  "tune), rising through 0.5 -> 0.75 -> 1",
                  c[0][4], c[1][4], c[2][4]);
    check(rising && c[0][4] >= 10.0 && c[1][4] >= 25.0 && c[2][4] >= 25.0, msg);
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
    auto run = [&](int block) {
        rv::Tank t;
        t.prepare(kFs, block);
        t.setParam(rv::ParamId::Mix, 1.0f);
        t.setParam(rv::ParamId::Attitude, 1.0f);
        t.setParam(rv::ParamId::Splash, 1.0f);
        t.setParam(rv::ParamId::Wobble, 1.0f);
        t.setParam(rv::ParamId::Springs, 1.0f);
        Out o{Buf(x.size()), Buf(x.size()), {}};
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
    check(same, "KICKED, SPLASH 1, WOBBLE 1, 3 Springs, hits + Kick: bit-identical for blocks 1, 7, 48, 333, 1024 and on a re-run");
}

// ---- 5. CPU (INFO) ---------------------------------------------------------------------------
// SPEC §5 worst case (3 Springs, KICKED, TONE/DRIVE max, TENSION loosest) with every M7
// part busy: a hard noise hit and a Kick 12 times a second each (Clatter,
// Jolt, rattle and Kick voices never idle), SPLASH 1, WOBBLE 1; against the
// same with SPLASH 0 / WOBBLE 0, no hits, no Kicks (steady noise). Daisy
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
    auto bench = [&](const Buf& in, bool m7) {
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
        t.setParam(rv::ParamId::Wobble, m7 ? 1.0f : 0.0f);
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
    double busy = 1e30, base = 1e30;
    for (int rep = 0; rep < 3; ++rep) { // best of 3: least disturbed by the OS
        busy = std::min(busy, bench(hits, true));
        base = std::min(base, bench(steady, false));
    }
    std::printf("INFO  CPU worst case, 3 Springs KICKED TONE/DRIVE 1 TENSION 0: M7 busy (SPLASH 1, WOBBLE 1, hits + Kicks "
                "12/s) %.1f ns/sample, est. Daisy %.0f-%.0f cycles/sample (%.0f-%.0f%% of 10k); M7 quiet (SPLASH 0, "
                "WOBBLE 0, steady noise) %.1f ns/sample (%.0f-%.0f%%); M7 share %.0f-%.0f cycles/sample\n",
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
