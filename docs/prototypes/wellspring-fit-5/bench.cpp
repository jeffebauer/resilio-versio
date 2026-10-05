// Wellspring fit round 5: desktop cost and pool memory, voicing 7 (as shipped)
// against round 5's 8-10. Cases: the chip's two worst (run 17): SPRINGS 2 and
// echo mode (SPRINGS 3), each loosest TENSION, KICKED, DRIVE 1, DECAY 1, SPLASH
// and WOBBLE busy; and the session-2 settings (2 Springs, CLEAN, DECAY 0.70,
// knobs at noon / defaults). 20 s of noise in 48-sample blocks, best of N runs,
// voicings interleaved so a busy machine biases them alike. Pool:
// Tank::poolFloatsForVoicing (the firmware's pool with that voicing as its
// only one; limit 30,000 floats, firmware/main.cpp).
// Build (desktop, every voicing compiled in; -ffp-contract=off as the project):
//   c++ -O3 -DNDEBUG -std=c++17 -ffp-contract=off -Icore docs/prototypes/wellspring-fit-5/bench.cpp core/dsp/*.cpp -o build-r/r5bench
#include "dsp/Tank.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
constexpr int kVoicings[] = {7, 8, 9, 10};
constexpr int kCases = 3;
const char* kCaseName[kCases] = {"SPRINGS 2 worst", "echo mode worst", "session-2 settings"};

double run(int voicing, int c, const std::vector<float>& in, std::vector<float>& l, std::vector<float>& r)
{
    rv::Tank t;
    t.prepare(48000.0f, 48);
    t.setTankVoicing(voicing);
    const bool worst = c < 2;
    t.setParam(rv::ParamId::Springs, c == 1 ? 1.0f : 0.5f);
    t.setParam(rv::ParamId::Tension, worst ? 0.0f : 0.5f);
    t.setParam(rv::ParamId::Attitude, worst ? 1.0f : 0.0f);
    t.setParam(rv::ParamId::Drive, worst ? 1.0f : 0.25f);
    t.setParam(rv::ParamId::Decay, worst ? 1.0f : 0.70f);
    t.setParam(rv::ParamId::Splash, worst ? 1.0f : 0.3f);
    t.setParam(rv::ParamId::Wobble, worst ? 0.1f : 0.45f);
    t.setParam(rv::ParamId::Tone, 0.5f);
    t.setParam(rv::ParamId::Mix, 1.0f);
    const size_t n  = in.size();
    const auto   t0 = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos < n; pos += 48) t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n);
}
} // namespace

int main(int argc, char** argv)
{
    const int runs = argc > 1 ? std::atoi(argv[1]) : 5;
    constexpr float fs = 48000.0f;
    const size_t    n  = size_t(20 * fs);
    std::vector<float> in(n), l(n), r(n);
    uint32_t s = 1;
    for (auto& x : in) {
        s = s * 1664525u + 1013904223u;
        x = 0.25f * float(int32_t(s)) / 2147483648.0f;
    }
    double best[kCases][4];
    for (auto& b : best)
        for (auto& v : b) v = 1e9;
    for (int k = 0; k < runs; ++k)
        for (int c = 0; c < kCases; ++c)
            for (int i = 0; i < 4; ++i) best[c][i] = std::min(best[c][i], run(kVoicings[i], c, in, l, r));
    std::printf("desktop pool (all voicings) %zu floats; Tank object %zu bytes\n", rv::Tank::requiredPoolFloats(fs), sizeof(rv::Tank));
    for (int c = 0; c < kCases; ++c) {
        std::printf("%s:", kCaseName[c]);
        for (int i = 0; i < 4; ++i)
            std::printf("  v%d %.1f ns/sample (%+.1f %%)", kVoicings[i], best[c][i], 100.0 * (best[c][i] / best[c][0] - 1.0));
        std::printf("\n");
    }
    for (int i = 0; i < 4; ++i) std::printf("voicing %d: firmware pool %zu floats\n", kVoicings[i], rv::Tank::poolFloatsForVoicing(fs, kVoicings[i]));
}
