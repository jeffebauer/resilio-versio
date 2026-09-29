// Drive chain + ATTITUDE + TONE tilt tests for M5 (SPEC §7 M5,
// docs/m5-contracts.md "Tests"). Dependency-free: prints PASS/FAIL lines,
// returns nonzero on any failure.
//
// Definitions used here:
//   0 dBFS ≈ 10 Vpp (docs/m5-contracts.md). "Typical" material = snare hits
//            peaking at -6 dBFS (as 02_hits' loudest hit).
//   loudness  ITU-R BS.1770 integrated loudness (K-weighting, 400 ms blocks,
//            75 % overlap, -70 LUFS absolute + -10 LU relative gate), stereo
//            = sum of both channels' mean squares. "LUFS-style".
//   THD       sqrt(sum of harmonics 2..10 power) / fundamental, steady 250 Hz
//            sine at -6 dBFS through the stage, DFT over whole periods.
//   alias     strongest spectral component 20 Hz–20 kHz that is not the
//            fundamental or one of its harmonics, relative to the
//            fundamental, stepped sines 5–15 kHz (every 500 Hz: a "sweep"
//            whose products we can separate), Blackman-Harris 32k FFT.
//   click     test_tank's click detector (second difference > 10x its local
//            ±10 ms RMS and > 1e-3).

#include "Wav.h"
#include "dsp/Drive.h"
#include "dsp/Oversampler.h"
#include "dsp/Tank.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <string>
#include <vector>

namespace {

int failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float kFs = 48000.0f;
const char* const kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};
float attValue(int a) { return rv::switchToNormalised(a); }

struct Settings {
    float decay = 0.6f, tension = 0.5f, tone = 0.5f, mix = 1.0f, drive = 0.5f;
    int   att = 1, springs = 1;
    // M7: < 0 = leave the ParamSpec default (SPLASH 0.3, WOBBLE 0.2).
    float splash = -1.0f, wobble = -1.0f;
};

void apply(rv::Tank& t, const Settings& s)
{
    t.setParam(rv::ParamId::Decay, s.decay);
    t.setParam(rv::ParamId::Tension, s.tension);
    t.setParam(rv::ParamId::Tone, s.tone);
    t.setParam(rv::ParamId::Mix, s.mix);
    t.setParam(rv::ParamId::Drive, s.drive);
    t.setParam(rv::ParamId::Attitude, attValue(s.att));
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(s.springs));
    if (s.splash >= 0.0f) t.setParam(rv::ParamId::Splash, s.splash);
    if (s.wobble >= 0.0f) t.setParam(rv::ParamId::Wobble, s.wobble);
}

struct Stereo {
    Buf l, r;
};

Stereo render(rv::Tank& t, const Buf& in, int block)
{
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int k = int(std::min(size_t(block), n - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
    }
    return o;
}

Stereo renderWith(const Settings& s, const Buf& in)
{
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, s);
    return render(t, in, 48);
}

Buf noise(size_t n, float amp, uint32_t seed)
{
    Buf b(n);
    rv::dsp::Rng rng;
    rng.seed(seed);
    for (auto& x : b) x = amp * rng.bipolar();
    return b;
}

Buf onePoleLp(Buf x, float hz)
{
    const float c = 1.0f - std::exp(-2.0f * rv::map::kPi * hz / kFs);
    float y = 0;
    for (auto& v : x) v = (y += c * (v - y));
    return x;
}
Buf onePoleHp(const Buf& x, float hz)
{
    Buf lp = onePoleLp(x, hz), y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = x[i] - lp[i];
    return y;
}

float peakAbs(const Buf& x)
{
    float p = 0;
    for (float v : x) p = std::max(p, std::fabs(v));
    return p;
}

// Synthetic snare (as test_tank / 02_hits): 185 Hz body + 800 Hz–7 kHz noise,
// `count` hits `spacing` s apart from 0.5 s, all peaking at `peak`.
Buf snareHits(size_t n, float peak, int count = 4, float spacing = 1.5f)
{
    Buf out(n, 0.0f);
    rv::dsp::Rng rng;
    rng.seed(1u);
    for (int h = 0; h < count; ++h) {
        const size_t len = size_t(0.25f * kFs), at = size_t(0.5f * kFs) + size_t(float(h) * spacing * kFs);
        Buf nz(len);
        for (auto& v : nz) v = rng.bipolar();
        nz = onePoleHp(onePoleLp(nz, 7000.0f), 800.0f);
        Buf hit(len);
        for (size_t i = 0; i < len; ++i) {
            const float t = float(i) / kFs;
            hit[i] = 0.6f * std::sin(2.0f * rv::map::kPi * 185.0f * t) * std::exp(-t / 0.03f)
                   + 1.2f * nz[i] * std::exp(-t / 0.06f);
        }
        const float p = peakAbs(hit);
        for (size_t i = 0; i < len && at + i < n; ++i) out[at + i] = hit[i] * peak / p;
    }
    return out;
}

double db(double p) { return 10.0 * std::log10(p + 1e-30); }
double power(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return to > from ? s / double(to - from) : 0.0;
}
bool allFinite(const Buf& x)
{
    for (float v : x)
        if (!std::isfinite(v)) return false;
    return true;
}

// ---- Loudness (BS.1770) ---------------------------------------------------------
Buf kWeight(const Buf& x)
{
    // 48 kHz coefficients from ITU-R BS.1770-4.
    const double b1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
    const double a1[3] = {1.0, -1.69065929318241, 0.73248077421585};
    const double b2[3] = {1.0, -2.0, 1.0};
    const double a2[3] = {1.0, -1.99004745483398, 0.99007225036621};
    Buf y(x.size());
    double s1 = 0, s2 = 0, t1 = 0, t2 = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        const double in = x[i];
        const double m  = b1[0] * in + s1;
        s1 = b1[1] * in - a1[1] * m + s2;
        s2 = b1[2] * in - a1[2] * m;
        const double o = b2[0] * m + t1;
        t1 = b2[1] * m - a2[1] * o + t2;
        t2 = b2[2] * m - a2[2] * o;
        y[i] = float(o);
    }
    return y;
}

double loudness(const std::vector<const Buf*>& chans)
{
    std::vector<Buf> k;
    for (const Buf* c : chans) k.push_back(kWeight(*c));
    const size_t block = size_t(0.4f * kFs), hop = block / 4, n = k[0].size();
    std::vector<double> z;
    for (size_t s = 0; s + block <= n; s += hop) {
        double sum = 0;
        for (const Buf& c : k) sum += power(c, s, s + block);
        z.push_back(sum);
    }
    auto gated = [&](double thresholdLufs) {
        double acc = 0;
        int cnt = 0;
        for (double v : z)
            if (-0.691 + db(v) > thresholdLufs) { acc += v; ++cnt; }
        return cnt ? acc / cnt : 0.0;
    };
    const double absMean = gated(-70.0);
    if (absMean <= 0) return -100.0;
    const double rel = -0.691 + db(absMean) - 10.0;
    return -0.691 + db(gated(std::max(-70.0, rel)));
}
double loudness(const Stereo& o) { return loudness({&o.l, &o.r}); }
double loudnessMono(const Buf& x) { return loudness({&x, &x}); }

// ---- Spectrum helpers -----------------------------------------------------------
void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * 3.14159265358979 / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k]           = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Power spectrum of x[from, from+N), 4-term Blackman-Harris (sidelobes -92 dB).
std::vector<double> spectrum(const Buf& x, size_t from, size_t N)
{
    std::vector<std::complex<double>> b(N);
    for (size_t i = 0; i < N; ++i) {
        const double t = 2.0 * 3.14159265358979 * double(i) / double(N);
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        b[i] = w * double(x[from + i]);
    }
    fft(b);
    std::vector<double> p(N / 2 + 1);
    for (size_t k = 0; k <= N / 2; ++k) p[k] = std::norm(b[k]);
    return p;
}

// Worst alias (dB re fundamental) of a steady sine f0 in x (20 Hz–20 kHz,
// excluding ±6 bins around every harmonic k·f0 < fs/2 and DC).
double worstAliasDb(const Buf& x, float f0, double* atHz)
{
    constexpr size_t N = 32768;
    const auto p = spectrum(x, x.size() - N, N);
    const double binHz = kFs / double(N);
    double worst = 0;
    for (size_t k = size_t(20.0 / binHz); k <= size_t(20000.0 / binHz); ++k) {
        const double hz = double(k) * binHz;
        bool harmonic = false;
        for (int h = 1; h * f0 < kFs / 2 + 7 * binHz; ++h)
            if (std::fabs(hz - h * f0) <= 6.5 * binHz) harmonic = true;
        if (harmonic) continue;
        if (p[k] > worst) {
            worst = p[k];
            if (atHz) *atHz = hz;
        }
    }
    // Compare like with like: the fundamental's peak bin vs the alias's peak bin.
    double fundPeak = 0;
    for (long k = long(f0 / binHz) - 3; k <= long(f0 / binHz) + 3; ++k) fundPeak = std::max(fundPeak, p[size_t(k)]);
    return db(worst / fundPeak);
}

