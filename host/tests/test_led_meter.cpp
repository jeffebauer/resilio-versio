// Release firmware LED meters (ADR 0031, SPEC §3 LEDs): firmware/LedMeter.h
// is pure maths, tested here on desktop. Level -> colour, ballistics (fast
// rise, ~0.3 s fall), red hold, and the red triggers: input near clip and
// the Tank's output limiter (Tank::limiterGain(), read-only) pulling down.
// Also the PWM tables the release firmware streams to the LED pins by DMA
// (30 Sep 2026 fix for "LEDs flicker rather than dim").
// And the tap tempo flash (firmware/TapLed.h, ADR 0043 amendment): purple on
// each tap in SPRINGS 3 and on the tapped beat for 2 s after the last.

#include "../../firmware/LedMeter.h"
#include "../../firmware/TapLed.h"

#include "dsp/Tank.h"
#include "params/ParamSpec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int  failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using rvled::LevelMeter;
using rvled::Rgb;

bool isOff(const Rgb& c) { return c.r == 0.0f && c.g == 0.0f && c.b == 0.0f; }
float brightest(const Rgb& c) { return std::max({c.r, c.g, c.b}); }
float dbfs(float db) { return rvled::dbToGain(db); }

void levelScale()
{
    check(rvled::levelToPosition(0.0f) == 0.0f, "silence -> position 0");
    check(rvled::levelToPosition(dbfs(-60.0f)) == 0.0f, "-60 dBFS (below the -48 floor) -> position 0");
    check(std::fabs(rvled::levelToPosition(dbfs(-24.0f)) - 0.5f) < 1e-4f, "-24 dBFS -> position 0.5 (dB scale)");
    check(std::fabs(rvled::levelToPosition(1.0f) - 1.0f) < 1e-6f, "0 dBFS -> position 1");
    check(rvled::levelToPosition(2.0f) == 1.0f, "over full scale clamps to 1");
}

void colours()
{
    check(isOff(rvled::positionColour(0.0f)), "floor: LED off");

    const Rgb quiet = rvled::positionColour(rvled::levelToPosition(dbfs(-40.0f)));
    std::snprintf(msg, sizeof msg, "-40 dBFS glows green (r %.3f g %.3f b %.3f; drive >= kMinGlow so it shows after the PWM's cube)",
                  double(quiet.r), double(quiet.g), double(quiet.b));
    check(quiet.r == 0.0f && quiet.b == 0.0f && quiet.g >= rvled::kMinGlow, msg);

    const Rgb mid = rvled::positionColour(rvled::levelToPosition(dbfs(-20.0f)));
    check(mid.r == 0.0f && mid.g > quiet.g, "-20 dBFS: still green, brighter");

    const Rgb warm = rvled::positionColour(rvled::levelToPosition(dbfs(-12.0f)));
    check(warm.r > 0.0f && warm.g >= warm.r, "-12 dBFS: warming (red joins the green: yellow)");

    const Rgb hot = rvled::positionColour(rvled::levelToPosition(dbfs(-3.0f)));
    std::snprintf(msg, sizeof msg, "-3 dBFS: amber (r %.3f g %.3f b %.3f)", double(hot.r), double(hot.g), double(hot.b));
    check(hot.r > hot.g && hot.g > 0.5f * hot.r && hot.b == 0.0f, msg);

    // Brightness never drops as level rises; level alone never makes red.
    bool monotonic = true, neverRed = true;
    float last = 0.0f;
    for (int db = -60; db <= 6; ++db) {
        const Rgb c = rvled::positionColour(rvled::levelToPosition(dbfs(float(db))));
        monotonic &= brightest(c) >= last - 1e-6f;
        last = brightest(c);
        if (!isOff(c)) neverRed &= c.g >= 0.5f * c.r;
    }
    check(monotonic, "brightness rises monotonically with level, -60..+6 dBFS");
    check(neverRed, "level alone never turns an LED red (green stays >= half the red)");
}

