// Round 5 fix-up probe: test_spring's sample-rate check (DECAY 0.5 tank IR, Schroeder T60 -5..-35 dB), broadband
// and per octave, at 48 / 96 kHz, tank voicings 7 and 8: which band moves with the rate.
//   c++ -O2 -std=c++17 -ffp-contract=off -Icore docs/prototypes/wellspring-fit-5/ratet60.cpp build-r/librv_core.a -o build-r/ratet60
#include "dsp/Tank.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::vector<float> ir(float fs, int v, float att)
{
    rv::Tank t;
    t.prepare(fs, 48);
    t.setTankVoicing(v);
    if (getenv("NOBOX")) t.setOutputBitsVoicing(0);
    t.setParam(rv::ParamId::Decay, 0.5f);
    t.setParam(rv::ParamId::Tension, 0.5f);
    t.setParam(rv::ParamId::Tone, 0.5f);
    t.setParam(rv::ParamId::Mix, 1.0f);
    if (att >= 0) t.setParam(rv::ParamId::Attitude, att);
    t.reset();
    const size_t n = size_t(4.0f * fs);
    std::vector<float> in(n, 0.0f), l(n), r(n), out(n);
    in[0] = getenv("IMP") ? float(atof(getenv("IMP"))) : 1.0f;
    for (size_t p = 0; p < n; p += 48) t.process(in.data() + p, in.data() + p, l.data() + p, r.data() + p, 48);
    for (size_t i = 0; i < n; ++i) out[i] = 0.5f * (l[i] + r[i]);
    return out;
}

static std::vector<float> band(const std::vector<float>& x, float fs, float f0)
{
    const double w = 2 * M_PI * f0 / fs, al = std::sin(w) / (2 * 1.41), a0 = 1 + al;
    const double b0 = al / a0, b2 = -al / a0, a1 = -2 * std::cos(w) / a0, a2 = (1 - al) / a0;
    std::vector<float> y(x.size());
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        const double v = b0 * x[i] + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = x[i], y2 = y1, y1 = v;
        y[i] = float(v);
    }
    return y;
}

static double t60(const std::vector<float>& x, float fs)
{
    std::vector<double> edc(x.size());
    double acc = 0;
    for (size_t i = x.size(); i-- > 0;) acc += double(x[i]) * x[i], edc[i] = acc;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (size_t i = 0; i < edc.size(); ++i) {
        const double db = 10.0 * std::log10(edc[i] / edc[0] + 1e-300);
        if (db > -5.0) continue;
        if (db < -35.0) break;
        const double t = double(i) / fs;
        sx += t, sy += db, sxx += t * t, sxy += t * db, ++n;
    }
    const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    return slope < 0 ? -60.0 / slope : 0;
}

int main()
{
    const float bands[] = {125, 250, 500, 1000, 1250, 2000, 4000, 8000};
    for (float att : {-1.0f, 0.0f, 0.5f, 1.0f})
    for (int v : {7, 8}) {
        for (float fs : {48000.0f, 96000.0f}) {
            const auto x = ir(fs, v, att);
            std::printf("att %.1f v%d %2.0fk broadband %.3f |", att, v, fs / 1000, t60(x, fs));
            for (float b : bands) std::printf(" %.0f:%.3f", b, t60(band(x, fs, b), fs));
            std::printf("\n");
        }
    }
}