// THD (harmonics 2..10) of a steady sine at f0 in x[from, from + n), n whole periods.
double thd(const Buf& x, size_t from, size_t n, float f0)
{
    auto amp2 = [&](float f) {
        double re = 0, im = 0;
        for (size_t i = 0; i < n; ++i) {
            const double ph = 2.0 * 3.14159265358979 * f * double(i) / kFs;
            re += x[from + i] * std::cos(ph);
            im += x[from + i] * std::sin(ph);
        }
        return re * re + im * im;
    };
    const double fund = amp2(f0);
    double h = 0;
    for (int k = 2; k <= 10 && k * f0 < kFs / 2; ++k) h += amp2(float(k) * f0);
    return std::sqrt(h / fund);
}

Buf sine(size_t n, float hz, float amp)
{
    Buf b(n);
    // Phase in double: in float, 2π·f·i/fs reaches ~1e5 rad within a second and
    // its rounding alone would put a noise floor near -60 dB.
    for (size_t i = 0; i < n; ++i)
        b[i] = float(amp * std::sin(2.0 * 3.14159265358979 * double(hz) * double(i) / double(kFs)));
    return b;
}

// A standalone DriveIn voiced exactly as the Tank voices it.
Buf driveInRender(int att, float drive, const Buf& in)
{
    std::array<float, 3> w{{0, 0, 0}};
    w[size_t(att)] = 1.0f;
    rv::dsp::DriveIn d;
    d.prepare(kFs);
    d.set(rv::dsp::driveInSettings(rv::dsp::blendVoice(w), drive), true, rv::Tank::kControlInterval);
    Buf out(in.size());
    for (size_t i = 0; i < in.size(); ++i) out[i] = d.process(in[i]);
    return out;
}

// Click detector as test_tank.
int countClicks(const Buf& x, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = size_t(0.010 * kFs);
    int clicks = 0;
    double worst = 0;
    for (size_t i = std::max<size_t>(from, half); i + half < n; ++i) {
        const double rms = std::sqrt((pre[i + half] - pre[i - half]) / double(2 * half));
        if (rms <= 0) continue;
        const double ratio = std::fabs(d2[i]) / rms;
        if (std::fabs(d2[i]) > 1e-3) worst = std::max(worst, ratio);
        if (ratio > 10.0 && std::fabs(d2[i]) > 1e-3) ++clicks;
    }
    if (maxRatio) *maxRatio = worst;
    return clicks;
}

// ---- 0. Building blocks ----------------------------------------------------------
void buildingBlocks()
{
    // Oversampler with nothing inside = allpass: |H| = 1 everywhere, and its
    // measured delay matches latencySamples() (which the Loop design uses).
    rv::dsp::Oversampler os;
    const size_t N = 8192;
    Buf ir(N);
    for (size_t i = 0; i < N; ++i) ir[i] = os.process(i == 0 ? 1.0f : 0.0f, [](float u) { return u; });
    std::vector<std::complex<double>> b(N);
    for (size_t i = 0; i < N; ++i) b[i] = ir[i];
    fft(b);
    double maxMag = 0, minMag = 9;
    for (size_t k = 1; k < N / 2; ++k) {
        maxMag = std::max(maxMag, std::abs(b[k]));
        minMag = std::min(minMag, std::abs(b[k]));
    }
    // Group delay at 1 kHz from the phase slope.
    const size_t k1 = size_t(1000.0 / (kFs / N));
    const double dph = std::arg(b[k1 + 1] / b[k1]);
    const double gd  = -dph / (2.0 * 3.14159265358979 / double(N));
    const double pred = rv::dsp::Oversampler::latencySamples(1000.0f, kFs);
    std::snprintf(msg, sizeof msg,
                  "Oversampler x%d, linear: |H| %.6f..%.6f (allpass), delay @1 kHz %.3f samples measured, %.3f "
                  "predicted (counted in the Loop round trip)",
                  rv::dsp::Oversampler::kFactor, minMag, maxMag, gd, pred);
    check(maxMag < 1.0 + 1e-4 && minMag > 1.0 - 1e-4 && std::fabs(gd - pred) < 0.05, msg);

    // Down-sampler rejects what it must: a 30 kHz tone at the doubled rate
    // (would alias to 18 kHz) is >= 80 dB down.
    if (rv::dsp::Oversampler::kFactor == 2) {
        rv::dsp::Halfband h;
        Buf out(N);
        double pin = 0, pout = 0;
        for (size_t i = 0; i < N; ++i) {
            const double t0 = double(2 * i) / (2 * kFs), t1 = double(2 * i + 1) / (2 * kFs);
            const float a = float(std::sin(2 * 3.14159265358979 * 30000.0 * t0));
            const float c = float(std::sin(2 * 3.14159265358979 * 30000.0 * t1));
            out[i] = h.down(a, c);
            if (i > N / 2) { pin += 0.5; pout += double(out[i]) * out[i]; }
        }
        std::snprintf(msg, sizeof msg, "Halfband down-sampler: 30 kHz at 2 fs rejected by %.1f dB (>= 80)",
                      -db(pout / pin));
        check(-db(pout / pin) >= 80.0, msg);
    }

    // Saturator slopes never exceed 1 (so a LoopSat can only lower Loop gain)
    // and are exactly 1 at 0.
    double maxSlope = 0;
    for (float x = -5.0f; x <= 5.0f; x += 0.001f) maxSlope = std::max(maxSlope, double(rv::dsp::softClipSlope(x)));
    bool asymOk = true;
    for (const auto& v : rv::drive::kVoice)
        for (float x = -3.0f; x < 3.0f; x += 0.001f) {
            const float h = 1e-3f;
            const float s = (rv::dsp::asymClip(x + h, v.loopKPos, v.loopKNeg) - rv::dsp::asymClip(x - h, v.loopKPos, v.loopKNeg)) / (2 * h);
            asymOk &= s <= 1.0f + 1e-3f;
        }
    std::snprintf(msg, sizeof msg, "Saturators: softClip slope max %.6f (limit 1, = 1 at 0: %.6f), LoopSat curves slope <= 1",
                  maxSlope, double(rv::dsp::softClipSlope(0.0f)));
    check(maxSlope <= 1.0 + 1e-6 && asymOk && rv::dsp::softClipSlope(0.0f) == 1.0f, msg);

    // Tape emphasis pair cancels exactly when the tape is off (CLEAN).
    rv::dsp::FirstOrder pre, de;
    pre.setHighShelf(3000.0f, 8.0f, 2 * kFs);
    de = pre;
    de.invert();
    const Buf nz = noise(4800, 0.5f, 3u);
    double err = 0;
    for (float x : nz) err = std::max(err, double(std::fabs(de.process(pre.process(x)) - x)));
    std::snprintf(msg, sizeof msg, "Tape de-emphasis is the exact inverse of pre-emphasis (max error %.1e)", err);
    check(err < 1e-5, msg);
}

// ---- 1. ATTITUDE loudness match -------------------------------------------------------
void attitudeLevels()
{
    const Buf in = snareHits(size_t(7.0f * kFs), 0.5f);
    for (float drive : {0.0f, 0.5f, 1.0f}) {
        double lev[3];
        for (int a = 0; a < 3; ++a) {
            Settings s;
            s.drive = drive;
            s.att   = a;
            lev[a]  = loudness(renderWith(s, in));
        }
        const double lo = std::min({lev[0], lev[1], lev[2]}), hi = std::max({lev[0], lev[1], lev[2]});
        std::snprintf(msg, sizeof msg,
                      "ATTITUDE loudness, DRIVE %.1f, -6 dBFS snare hits: CLEAN %.2f / DRIVEN %.2f / KICKED %.2f LUFS "
                      "(spread %.2f dB, limit +-2 -> 4)",
                      drive, lev[0], lev[1], lev[2], hi - lo);
        check(hi - lo <= 4.0 && std::fabs(lev[0] - lev[1]) <= 2.0 && std::fabs(lev[2] - lev[1]) <= 2.0, msg);
    }
}

