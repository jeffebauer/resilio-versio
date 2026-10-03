// Wellspring fit round 4: desktop cost and pool memory per Tank voicing (core/params/TankVoicing.h).
// Cost: the SPEC §5 worst case (3 Springs, loosest TENSION, KICKED, DRIVE 1, DECAY 1, TONE noon) and the
// closest-to-Wellspring settings (2 Springs, TENSION 0.5, TONE 0.7), 20 s of noise in 48-sample blocks, best of N
// runs, voicings interleaved so a busy machine biases them alike. Pool: Tank::poolFloatsForVoicing (what
// the firmware's pool would need with that voicing as its only one; limit 30,000 floats, firmware/main.cpp).
// Build (desktop, every voicing compiled in):
//   c++ -O3 -DNDEBUG -std=c++17 -Icore docs/prototypes/wellspring-fit-4/bench.cpp core/dsp/*.cpp -o build-r/wf4_bench
#include "dsp/Tank.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
double run(int voicing, bool worst, const std::vector<float>& in, std::vector<float>& l, std::vector<float>& r)
{
    rv::Tank t;
    t.prepare(48000.0f, 48);
    t.setTankVoicing(voicing);
    t.setParam(rv::ParamId::Springs, worst ? 1.0f : 0.5f);
    t.setParam(rv::ParamId::Tension, worst ? 0.0f : 0.5f);
    t.setParam(rv::ParamId::Attitude, worst ? 1.0f : 0.0f);
    t.setParam(rv::ParamId::Drive, worst ? 1.0f : 0.25f);
    t.setParam(rv::ParamId::Decay, worst ? 1.0f : 0.664f);
    t.setParam(rv::ParamId::Tone, worst ? 0.5f : 0.7f);
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
    double best[2][rv::tankv::kNumVoicings];
    for (auto& b : best)
        for (auto& v : b) v = 1e9;
    for (int k = 0; k < runs; ++k)
        for (int w = 0; w < 2; ++w)
            for (int v = 0; v < rv::tankv::kNumVoicings; ++v) best[w][v] = std::min(best[w][v], run(v, w == 0, in, l, r));
    std::printf("desktop pool (all voicings) %zu floats; Tank object %zu bytes\n", rv::Tank::requiredPoolFloats(fs), sizeof(rv::Tank));
    for (int v = 0; v < rv::tankv::kNumVoicings; ++v)
        std::printf("voicing %d: worst case %.1f ns/sample (%+.1f %%), closest settings %.1f ns/sample (%+.1f %%), pool %zu floats\n", v,
                    best[0][v], 100.0 * (best[0][v] / best[0][0] - 1.0), best[1][v], 100.0 * (best[1][v] / best[1][0] - 1.0),
                    rv::Tank::poolFloatsForVoicing(fs, v));
}
