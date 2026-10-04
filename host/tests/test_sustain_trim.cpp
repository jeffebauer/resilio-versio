// Sustain trim tests (M8, ADR 0035, core/params/DriveVoicing.h "Sustain trim").
// Dependency-free: prints PASS/FAIL lines, returns nonzero on any failure.
//
// The owner's pad (1 Oct 2026, release b3e5ac3 on the Versio): a low-mid
// synth pad at mild settings turned the output LEDs red (limiter gain
// reduction >= 0.5 dB, firmware/LedMeter.h). The held sounds here are the
// ones tools/make_sustain_stimulus.py writes, synthesised in place (so the
// suite needs no WAVs): a C minor pad of detuned saws low-passed at 700 Hz
// (3 s swell, 6 s hold), a C2 drone (sine + 2nd harmonic), an organ chord.
//
// It tests the default voicing, round 3's gentle one (a safety net, not a
// level rider; DriveVoicing.h "Sustain trim voicings"):
//   1. Held sounds at -6 dBFS peak, the owner's settings (CLEAN, DRIVE 0,
//      SPLASH 0, DECAY noon) across SPRINGS x TONE x TENSION, at WOBBLE 0,
//      0.25, the default, 0.75 and 1: at the default and right of noon the
//      limiter (30 ms hold) pulls past 2.5 dB for no more than a moment, and
//      no moment past 3 dB; the trim never cuts more than 5 dB. How often the
//      red LED (0.5 dB) would light is printed. Left of noon (Drift) printed,
//      not checked (see heldSoundsAcrossWobble). MIX doesn't enter: the
//      limiter is on the wet, before MIX.
//   2. Hits and chord stabs are never trimmed (the trim stays exactly 1).
//   3. It lets go: a hit 0.4 s after a pad stops meets no trim.
//   4. No swell, no pumping: over the pad's and the drone's hold the trim
//      never eases back up by more than 1 dB, and on the steady drone, once
//      settled, it stays within 1.5 dB (same WOBBLEs as 1).
//   5. The Howl: a Kick into KICKED DECAY 1 is never trimmed; a pad into the
//      Howl leaves it as loud as the Kick's alone, within 1 dB.
//   6. The Big Knob (Renderer TONE voicings 1-3, ADR 0036 Proposed): the
//      held sounds at TONE 0.7 / 0.85 / 1 meet 1.'s limiter limits.
// `rv_test_sustain_trim --voicings` prints the same grids for every voicing
// (0 off, 1 round 2, 2 gentle) for the backlog; ctest runs only the checks.
// `--big-knob` runs 6. alone.

#include "dsp/Tank.h"
#include "params/DriveVoicing.h"
#include "params/ParamSpec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <future>
#include <random>
#include <string>
#include <vector>