// ---- 2. DRIVE sweep: level steady, THD rising -------------------------------------------
void driveSweep()
{
    const Buf in = snareHits(size_t(7.0f * kFs), 0.5f);
    const Buf tone = sine(size_t(1.5f * kFs), 250.0f, 0.5f);
    for (int a = 0; a < 3; ++a) {
        double lev[11], th[11];
        for (int i = 0; i <= 10; ++i) {
            Settings s;
            s.att   = a;
            s.drive = float(i) / 10.0f;
            lev[i]  = loudness(renderWith(s, in));
            const Buf y = driveInRender(a, s.drive, tone);
            th[i] = thd(y, size_t(0.5f * kFs), size_t(kFs), 250.0f);
        }
        double lo = 1e9, hi = -1e9, maxDev = 0;
        bool mono = true;
        for (int i = 0; i <= 10; ++i) {
            lo = std::min(lo, lev[i]);
            hi = std::max(hi, lev[i]);
            maxDev = std::max(maxDev, std::fabs(lev[i] - lev[0]));
            if (i > 0) mono &= th[i] >= th[i - 1] * 0.98;
        }
        std::snprintf(msg, sizeof msg,
                      "DRIVE 0->1 %s: loudness %.2f..%.2f LUFS (max %.2f dB from DRIVE 0, limit 2)", kAttName[a], lo, hi,
                      maxDev);
        check(maxDev <= 2.0, msg);
        std::snprintf(msg, sizeof msg,
                      "DRIVE 0->1 %s: THD rises monotonically: %.2f / %.2f / %.2f / %.2f / %.2f / %.2f %% at DRIVE 0 / "
                      ".25 / .5 / .75 / .85 / 1",
                      kAttName[a], 100 * th[0], 100 * th[2] * 0.5 + 100 * th[3] * 0.5, 100 * th[5],
                      100 * th[7] * 0.5 + 100 * th[8] * 0.5, 100 * th[8] * 0.5 + 100 * th[9] * 0.5, 100 * th[10]);
        check(mono && th[10] > th[0], msg);
        // ADR 0014 shape: clean-ish at 25 %, coloured by 85 %; CLEAN stays mild.
        const double th25 = 0.5 * (th[2] + th[3]), th85 = 0.5 * (th[8] + th[9]);
        bool shape = th25 < 0.02;
        if (a == 0) shape &= th[10] < 0.05;
        else shape &= th85 > 0.05 && th85 > 4 * th25;
        std::snprintf(msg, sizeof msg,
                      "DRIVE curve %s (ADR 0014): THD %.2f %% at 25 %% (clean-ish < 2), %.1f %% at 85 %% (%s), %.1f %% max",
                      kAttName[a], 100 * th25, 100 * th85, a == 0 ? "CLEAN: max < 5 %" : "coloured > 5 % and > 4x 25 %",
                      100 * th[10]);
        check(shape, msg);
    }
}

