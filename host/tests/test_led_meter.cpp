// Release firmware LED meters (ADR 0031, SPEC §3 LEDs): firmware/LedMeter.h
// is pure maths, tested here on desktop. Level -> colour, ballistics (fast
// rise, ~0.3 s fall), red hold, and the red triggers: input near clip and
// the Tank's output limiter (Tank::limiterGain(), read-only) pulling down.

#include "../../firmware/LedMeter.h"

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
    std::snprintf(msg, sizeof msg, "-40 dBFS glows green (r %.3f g %.3f b %.3f; drive >= 0.35 so it shows after libDaisy's cube)",
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
    // Hz, 4 s) into 2 Springs, CLEAN, DECAY 0.62, fully wet. At -3 dBFS its
    // modes build up into the limiter; at -30 dBFS they stay well under it.
    auto run = [&](float gainDb) {
        t.reset();
        t.setParam(rv::ParamId::Decay, 0.62f);
        t.setParam(rv::ParamId::Tension, 0.5f);
        t.setParam(rv::ParamId::Tone, 0.5f);
        t.setParam(rv::ParamId::Splash, 0.0f);
        t.setParam(rv::ParamId::Wobble, 0.0f);
        t.setParam(rv::ParamId::Mix, 1.0f);
        t.setParam(rv::ParamId::Springs, rv::switchToNormalised(1));
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

    const Run hot = run(-3.0f);
    std::snprintf(msg, sizeof msg, "held chord -3 dBFS: limiter pulls down (lowest gain %.3f = %.1f dB)",
                  double(hot.minGain), double(20.0f * std::log10(hot.minGain)));
    check(hot.minGain <= rvled::dbToGain(-rvled::kLimiterRedDb), msg);
    check(hot.red, "  ... and both output LEDs turn red");

    const Run quiet = run(-30.0f);
    std::snprintf(msg, sizeof msg, "held chord -30 dBFS: limiter at unity (lowest gain %.4f), output LEDs never red",
                  double(quiet.minGain));
    check(quiet.minGain == 1.0f && !quiet.red, msg);
}

} // namespace

int main()
{
    levelScale();
    colours();
    ballistics();
    redHold();
    limiterFromTank();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
