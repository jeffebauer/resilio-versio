// Held-tone click regression (the "DECAY-0.5 tick", TASKS after M7).
// Dependency-free: prints PASS/FAIL lines, returns nonzero on any failure.
//
// Root cause: a held chord landing on Loop resonances drives the wet above
// the output limiter. The old limiter had an instant-attack gain, so every
// new, higher peak was pinned flat at kLimitThreshold for a few samples: a
// corner in the waveform on each beat of the chord, which the Renderer's
// click detector (host/common/Metrics.cpp) counted in the hundreds and
// which is audible as a crackle. Where it happened depended only on whether
// the chord tones sat on Loop modes (so on DECAY's L, fC), not on DRIVE,
// SPLASH or WOBBLE: DECAY 0.44/0.50/0.54/0.58/0.62/0.69 at SPRINGS 2, all
// SPRINGS and BOING/TONE settings, CLEAN and DRIVEN (KICKED's LoopSat kept
// the level under the limiter). The limiter now glides its gain and
// catches the overshoot with a curvature-free soft knee (Tank.cpp).
//
// Checks (all MIX 1, SPLASH 0, WOBBLE 0, 2 s tail):
//   1. held A minor chord (08_held_tones.wav's, -12 dBFS) at DECAY 0.40 ..
//      0.70 in 0.01 steps, SPRINGS 2, CLEAN, DRIVE 0: click_count 0 each;
//   2. spot checks at DECAY 0.50 (the default) over SPRINGS 1/2/3 and
//      ATTITUDE x DRIVE: click_count 0;
//   3. the limiter itself, pushed hard (chord at -3 dBFS): peaks never
//      exceed kLimitThreshold, the waveform is never held flat there
//      (the old limiter's signature), and click_count is 0.

#include "Metrics.h"
#include "dsp/Tank.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int  failures = 0;
char msg[256];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

constexpr float kFs = 48000.0f;
using Buf           = std::vector<float>;

// 08_held_tones.wav's chord (tools/make_stimulus.py): 4 s, 10 ms fades,
// then 2 s of silence for the tail.
Buf heldChord(float gainDb)
{
    const size_t n = size_t(4.0f * kFs), fade = size_t(0.01f * kFs), tail = size_t(2.0f * kFs);
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    const double f[3] = {220.0, 261.63, 329.63};
    Buf x(n + tail, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const double env = std::min({1.0, double(i) / double(fade), double(n - 1 - i) / double(fade)});
        double s = 0.0;
        for (double hz : f) s += std::sin(2.0 * 3.14159265358979 * hz * double(i) / double(kFs));
        x[i] = float(g * env * s / 3.0);
    }
    return x;
}

struct Result {
    long  clicks   = 0;
    float peak     = 0.0f;
    long  flatRuns = 0; // runs of >= 3 samples held within 1e-6 of the threshold
};

Result render(const Buf& in, float decay, int springs, float attitude, float drive)
{
    rv::Tank t;
    t.prepare(kFs, 48);
    t.setParam(rv::ParamId::Decay, decay);
    t.setParam(rv::ParamId::Boing, 0.5f);
    t.setParam(rv::ParamId::Tone, 0.5f);
    t.setParam(rv::ParamId::Splash, 0.0f);
    t.setParam(rv::ParamId::Wobble, 0.0f);
    t.setParam(rv::ParamId::Mix, 1.0f);
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(springs - 1));
    t.setParam(rv::ParamId::Attitude, attitude);
    t.setParam(rv::ParamId::Drive, drive);
    const size_t n = in.size();
    Buf l(n), r(n);
    for (size_t p = 0; p < n; p += 48) {
        const int m = int(std::min<size_t>(48, n - p));
        t.process(&in[p], &in[p], &l[p], &r[p], m);
    }
    Result res;
    res.clicks = rv::metrics::compute({l, r}, kFs).clickCount;
    for (const Buf* ch : {&l, &r}) {
        int run = 0;
        for (float v : *ch) {
            res.peak = std::max(res.peak, std::fabs(v));
            run = std::fabs(std::fabs(v) - rv::Tank::kLimitThreshold) < 1e-6f ? run + 1 : 0;
            if (run == 3) ++res.flatRuns;
        }
    }
    return res;
}

void decayScan()
{
    const Buf in = heldChord(-12.0f);
    long worst = 0, total = 0, limited = 0;
    float worstAt = 0.0f;
    for (int k = 0; k <= 30; ++k) {
        const float  decay = 0.40f + 0.01f * float(k);
        const Result r     = render(in, decay, 2, 0.0f, 0.0f);
        total += r.clicks;
        if (r.peak > rv::Tank::kLimitKnee) ++limited;
        if (r.clicks > worst) { worst = r.clicks; worstAt = decay; }
    }
    std::snprintf(msg, sizeof msg,
                  "held chord, DECAY 0.40..0.70 step 0.01, 2 Springs CLEAN: %ld click(s) total (worst %ld at %.2f); "
                  "limiter engaged at %ld of 31",
                  total, worst, double(worstAt), limited);
    check(total == 0, msg);
    // The scan must actually exercise the limiter, or it proves nothing.
    check(limited >= 5, "held chord scan drives the limiter (>= 5 DECAY settings above the knee)");
}

void spotChecks()
{
    const Buf in = heldChord(-12.0f);
    const float attitudes[3] = {0.0f, 0.5f, 1.0f};
    const float drives[3]    = {0.0f, 0.25f, 0.5f};
    for (int springs = 1; springs <= 3; ++springs) {
        long total = 0;
        for (float a : attitudes)
            for (float d : drives) total += render(in, 0.5f, springs, a, d).clicks;
        std::snprintf(msg, sizeof msg, "held chord, DECAY 0.50, %d Spring(s), ATTITUDE x DRIVE (9): %ld click(s)",
                      springs, total);
        check(total == 0, msg);
    }
}

void limiterPushed()
{
    const Buf in = heldChord(-3.0f);
    for (float decay : {0.5f, 0.62f}) {
        const Result r = render(in, decay, 2, 0.0f, 0.0f);
        std::snprintf(msg, sizeof msg,
                      "limiter pushed (chord -3 dBFS, DECAY %.2f): peak %.4f <= %.2f, %ld flat-top run(s), %ld click(s)",
                      double(decay), double(r.peak), double(rv::Tank::kLimitThreshold), r.flatRuns, r.clicks);
        check(r.peak <= rv::Tank::kLimitThreshold && r.flatRuns == 0 && r.clicks == 0, msg);
    }
}

} // namespace

int main()
{
    decayScan();
    spotChecks();
    limiterPushed();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