// ---- 2b. DRIVE audibility on 02_hits (ADR 0022) ------------------------------------------
// "Null difference" D(a, b) = energy of (out at DRIVE b - out at DRIVE a)
// over energy of out at DRIVE a, both channels, whole render (hits + 4 s
// tail), in dB. -20 dB = the change is a tenth of the signal's amplitude:
// clearly audible. Also reported level-matched (the DRIVE b render scaled
// by the gain that best matches DRIVE a): that part cannot be a loudness
// cue, so it shows the change is character. Settings as ADR 0022: MIX 1,
// SPRINGS 2, DECAY 0.6, TENSION 0.5, TONE 0.5.
bool readStimulus(const std::string& name, rv::wav::Audio& out)
{
    std::string error;
    for (const std::string& prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (rv::wav::read(prefix + name, out, error)) return true;
    std::fprintf(stderr, "could not read stimulus %s: %s\n", name.c_str(), error.c_str());
    return false;
}

void driveAudibility()
{
    rv::wav::Audio a;
    if (!readStimulus("02_hits.wav", a)) {
        check(false, "DRIVE audibility (ADR 0022): 02_hits.wav found");
        return;
    }
    const size_t n = a.frames() + size_t(4.0f * kFs);
    Buf il(n, 0.0f), ir(n, 0.0f);
    for (size_t i = 0; i < a.frames(); ++i) {
        il[i] = a.channels[0][i];
        ir[i] = a.channels[a.channels.size() > 1 ? 1 : 0][i];
    }
    auto run = [&](int att, float drive) {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.att = att;
        s.drive = drive;
        apply(t, s);
        Stereo o{Buf(n), Buf(n)};
        for (size_t pos = 0; pos < n; pos += 48) {
            const int k = int(std::min<size_t>(48, n - pos));
            t.process(il.data() + pos, ir.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
        }
        return o;
    };
    auto nullDb = [](const Stereo& ref, const Stereo& x, bool matched) {
        double pr = 0, px = 0, c = 0;
        for (const auto& [r, y] : {std::pair{&ref.l, &x.l}, std::pair{&ref.r, &x.r}})
            for (size_t i = 0; i < r->size(); ++i) {
                pr += double((*r)[i]) * (*r)[i];
                px += double((*y)[i]) * (*y)[i];
                c += double((*r)[i]) * (*y)[i];
            }
        const double g = matched && px > 0 ? c / px : 1.0;
        double d = 0;
        for (const auto& [r, y] : {std::pair{&ref.l, &x.l}, std::pair{&ref.r, &x.r}})
            for (size_t i = 0; i < r->size(); ++i) {
                const double e = g * (*y)[i] - (*r)[i];
                d += e * e;
            }
        return db(d / pr);
    };
    for (int att = 0; att < 3; ++att) {
        const float drives[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        Stereo o[5];
        double lev[5], lo = 1e9, hi = -1e9;
        for (int d = 0; d < 5; ++d) {
            o[d] = run(att, drives[d]);
            lev[d] = loudness(o[d]);
            lo = std::min(lo, lev[d]);
            hi = std::max(hi, lev[d]);
        }
        const double n25 = nullDb(o[0], o[1], false), n50 = nullDb(o[0], o[2], false), n100 = nullDb(o[0], o[4], false);
        const double m50 = nullDb(o[0], o[2], true), m100 = nullDb(o[0], o[4], true);
        bool ok = hi - lo <= 2.0;
        const char* want = "";
        if (att == 0) {
            ok &= n100 <= -15.0; // CLEAN: a gentle tint, never a drive
            want = "CLEAN stays mild: 0 vs 1 <= -15";
        } else {
            ok &= n50 >= -20.0 && m50 >= -20.0; // clearly coloured by noon, and not by loudness
            if (att == 2) ok &= n100 >= -6.0;   // cranked at max
            want = att == 1 ? "0 vs .5 >= -20" : "0 vs .5 >= -20, 0 vs 1 >= -6";
        }
        std::snprintf(msg, sizeof msg,
                      "DRIVE audibility %s, 02_hits (ADR 0022): null 0 vs .25 / .5 / 1 = %.1f / %.1f / %.1f dB "
                      "(level-matched .5 / 1: %.1f / %.1f; %s); loudness across DRIVE %.2f dB (limit 2)",
                      kAttName[att], n25, n50, n100, m50, m100, want, hi - lo);
        check(ok, msg);
    }
}

// ---- 2c. DRIVE sweet spot: no dead patch (M8, docs/m8-sweetspot.md) ----------------------
// The M8 sweet-spot report's test: DRIVE in 0.1 steps on 02_hits' first
// 10 s (MIX 0.5, SPRINGS 2, DECAY / TONE / TENSION noon, WOBBLE 0.2), mono
// sum; a step is audible if the null between neighbours is >= -40 dB or
// the RMS moves >= 0.5 dB. Dead patch = 3 or more silent steps in a row.
// SPLASH 0 here, so only DRIVE's own sound counts (at SPLASH 0.3 the
// Clatter moving with the driven level makes every step "audible").
// M7 build: CLEAN dead 0-0.4 and 0.5-1, DRIVEN 0-0.3 (clean-ish below
// ~9 o'clock is ADR 0014's intent: 2 silent steps are allowed).
void driveSweetSpot()
{
    rv::wav::Audio a;
    std::string error;
    bool loaded = false;
    for (const std::string& prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (!loaded) loaded = rv::wav::read(prefix + "02_hits.wav", a, error);
    if (!loaded) {
        std::printf("SKIP  DRIVE sweet spot: 02_hits.wav not found\n");
        return;
    }
    Buf in = a.channels[0];
    in.resize(size_t(10.0f * kFs));
    for (int att = 0; att < 3; ++att) {
        Buf m[11];
        for (int d = 0; d <= 10; ++d) {
            Settings s;
            s.att    = att;
            s.drive  = 0.1f * float(d);
            s.decay  = 0.5f;
            s.mix    = 0.5f;
            s.splash = 0.0f;
            s.wobble = 0.2f;
            const Stereo o = renderWith(s, in);
            m[d].resize(o.l.size());
            for (size_t i = 0; i < o.l.size(); ++i) m[d][i] = o.l[i] + o.r[i];
        }
        int run = 0, worst = 0;
        char steps[256] = {};
        for (int d = 1; d <= 10; ++d) {
            double e = 0, r = 0, pa = 0, pb = 0;
            for (size_t i = 0; i < m[d].size(); ++i) {
                const double x = double(m[d][i]) - m[d - 1][i];
                e += x * x;
                r += double(m[d - 1][i]) * m[d - 1][i];
                pb += double(m[d][i]) * m[d][i];
            }
            pa = r;
            const double nul = db(e / r), dl = std::fabs(db(pb / pa));
            const bool heard = nul >= -40.0 || dl >= 0.5;
            run = heard ? 0 : run + 1;
            worst = std::max(worst, run);
            std::snprintf(steps + std::strlen(steps), sizeof steps - std::strlen(steps), " %.0f", nul);
        }
        std::snprintf(msg, sizeof msg, "DRIVE sweet spot %s, 02_hits: step nulls (dB, 0.1 steps)%s; longest silent run %d (< 3)",
                      kAttName[att], steps, worst);
        check(worst < 3, msg);
    }
}

// ---- 2d. Wet level vs material: the excitation trim (M8, DriveVoicing.h) ---------------------
// Wet (MIX 1) minus dry RMS, whole stimulus, on 02_hits, 04_skank, held
// tones and steady pink-ish noise at -26 dBFS RMS: the Tank came back 5-6 dB
// louder on in-band material (skank, held tones) than on broadband (noise)
// or bright (hits). Spread across the four, per ATTITUDE, at DECAY 0.25 and
// 0.5: <= 3.5 dB (M7 build: 4.8-5.9 dB). Longer DECAYs are reported.
// Also: the trim holds while the input is silent (a tail is never trimmed).
//
// Held tones = 08_held_tones' notes (1 kHz, then an A minor chord; same
// levels) played at 12 pitches across a whole tone (-92..+92 cents), and
// the Tank's level is their power average: "held notes", not four exact
// frequencies. Why: a steady sine's level through a feedback Loop depends
// on where it lands between the Loop's narrow modes (peaks and dips
// ~16 Hz apart at 1 kHz, DECAY noon), so one fixed set of four sines is a
// lottery. Measured when the chirp direction flipped (HighsLater, M8): the
// file's exact pitches read -1.7 dB wet - dry at DECAY 0.5, the same notes
// 12 cents sharp +1.3 and 12 cents flat +8.5; the LowsLater tank swung as
// much (-2.6..+7.0 dB over +-50 cents), it just happened to land on peaks at
// DECAY <= 0.5 (and on dips at 0.75 / 1, which is why those were INFO).
// Averaged over pitch, held notes sit within ~1.5 dB of the other material
// in both directions. 4 s notes (not the file's 8 s) keep the cost down; the
// 12 renders run in parallel.
Buf heldTones(double cents)
{
    const double r = std::pow(2.0, cents / 1200.0);
    const double tone = 4.0, t1 = 0.5, t2 = t1 + tone + 1.0, end = t2 + tone + 2.0;
    Buf b(size_t(end * kFs), 0.0f);
    const size_t fade = size_t(0.01f * kFs);
    auto add = [&](double from, std::initializer_list<std::pair<double, double>> partials) {
        const size_t a = size_t(from * kFs), n = size_t(tone * kFs);
        for (size_t i = 0; i < n; ++i) {
            double v = 0;
            for (const auto& [hz, amp] : partials) v += amp * std::sin(2.0 * 3.14159265358979 * hz * r * double(i) / kFs);
            const size_t k = std::min(i, n - 1 - i);
            const double g = k < fade ? 0.5 - 0.5 * std::cos(3.14159265358979 * double(k) / double(fade)) : 1.0;
            b[a + i] = float(g * v);
        }
    };
    add(t1, {{1000.0, 0.2512}});                                 // 08_held_tones: 1 kHz at -12 dBFS peak
    add(t2, {{220.0, 0.0838}, {261.63, 0.0838}, {329.63, 0.0838}}); // then A minor, -21.5 dBFS each
    return b;
}

void wetLevelVsMaterial()
{
    std::vector<std::pair<const char*, Buf>> st;
    for (const char* f : {"02_hits.wav", "04_skank.wav"}) {
        rv::wav::Audio a;
        std::string error;
        bool loaded = false;
        for (const std::string& prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
            if (!loaded) loaded = rv::wav::read(prefix + f, a, error);
        if (!loaded) {
            std::printf("SKIP  wet level vs material: %s not found\n", f);
            return;
        }
        st.push_back({f, a.channels[0]});
    }
    {
        // Pink-ish noise: white through a -3 dB/oct approximation (Kellet), -26 dBFS RMS.
        Buf p(size_t(6.0f * kFs));
        rv::dsp::Rng rng;
        rng.seed(8u);
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, e = 0;
        for (auto& v : p) {
            const double w = rng.bipolar();
            b0 = 0.99886 * b0 + w * 0.0555179;
            b1 = 0.99332 * b1 + w * 0.0750759;
            b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856;
            b4 = 0.55000 * b4 + w * 0.5329522;
            b5 = -0.7616 * b5 - w * 0.0168980;
            v  = float(b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362);
            b6 = w * 0.115926;
            e += double(v) * v;
        }
        const float g = float(std::pow(10.0, -26.0 / 20.0) / std::sqrt(e / double(p.size())));
        for (auto& v : p) v *= g;
        st.push_back({"pink -26 dBFS", p});
    }
    constexpr int kPitches = 12;
    std::vector<Buf> held;
    for (int k = 0; k < kPitches; ++k) held.push_back(heldTones(-100.0 + 200.0 * (double(k) + 0.5) / double(kPitches)));
    for (float decay : {0.25f, 0.5f, 0.75f, 1.0f}) {
        char line[256] = {};
        bool ok = true;
        for (int att = 0; att < 3; ++att) {
            double lo = 1e9, hi = -1e9;
            Settings s;
            s.att   = att;
            s.decay = decay;
            s.drive = rv::spec(rv::ParamId::Drive).defaultValue;
            auto wetPower = [&s](const Buf& x) {
                const Stereo o = renderWith(s, x);
                return 0.5 * (power(o.l, 0, o.l.size()) + power(o.r, 0, o.r.size()));
            };
            for (const auto& [name, x] : st) {
                const double wd = db(wetPower(x)) - db(power(x, 0, x.size()));
                lo = std::min(lo, wd);
                hi = std::max(hi, wd);
            }
            {
                std::vector<std::future<double>> jobs;
                for (const Buf& x : held) jobs.push_back(std::async(std::launch::async, wetPower, std::cref(x)));
                double wet = 0, dry = 0;
                for (int k = 0; k < kPitches; ++k) {
                    wet += jobs[size_t(k)].get();
                    dry += power(held[size_t(k)], 0, held[size_t(k)].size());
                }
                const double wd = db(wet) - db(dry);
                lo = std::min(lo, wd);
                hi = std::max(hi, wd);
            }
            std::snprintf(line + std::strlen(line), sizeof line - std::strlen(line), " %s %.1f", kAttName[att], hi - lo);
            ok &= hi - lo <= 3.5;
        }
        if (decay <= 0.5f) {
            std::snprintf(msg, sizeof msg,
                          "Wet - dry spread across hits / skank / held tones / pink noise, DECAY %.2f (dB):%s (<= 3.5)",
                          decay, line);
            check(ok, msg);
        } else {
            std::printf("INFO  Wet - dry spread across hits / skank / held tones / pink noise, DECAY %.2f (dB):%s\n", decay, line);
        }
    }
    // The trim holds in silence: a hit, then 3 s of tail with no input.
    {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.decay = 1.0f;
        apply(t, s);
        Buf x = noise(size_t(4.0f * kFs), 0.0f, 1u);
        const Buf burst = noise(size_t(0.1f * kFs), 0.3f, 5u);
        std::copy(burst.begin(), burst.end(), x.begin());
        Buf l(48), r(48);
        float atEnd = 0.0f, lo = 1e9f, hi = -1e9f;
        for (size_t pos = 0; pos + 48 <= x.size(); pos += 48) {
            t.process(x.data() + pos, x.data() + pos, l.data(), r.data(), 48);
            if (pos == size_t(0.1f * kFs) / 48 * 48) atEnd = t.excitationTrim();
            if (pos > size_t(0.2f * kFs)) {
                lo = std::min(lo, t.excitationTrim());
                hi = std::max(hi, t.excitationTrim());
            }
        }
        std::snprintf(msg, sizeof msg,
                      "Excitation trim holds while the input is silent: %.2f dB at the end of a burst, %.2f .. %.2f dB over the "
                      "3.8 s tail (DECAY max; within 0.05 dB)",
                      20.0 * std::log10(atEnd), 20.0 * std::log10(lo), 20.0 * std::log10(hi));
        check(20.0 * std::log10(hi / lo) < 0.05 && std::fabs(20.0 * std::log10(hi / atEnd)) < 0.05, msg);
    }
}

// Level stays put on quiet sustained material too (ADR 0022: no loudness
// cue): steady noise ~ -25 dBFS RMS, a pad-like input that the saturators
// hardly squash, so a fixed makeup tuned on hits would make it louder.
void driveLevelHeld()
{
    const Buf in = noise(size_t(3.0f * kFs), 0.1f, 3u);
    for (int a = 0; a < 3; ++a) {
        double lev[6], lo = 1e9, hi = -1e9;
        for (int i = 0; i <= 5; ++i) {
            Settings s;
            s.att = a;
            s.drive = float(i) / 5.0f;
            const Stereo o = renderWith(s, in);
            lev[i] = db(power(o.l, size_t(1.5f * kFs), size_t(2.9f * kFs)));
            lo = std::min(lo, lev[i]);
            hi = std::max(hi, lev[i]);
        }
        std::snprintf(msg, sizeof msg,
                      "DRIVE 0->1 %s, steady noise at -25 dBFS RMS: level %.2f / %.2f / %.2f / %.2f / %.2f / %.2f dB "
                      "(spread %.2f, limit 2)",
                      kAttName[a], lev[0], lev[1], lev[2], lev[3], lev[4], lev[5], hi - lo);
        check(hi - lo <= 2.0, msg);
    }
}

// ---- 3. Reverb audible at DRIVE 0, 10 Vpp input ----------------------------------------
void audibleAtDriveZero()
{
    // 0 dBFS-peak snare hits (10 Vpp). Wet vs dry loudness: at MIX noon both
    // are scaled by the same sqrt(0.5), so compare the wet-only render with
    // the dry input.
    const Buf in = snareHits(size_t(7.0f * kFs), 1.0f);
    const double dry = loudnessMono(in);
    for (int a = 0; a < 3; ++a) {
        Settings s;
        s.att   = a;
        s.drive = 0.0f;
        s.decay = 0.5f;
        const double wet = loudness(renderWith(s, in));
        // Stereo loudness sums both channels; the dry reference is the same
        // mono signal on both sides, so the two are on the same scale.
        std::snprintf(msg, sizeof msg, "DRIVE 0, %s, 10 Vpp hits: wet %.2f LUFS vs dry %.2f LUFS (%+.2f dB, limit +-6)",
                      kAttName[a], wet, dry, wet - dry);
        check(std::fabs(wet - dry) <= 6.0, msg);
    }
}

// ---- 4. Aliasing ------------------------------------------------------------------------
// Pass = every product in 20 Hz–20 kHz that is not a harmonic is <= -60 dB
// re the fundamental, or below -100 dBFS absolute (under the Versio's
// converter noise floor: this only matters where the band-limit has already
// removed most of the tone, e.g. 15 kHz through KICKED's 5 kHz transducer).
struct AliasResult {
    double worst = -300, atHz = 0, fromHz = 0, absDb = -300;
    bool ok = true;
};

void accumulate(AliasResult& r, const Buf& y, float f0)
{
    double at = 0;
    const double rel = worstAliasDb(y, f0, &at);
    // Absolute level of that product: fundamental's level + rel.
    constexpr size_t N = 32768;
    const auto p = spectrum(y, y.size() - N, N);
    double fundPeak = 0;
    const double binHz = kFs / double(N);
    for (long k = long(f0 / binHz) - 3; k <= long(f0 / binHz) + 3; ++k) fundPeak = std::max(fundPeak, p[size_t(k)]);
    // Blackman-Harris coherent gain 0.35875, N/2 for a sine's peak bin.
    const double fundDbfs = db(fundPeak) - 20.0 * std::log10(0.35875 * double(N) / 2.0);
    const double absDb = fundDbfs + rel;
    const bool good = rel <= -60.0 || absDb <= -100.0;
    r.ok &= good;
    if (rel > r.worst) {
        r.worst = rel;
        r.atHz = at;
        r.fromHz = f0;
        r.absDb = absDb;
    }
}

Buf fadedSine(size_t n, float hz, float amp)
{
    Buf b = sine(n, hz, amp);
    const size_t fade = size_t(0.02f * kFs); // no broadband onset click
    for (size_t i = 0; i < fade; ++i) b[i] *= 0.5f - 0.5f * std::cos(3.14159265f * float(i) / float(fade));
    return b;
}

void aliasing()
{
    std::array<float, 3> kicked{{0, 0, 1}};
    const rv::drive::Voice v = rv::dsp::blendVoice(kicked);
    auto report = [](const char* what, const AliasResult& r) {
        std::snprintf(msg, sizeof msg,
                      "Aliasing %s: worst non-harmonic product %.1f dB re fundamental (%.0f Hz, from %.0f Hz; %.0f dBFS "
                      "abs). Limit -60 dB re fundamental or -100 dBFS",
                      what, r.worst, r.atHz, r.fromHz, r.absDb);
        check(r.ok, msg);
    };
    // DriveIn: the stage that sees the raw input. 5–15 kHz, every 500 Hz.
    for (float amp : {1.0f, 0.5f}) {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 500.0f) accumulate(r, driveInRender(2, 1.0f, sine(size_t(kFs), f0, amp)), f0);
        report(amp == 1.0f ? "DriveIn KICKED DRIVE 1, 5-15 kHz at 0 dBFS (10 Vpp)" : "DriveIn KICKED DRIVE 1, 5-15 kHz at -6 dBFS", r);
    }
    // The whole Tank (wet): what the listener hears. DECAY 0.3 so the tail
    // reaches steady state; analysed from 2.3 s. DRIVEN too at 0 dBFS
    // (since ADR 0022 its pre-gain reaches higher than KICKED's).
    // SPLASH 0 and WOBBLE 0 (M7): WOBBLE's pitch movement and KICKED's
    // energy-dependent rattle put modulation sidebands within ~10-20 Hz of
    // the tone (-41 dB at WOBBLE 0.2 / SPLASH 0.3 in KICKED). Those are
    // not aliasing; this measures the drive stages.
    {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 1000.0f) {
            Settings s;
            s.att = 1;
            s.drive = 1.0f;
            s.decay = 0.3f;
            s.tension = 1.0f;
            s.springs = 2;
            s.splash  = 0.0f;
            s.wobble  = 0.0f;
            accumulate(r, renderWith(s, fadedSine(size_t(3.0f * kFs), f0, 1.0f)).l, f0);
        }
        report("Tank wet DRIVEN DRIVE 1 (3 Springs), 5-15 kHz at 0 dBFS", r);
    }
    for (float amp : {1.0f, 0.5f}) {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 1000.0f) {
            Settings s;
            s.att = 2;
            s.drive = 1.0f;
            s.decay = 0.3f;
            s.tension = 1.0f;
            s.springs = 2;
            s.splash  = 0.0f;
            s.wobble  = 0.0f;
            const Stereo o = renderWith(s, fadedSine(size_t(3.0f * kFs), f0, amp));
            accumulate(r, o.l, f0);
        }
        report(amp == 1.0f ? "Tank wet KICKED DRIVE 1 (3 Springs), 5-15 kHz at 0 dBFS"
                           : "Tank wet KICKED DRIVE 1 (3 Springs), 5-15 kHz at -6 dBFS",
               r);
    }
    // LoopSat sees the Loop's own band (< fC ~4.4 kHz: the chirp low-pass is
    // inside the Loop), at a hot, Howl-level signal.
    {
        AliasResult r;
        for (float f0 = 1000.0f; f0 <= 4500.0f; f0 += 500.0f) {
            const Buf x = sine(size_t(kFs), f0, 0.5f);
            Buf y(x.size());
            rv::dsp::LoopSat ls;
            ls.set(v.loopAmount, v.loopKPos, v.loopKNeg);
            for (size_t i = 0; i < x.size(); ++i) y[i] = ls.process(x[i]);
            accumulate(r, y, f0);
        }
        report("LoopSat KICKED, 1-4.5 kHz at 0.5 peak", r);
    }
    // DriveOut: the wet output, 5–15 kHz at -6 dBFS.
    {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 500.0f) {
            const Buf x = sine(size_t(kFs), f0, 0.5f);
            Buf y(x.size());
            rv::dsp::DriveOut d;
            d.prepare(kFs);
            d.set(v, 1.0f);
            for (size_t i = 0; i < x.size(); ++i) y[i] = d.process(x[i]);
            accumulate(r, y, f0);
        }
        report("DriveOut KICKED DRIVE 1, 5-15 kHz at -6 dBFS", r);
    }
}

// ---- 5. TONE -------------------------------------------------------------------------------
double centroidSeconds(const Buf& x, size_t end)
{
    double num = 0, den = 0;
    for (size_t i = 0; i < std::min(end, x.size()); ++i) {
        const double e = double(x[i]) * x[i];
        num += e * double(i);
        den += e;
    }
    return den > 0 ? num / den / kFs : 0.0;
}
Buf bandpass(const Buf& x, float lo, float hi)
{
    const float f0 = std::sqrt(lo * hi), q = f0 / (hi - lo);
    const float w = 2.0f * rv::map::kPi * f0 / kFs, alpha = std::sin(w) / (2.0f * q), a0 = 1.0f + alpha;
    const float b0 = alpha / a0, b2 = -alpha / a0, a1 = -2.0f * std::cos(w) / a0, a2 = (1.0f - alpha) / a0;
    Buf y = x;
    for (int pass = 0; pass < 2; ++pass) {
        double s1 = 0, s2 = 0;
        for (auto& v : y) {
            const double in = v, out = b0 * in + s1;
            s1 = -a1 * out + s2;
            s2 = b2 * in - a2 * out;
            v  = float(out);
        }
    }
    return y;
}

void tone()
{
    // Chirp at full CCW, every ATTITUDE (same measure as test_spring: in the
    // first echo, the high band arrives before 200–500 Hz, or after it for HighsLater).
    for (int a = 0; a < 3; ++a)
        for (float tension : {0.0f, 1.0f}) {
            Settings s;
            s.decay = 0.5f;
            s.tension = tension;
            s.tone  = 0.0f;
            s.att   = a;
            s.springs = 0;
            Buf imp(size_t(0.5f * kFs), 0.0f);
            imp[0] = 1.0f;
            const Stereo o = renderWith(s, imp);
            Buf m(o.l.size());
            for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
            const float fC  = rv::map::tensionTransitionHz(tension);
            const size_t end = size_t(1.5f * rv::map::tensionLoopDelaySeconds(tension) * kFs);
            // High band per map::kChirpDirection, as test_spring.
            const float hiLo = rv::map::kHighsLater ? 0.65f : 0.5f, hiHi = rv::map::kHighsLater ? 0.95f : 0.85f;
            const double tHi = centroidSeconds(bandpass(m, hiLo * fC, hiHi * fC), end);
            const double tLo = centroidSeconds(bandpass(m, 200.0f, 500.0f), end);
            // Late band per map::kChirpDirection (lows for LowsLater).
            const double dir = rv::map::kHighsLater ? -1.0 : 1.0;
            std::snprintf(msg, sizeof msg, "TONE 0 chirp, %s TENSION %.0f: highs at %.1f ms, lows at %.1f ms (%s later)",
                          kAttName[a], tension, tHi * 1e3, tLo * 1e3, rv::map::kHighsLater ? "highs" : "lows");
            check(dir * (tLo - tHi) > 0.001, msg);
        }

    // Level across TONE and energy above 10 kHz at CW vs noon, snare hits.
    const Buf in = snareHits(size_t(7.0f * kFs), 0.5f);
    for (int a = 0; a < 3; ++a) {
        double lev[5], hf[5];
        for (int i = 0; i < 5; ++i) {
            Settings s;
            s.att  = a;
            s.tone = float(i) / 4.0f;
            const Stereo o = renderWith(s, in);
            lev[i] = loudness(o);
            // Energy above 10 kHz: 4th-order-ish high-pass (two one-pole HPs twice).
            Buf hl = onePoleHp(onePoleHp(onePoleHp(onePoleHp(o.l, 10000.0f), 10000.0f), 10000.0f), 10000.0f);
            Buf hr = onePoleHp(onePoleHp(onePoleHp(onePoleHp(o.r, 10000.0f), 10000.0f), 10000.0f), 10000.0f);
            hf[i] = db(power(hl, 0, hl.size()) + power(hr, 0, hr.size()));
        }
        double maxDev = 0;
        for (int i = 0; i < 5; ++i) maxDev = std::max(maxDev, std::fabs(lev[i] - lev[2]));
        std::snprintf(msg, sizeof msg,
                      "TONE 0/.25/.5/.75/1 %s: loudness %.1f / %.1f / %.1f / %.1f / %.1f LUFS (max %.2f dB from noon, "
                      "limit 3)",
                      kAttName[a], lev[0], lev[1], lev[2], lev[3], lev[4], maxDev);
        check(maxDev <= 3.0, msg);
        std::snprintf(msg, sizeof msg, "TONE CW %s: energy > 10 kHz %+.1f dB vs noon (limit +6; CCW %+.1f dB)",
                      kAttName[a], hf[4] - hf[2], hf[0] - hf[2]);
        check(hf[4] - hf[2] <= 6.0 && hf[4] > hf[2], msg);
    }
}

// ---- 6. Loop magnitude below 1 (AntiRes layer 1) ----------------------------------------
void loopMagnitude()
{
    int cells = 0, bad = 0;
    double worst = 0;
    char worstAt[96] = {};
    for (int a = 0; a < 3; ++a)
        for (float tn : {0.0f, 0.5f, 1.0f})
            for (float d : {0.0f, 0.5f, 0.89f, 1.0f})
                for (float b : {0.0f, 1.0f}) {
                    const bool howlZone = a == 2 && d >= rv::drive::kHowlZoneStart;
                    if (howlZone) continue;
                    Settings s;
                    s.att = a;
                    s.tone = tn;
                    s.decay = d;
                    s.tension = b;
                    s.springs = 2;
                    rv::Tank t;
                    t.prepare(kFs, 48);
                    apply(t, s);
                    render(t, Buf(4800, 0.0f), 48);
                    for (int sp = 0; sp < 3; ++sp) {
                        const rv::Spring& spr = t.spring(sp);
                        double peak = 0;
                        for (float hz = 10.0f; hz < 0.5f * kFs; hz *= 1.01f)
                            peak = std::max(peak, double(spr.feedbackGain() * spr.loopMagnitude(hz)));
                        peak = std::max(peak, double(spr.highFeedbackGain()));
                        ++cells;
                        if (peak >= 1.0) ++bad;
                        if (peak > worst) {
                            worst = peak;
                            std::snprintf(worstAt, sizeof worstAt, "%s TONE %.1f DECAY %.2f TENSION %.0f Spring %d",
                                          kAttName[a], tn, d, b, sp);
                        }
                    }
                }
    std::snprintf(msg, sizeof msg,
                  "Loop gain < 1 at every frequency, per Spring, ATTITUDE x TONE x DECAY x TENSION outside the Howl zone "
                  "(%d cells, worst %.4f at %s)",
                  cells, worst, worstAt);
    check(bad == 0, msg);

    // Inside the zone the peak is lifted above 1 (that is what lets it Howl).
    Settings s;
    s.att = 2;
    s.decay = 1.0f;
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, s);
    render(t, Buf(4800, 0.0f), 48);
    double peak = 0;
    for (float hz = 10.0f; hz < 0.5f * kFs; hz *= 1.01f)
        peak = std::max(peak, double(t.spring(0).feedbackGain() * t.spring(0).loopMagnitude(hz)));
    std::snprintf(msg, sizeof msg, "Howl zone (KICKED DECAY 1): small-signal Loop peak %.3f (> 1, target %.2f)", peak,
                  double(rv::drive::kHowlPeakGain));
    check(peak > 1.0 && peak < rv::drive::kHowlPeakGain + 0.01, msg);
}

