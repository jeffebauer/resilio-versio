// Wellspring fit round 5: measurements on stereo WAVs (the owner's Wellspring
// takes and our renders of the same stimulus). Stdlib C++ only (no numpy on
// this Mac), header-only WAV reader from host/common. Build:
//   clang++ -std=c++17 -O2 -I host/common docs/prototypes/wellspring-fit-5/r5an.cpp -o build-r/r5an
// Commands (every one prints one JSON object on stdout):
//   bursts <wav> [t0 period n]   15_tone_bursts / take J: per octave 125 Hz-8 kHz,
//                                 averaged over the two passes: when it peaks,
//                                 how fast it falls, front vs tail energy, T60,
//                                 L/R correlation and balance, front and tail.
//   steady <wav> t0 t1            09_pink_noise / take K: 1/3-octave levels,
//                                 octave L/R correlation and balance.
//   env <wav> t0 t1 win [stim]    short-time level (dB) every win s; with the
//                                 stimulus, its level and the gain too.
//   clicks <wav> t0 period n      01_clicks / take A: per octave, front (first
//                                 100 ms from arrival) and tail (0.2-1.5 s) level
//                                 and L/R correlation.
#include "Wav.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using Vec = std::vector<double>;
constexpr double kPi = 3.14159265358979323846;