void ballistics()
{
    constexpr float dt = 0.001f; // the firmware main loop runs about every 1 ms
    LevelMeter m;
    m.update(1.0f, false, dt);
    check(m.position() == 1.0f, "fast rise: one update at 0 dBFS reaches full");

    for (int i = 0; i < 300; ++i) m.update(0.0f, false, dt);
    std::snprintf(msg, sizeof msg, "fall: 0.3 s after the signal stops, position %.3f ~ e^-1", double(m.position()));
    check(std::fabs(m.position() - std::exp(-1.0f)) < 0.01f, msg);

    for (int i = 0; i < 1200; ++i) m.update(0.0f, false, dt);
    check(isOff(m.colour()), "dark 1.5 s after the signal stops");

    // A held level settles on that level (falls to it, not below).
    LevelMeter h;
    h.update(1.0f, false, dt);
    for (int i = 0; i < 3000; ++i) h.update(dbfs(-24.0f), false, dt);
    check(std::fabs(h.position() - 0.5f) < 1e-3f, "held -24 dBFS after a 0 dBFS peak settles at position 0.5");
}

void redHold()
{
    constexpr float dt = 0.001f;
    check(rvled::inputNearClip(dbfs(-0.5f)), "input at -0.5 dBFS is near clip");
    check(!rvled::inputNearClip(dbfs(-3.0f)), "input at -3 dBFS is not");
    check(rvled::limiterReducing(dbfs(-1.0f)), "limiter pulling 1 dB -> red");
    check(!rvled::limiterReducing(1.0f), "limiter at unity -> not red");
    check(!rvled::limiterReducing(dbfs(-0.2f)), "limiter pulling 0.2 dB -> not red (below 0.5 dB)");

    LevelMeter m;
    m.update(dbfs(-0.5f), rvled::inputNearClip(dbfs(-0.5f)), dt);
    const Rgb c = m.colour();
    check(m.isRed() && c.r == 1.0f && c.g == 0.0f && c.b == 0.0f, "trigger -> full red");
    int redMs = 0;
    while (m.isRed() && redMs < 5000) {
        m.update(0.0f, false, dt);
        ++redMs;
    }
    std::snprintf(msg, sizeof msg, "red holds %d ms after the last trigger (~500)", redMs);
    check(redMs >= 495 && redMs <= 505, msg);
    check(!m.isRed() && m.colour().r == 0.0f, "after the hold: back to the level colour");

    // A new trigger during the hold restarts it.
    LevelMeter r;
    r.update(1.0f, true, dt);
    for (int i = 0; i < 400; ++i) r.update(0.0f, false, dt);
    r.update(1.0f, true, dt);
    for (int i = 0; i < 400; ++i) r.update(0.0f, false, dt);
    check(r.isRed(), "a second trigger restarts the 0.5 s hold");
}