// ---- 7. ATTITUDE Morph is click-free ---------------------------------------------------------
void morphClickFree()
{
    const size_t n = size_t(4.0f * kFs);
    const Buf held = noise(n, 0.1f, 3u);
    Buf hit = snareHits(n, 0.5f, 2, 1.0f);
    const size_t tailAt = size_t(2.0f * kFs); // 0.5 s into the second hit's tail
    for (int from = 0; from < 3; ++from)
        for (int to = 0; to < 3; ++to) {
            if (from == to) continue;
            int clicks = 0;
            double worst = 0, gapDb = 0;
            for (int input = 0; input < 2; ++input) {
                const Buf& in = input == 0 ? held : hit;
                const size_t at = input == 0 ? size_t(2.0f * kFs) : tailAt;
                rv::Tank t;
                t.prepare(kFs, 16);
                Settings s;
                s.att = from;
                s.drive = 0.6f;
                s.decay = 0.7f;
                apply(t, s);
                Stereo o{Buf(n), Buf(n)};
                for (size_t pos = 0; pos < n; pos += 16) {
                    if (pos == (at / 16) * 16) t.setParam(rv::ParamId::Attitude, attValue(to));
                    t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
                }
                for (const Buf* ch : {&o.l, &o.r}) {
                    double r = 0;
                    clicks += countClicks(*ch, at - size_t(0.1f * kFs), &r);
                    worst = std::max(worst, r);
                }
                if (input == 0) {
                    const size_t w = size_t(0.05f * kFs);
                    for (const Buf* ch : {&o.l, &o.r})
                        gapDb = std::max(gapDb, std::fabs(db(power(*ch, at, at + w) / power(*ch, at - w, at))));
                }
            }
            std::snprintf(msg, sizeof msg,
                          "ATTITUDE %s -> %s mid-tail (held + decaying, DRIVE 0.6): %d clicks (worst ratio %.1f, limit 10), "
                          "held level change across Morph %.1f dB (limit 2)",
                          kAttName[from], kAttName[to], clicks, worst, gapDb);
            check(clicks == 0 && gapDb < 2.0, msg);
        }

    // Morph really is gradual: weights move for >= 20 ms, never jump.
    rv::Tank t;
    t.prepare(kFs, 16);
    Settings s;
    s.att = 0;
    apply(t, s);
    Buf in(16, 0.0f), l(16), r(16);
    t.process(in.data(), in.data(), l.data(), r.data(), 16);
    t.setParam(rv::ParamId::Attitude, attValue(2));
    int blocks = 0;
    float maxStep = 0, prev = t.attitudeWeights()[2];
    while (t.attitudeWeights()[2] < 1.0f && blocks < 1000) {
        t.process(in.data(), in.data(), l.data(), r.data(), 16);
        maxStep = std::max(maxStep, t.attitudeWeights()[2] - prev);
        prev = t.attitudeWeights()[2];
        ++blocks;
    }
    const double ms = blocks * 16 / kFs * 1e3;
    std::snprintf(msg, sizeof msg, "ATTITUDE CLEAN -> KICKED Morph takes %.1f ms (>= 20), largest weight step per tick %.3f",
                  ms, maxStep);
    check(ms >= 20.0 && maxStep < 0.05f, msg);
}