struct BQ {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double run(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
BQ lowpass(double f, double q, double fs)
{
    const double w = 2 * kPi * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
    BQ b;
    b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
    return b;
}
BQ highpass(double f, double q, double fs)
{
    const double w = 2 * kPi * f / fs, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
    BQ b;
    b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = b.b0; b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
    return b;
}
// 4th-order Butterworth band (HP at lo, LP at hi).
Vec band(const Vec& x, double lo, double hi, double fs)
{
    std::vector<BQ> f;
    if (lo > 0) { f.push_back(highpass(lo, 0.5412, fs)); f.push_back(highpass(lo, 1.3066, fs)); }
    if (hi < 0.45 * fs) { f.push_back(lowpass(hi, 0.5412, fs)); f.push_back(lowpass(hi, 1.3066, fs)); }
    Vec y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        double v = x[i];
        for (auto& b : f) v = b.run(v);
        y[i] = v;
    }
    return y;
}

struct Stereo {
    Vec l, r;
    double fs = 48000;
};
Stereo load(const char* path)
{
    rv::wav::Audio a;
    std::string err;
    if (!rv::wav::read(path, a, err)) { std::fprintf(stderr, "%s: %s\n", path, err.c_str()); std::exit(1); }
    Stereo s;
    s.fs = a.sampleRate;
    s.l.assign(a.channels[0].begin(), a.channels[0].end());
    const auto& rc = a.channels.size() > 1 ? a.channels[1] : a.channels[0];
    s.r.assign(rc.begin(), rc.end());
    return s;
}

double db10(double p) { return 10.0 * std::log10(std::max(p, 1e-30)); }

double corr(const Vec& a, const Vec& b, size_t i0, size_t i1)
{
    double ab = 0, aa = 0, bb = 0;
    i1 = std::min(i1, a.size());
    for (size_t i = i0; i < i1; ++i) { ab += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
    return ab / std::sqrt(std::max(aa * bb, 1e-60));
}
double energy(const Vec& a, size_t i0, size_t i1)
{
    double e = 0;
    i1 = std::min(i1, a.size());
    for (size_t i = i0; i < i1; ++i) e += a[i] * a[i];
    return e;
}

// T60 from the Schroeder curve of frame energies (1 ms frames) from frame
// f0: fit between -5 and -5-span dB (span 20: T20 x 3), noise subtracted.
double t60(const Vec& fr, size_t f0, double frameS, double span)
{
    // noise: mean of the last 10 % of frames
    const size_t n = fr.size();
    size_t nn = std::max<size_t>(1, n / 10);
    double noise = 0;
    for (size_t i = n - nn; i < n; ++i) noise += fr[i];
    noise /= double(nn);
    Vec e(n, 0.0);
    double acc = 0;
    for (size_t i = n; i-- > f0;) {
        acc += std::max(0.0, fr[i] - noise);
        e[i] = acc;
    }
    const double top = e[f0];
    if (top <= 0) return 0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int k = 0;
    for (size_t i = f0; i < n; ++i) {
        const double d = db10(e[i] / top);
        if (d > -5.0) continue;
        if (d < -5.0 - span) break;
        const double t = double(i - f0) * frameS;
        sx += t; sy += d; sxx += t * t; sxy += t * d; ++k;
    }
    if (k < 5) return 0;
    const double slope = (k * sxy - sx * sy) / (k * sxx - sx * sx);
    return slope < 0 ? -60.0 / slope : 0;
}

const double kOct[7] = {125, 250, 500, 1000, 2000, 4000, 8000};

int cmdBursts(int argc, char** argv)
{
    Stereo s = load(argv[2]);
    const double t0 = argc > 3 ? std::atof(argv[3]) : 1.0;
    const double period = argc > 4 ? std::atof(argv[4]) : 5.06;
    const int nb = argc > 5 ? std::atoi(argv[5]) : 14;
    const double fs = s.fs;
    const size_t fl = size_t(fs * 0.001); // 1 ms frames
    std::printf("{\"bands\":[");
    for (int bi = 0; bi < 7; ++bi) {
        const double f = kOct[bi];
        const Vec bl = band(s.l, f / std::sqrt(2.0), f * std::sqrt(2.0), fs);
        const Vec br = band(s.r, f / std::sqrt(2.0), f * std::sqrt(2.0), fs);
        double acc[13] = {0};
        int passes = 0;
        for (int k = bi; k < nb; k += 7) {
            const size_t on = size_t((t0 + k * period) * fs);
            const size_t len = size_t(std::min(period - 0.07, 4.95) * fs);
            if (on + len > bl.size()) continue;
            const size_t nf = len / fl;
            Vec fr(nf), sm(nf);
            for (size_t j = 0; j < nf; ++j) fr[j] = energy(bl, on + j * fl, on + (j + 1) * fl) + energy(br, on + j * fl, on + (j + 1) * fl);
            // 10 ms smoothing for peak / fall
            for (size_t j = 0; j < nf; ++j) {
                double a = 0; int c = 0;
                for (size_t q = j >= 5 ? j - 5 : 0; q < std::min(nf, j + 5); ++q) { a += fr[q]; ++c; }
                sm[j] = a / c;
            }
            size_t pk = 0;
            for (size_t j = 0; j < nf; ++j) if (sm[j] > sm[pk]) pk = j;
            const double pkDb = db10(sm[pk]);
            size_t arr = 0;
            while (arr < pk && db10(sm[arr]) < pkDb - 20.0) ++arr;
            size_t fall = pk;
            while (fall < nf && db10(sm[fall]) > pkDb - 20.0) ++fall;
            // front: arrival..+100 ms; tail mean power 100..500 ms after arrival
            const size_t a0 = on + arr * fl;
            const double eFront = energy(bl, a0, a0 + size_t(0.1 * fs)) + energy(br, a0, a0 + size_t(0.1 * fs));
            const double eNext  = energy(bl, a0 + size_t(0.1 * fs), a0 + size_t(0.5 * fs)) + energy(br, a0 + size_t(0.1 * fs), a0 + size_t(0.5 * fs));
            const double frontRatio = db10(eFront / 0.1) - db10(eNext / 0.4);
            const double tA = t60(fr, arr, 0.001, 20.0);
            const double edt = t60(fr, arr, 0.001, 10.0);
            const double cF = corr(bl, br, a0, a0 + size_t(0.1 * fs));
            const double cT = corr(bl, br, a0 + size_t(0.2 * fs), a0 + size_t(1.5 * fs));
            const double bF = db10(energy(bl, a0, a0 + size_t(0.1 * fs)) / std::max(1e-30, energy(br, a0, a0 + size_t(0.1 * fs))));
            const double bA = db10(energy(bl, a0, a0 + size_t(2.0 * fs)) / std::max(1e-30, energy(br, a0, a0 + size_t(2.0 * fs))));
            const double frontDb = db10(eFront);
            // gappiness: spread (sd, dB) of the 10 ms envelope from arrival + 60 ms to + 500 ms
            double gs = 0, gss = 0;
            int gn = 0;
            for (size_t j = arr + 60; j + 10 <= std::min(nf, arr + 500); j += 10) {
                double e = 0;
                for (size_t q = j; q < j + 10; ++q) e += fr[q];
                const double d = db10(e);
                gs += d; gss += d * d; ++gn;
            }
            const double gap = gn > 1 ? std::sqrt(std::max(0.0, gss / gn - (gs / gn) * (gs / gn))) : 0.0;
            const double tailDb = db10(energy(bl, a0 + size_t(0.2 * fs), a0 + size_t(1.5 * fs)) + energy(br, a0 + size_t(0.2 * fs), a0 + size_t(1.5 * fs)));
            const double v[13] = {double(pk), double(fall - pk), frontRatio, tA, edt, cF, cT, bF, bA, frontDb, tailDb, double(arr), gap};
            for (int q = 0; q < 13; ++q) acc[q] += v[q];
            ++passes;
        }
        if (passes == 0) passes = 1;
        for (double& q : acc) q /= passes;
        std::printf("%s{\"hz\":%g,\"peak_ms\":%.0f,\"fall20_ms\":%.0f,\"front_ratio_db\":%.2f,\"t60\":%.3f,\"edt\":%.3f,"
                    "\"corr_front\":%.3f,\"corr_tail\":%.3f,\"bal_front_db\":%.2f,\"bal_db\":%.2f,\"front_db\":%.2f,\"tail_db\":%.2f,\"arrival_ms\":%.0f,\"gap_db\":%.2f}",
                    bi ? "," : "", f, acc[0], acc[1], acc[2], acc[3], acc[4], acc[5], acc[6], acc[7], acc[8], acc[9], acc[10], acc[11], acc[12]);
    }
    std::printf("]}\n");
    return 0;
}

int cmdSteady(int argc, char** argv)
{
    if (argc < 5) return 2;
    Stereo s = load(argv[2]);
    const double fs = s.fs;
    const size_t i0 = size_t(std::atof(argv[3]) * fs), i1 = std::min(s.l.size(), size_t(std::atof(argv[4]) * fs));
    const double thirds[] = {63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000};
    std::printf("{\"thirds\":[");
    bool first = true;
    for (double f : thirds) {
        const double k = std::pow(2.0, 1.0 / 6.0);
        const Vec bl = band(s.l, f / k, f * k, fs), br = band(s.r, f / k, f * k, fs);
        const double p = 0.5 * (energy(bl, i0, i1) + energy(br, i0, i1)) / double(i1 - i0);
        std::printf("%s[%g,%.2f]", first ? "" : ",", f, db10(p));
        first = false;
    }
    std::printf("],\"oct\":[");
    const double octs[] = {63, 125, 250, 500, 1000, 2000, 4000, 8000};
    first = true;
    for (double f : octs) {
        const Vec bl = band(s.l, f / std::sqrt(2.0), f * std::sqrt(2.0), fs), br = band(s.r, f / std::sqrt(2.0), f * std::sqrt(2.0), fs);
        std::printf("%s[%g,%.3f,%.2f]", first ? "" : ",", f, corr(bl, br, i0, i1), db10(energy(bl, i0, i1) / energy(br, i0, i1)));
        first = false;
    }
    std::printf("]}\n");
    return 0;
}

int cmdEnv(int argc, char** argv)
{
    if (argc < 6) return 2;
    Stereo s = load(argv[2]);
    const double fs = s.fs, t0 = std::atof(argv[3]), t1 = std::atof(argv[4]), win = std::atof(argv[5]);
    Stereo st;
    const bool hasStim = argc > 6;
    if (hasStim) st = load(argv[6]);
    std::printf("{\"env\":[");
    bool first = true;
    for (double t = t0; t + win <= t1 + 1e-9; t += win) {
        const size_t a = size_t(t * fs), b = size_t((t + win) * fs);
        const double p = 0.5 * (energy(s.l, a, b) + energy(s.r, a, b)) / double(b - a);
        double pi = 0;
        if (hasStim) pi = 0.5 * (energy(st.l, a, b) + energy(st.r, a, b)) / double(b - a);
        std::printf("%s[%.3f,%.2f,%.2f]", first ? "" : ",", t, db10(p), hasStim ? db10(pi) : 0.0);
        first = false;
    }
    std::printf("]}\n");
    return 0;
}

int cmdClicks(int argc, char** argv)
{
    Stereo s = load(argv[2]);
    const double t0 = argc > 3 ? std::atof(argv[3]) : 1.0;
    const double period = argc > 4 ? std::atof(argv[4]) : 8.0;
    const int n = argc > 5 ? std::atoi(argv[5]) : 6;
    const double fs = s.fs;
    // arrival: first sample of the broadband wet above -30 dB re its peak, per click
    std::printf("{\"bands\":[");
    const double octs[] = {63, 125, 250, 500, 1000, 2000, 4000, 8000};
    bool first = true;
    for (double f : octs) {
        const Vec bl = band(s.l, f / std::sqrt(2.0), f * std::sqrt(2.0), fs), br = band(s.r, f / std::sqrt(2.0), f * std::sqrt(2.0), fs);
        double fr = 0, ta = 0, cf = 0, ct = 0, nx = 0;
        for (int k = 0; k < n; ++k) {
            const size_t on = size_t((t0 + k * period) * fs);
            const size_t a0 = on + size_t(0.02 * fs); // the first echo comes after ~28-35 ms
            fr += energy(bl, a0, a0 + size_t(0.1 * fs)) + energy(br, a0, a0 + size_t(0.1 * fs));
            nx += energy(bl, a0 + size_t(0.1 * fs), a0 + size_t(0.5 * fs)) + energy(br, a0 + size_t(0.1 * fs), a0 + size_t(0.5 * fs));
            ta += energy(bl, on + size_t(0.2 * fs), on + size_t(1.5 * fs)) + energy(br, on + size_t(0.2 * fs), on + size_t(1.5 * fs));
            cf += corr(bl, br, a0, a0 + size_t(0.1 * fs));
            ct += corr(bl, br, on + size_t(0.2 * fs), on + size_t(1.5 * fs));
        }
        // front_ratio: the first 100 ms's mean power over the next 400 ms's (dB)
        std::printf("%s{\"hz\":%g,\"front_db\":%.2f,\"tail_db\":%.2f,\"corr_front\":%.3f,\"corr_tail\":%.3f,\"front_ratio_db\":%.2f}",
                    first ? "" : ",", f, db10(fr / n), db10(ta / n), cf / n, ct / n, db10(fr / 0.1) - db10(nx / 0.4));
        first = false;
    }
    std::printf("]}\n");
    return 0;
}

// benv <wav> onset_s band_hz frame_ms n: one burst's band envelope (dB, L+R), and L / R separately.
int cmdBenv(int argc, char** argv)
{
    if (argc < 7) return 2;
    Stereo s = load(argv[2]);
    const double fs = s.fs, on = std::atof(argv[3]), f = std::atof(argv[4]), fm = std::atof(argv[5]);
    const int n = std::atoi(argv[6]);
    const Vec bl = band(s.l, f / std::sqrt(2.0), f * std::sqrt(2.0), fs), br = band(s.r, f / std::sqrt(2.0), f * std::sqrt(2.0), fs);
    const size_t fl = size_t(fm * 0.001 * fs), i0 = size_t(on * fs);
    std::printf("{\"lr\":[");
    for (int j = 0; j < n; ++j) {
        const double el = energy(bl, i0 + j * fl, i0 + (j + 1) * fl), er = energy(br, i0 + j * fl, i0 + (j + 1) * fl);
        std::printf("%s[%.1f,%.1f,%.1f]", j ? "," : "", db10(el + er), db10(el), db10(er));
    }
    std::printf("]}\n");
    return 0;
}

// sweepdist <wav> [t0 T]: the 20 Hz-20 kHz exponential sweep (make_stimulus.py
// sweep(): starts at t0 = 1 s, T = 10 s). While it plays f = 200 .. 800 Hz, the
// energy in 1.1-1.8 kHz (above every harmonic-free part of the sweep so far:
// only distortion products and noise land there) re the energy around f
// (f / 1.2 .. f x 1.2), 60 ms windows, dB. Also the 2nd and 3rd harmonics
// (bands around 2f, 3f) re f.
int cmdSweepDist(int argc, char** argv)
{
    Stereo s = load(argv[2]);
    const double t0 = argc > 3 ? std::atof(argv[3]) : 1.0, T = argc > 4 ? std::atof(argv[4]) : 10.0;
    const double fs = s.fs, k = std::log(1000.0);
    Vec m(s.l.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5 * (s.l[i] + s.r[i]);
    const Vec hb = band(m, 1100.0, 1800.0, fs);
    std::printf("{\"dist\":[");
    const double fsv[] = {200, 283, 400, 566, 800};
    bool first = true;
    for (double f : fsv) {
        const double t = t0 + T * std::log(f / 20.0) / k;
        const size_t a = size_t((t - 0.03) * fs), b = size_t((t + 0.03) * fs);
        const Vec fb = band(m, f / 1.2, f * 1.2, fs), h2 = band(m, 2 * f / 1.12, 2 * f * 1.12, fs), h3 = band(m, 3 * f / 1.12, 3 * f * 1.12, fs);
        const double ef = energy(fb, a, b);
        std::printf("%s[%g,%.1f,%.1f,%.1f]", first ? "" : ",", f, db10(energy(hb, a, b) / ef), db10(energy(h2, a, b) / ef),
                    db10(energy(h3, a, b) / ef));
        first = false;
    }
    std::printf("]}\n");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: r5an bursts|steady|env|clicks <wav> ...\n"); return 2; }
    const std::string c = argv[1];
    if (c == "bursts") return cmdBursts(argc, argv);
    if (c == "steady") return cmdSteady(argc, argv);
    if (c == "env") return cmdEnv(argc, argv);
    if (c == "clicks") return cmdClicks(argc, argv);
    if (c == "benv") return cmdBenv(argc, argv);
    if (c == "sweepdist") return cmdSweepDist(argc, argv);
    return 2;
}