// Tank::limiterGain() on a real Tank, driven the way the firmware does:
// per-block peaks and the lowest limiter gain feed the output meters.
void limiterFromTank()
{
    constexpr float kFs = 48000.0f;
    constexpr int   kBlock = 48;
    rv::Tank t;
    t.prepare(kFs, kBlock);
    check(t.limiterGain() == 1.0f, "limiterGain() is 1 after prepare");

    std::vector<float> zero(kBlock, 0.0f), l(kBlock), r(kBlock);
    for (int b = 0; b < 100; ++b) t.process(zero.data(), zero.data(), l.data(), r.data(), kBlock);
    check(t.limiterGain() == 1.0f, "silence: limiter at unity");

    struct Run {
        float minGain = 1.0f;
        bool  red     = false;
    };
    // test_clicks' "limiter pushed" case: a held chord (220 / 261.63 / 329.63
    // Hz, 4 s) into 3 Springs (2 before tank voicing 7, below), CLEAN, DECAY
    // 0.62, fully wet. At -3 dBFS its modes build up into the limiter; at
    // -30 dBFS they stay well under it.
    // Sustain trim off (ADR 0035): it would keep this held chord under the
    // limiter, and this checks the LEDs when the limiter does pull.
    auto run = [&](float gainDb, int springs) {
        t.reset();
        t.setSustainTrimEnabled(false);
        // Three Springs (setEchoMode(false): SPRINGS 3's reference since ADR
        // 0041): their modes add up into the limiter, which is what this needs.
        t.setEchoMode(false);
        t.setParam(rv::ParamId::Decay, 0.62f);
        t.setParam(rv::ParamId::Tension, 0.5f);
        t.setParam(rv::ParamId::Tone, 0.5f);
        t.setParam(rv::ParamId::Splash, 0.0f);
        t.setParam(rv::ParamId::Wobble, 0.5f); // noon: still
        t.setParam(rv::ParamId::Mix, 1.0f);
        t.setParam(rv::ParamId::Springs, rv::switchToNormalised(springs - 1));
        t.setParam(rv::ParamId::Attitude, 0.0f);
        t.setParam(rv::ParamId::Drive, 0.0f);
        const double g = std::pow(10.0, double(gainDb) / 20.0), f[3] = {220.0, 261.63, 329.63};
        std::vector<float> in(kBlock);
        LevelMeter outL, outR;
        Run res;
        const int blocks = int(4.0f * kFs) / kBlock;
        for (int b = 0; b < blocks; ++b) {
            for (int i = 0; i < kBlock; ++i) {
                const double n = double(b * kBlock + i), env = std::min(1.0, n / 480.0);
                double s = 0.0;
                for (double hz : f) s += std::sin(2.0 * 3.14159265358979 * hz * n / double(kFs));
                in[size_t(i)] = float(g * env * s / 3.0);
            }
            t.process(in.data(), in.data(), l.data(), r.data(), kBlock);
            float pl = 0.0f, pr = 0.0f;
            for (int i = 0; i < kBlock; ++i) {
                pl = std::max(pl, std::fabs(l[size_t(i)]));
                pr = std::max(pr, std::fabs(r[size_t(i)]));
            }
            const float lg  = t.limiterGain();
            const bool  red = rvled::limiterReducing(lg);
            res.minGain     = std::min(res.minGain, lg);
            outL.update(pl, red, 0.001f);
            outR.update(pr, red, 0.001f);
            res.red |= outL.isRed() && outR.isRed();
        }
        return res;
    };

    // Since tank voicing 7 (ADR 0038: the wet -2.5 dB, the transducers, the
    // low cut) the chord into 2 Springs no longer reached the limiter (0.0 dB),
    // so the red-LED check had nothing to show; into 3 Springs (their modes
    // add up) it pulls about as hard as it did into 2 before (-3.3 vs -2.7 dB).
    const Run hot = run(-3.0f, 3);
    std::snprintf(msg, sizeof msg, "held chord -3 dBFS, 3 Springs: limiter pulls down (lowest gain %.3f = %.1f dB)",
                  double(hot.minGain), double(20.0f * std::log10(hot.minGain)));
    check(hot.minGain <= rvled::dbToGain(-rvled::kLimiterRedDb), msg);
    check(hot.red, "  ... and both output LEDs turn red");

    const Run quiet = run(-30.0f, 3);
    std::snprintf(msg, sizeof msg, "held chord -30 dBFS: limiter at unity (lowest gain %.4f), output LEDs never red",
                  double(quiet.minGain));
    check(quiet.minGain == 1.0f && !quiet.red, msg);
}