// ---- 8. Stability grid + Howl zone -----------------------------------------------------------
void stabilityGrid()
{
    const size_t sec = size_t(kFs);
    int bad = 0, cells = 0;
    float worstPeak = 0;
    for (int a = 0; a < 3; ++a)
        for (float dr : {0.0f, 0.5f, 1.0f})
            for (float d : {0.0f, 0.5f, 0.89f, 1.0f})
                for (int m = 0; m < 3; ++m)
                    for (int input = 0; input < 2; ++input) {
                        const bool howl = a == 2 && d >= rv::drive::kHowlZoneStart;
                        Buf in;
                        if (input == 0) {
                            in.assign(6 * sec, 0.0f);
                            in[0] = 1.0f;
                        } else {
                            in = noise(6 * sec, 1.0f, 99u);
                            std::fill(in.begin() + long(sec), in.end(), 0.0f);
                        }
                        Settings s;
                        s.att = a;
                        s.drive = dr;
                        s.decay = d;
                        s.springs = m;
                        s.tension = 1.0f;
                        const Stereo o = renderWith(s, in);
                        const float pk = std::max(peakAbs(o.l), peakAbs(o.r));
                        worstPeak = std::max(worstPeak, pk);
                        bool good = allFinite(o.l) && allFinite(o.r) && pk < 1.0f;
                        if (!howl) {
                            // Energy after the input stops falls (+1 dB slack), and ends lower.
                            const size_t start = input == 0 ? sec / 2 : sec + sec / 2;
                            bool falls = true;
                            for (const Buf* ch : {&o.l, &o.r}) {
                                double prev = power(*ch, start, start + sec / 2);
                                for (size_t w = start + sec / 2; w + sec / 2 <= ch->size(); w += sec / 2) {
                                    const double e = power(*ch, w, w + sec / 2);
                                    if (e > prev * 1.26 && e > 1e-16) falls = false;
                                    prev = e;
                                }
                            }
                            good &= falls && power(o.l, 5 * sec, 6 * sec) < power(o.l, start, start + sec);
                        }
                        ++cells;
                        if (!good) {
                            ++bad;
                            std::printf("      grid fail: %s drive %.1f decay %.2f springs %d %s peak %.3f\n",
                                        kAttName[a], dr, d, m + 1, input ? "noise" : "impulse", pk);
                        }
                    }
    std::snprintf(msg, sizeof msg,
                  "Stability ATTITUDE x DRIVE x DECAY {0,.5,.89,1} x SPRINGS (%d cells, impulse + 1 s full-scale "
                  "noise, TENSION 1): finite, peak < 1 (worst %.3f), decaying outside the Howl zone (%d bad)",
                  cells, worstPeak, bad);
    check(bad == 0, msg);

    // CLEAN / DRIVEN at max DECAY decay (ADR 0001): 3 s after a noise burst
    // the level is well below where it started.
    for (int a = 0; a < 2; ++a) {
        Buf in = noise(10 * sec, 0.5f, 5u);
        std::fill(in.begin() + long(sec), in.end(), 0.0f);
        Settings s;
        s.att = a;
        s.decay = 1.0f;
        s.drive = 1.0f;
        s.springs = 2;
        const Stereo o = renderWith(s, in);
        const double e1 = db(power(o.l, 1 * sec + sec / 2, 2 * sec)), e9 = db(power(o.l, 9 * sec, 10 * sec));
        std::snprintf(msg, sizeof msg, "%s DECAY 1 DRIVE 1: tail falls %.1f dB from 1.5 s to 9.5 s (fades, ADR 0001)",
                      kAttName[a], e1 - e9);
        check(e1 - e9 > 25.0, msg);
    }
}

