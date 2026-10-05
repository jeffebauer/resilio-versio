// Round 5 fix-up probe: test_drive's ATTITUDE CLEAN -> KICKED Morph on held noise (DRIVE 0.6, DECAY 0.7,
// block 16), the wet's L + R level in 10 ms steps around the switch, per tank voicing, with the output's
// mu-law box on / off and the Sustain trim on / off, to find what dips.
//   c++ -O2 -std=c++17 -ffp-contract=off -Icore docs/prototypes/wellspring-fit-5/morphprobe.cpp build-r/librv_core.a -o build-r/morphprobe
#include "dsp/Tank.h"

#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    const float fs = 48000.0f;
    const size_t n = size_t(4.0f * fs), at = size_t(2.0f * fs);
    std::vector<float> in(n);
    uint32_t s = 3u;
    for (auto& x : in) {
        s = s * 1664525u + 1013904223u;
        x = 0.1f * float(int32_t(s)) / 2147483648.0f;
    }
    for (int v : {7, 8})
        for (int box : {1, 0})
            for (int sus : {1, 0}) {
                rv::Tank t;
                t.prepare(fs, 16);
                t.setTankVoicing(v);
                t.setOutputBitsVoicing(box);
                t.setSustainTrimEnabled(sus != 0);
                t.setParam(rv::ParamId::Attitude, 0.0f);
                t.setParam(rv::ParamId::Drive, 0.6f);
                t.setParam(rv::ParamId::Decay, 0.7f);
                t.setParam(rv::ParamId::Mix, 1.0f);
                std::vector<float> l(n), r(n);
                for (size_t p = 0; p < n; p += 16) {
                    if (p == (at / 16) * 16) t.setParam(rv::ParamId::Attitude, 1.0f);
                    t.process(in.data() + p, in.data() + p, l.data() + p, r.data() + p, 16);
                }
                auto pw = [&](size_t a, size_t b) {
                    double e = 0;
                    for (size_t i = a; i < b; ++i) e += double(l[i]) * l[i] + double(r[i]) * r[i];
                    return e;
                };
                const size_t w = size_t(0.2f * fs);
                std::printf("v%d box %d sus %d: gap %.2f dB | 10 ms:", v, box, sus, 10 * std::log10(pw(at, at + w) / pw(at - w, at)));
                for (int k = -3; k < 12; ++k) {
                    const size_t a = at + size_t(k * 480);
                    std::printf(" %.1f", 10 * std::log10(pw(a, a + 480) / pw(at - 2400, at)));
                }
                std::printf("\n");
            }
}