void pwm()
{
    constexpr int N = rvled::kPwmSteps;
    check(N >= 256, "PWM has at least 256 steps per period");
    check(rvled::pwmCount(0.0f) == 0 && rvled::pwmCount(-1.0f) == 0, "drive 0 (or below) -> 0 steps on");
    check(rvled::pwmCount(1.0f) == N && rvled::pwmCount(2.0f) == N, "drive 1 (or above) -> on all period");
    check(rvled::pwmCount(0.5f) == int(0.125f * float(N) + 0.5f), "drive is cubed (0.5 -> 1/8 duty)");

    // The quietest lit meter step: visibly on, clearly dim.
    const Rgb   quietest = rvled::positionColour(rvled::kOffPosition + 1e-4f);
    const int   q        = rvled::pwmCount(brightest(quietest));
    const float duty     = float(q) / float(N);
    std::snprintf(msg, sizeof msg, "quietest lit step: %d of %d steps (%.1f %% duty), between 1 %% and 5 %%", q, N,
                  double(100.0f * duty));
    check(duty >= 0.01f && duty <= 0.05f, msg);

    // Smooth dimming: many distinct, never-decreasing duties across the meter.
    int  distinct = 0, last = -1;
    bool rising   = true;
    for (int i = 0; i <= 4800; ++i) {
        const float db = -48.0f + 48.0f * float(i) / 4800.0f;
        const int   n  = rvled::pwmCount(brightest(rvled::positionColour(rvled::levelToPosition(dbfs(db)))));
        rising &= n >= last;
        if (n != last) ++distinct;
        last = n;
    }
    std::snprintf(msg, sizeof msg,
                  "meter -48..0 dBFS covers %d distinct brightness steps (>= 64), never dimming as level rises",
                  distinct);
    check(distinct >= 64 && rising, msg);

    // Active-low pins: on = reset bit (high half of BSRR), off = set bit.
    const rvled::PwmPin p5 = rvled::pwmPin(5, true);
    check(p5.onBits == (1u << 21) && p5.offBits == (1u << 5), "active-low pin 5: on resets it, off sets it");
    const rvled::PwmPin h5 = rvled::pwmPin(5, false);
    check(h5.onBits == (1u << 5) && h5.offBits == (1u << 21), "active-high pin 5: the other way round");

    // A port with six LED pins (like the Versio's port B), assorted duties.
    const int     pinNo[6]  = {5, 8, 9, 6, 7, 14};
    const int     counts[6] = {0, 1, 14, 256, N - 1, N};
    rvled::PwmPin pins[6];
    uint32_t      all = 0;
    for (int k = 0; k < 6; ++k) {
        pins[k] = rvled::pwmPin(pinNo[k], true);
        all |= pins[k].onBits | pins[k].offBits;
    }
    std::vector<uint32_t> words(size_t(N), 0xdeadbeefu);
    rvled::fillPwmWords(words.data(), N, pins, counts, 6);
    bool exact = true, noStray = true;
    for (int s = 0; s < N; ++s) {
        const uint32_t w = words[size_t(s)];
        noStray &= (w & ~all) == 0;
        for (int k = 0; k < 6; ++k) {
            const bool on = (w & pins[k].onBits) != 0, off = (w & pins[k].offBits) != 0;
            exact &= on != off && on == (s < counts[k]);
        }
    }
    check(exact, "PWM table: each pin on for exactly its first `count` steps, off after, every step drives it");
    check(noStray, "PWM table: no bits for pins outside the port's LED pins");

    // Rebuilding after a change leaves nothing of the old table.
    const int counts2[6] = {N, 0, 3, 3, 400, 1};
    rvled::fillPwmWords(words.data(), N, pins, counts2, 6);
    bool exact2 = true;
    for (int s = 0; s < N; ++s)
        for (int k = 0; k < 6; ++k) exact2 &= ((words[size_t(s)] & pins[k].onBits) != 0) == (s < counts2[k]);
    check(exact2, "PWM table rebuilt with new duties matches them exactly");
}


// ---- Tap tempo flash (TapLed.h) ---------------------------------------------
// One simulated main loop: 1 ms steps from t0 (us, may wrap). Events change
// the inputs at given milliseconds; returns the times (ms) each purple pulse
// started and how long it lasted.
struct TapEvent {
    int      ms;
    int      tap;      // 1: a tap now
    int      beatUs;   // >= 0: the tapped beat from now on (0 = none)
    int      tapping;  // >= 0: Tank::tapping() from now on
};
struct Pulse {
    int start, len;
};
std::vector<Pulse> tapRun(const std::vector<TapEvent>& ev, int endMs, uint32_t t0 = 0)
{
    rvled::TapFlash f;
    uint32_t taps = 0, beat = 0;
    bool     tapping = true;
    std::vector<Pulse> out;
    bool was = false;
    for (int ms = 0; ms < endMs; ++ms) {
        for (const auto& e : ev)
            if (e.ms == ms) {
                taps += uint32_t(e.tap);
                if (e.beatUs >= 0) beat = uint32_t(e.beatUs);
                if (e.tapping >= 0) tapping = e.tapping != 0;
            }
        const bool on = f.update(t0 + uint32_t(ms) * 1000u, taps, beat, tapping);
        if (on && !was) out.push_back({ms, 0});
        if (on) ++out.back().len;
        was = on;
    }
    return out;
}
bool pulsesAre(const std::vector<Pulse>& p, std::vector<int> starts, int len = 70)
{
    if (p.size() != starts.size()) return false;
    for (size_t i = 0; i < p.size(); ++i)
        if (p[i].start != starts[i] || p[i].len != len) return false;
    return true;
}