// Howl: sustains in the zone, stays below the limiter, exits naturally.
void howl()
{
    const size_t sec = size_t(kFs);
    for (int m = 0; m < 3; ++m) {
        const size_t n = 14 * sec, pullAt = 8 * sec;
        Buf in = snareHits(n, 0.5f, 1);
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.att = 2;
        s.decay = 1.0f;
        s.drive = 0.8f;
        s.springs = m;
        apply(t, s);
        Stereo o{Buf(n), Buf(n)};
        for (size_t pos = 0; pos < n; pos += 48) {
            if (pos == pullAt) t.setParam(rv::ParamId::Decay, 0.7f);
            t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
        }
        const double lev6 = db(0.5 * (power(o.l, 6 * sec, 7 * sec) + power(o.r, 6 * sec, 7 * sec)));
        const double lev3 = db(0.5 * (power(o.l, 3 * sec, 4 * sec) + power(o.r, 3 * sec, 4 * sec)));
        double pk = 0;
        for (size_t i = 3 * sec; i < pullAt; ++i) pk = std::max(pk, double(std::max(std::fabs(o.l[i]), std::fabs(o.r[i]))));
        const double after = db(0.5 * (power(o.l, pullAt + 3 * sec - sec / 4, pullAt + 3 * sec + sec / 4)
                                       + power(o.r, pullAt + 3 * sec - sec / 4, pullAt + 3 * sec + sec / 4)));
        std::snprintf(msg, sizeof msg,
                      "Howl KICKED DECAY 1 DRIVE 0.8, %d Spring%s: sustains (%.1f dBFS at 3-4 s, %.1f at 6-7 s), peak "
                      "%.2f (below limiter %.2f), DECAY -> 0.7: %.1f dB lower 3 s later (limit 30)",
                      m + 1, m ? "s" : "", lev3, lev6, pk, double(rv::Tank::kLimitThreshold), lev6 - after);
        check(lev6 > -40.0 && lev6 > lev3 - 6.0 && pk < 0.85 && lev6 - after >= 30.0, msg);

        // ADR 0019 (Howl zone, starting thresholds): rough, not a bare sine.
        // Broadband floor, read as an RTA would: 1/3-octave band levels
        // (energy per band) from 200 Hz to 5 kHz; the median band must be
        // within 25 dB of the strongest band (a bare sine: the median band is
        // ~100 dB down). Movement: over 2 s windows the strongest spectral
        // peak's frequency moves >= 0.5 % or its level >= 3 dB.
        // (M6 owns the final metric; thresholds are starting values.)
        constexpr size_t N = 16384;
        Buf mono(n);
        for (size_t i = 0; i < n; ++i) mono[i] = 0.5f * (o.l[i] + o.r[i]);
        std::vector<double> p(N / 2 + 1, 0.0);
        for (size_t w = 4 * sec; w + N <= pullAt; w += N / 2) { // average 4 s .. 8 s
            const auto q = spectrum(mono, w, N);
            for (size_t k = 0; k < p.size(); ++k) p[k] += q[k];
        }
        const double binHz = kFs / double(N);
        size_t pkBin = 1;
        for (size_t k = size_t(100 / binHz); k < size_t(8000 / binHz); ++k)
            if (p[k] > p[pkBin]) pkBin = k;
        std::vector<double> bands;
        for (double f = 200.0; f <= 5000.0; f *= std::pow(2.0, 1.0 / 3.0)) {
            double e = 0;
            for (size_t k = size_t(f / std::pow(2.0, 1.0 / 6.0) / binHz); k <= size_t(f * std::pow(2.0, 1.0 / 6.0) / binHz); ++k)
                e += p[k];
            bands.push_back(e);
        }
        const double strongest = *std::max_element(bands.begin(), bands.end());
        std::nth_element(bands.begin(), bands.begin() + long(bands.size() / 2), bands.end());
        const double floorDb = db(bands[bands.size() / 2]) - db(strongest);
        double fMin = 1e9, fMax = 0, lMin = 1e9, lMax = -1e9;
        for (size_t w = 3 * sec; w + N <= 7 * sec; w += sec / 4) {
            const auto q = spectrum(mono, w, N);
            size_t b = 1;
            for (size_t k = size_t(100 / binHz); k < size_t(8000 / binHz); ++k)
                if (q[k] > q[b]) b = k;
            // Parabolic interpolation for the peak frequency.
            const double ym = db(q[b - 1]), y0 = db(q[b]), yp = db(q[b + 1]);
            const double off = 0.5 * (ym - yp) / (ym - 2 * y0 + yp);
            const double hz = (double(b) + off) * binHz;
            fMin = std::min(fMin, hz);
            fMax = std::max(fMax, hz);
            lMin = std::min(lMin, y0);
            lMax = std::max(lMax, y0);
        }
        const double fDev = (fMax - fMin) / fMin * 100.0, lDev = lMax - lMin;
        std::snprintf(msg, sizeof msg,
                      "Howl %d Spring%s rough (ADR 0019): 1/3-oct median band %.1f dB re strongest (peak %.0f Hz; limit -25), "
                      "peak moves %.1f %% / %.1f dB over 3-7 s (limit 0.5 %% or 3 dB)",
                      m + 1, m ? "s" : "", floorDb, double(pkBin) * binHz, fDev, lDev);
        check(floorDb >= -25.0 && (fDev >= 0.5 || lDev >= 3.0), msg);
    }

    // Leaving via ATTITUDE (KICKED -> DRIVEN at DECAY 1): falls back to a
    // normal tail, which then fades at DECAY 1's T60 (reported).
    const size_t n = 12 * sec, flipAt = 6 * sec;
    Buf in = snareHits(n, 0.5f, 1);
    rv::Tank t;
    t.prepare(kFs, 48);
    Settings s;
    s.att = 2;
    s.decay = 1.0f;
    s.drive = 0.8f;
    apply(t, s);
    Stereo o{Buf(n), Buf(n)};
    for (size_t pos = 0; pos < n; pos += 48) {
        if (pos == flipAt) t.setParam(rv::ParamId::Attitude, attValue(1));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
    }
    const double before = db(power(o.l, flipAt - sec / 2, flipAt));
    const double a3 = db(power(o.l, flipAt + 3 * sec - sec / 4, flipAt + 3 * sec + sec / 4));
    const double a5 = db(power(o.l, flipAt + 5 * sec, flipAt + 5 * sec + sec / 2));
    std::snprintf(msg, sizeof msg,
                  "Howl exit via ATTITUDE KICKED -> DRIVEN at DECAY 1: %.1f dB lower after 3 s, %.1f dB after 5 s "
                  "(falls steadily; DECAY 1 T60 ~9 s)",
                  before - a3, before - a5);
    check(before - a3 > 10.0 && a5 < a3, msg);
}

