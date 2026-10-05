// Round 5 fix-up probe: test_drive's "DRIVE audibility" level-matched null (DRIVE 0 vs 0.5, DRIVEN, 02_hits,
// MIX 1, SPRINGS 2, DECAY 0.6, TENSION / TONE noon, SPLASH 0) for tank voicings 7 and 8 and a few round-5
// Tuning overrides, to see which part of round 5 hides DRIVE's colour.
//   c++ -O2 -std=c++17 -ffp-contract=off -Icore -Ihost/common docs/prototypes/wellspring-fit-5/nullprobe.cpp build-r/librv_core.a -o build-r/nullprobe
//   build-r/nullprobe   (from the repo root: reads test_audio/stimulus/02_hits.wav)
#include "Wav.h"
#include "dsp/Tank.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using Buf = std::vector<float>;

int main()
{
    rv::wav::Audio a;
    std::string err;
    if (!rv::wav::read("test_audio/stimulus/02_hits.wav", a, err)) return 1;
    const float fs = 48000.0f;
    const size_t n = a.frames() + size_t(4.0f * fs);
    Buf in(n, 0.0f);
    for (size_t i = 0; i < a.frames(); ++i) in[i] = a.channels[0][i];
    auto run = [&](int v, float drive, Buf& l, Buf& r) {
        rv::Tank t;
        t.prepare(fs, 48);
        t.setTankVoicing(v);
        t.setParam(rv::ParamId::Attitude, 0.5f);
        t.setParam(rv::ParamId::Drive, drive);
        t.setParam(rv::ParamId::Splash, 0.0f);
        t.setParam(rv::ParamId::Decay, 0.6f);
        t.setParam(rv::ParamId::Mix, 1.0f);
        l.assign(n, 0.0f);
        r.assign(n, 0.0f);
        for (size_t p = 0; p < n; p += 48) t.process(in.data() + p, in.data() + p, l.data() + p, r.data() + p, int(std::min<size_t>(48, n - p)));
    };
    bool hitsOnly = false; // the first 0.5 s after each hit (1 + 6 k s), not the tails
    auto inWin = [&](size_t i) {
        if (!hitsOnly) return true;
        const double t = double(i) / fs - 1.0;
        return t >= 0.0 && std::fmod(t, 6.0) < 0.5;
    };
    auto null = [&](int v) {
        Buf l0, r0, l1, r1;
        run(v, 0.0f, l0, r0);
        run(v, 0.5f, l1, r1);
        double pr = 0, px = 0, c = 0, d = 0;
        for (size_t i = 0; i < n; ++i) {
            if (!inWin(i)) continue;
            pr += double(l0[i]) * l0[i] + double(r0[i]) * r0[i];
            px += double(l1[i]) * l1[i] + double(r1[i]) * r1[i];
            c += double(l0[i]) * l1[i] + double(r0[i]) * r1[i];
        }
        const double g = c / px;
        for (size_t i = 0; i < n; ++i) {
            if (!inWin(i)) continue;
            const double el = g * l1[i] - l0[i], er = g * r1[i] - r0[i];
            d += el * el + er * er;
        }
        return 10.0 * std::log10(d / pr);
    };
    auto& t = rv::tankv::mutableTuning();
    const rv::tankv::Tuning keep = t;
    std::printf("7: %.2f dB\n8: %.2f dB\n", null(7), null(8));
    hitsOnly = true;
    std::printf("hits only (first 0.5 s of each): 7 %.2f dB, 8 %.2f dB\n", null(7), null(8));
    hitsOnly = false;
    struct Try {
        const char* name;
        std::function<void()> set;
    };
    const Try tries[] = {
        {"8, high path ratio 1.5 (7's)", [&] { t.r5HighT60Ratio = 1.5f; }},
        {"8, no 1.2 kHz cut", [&] { t.r5EqDb = 0.0f; t.r5EqDbBright = 0.0f; }},
        {"8, 7's coil", [&] { t.r5TdInHz = t.tdInHz; t.r5TdInQ = t.tdInQ; }},
        {"8, 7's diffusers coefficient", [&] { t.r5DiffCoeff = t.diffCoeff; }},
        {"8, 7's width", [&] { t.r5WideW = t.wideW; t.r5WideSide = t.wideSide; }},
        {"8, 7's low cut", [&] { t.r5LcHpHz = 155.0f; t.r5LcShelfDb = -2.0f; }},
        {"8, no noon trim", [&] { t.r5NoonTrimDb = 0.0f; }},
        {"8, side 0.5", [&] { t.r5WideSide = 0.5f; }},
        {"8, side 0.45", [&] { t.r5WideSide = 0.45f; }},
        {"8, cut -0.6 dB", [&] { t.r5EqDb = -0.6f; }},
        {"8, cut -0.5 dB", [&] { t.r5EqDb = -0.5f; }},
        {"8, DRIVEN push +2 dB", [&] { t.r5DrivenPushDb = 2.0f; }},
        {"8, DRIVEN push +4 dB", [&] { t.r5DrivenPushDb = 4.0f; }},
        {"8, cut -0.6, Q 1.2", [&] { t.r5EqDb = -0.6f; t.r5EqQ = 1.2f; }},
    };
    for (const auto& x : tries) {
        t = keep;
        x.set();
        std::printf("%s: %.2f dB\n", x.name, null(8));
    }
}