void tapFlash()
{
    // No tempo: each tap flashes 70 ms, nothing more.
    auto p = tapRun({{0, 1, 0, -1}, {1000, 1, 0, -1}}, 5000);
    check(pulsesAre(p, {0, 1000}), "tap LED: no tempo -> each tap one 70 ms purple flash, no continuation");

    // A tempo locked (120 bpm): the tap's flash, then a pulse on each beat
    // whose start is within 2 s of the tap (0.5 .. 2.0 s), then dark.
    p = tapRun({{0, 1, 500000, -1}}, 6000);
    check(pulsesAre(p, {0, 500, 1000, 1500, 2000}), "tap LED: 120 bpm -> pulses at 0.5 / 1 / 1.5 / 2 s after the tap, then off");
    p = tapRun({{0, 1, 600000, -1}}, 6000);
    check(pulsesAre(p, {0, 600, 1200, 1800}), "tap LED: 100 bpm -> pulses at 0.6 / 1.2 / 1.8 s (2.4 s is past the 2 s), then off");
    p = tapRun({{0, 1, 2000000, -1}}, 6000);
    check(pulsesAre(p, {0, 2000}), "tap LED: 30 bpm -> one pulse at 2 s");
    // The tempo settles a block after the tap (the Tank's control tick):
    // the beat arriving during the flash still counts.
    p = tapRun({{0, 1, 0, -1}, {1, 0, 500000, -1}}, 6000);
    check(pulsesAre(p, {0, 500, 1000, 1500, 2000}), "tap LED: the beat arriving 1 ms after the tap still continues");

    // A new tap restarts it, phase-locked to the new tap.
    p = tapRun({{0, 1, 500000, -1}, {1250, 1, -1, -1}}, 6000);
    check(pulsesAre(p, {0, 500, 1000, 1250, 1750, 2250, 2750, 3250}), "tap LED: a new tap restarts the pulses from itself");

    // SPRINGS off 3 mid-continuation: dark at once, and back in 3 without a
    // new tap it stays dark. Mid-flash too.
    p = tapRun({{0, 1, 500000, -1}, {700, 0, -1, 0}, {900, 0, -1, 1}}, 6000);
    check(pulsesAre(p, {0, 500}), "tap LED: leaving SPRINGS 3 stops the pulses at once (and they don't come back)");
    p = tapRun({{0, 1, 500000, -1}, {30, 0, -1, 0}}, 6000);
    check(p.size() == 1 && p[0].start == 0 && p[0].len == 30, "tap LED: leaving SPRINGS 3 mid-flash darkens at once");
    // Taps outside SPRINGS 3 (the Tank doesn't count them, but if the count
    // moved anyway): nothing.
    p = tapRun({{0, 0, -1, 0}, {10, 1, 500000, -1}}, 3000);
    check(p.empty(), "tap LED: no purple when the button doesn't tap (SPRINGS 1-2)");

    // The tempo released (or taken by the gate clock / host) mid-continuation.
    p = tapRun({{0, 1, 500000, -1}, {1200, 0, 0, -1}, {1300, 0, 500000, -1}}, 6000);
    check(pulsesAre(p, {0, 500, 1000}), "tap LED: the tempo released stops the pulses at once");

    // The us clock's wrap (~71.6 min): a tap 1 s before it pulses exactly
    // as anywhere else, and once over nothing lights again for over a wrap.
    const uint32_t nearWrap = 0xFFFFFFFFu - 999999u;
    p = tapRun({{0, 1, 500000, -1}}, 6000, nearWrap);
    check(pulsesAre(p, {0, 500, 1000, 1500, 2000}), "tap LED: a tap 1 s before the us clock wraps pulses the same");
    {
        rvled::TapFlash f;
        bool lit = false, first = f.update(0, 1, 500000, true);
        for (uint32_t ms = 1; ms < 75u * 60u * 1000u; ++ms) {
            const bool on = f.update(ms * 1000u, 1, 500000, true);
            if (ms > 2100) lit |= on;
        }
        check(first && !lit, "tap LED: dark for 75 min after the show ends (no false flash as the clock wraps)");
    }

    // White (the throw exit) wins over purple; purple over the meter.
    const Rgb meter{0.3f, 0.3f, 0.0f};
    const Rgb w = rvled::shown(true, true, meter), pu = rvled::shown(false, true, meter), m = rvled::shown(false, false, meter);
    check(w.r == 1.0f && w.g == 1.0f && w.b == 1.0f && pu.r == rvled::kTapPurple.r && pu.b == rvled::kTapPurple.b
              && pu.g == 0.0f && m.r == meter.r && m.g == meter.g,
          "tap LED: white wins over purple, purple over the meter");
    // Purple is a colour the meters never make: they have no blue.
    bool noBlue = rvled::kRed.b == 0.0f;
    for (int i = 0; i <= 100; ++i) noBlue &= rvled::positionColour(float(i) / 100.0f).b == 0.0f;
    check(noBlue && rvled::kTapPurple.b == 1.0f && rvled::pwmCount(rvled::kTapPurple.b) == rvled::kPwmSteps,
          "tap LED: purple (full blue) is never a meter colour (no blue in green/amber/red), full brightness");
}