namespace {

int  failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float  kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;
constexpr double C2 = 65.406, C3 = 130.813, EB3 = 155.563, G3 = 195.998;

size_t sec(double s) { return size_t(s * double(kFs)); }

void normalise(Buf& x, float peakDb)
{
    float p = 0.0f;
    for (float v : x) p = std::max(p, std::fabs(v));
    const float g = std::pow(10.0f, peakDb / 20.0f) / std::max(p, 1.0e-9f);
    for (float& v : x) v *= g;
}

// Raised-cosine attack / hold / release, framed by lead-in and tail silence.
Buf withEnvelope(const Buf& body, double attack, double hold, double release, double lead = 1.0, double tail = 6.0)
{
    const size_t na = sec(attack), nh = sec(hold), nr = sec(release);
    Buf x(sec(lead) + na + nh + nr + sec(tail), 0.0f);
    for (size_t i = 0; i < na + nh + nr; ++i) {
        double e = 1.0;
        if (i < na) e = 0.5 - 0.5 * std::cos(kPi * double(i) / double(na));
        else if (i >= na + nh) e = 0.5 + 0.5 * std::cos(kPi * double(i - na - nh) / double(nr));
        x[sec(lead) + i] = float(e) * body[i];
    }
    normalise(x, -6.0f);
    return x;
}

// RBJ low-pass, Q 0.707.
void lowpass(Buf& x, double fc)
{
    const double w = 2 * kPi * fc / kFs, alpha = std::sin(w) / (2 * 0.7071), cw = std::cos(w), a0 = 1 + alpha;
    const double b0 = (1 - cw) / 2 / a0, b1 = (1 - cw) / a0, a1 = -2 * cw / a0, a2 = (1 - alpha) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (float& v : x) {
        const double y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = v, y2 = y1, y1 = y;
        v = float(y);
    }
}

Buf pad()
{
    const size_t n = sec(12.0);
    Buf b(n, 0.0f);
    std::mt19937 rng(10);
    std::uniform_real_distribution<double> ph(0.0, 1.0);
    for (auto [f, amp] : {std::pair{C2, 1.0}, {C3, 0.8}, {EB3, 0.7}, {G3, 0.7}})
        for (double c : {-7.0, 0.0, 7.0}) {
            const double inc = f * std::pow(2.0, c / 1200.0) / kFs;
            double p = ph(rng);
            for (size_t i = 0; i < n; ++i) {
                b[i] += float(amp / 3.0 * (2 * p - 1));
                if ((p += inc) >= 1.0) p -= 1.0;
            }
        }
    for (int k = 0; k < 2; ++k) lowpass(b, 700.0);
    return withEnvelope(b, 3.0, 6.0, 3.0);
}

Buf drone()
{
    const size_t n = sec(12.0);
    Buf b(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = double(i) / kFs;
        b[i] = float(std::sin(2 * kPi * C2 * t) + 0.5 * std::sin(2 * kPi * 2 * C2 * t + 0.3));
    }
    return withEnvelope(b, 1.0, 10.0, 1.0);
}

Buf organ()
{
    const size_t n = sec(8.07);
    Buf b(n, 0.0f);
    int k = 0;
    for (auto [f, a] : {std::pair{C2, 0.8}, {C3, 1.0}, {EB3, 1.0}, {G3, 1.0}})
        for (auto [h, d] : {std::pair{1, 1.0}, {2, 0.8}, {3, 0.6}, {4, 0.5}, {6, 0.3}, {8, 0.25}}) {
            const double w = 2 * kPi * f * h / kFs, p0 = 0.37 * k++;
            for (size_t i = 0; i < n; ++i) b[i] += float(a * d * std::sin(w * double(i) + p0));
        }
    return withEnvelope(b, 0.02, 8.0, 0.05);
}

// Snare-like hits (noise + 185 Hz body) and chord stabs (tools/make_stimulus.py
// hits() / skank(), shortened), -6 dBFS peak.
Buf hits()
{
    Buf x(sec(14.0), 0.0f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int h = 0; h < 4; ++h) {
        const size_t at = sec(1.0 + 3.0 * h);
        for (size_t i = 0; i < sec(0.25); ++i) {
            const double t = double(i) / kFs;
            x[at + i] = float(0.6 * std::sin(2 * kPi * 185 * t) * std::exp(-t / 0.03) + 1.2 * u(rng) * std::exp(-t / 0.06));
        }
    }
    normalise(x, -6.0f);
    return x;
}

Buf stabs()
{
    Buf x(sec(12.0), 0.0f);
    const double beat = 60.0 / 75.0;
    for (int k = 0; k < 12; ++k) {
        const size_t at = sec(1.0 + k * beat + beat / 2);
        static constexpr double kAm[3] = {220.0, 261.63, 329.63}, kD[3] = {293.66, 369.99, 440.0};
        const double* ch = (k / 4) % 2 ? kD : kAm;
        for (size_t i = 0; i < sec(0.12); ++i) {
            const double t = double(i) / kFs;
            double s = 0;
            for (int j = 0; j < 3; ++j) s += 2 * std::fmod(ch[j] * t, 1.0) - 1;
            x[at + i] += float(s / 3 * std::exp(-t / 0.035));
        }
    }
    lowpass(x, 2500.0);
    normalise(x, -6.0f);
    return x;
}

// WOBBLE moves the Loops' pitch, so a held sound drifts on and off the
// tank's modes and the build-up comes and goes with it (fully left a pure
// drone's wet swings ~10 dB untrimmed). The grids run at five WOBBLEs: the
// trim has to hold for all of them, not only the default.
const float kWobbles[] = {0.0f, 0.25f, rv::spec(rv::ParamId::Wobble).defaultValue, 0.75f, 1.0f};
constexpr int kNumWobbles = 5;

// The gentle voicing's promise (round 3, ADR 0035). Round 2 promised no red
// LED at all (< 0.5 dB); the owner heard the trim that took moving instead.
// Now the limiter (with its 30 ms hold, clean) may take the loudest peaks:
// - kHardDb / kHardMaxS: it never works past ~2 dB for longer than a moment
//   (2.5: the C2 drone at SPRINGS 3 TONE 0 TENSION 1, where the trim stops at
//   its 5 dB, sits at 2.3 for the whole hold);
// - kMomentMaxDb: that moment (a swell growing faster than the trim's 0.3 s
//   glide) pulls at most this much;
// - kRiseMaxDb: the trim never eases back up during a hold (no swell);
// - kRangeMaxDb: on a steady drone, once settled, it moves at most this
//   (round 2: 2 dB). What it does move is one way, down: WOBBLE carrying
//   the drone onto a louder resonance, caught by a slow glide (~1 dB).
constexpr float kHardDb      = 2.5f;
constexpr float kHardMaxS    = 0.25f;
constexpr float kMomentMaxDb = 3.0f;
constexpr float kRiseMaxDb   = 1.0f;
constexpr float kRangeMaxDb  = 1.5f;

struct Set {
    int   att = 0, springs = 1;
    float decay = 0.5f, tone = 0.3f, tension = 0.8f, drive = 0.0f, splash = 0.0f;
    float wobble = rv::spec(rv::ParamId::Wobble).defaultValue;
};

struct Run {
    float minLimGain = 1.0f;          // limiter gain, lowest per 16-sample block
    std::vector<float> trimDb;        // Sustain trim per block (dB)
    std::vector<float> grDb;          // limiter gain reduction per block (dB, >= 0)
    Buf wet;                          // (L + R) / 2 at MIX 1
};

constexpr int kBlock = 16;
constexpr double kBlockS = double(kBlock) / double(kFs);

Run render(rv::Tank& t, const Set& s, const Buf& in, const std::vector<double>& kicks = {})
{
    using rv::ParamId;
    t.reset();
    t.setParam(ParamId::Attitude, rv::switchToNormalised(s.att));
    // SPRINGS 3 here is the three-Spring reference (setEchoMode(false), Renderer-only since
    // ADR 0041): these checks hold the Springs to their bars; echo mode has test_echo_mode.
    t.setEchoMode(false);
    t.setParam(ParamId::Springs, rv::switchToNormalised(s.springs));
    t.setParam(ParamId::Decay, s.decay);
    t.setParam(ParamId::Tone, s.tone);
    t.setParam(ParamId::Tension, s.tension);
    t.setParam(ParamId::Drive, s.drive);
    t.setParam(ParamId::Splash, s.splash);
    t.setParam(ParamId::Mix, 1.0f);
    t.setParam(ParamId::Wobble, s.wobble);
    Run r;
    r.wet.resize(in.size());
    Buf l(kBlock), rr(kBlock);
    size_t ki = 0;
    for (size_t pos = 0; pos < in.size(); pos += kBlock) {
        const int n = int(std::min<size_t>(kBlock, in.size() - pos));
        while (ki < kicks.size() && sec(kicks[ki]) < pos + size_t(n)) t.kick(int(sec(kicks[ki++]) - pos));
        t.process(in.data() + pos, in.data() + pos, l.data(), rr.data(), n);
        for (int i = 0; i < n; ++i) r.wet[pos + size_t(i)] = 0.5f * (l[size_t(i)] + rr[size_t(i)]);
        r.minLimGain = std::min(r.minLimGain, t.limiterGain());
        r.trimDb.push_back(20.0f * std::log10(t.sustainTrim()));
        r.grDb.push_back(std::max(0.0f, -20.0f * std::log10(t.limiterGain())));
    }
    return r;
}

double rmsDb(const Buf& x, double from, double to)
{
    double s = 0;
    const size_t a = sec(from), b = std::min(sec(to), x.size());
    for (size_t i = a; i < b; ++i) s += double(x[i]) * x[i];
    return 10 * std::log10(std::max(s / double(std::max<size_t>(1, b - a)), 1e-30));
}

float trimAt(const Run& r, double s) { return r.trimDb[std::min(r.trimDb.size() - 1, sec(s) / kBlock)]; }

// The three held sounds: their hold (where the trim's movement is read: the
// pad 1 s after its swell, the drone 2 s into it, the organ after its attack)
// and the loudest 200 ms of it (the held level).
struct Stim {
    const char* name;
    Buf         x;
    double      holdFrom, holdTo;
};

// One held sound at one WOBBLE over the 27 SPRINGS x TONE x TENSION cells, on
// its own Tank (the grids run in parallel).
struct Grid {
    float gr = 0.0f;                    // worst limiter pull, any moment
    char  at[80] = "none limited";
    float over2 = 0.0f;                 // the worst cell's time with the limiter past kHardDb (s)
    char  over2At[80] = "-";
    int   redCells = 0;                 // cells where the output LED lights red (>= 0.5 dB) at all
    float redSeconds = 0.0f;            // the worst cell's time in red (s)
    float move = 0.0f;                  // the trim's range over the hold (max - min)
    char  moveAt[80] = "-";
    float rise = 0.0f;                  // the trim's largest rise in the hold (heard as a swell)
    int   lateCells = 0;                // cells the trim first moves in the hold (a sound growing into the limiter)
    float deepest = 0.0f;               // the deepest trim (dB, <= 0)
    int   trimmedCells = 0;             // cells the trim engaged at all
    float levelDelta = 0.0f;            // loudest 200 ms of the hold vs the voicing `ref`, worst (most negative)
};

double loudest200(const Buf& w, double from, double to)
{
    double best = -300.0;
    for (double a = from; a + 0.2 <= to; a += 0.05) best = std::max(best, rmsDb(w, a, a + 0.2));
    return best;
}

// toneVoicing >= 0: that Big Knob voicing (DriveVoicing.h, Renderer
// voicings; 0 = today) on TONE's right half (0.7 / 0.85 / 1); < 0: today's,
// at TONE 0 / 0.5 / 0.9.
Grid runGrid(const Stim& st, float wobble, int voicing, int ref, int toneVoicing)
{
    rv::Tank t, tr;
    t.prepare(kFs, kBlock);
    tr.prepare(kFs, kBlock);
    t.setSustainVoicing(voicing);
    tr.setSustainVoicing(ref);
    t.setToneVoicing(std::max(0, toneVoicing));
    tr.setToneVoicing(std::max(0, toneVoicing));
    const std::array<float, 3> tones = toneVoicing >= 0 ? std::array<float, 3>{0.7f, 0.85f, 1.0f} : std::array<float, 3>{0.0f, 0.5f, 0.9f};
    Grid g;
    for (int sp = 0; sp < 3; ++sp)
        for (float tone : tones)
            for (float ten : {0.5f, 0.8f, 1.0f}) {
                Set s;
                s.springs = sp, s.tone = tone, s.tension = ten, s.wobble = wobble;
                const Run r = render(t, s, st.x);
                char cell[80];
                std::snprintf(cell, sizeof cell, "SPRINGS %d TONE %.1f TENSION %.1f", sp + 1, double(tone), double(ten));
                const float gr = std::max(0.0f, -20.0f * std::log10(r.minLimGain));
                if (gr > g.gr) { g.gr = gr; std::snprintf(g.at, sizeof g.at, "%s", cell); }
                float red = 0.0f, over2 = 0.0f, lo = 0.0f, hi = -100.0f, mn = 0.0f, rise = 0.0f, deep = 0.0f;
                for (float d : r.grDb) {
                    red   += d >= 0.5f ? float(kBlockS) : 0.0f;
                    over2 += d > kHardDb ? float(kBlockS) : 0.0f;
                }
                for (float d : r.trimDb) deep = std::min(deep, d);
                for (double tt = st.holdFrom; tt < st.holdTo; tt += 0.05) {
                    const float d = trimAt(r, tt);
                    lo = std::min(lo, d), hi = std::max(hi, d);
                    mn = std::min(mn, d), rise = std::max(rise, d - mn);
                }
                if (red > 0.0f) ++g.redCells;
                g.redSeconds = std::max(g.redSeconds, red);
                if (over2 > g.over2) { g.over2 = over2; std::snprintf(g.over2At, sizeof g.over2At, "%s", cell); }
                if (hi - lo > g.move) { g.move = hi - lo; std::snprintf(g.moveAt, sizeof g.moveAt, "%s", cell); }
                g.rise = std::max(g.rise, rise);
                if (deep < 0.0f) ++g.trimmedCells;
                if (deep < 0.0f && trimAt(r, st.holdFrom) > -0.1f) ++g.lateCells;
                g.deepest = std::min(g.deepest, deep);
                if (ref != voicing) {
                    const Run rf = render(tr, s, st.x);
                    const float d = float(loudest200(r.wet, st.holdFrom, st.holdTo) - loudest200(rf.wet, st.holdFrom, st.holdTo));
                    g.levelDelta = std::min(g.levelDelta, d);
                }
            }
    return g;
}

// The hold: the pad 1 s after its 3 s swell, the drone 2 s into it (its trim
// has settled), the organ 2 s after its attack (the tank has filled).
std::vector<Stim> heldStims()
{
    return {{"pad", pad(), 5.0, 9.8}, {"drone", drone(), 4.0, 12.0}, {"organ", organ(), 3.0, 9.0}};
}

void printGrid(const char* name, const Grid& g)
{
    std::printf("  %s %4.2f, >2.5 dB %4.2f s, red %2d (%5.2f s), range %4.2f, rise %4.2f, late %d, cells %2d, deepest %5.2f", name,
                double(g.gr), double(g.over2), g.redCells, double(g.redSeconds), double(g.move), double(g.rise), g.lateCells,
                g.trimmedCells, double(g.deepest));
}

const char* const kGridLegend =
    "worst limiter pull (dB); the worst cell's time past 2.5 dB (s); cells of 27 where the red LED lights (the worst cell's time in "
    "red, s); the trim's range and largest rise over the hold (dB); cells it first moves in the hold; cells trimmed at all; the "
    "deepest trim (dB)";

// Every voicing (rv_test_sustain_trim --voicings): the table the backlog
// keeps, plus the loudest 200 ms of the hold vs voicing 0 (no trim). Not part
// of ctest (it runs about three times as long).
void reportVoicings()
{
    const auto stims = heldStims();
    std::printf("INFO  27 cells per WOBBLE, -6 dBFS peak, the owner's settings. Per held sound: %s; level = loudest 200 ms of the hold vs "
                "voicing 0, worst cell (dB)\n", kGridLegend);
    for (int v = 0; v < 3; ++v) {
        std::vector<std::future<Grid>> jobs;
        for (float w : kWobbles)
            for (const Stim& st : stims) jobs.push_back(std::async(std::launch::async, runGrid, std::cref(st), w, v, 0, -1));
        static const char* const kNames[] = {"0 off (limiter hold only)", "1 round 2", "2 gentle"};
        std::printf("INFO  sustain_voicing %s\n", kNames[v]);
        for (int wi = 0; wi < kNumWobbles; ++wi)
            for (size_t k = 0; k < stims.size(); ++k) {
                const Grid g = jobs[size_t(wi) * stims.size() + k].get();
                std::printf("INFO    WOBBLE %.2f", double(kWobbles[wi]));
                printGrid(stims[k].name, g);
                std::printf(", level %+5.2f\n", double(g.levelDelta));
            }
    }
}

// Held sounds and the limiter (1) and the trim holding still (4), at every
// WOBBLE in kWobbles, for the default (gentle) voicing.
void heldSoundsAcrossWobble()
{
    const auto stims = heldStims();
    std::vector<std::future<Grid>> jobs;
    for (float w : kWobbles)
        for (const Stim& st : stims)
            jobs.push_back(std::async(std::launch::async, runGrid, std::cref(st), w, rv::drive::kSusDefaultVoicing, rv::drive::kSusDefaultVoicing, -1));
    Grid g[kNumWobbles][3];
    for (int wi = 0; wi < kNumWobbles; ++wi)
        for (int k = 0; k < 3; ++k) g[wi][k] = jobs[size_t(wi * 3 + k)].get();
    std::printf("INFO  per WOBBLE: %s\n", kGridLegend);
    for (int wi = 0; wi < kNumWobbles; ++wi)
        for (int k = 0; k < 3; ++k) {
            std::printf("INFO    WOBBLE %.2f", double(kWobbles[wi]));
            printGrid(stims[size_t(k)].name, g[wi][k]);
            std::printf("\n");
        }
    // Checked at the default WOBBLE and on the right side (Warble, a steady
    // vibrato). Left of noon (Drift) the springs themselves swell and dip by
    // ~10 dB on a held note as the random wow moves it on and off the tank's
    // resonances, unforeseeably: printed above (ADR 0035 round 2), not
    // checked.
    constexpr int kFirstChecked = 2; // kWobbles[2] = the default
    auto worstOf = [&](int k, auto field) {
        int at = kFirstChecked;
        for (int wi = kFirstChecked + 1; wi < kNumWobbles; ++wi)
            if (field(g[wi][k]) > field(g[at][k])) at = wi;
        return at;
    };
    for (int k = 0; k < 3; ++k) {
        const char* nm = stims[size_t(k)].name;
        int at = worstOf(k, [](const Grid& x) { return x.gr; });
        std::snprintf(msg, sizeof msg,
                      "Held %s, -6 dBFS peak, CLEAN DRIVE 0 SPLASH 0 DECAY noon, 27 SPRINGS x TONE x TENSION cells, WOBBLE "
                      "default / 0.75 / 1: the limiter pulls at most %.2f dB for a moment (WOBBLE %.2f, %s; limit %.1f)",
                      nm, double(g[at][k].gr), double(kWobbles[at]), g[at][k].at, double(kMomentMaxDb));
        check(g[at][k].gr <= kMomentMaxDb, msg);
        at = worstOf(k, [](const Grid& x) { return x.over2; });
        std::snprintf(msg, sizeof msg, "... and past %.1f dB for %.2f s at most (WOBBLE %.2f, %s; limit %.2f s: it never works hard for long)",
                      double(kHardDb), double(g[at][k].over2), double(kWobbles[at]), g[at][k].over2At, double(kHardMaxS));
        check(g[at][k].over2 <= kHardMaxS, msg);
        if (k == 2) continue; // the organ: printed
        at = worstOf(k, [](const Grid& x) { return x.rise; });
        std::snprintf(msg, sizeof msg, "... the trim never eases back up over the %s's hold (no swell): rises %.2f dB at most (WOBBLE %.2f; limit %.1f)",
                      nm, double(g[at][k].rise), double(kWobbles[at]), double(kRiseMaxDb));
        check(g[at][k].rise <= kRiseMaxDb, msg);
    }
    // The drone: steady, so once settled the trim holds still.
    const int at = worstOf(1, [](const Grid& x) { return x.move; });
    std::snprintf(msg, sizeof msg, "No pumping: on a held drone the settled trim moves %.2f dB at most (WOBBLE %.2f, %s; limit %.1f)",
                  double(g[at][1].move), double(kWobbles[at]), g[at][1].moveAt, double(kRangeMaxDb));
    check(g[at][1].move <= kRangeMaxDb, msg);
    // At most kSusGentleMaxDb, anywhere.
    float deepest = 0.0f;
    for (int wi = 0; wi < kNumWobbles; ++wi)
        for (int k = 0; k < 3; ++k) deepest = std::min(deepest, g[wi][k].deepest);
    std::snprintf(msg, sizeof msg, "The trim cuts at most %.2f dB (every WOBBLE; limit %.1f)", double(-deepest),
                  double(rv::drive::kSusGentleMaxDb));
    check(-deepest <= rv::drive::kSusGentleMaxDb + 0.01f, msg);
    // Teeth: the drone's worst cell without the trim. Since tank voicing 7
    // (ADR 0038) the low cut in front of the Springs takes the C2 drone's
    // 65 / 131 Hz, so it no longer reaches the limiter with or without the
    // trim and this check had nothing to show (0.00 dB). The organ (the same
    // chord with harmonics, which the tank hears) in the same cell shows the
    // trim's teeth: well past the bar without it, under it with it.
    rv::Tank t;
    t.prepare(kFs, kBlock);
    t.setSustainVoicing(rv::drive::kSusVoicingOff);
    Set s;
    s.springs = 2, s.tone = 0.0f, s.tension = 1.0f;
    const float off = -20.0f * std::log10(render(t, s, stims[2].x).minLimGain);
    rv::Tank tOn;
    tOn.prepare(kFs, kBlock);
    const float on = -20.0f * std::log10(render(tOn, s, stims[2].x).minLimGain);
    std::snprintf(msg, sizeof msg,
                  "... and it is the Sustain trim: the organ at SPRINGS 3 TONE 0 TENSION 1 without it pulls %.2f dB (> %.1f), "
                  "with it %.2f",
                  double(off), double(kMomentMaxDb), double(on));
    check(off > kMomentMaxDb && on <= kMomentMaxDb, msg);
}

// The Big Knob (ADR 0036 Proposed, Renderer voicings 1-3): with the right
// half thinned, made up and (voicings 2-3) bumped, held sounds still stay
// off the limiter: the same limits as heldSoundsAcrossWobble, default WOBBLE,
// TONE 0.7 / 0.85 / 1. Also the default voicing 5 (bump on hits) at the
// default placement, the Big Knob after the Springs (ADR 0036 amendment).
void bigKnobHeld()
{
    const auto stims = heldStims();
    const int tvs[] = {0, 1, 2, 3, rv::drive::kToneDefaultVoicing};
    std::vector<std::future<Grid>> jobs;
    for (int tv : tvs) // 0: today's, for reference (printed)
        for (const Stim& st : stims)
            jobs.push_back(std::async(std::launch::async, runGrid, std::cref(st), kWobbles[2], rv::drive::kSusDefaultVoicing,
                                      rv::drive::kSusDefaultVoicing, tv));
    for (size_t i = 0; i < std::size(tvs); ++i)
        for (size_t k = 0; k < stims.size(); ++k) {
            const int tv = tvs[i];
            const Grid g = jobs[i * stims.size() + k].get();
            std::printf("INFO    tone_voicing %d", tv);
            printGrid(stims[k].name, g);
            std::printf("\n");
            if (tv == 0) continue;
            std::snprintf(msg, sizeof msg,
                          "Big Knob voicing %d, held %s at TONE 0.7 / 0.85 / 1 (27 cells, default WOBBLE): the limiter pulls at most "
                          "%.2f dB (%s; limit %.1f), past %.1f dB for %.2f s (limit %.2f s)",
                          tv, stims[k].name, double(g.gr), g.at, double(kMomentMaxDb), double(kHardDb), double(g.over2), double(kHardMaxS));
            check(g.gr <= kMomentMaxDb && g.over2 <= kHardMaxS, msg);
        }
}

void hitsAreNeverTrimmed(rv::Tank& t)
{
    const Buf h = hits(), k = stabs();
    float lowest = 0.0f;
    for (int att : {0, 1, 2})
        for (int sp : {0, 1, 2})
            for (const Buf* x : {&h, &k}) {
                Set s;
                s.att = att, s.springs = sp, s.splash = 0.3f;
                const Run r = render(t, s, *x);
                for (float d : r.trimDb) lowest = std::min(lowest, d);
            }
    std::snprintf(msg, sizeof msg, "Snare hits and chord stabs, every ATTITUDE and SPRINGS: Sustain trim %.3f dB (limit exactly 0)",
                  double(lowest));
    check(lowest == 0.0f, msg);
}

void letsGoForTheNextHit(rv::Tank& t)
{
    // The pad, then a hit 0.4 s after its release ends (13.4 s). The pad at
    // -1 dBFS peak: since tank voicing 7 (ADR 0038: the wet -2.5 dB, the low
    // cut) the -6 dBFS pad no longer needs the trim here (held -0.3 dB), so
    // there was nothing to let go of.
    Buf x = pad();
    normalise(x, -1.0f);
    const Buf h = hits();
    x.resize(sec(16.0), 0.0f);
    for (size_t i = 0; i < sec(0.3); ++i) x[sec(13.4) + i] = h[sec(1.0) + i];
    Set s;
    s.springs = 2, s.tension = 1.0f;
    const Run r = render(t, s, x);
    const float held = trimAt(r, 8.0), atHit = trimAt(r, 13.4);
    std::snprintf(msg, sizeof msg, "Lets go: pad held %.1f dB, a hit 0.4 s after the pad ends meets %.2f dB (limit > -0.1)",
                  double(held), double(atHit));
    check(held < -1.0f && atHit > -0.1f, msg);
}

void howlStaysLoud(rv::Tank& t)
{
    Set s;
    s.att = 2, s.decay = 1.0f, s.springs = 1;
    Buf silence(sec(24.0), 0.0f);
    const Run kick = render(t, s, silence, {1.0});
    float lowest = 0.0f;
    for (float d : kick.trimDb) lowest = std::min(lowest, d);
    Buf p = pad();
    p.resize(sec(24.0), 0.0f);
    const Run fed = render(t, s, p);
    const double a = rmsDb(kick.wet, 18.0, 24.0), b = rmsDb(fed.wet, 18.0, 24.0);
    std::snprintf(msg, sizeof msg,
                  "Howl (KICKED DECAY 1, 2 Springs): a Kick's Howl is never trimmed (%.3f dB); fed by the pad it settles at "
                  "%.1f dB vs %.1f from the Kick (limit +-1)",
                  double(lowest), b, a);
    check(lowest == 0.0f && std::fabs(a - b) <= 1.0, msg);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--voicings") {
        reportVoicings();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--big-knob") { // 6. alone
        bigKnobHeld();
        std::printf("%d failure(s)\n", failures);
        return failures == 0 ? 0 : 1;
    }
    rv::Tank t;
    t.prepare(kFs, kBlock);
    heldSoundsAcrossWobble();
    bigKnobHeld();
    hitsAreNeverTrimmed(t);
    letsGoForTheNextHit(t);
    howlStaysLoud(t);
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