// ---- Determinism with ATTITUDE / DRIVE / TONE moves -------------------------------------------
Buf detRender(rv::Tank& tank, const Buf& in, int block)
{
    Settings a;
    a.att = 0;
    a.drive = 0.2f;
    apply(tank, a);
    const size_t n = in.size(), c1 = 20000, c2 = 50000, c3 = 70000;
    Buf l(n), r(n), out(2 * n);
    size_t pos = 0;
    while (pos < n) {
        if (pos == c1) { tank.setParam(rv::ParamId::Attitude, attValue(2)); tank.setParam(rv::ParamId::Drive, 0.9f); }
        if (pos == c2) { tank.setParam(rv::ParamId::Attitude, attValue(1)); tank.setParam(rv::ParamId::Tone, 0.1f); }
        if (pos == c3) tank.setParam(rv::ParamId::Decay, 1.0f);
        size_t k = std::min(size_t(block), n - pos);
        for (size_t c : {c1, c2, c3})
            if (pos < c && pos + k > c) k = c - pos;
        tank.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, int(k));
        pos += k;
    }
    std::copy(l.begin(), l.end(), out.begin());
    std::copy(r.begin(), r.end(), out.begin() + long(n));
    return out;
}

void determinism()
{
    Buf in = snareHits(size_t(2.0f * kFs), 0.8f, 3, 0.5f);
    rv::Tank tank;
    tank.prepare(kFs, 512);
    const Buf first = detRender(tank, in, 48);
    tank.reset();
    check(detRender(tank, in, 48) == first,
          "Determinism: ATTITUDE / DRIVE / TONE / DECAY moves, same input twice (reset() between) is bit-identical");
    bool same = true;
    for (int block : {1, 7, 48, 512}) {
        rv::Tank t;
        t.prepare(kFs, 512);
        same &= detRender(t, in, block) == first;
    }
    check(same, "Determinism: ATTITUDE / DRIVE / TONE / DECAY moves, block sizes 1, 7, 48, 512 are bit-identical");
}

// ---- CPU and memory ------------------------------------------------------------------------
void performance()
{
    // Same method as M1/M4 (test_spring, test_tank): desktop ns/sample x
    // (15..25 x slower per sample on a Cortex-M7 @ 480 MHz) x 0.48 cycles/ns.
    // Worst-case knobs: DECAY / TENSION / TONE / DRIVE 1, MIX 0.5, noise in.
    // Best of 3 runs per cell (the desktop is shared with other processes).
    const size_t n = size_t(4.0f * kFs);
    const Buf in = noise(n, 0.3f, 5u);
    Buf l(n), r(n);
    std::printf("INFO  CPU estimate (Daisy cycles/sample, budget 10000, target <= 6500):\n");
    for (int a = 0; a < 3; ++a)
        for (int m = 0; m < 3; ++m) {
            double best = 1e9;
            for (int rep = 0; rep < 3; ++rep) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings s;
                s.decay = 1.0f;
                s.tension = 1.0f;
                s.tone = 1.0f;
                s.drive = 1.0f;
                s.mix = 0.5f;
                s.att = a;
                s.springs = m;
                apply(t, s);
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < n; pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n));
            }
            std::printf("INFO    %-6s %d Spring%s: %6.1f ns/sample desktop -> %5.0f-%5.0f cycles (%2.0f-%2.0f %%)\n",
                        kAttName[a], m + 1, m ? "s" : " ", best, best * 15 * 0.48, best * 25 * 0.48,
                        best * 15 * 0.48 / 100, best * 25 * 0.48 / 100);
        }
    // Drive chain alone: what the M5 stages add.
    {
        rv::dsp::DriveIn d;
        d.prepare(kFs);
        d.set(rv::dsp::driveInSettings(rv::dsp::blendVoice({{0, 0, 1}}), 1.0f), true, 32);
        rv::dsp::LoopSat ls;
        ls.set(1.0f, 1.6f, 2.6f);
        rv::dsp::DriveOut o;
        o.prepare(kFs);
        o.set(rv::dsp::blendVoice({{0, 0, 1}}), 1.0f);
        double sum = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (int rep = 0; rep < 4; ++rep)
            for (float x : in) sum += d.process(x);
        auto t1 = std::chrono::steady_clock::now();
        const double nsIn = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(4 * n);
        t0 = std::chrono::steady_clock::now();
        for (int rep = 0; rep < 4; ++rep)
            for (float x : in) sum += ls.process(x);
        t1 = std::chrono::steady_clock::now();
        const double nsLoop = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(4 * n);
        t0 = std::chrono::steady_clock::now();
        for (int rep = 0; rep < 4; ++rep)
            for (float x : in) sum += o.process(x);
        t1 = std::chrono::steady_clock::now();
        const double nsOut = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(4 * n);
        const double total = nsIn + 3 * nsLoop + 2 * nsOut;
        std::printf("INFO  M5 stages (x%d oversampling): DriveIn %.1f, LoopSat %.1f (x3 Springs), DriveOut %.1f (x2) ns/sample "
                    "-> %.1f ns = %.0f-%.0f Daisy cycles/sample (checksum %.3g)\n",
                    rv::dsp::Oversampler::kFactor, nsIn, nsLoop, nsOut, total, total * 15 * 0.48, total * 25 * 0.48, sum);
    }
    rv::Tank t;
    t.prepare(kFs, 48);
    std::printf("INFO  Tank memory: %zu bytes at 48 kHz (object %zu + pool)\n", t.memoryBytes(), sizeof(rv::Tank));
}

} // namespace

int main(int argc, char** argv)
{
    // Optional filter (development): run only tests whose name contains argv[1].
    const std::string only = argc > 1 ? argv[1] : "";
    struct T {
        const char* name;
        void (*fn)();
    };
    const T tests[] = {{"blocks", buildingBlocks}, {"loop", loopMagnitude},       {"attitude", attitudeLevels},
                       {"drive", driveSweep},      {"drive-audibility", driveAudibility},
                       {"drive-sweetspot", driveSweetSpot}, {"wet-level", wetLevelVsMaterial},
                       {"drive-held", driveLevelHeld}, {"audible", audibleAtDriveZero}, {"alias", aliasing},
                       {"tone", tone},             {"morph", morphClickFree},     {"determinism", determinism},
                       {"howl", howl},             {"stability", stabilityGrid},  {"performance", performance}};
    for (const T& t : tests)
        if (only.empty() || std::string(t.name).find(only) != std::string::npos) t.fn();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