// The Tank's side, end to end: button taps in SPRINGS 3 through Tank::taps()
// / tappedBeatSamples() / tapping() into TapFlash, one 1 ms block at a time
// (as the firmware's callback and main loop).
std::vector<Pulse> tankTapRun(const std::vector<double>& pressSec, int springs, float hostBpm, double endSec,
                              bool* tapClockAtEnd = nullptr)
{
    constexpr float kFs = 48000.0f;
    constexpr int   kBlock = 48;
    rv::Tank t;
    t.prepare(kFs, kBlock);
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(springs - 1));
    t.setParam(rv::ParamId::Mix, 0.5f);
    t.setHostTempo(hostBpm);
    std::vector<float> zero(kBlock, 0.0f), l(kBlock), r(kBlock);
    rvled::TapFlash f;
    std::vector<Pulse> out;
    bool was = false;
    const int blocks = int(endSec * 1000.0);
    for (int b = 0; b < blocks; ++b) {
        for (double p : pressSec) {
            const int at = int(std::lround(p * 1000.0));
            if (b == at) t.button(true, 0);
            if (b == at + 30) t.button(false, 0);
        }
        t.process(zero.data(), zero.data(), l.data(), r.data(), kBlock);
        const uint32_t beatUs = uint32_t(t.tappedBeatSamples() * (1.0e6f / kFs));
        const bool on = f.update(uint32_t(b) * 1000u, t.taps(), beatUs, t.tapping());
        if (on && !was) out.push_back({b, 0});
        if (on) ++out.back().len;
        was = on;
    }
    if (tapClockAtEnd) *tapClockAtEnd = t.tapClockInUse();
    return out;
}

void tapFlashFromTank()
{
    // Four taps at 120 bpm: a flash on each, then the beat to 2 s after the last.
    auto p = tankTapRun({1.0, 1.5, 2.0, 2.5}, 3, 0.0f, 6.0);
    check(pulsesAre(p, {1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500}),
          "tap LED via the Tank: taps at 120 bpm flash, then pulse at 3 / 3.5 / 4 / 4.5 s, then the meters");
    // Then a lone tap at 8 s (it lets the tempo go): just its flash.
    bool inUse = true;
    p = tankTapRun({1.0, 1.5, 2.0, 2.5, 8.0}, 3, 0.0f, 12.0, &inUse);
    check(pulsesAre(p, {1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 8000}) && !inUse,
          "tap LED via the Tank: a lone tap (it releases the tempo) flashes once, no continuation");
    // One tap alone: one flash.
    p = tankTapRun({1.0}, 3, 0.0f, 4.0);
    check(pulsesAre(p, {1000}), "tap LED via the Tank: the first tap flashes once (no tempo yet)");
    // SPRINGS 1 / 2: presses throw, no purple.
    p = tankTapRun({1.0, 1.5, 2.0, 2.5}, 1, 0.0f, 5.0);
    const auto p2 = tankTapRun({1.0, 1.5, 2.0, 2.5}, 2, 0.0f, 5.0);
    check(p.empty() && p2.empty(), "tap LED via the Tank: no purple in SPRINGS 1 or 2 (the button throws)");
    // The Plugin in a DAW (the host's tempo wins, ADR 0043): each tap still
    // flashes, no continuation.
    p = tankTapRun({1.0, 1.5, 2.0, 2.5}, 3, 100.0f, 5.0);
    check(pulsesAre(p, {1000, 1500, 2000, 2500}), "tap LED via the Tank: with the host's tempo each tap flashes, no continuation");
}
} // namespace

int main()
{
    levelScale();
    colours();
    ballistics();
    redHold();
    limiterFromTank();
    pwm();
    tapFlash();
    tapFlashFromTank();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
