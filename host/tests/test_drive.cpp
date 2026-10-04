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
#include "params/SpringModes.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"
#include "params/ThrowHold.h"

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
    // M7: < 0 = leave the ParamSpec default (SPLASH 0.3, WOBBLE 0.45: a touch of shared Drift).
    float splash = -1.0f, wobble = -1.0f;
    int   toneVoicing = rv::drive::kToneDefaultVoicing; // Big Knob (Renderer-only key)
};

void apply(rv::Tank& t, const Settings& s)
{
    t.setParam(rv::ParamId::Decay, s.decay);
    t.setParam(rv::ParamId::Tension, s.tension);
    t.setParam(rv::ParamId::Tone, s.tone);
    t.setParam(rv::ParamId::Mix, s.mix);
    t.setParam(rv::ParamId::Drive, s.drive);
    t.setParam(rv::ParamId::Attitude, attValue(s.att));
    // SPRINGS 3 here is the three-Spring reference (setEchoMode(false), Renderer-only since
    // ADR 0041): these checks hold the Springs to their bars; echo mode has test_echo_mode.
    t.setEchoMode(false);
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(s.springs));
    if (s.splash >= 0.0f) t.setParam(rv::ParamId::Splash, s.splash);
    if (s.wobble >= 0.0f) t.setParam(rv::ParamId::Wobble, s.wobble);
    if (s.toneVoicing != rv::drive::kToneDefaultVoicing) t.setToneVoicing(s.toneVoicing);
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

// outBits: output_bits_voicing (-1 = the default, ADR 0042's mu-law box; 0 = before the box, a test hook).
Stereo renderWith(const Settings& s, const Buf& in, int outBits = -1)
{
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, s);
    if (outBits >= 0) t.setOutputBitsVoicing(outBits);
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

// The level DRIVE adds on purpose (ADR 0033, "partly louder"): the INPUT
// gain's heard share, dB. 0 at DRIVE 0, +6 dB at DRIVE 1, the same in every
// ATTITUDE. The DRIVE loudness gates below allow +-2 dB around this curve
// (the M5 / ADR 0022 +-2 dB, now around the intended rise instead of flat).
double heardDb(float drive) { return rv::drive::kInputHeard * rv::drive::inputGainDb(drive); }
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
// SPLASH 0 (the drive chain's own level, M5): since ADR 0032 SPLASH's Bite
// pushes drum hits harder into DRIVEN / KICKED's transducer on purpose (a
// few dB louder hits there than in CLEAN, which only clangs), printed as INFO
// at the default SPLASH.
void attitudeLevels()
{
    const Buf in = snareHits(size_t(7.0f * kFs), 0.5f);
    for (float drive : {0.0f, 0.5f, 1.0f}) {
        double lev[3], dflt[3];
        for (int a = 0; a < 3; ++a) {
            Settings s;
            s.drive = drive;
            s.att   = a;
            dflt[a] = loudness(renderWith(s, in));
            s.splash = 0.0f;
            lev[a]  = loudness(renderWith(s, in));
        }
        std::printf("INFO  ATTITUDE loudness, DRIVE %.1f, default SPLASH: CLEAN %.2f / DRIVEN %.2f / KICKED %.2f LUFS\n", drive,
                    dflt[0], dflt[1], dflt[2]);
        const double lo = std::min({lev[0], lev[1], lev[2]}), hi = std::max({lev[0], lev[1], lev[2]});
        std::snprintf(msg, sizeof msg,
                      "ATTITUDE loudness, DRIVE %.1f, -6 dBFS snare hits, SPLASH 0: CLEAN %.2f / DRIVEN %.2f / KICKED %.2f LUFS "
                      "(spread %.2f dB, limit +-2 -> 4)",
                      drive, lev[0], lev[1], lev[2], hi - lo);
        check(hi - lo <= 4.0 && std::fabs(lev[0] - lev[1]) <= 2.0 && std::fabs(lev[2] - lev[1]) <= 2.0, msg);
    }
}

// ---- 2. DRIVE sweep: level follows the intended rise, THD rising -----------------------
// Loudness at SPLASH 0 (DRIVE's own level rule, ADR 0033), the default
// SPLASH's reported: SPLASH's Bite and Clang grow with DRIVE (ADR 0032), so
// with SPLASH up drum hits get louder with DRIVE than the rule on its own.
void driveSweep()
{
    const Buf in = snareHits(size_t(7.0f * kFs), 0.5f);
    const Buf tone = sine(size_t(1.5f * kFs), 250.0f, 0.5f);
    for (int a = 0; a < 3; ++a) {
        double lev[11], th[11], dflt1 = 0, dflt0 = 0;
        for (int i = 0; i <= 10; ++i) {
            Settings s;
            s.att   = a;
            s.drive = float(i) / 10.0f;
            if (i == 0 || i == 10) (i == 0 ? dflt0 : dflt1) = loudness(renderWith(s, in));
            s.splash = 0.0f;
            lev[i]  = loudness(renderWith(s, in));
            const Buf y = driveInRender(a, s.drive, tone);
            th[i] = thd(y, size_t(0.5f * kFs), size_t(kFs), 250.0f);
        }
        double maxDev = 0;
        bool mono = true;
        for (int i = 0; i <= 10; ++i) {
            maxDev = std::max(maxDev, std::fabs(lev[i] - lev[0] - heardDb(float(i) / 10.0f)));
            if (i > 0) mono &= th[i] >= th[i - 1] * 0.98;
        }
        std::printf("INFO  DRIVE 0->1 %s, default SPLASH: loudness %+.2f dB at DRIVE 1 re DRIVE 0\n", kAttName[a], dflt1 - dflt0);
        std::snprintf(msg, sizeof msg,
                      "DRIVE 0->1 %s, SPLASH 0: loudness %+.2f / %+.2f / %+.2f dB re DRIVE 0 at .5 / .8 / 1 (intended %+.1f / %+.1f / "
                      "%+.1f, ADR 0033); max %.2f dB off the curve (limit 2)",
                      kAttName[a], lev[5] - lev[0], lev[8] - lev[0], lev[10] - lev[0], heardDb(0.5f), heardDb(0.8f),
                      heardDb(1.0f), maxDev);
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
// clearly audible. Level-matched = the DRIVE b render scaled by the gain
// that best matches DRIVE a: that part cannot be a loudness cue, so it
// shows the change is character. Since ADR 0033 DRIVE also raises the level
// on purpose (+6 dB at 1), so the ADR 0022 bars apply to the level-matched
// nulls (the raw ones are reported): DRIVEN / KICKED 0 vs .5 >= -20, KICKED
// 0 vs 1 >= -6, CLEAN 0 vs 1 <= -15 (a tint). Loudness follows heardDb()
// within 2 dB. Settings as ADR 0022: MIX 1, SPRINGS 2, DECAY 0.6, TENSION
// 0.5, TONE 0.5 (SPLASH 0: DRIVE's own sound).
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
        // SPLASH 0, as the sweet-spot test below: the Clatter follows the
        // driven level, and since SPLASH round 2 its knocks land differently
        // at each DRIVE, which a null test counts as DRIVE's sound.
        s.splash = 0.0f;
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
        double lev[5], dev = 0;
        for (int d = 0; d < 5; ++d) {
            o[d] = run(att, drives[d]);
            lev[d] = loudness(o[d]);
            dev = std::max(dev, std::fabs(lev[d] - lev[0] - heardDb(drives[d])));
        }
        const double n25 = nullDb(o[0], o[1], false), n50 = nullDb(o[0], o[2], false), n100 = nullDb(o[0], o[4], false);
        const double m50 = nullDb(o[0], o[2], true), m100 = nullDb(o[0], o[4], true);
        bool ok = dev <= 2.0;
        const char* want = "";
        if (att == 0) {
            ok &= m100 <= -15.0; // CLEAN: a gentle tint, never a drive
            want = "CLEAN stays mild: matched 0 vs 1 <= -15";
        } else {
            ok &= m50 >= -20.0;               // clearly coloured by noon, and not by loudness
            if (att == 2) ok &= m100 >= -6.0; // cranked at max
            want = att == 1 ? "matched 0 vs .5 >= -20" : "matched 0 vs .5 >= -20, 0 vs 1 >= -6";
        }
        std::snprintf(msg, sizeof msg,
                      "DRIVE audibility %s, 02_hits (ADR 0022): null 0 vs .25 / .5 / 1 = %.1f / %.1f / %.1f dB "
                      "(level-matched .5 / 1: %.1f / %.1f; %s); loudness %+.2f dB at DRIVE 1, max %.2f off the "
                      "ADR 0033 curve (limit 2)",
                      kAttName[att], n25, n50, n100, m50, m100, want, lev[4] - lev[0], dev);
        check(ok, msg);
    }
}

// ---- 2c. DRIVE sweet spot: no dead patch (M8, docs/m8-sweetspot.md) ----------------------
// The M8 sweet-spot report's test: DRIVE in 0.1 steps on 02_hits' first
// 10 s (MIX 0.5, SPRINGS 2, DECAY / TONE / TENSION noon, WOBBLE 0.45), mono
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
            s.wobble = 0.45f;
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
            // SPLASH 0: this is the tank's evenness across material; the
            // splash adds to hits by design (SPLASH stronger C, ADR 0032,
            // owner 2 Oct 2026, read +0.2-0.5 dB over the limit at the
            // default SPLASH in KICKED).
            s.splash = 0.0f;
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

// ---- 2e. First-hit level jump (owner, hardware, 30 Sep 2026) ---------------------------
// 04_skank at the owner's H2 settings (CLEAN, MIX 1, DECAY / TONE / TENSION
// noon, SPLASH / DRIVE / WOBBLE 0, 2 Springs), straight after power-up: the
// first chord's wet peak must not sit over the later chords' (it did by
// 3.2 dB: -3.8 vs -7.0 dBFS, the excitation trim starting at 0 dB). Limit
// +1 dB over the loudest of chords 2-4 (the same Am chord at the same input
// peak). Also after reset() (the Plugin's transport restart), and 02_hits'
// first snare may come in at most 1 dB quieter than it would with a warm
// trim (the price of starting low), never louder.
void firstHit()
{
    rv::wav::Audio sk, hits;
    if (!readStimulus("04_skank.wav", sk) || !readStimulus("02_hits.wav", hits)) {
        check(false, "First-hit level: 04_skank.wav and 02_hits.wav found");
        return;
    }
    Settings s;
    s.att = 0;
    s.decay = s.tension = s.tone = 0.5f;
    s.mix = 1.0f;
    s.splash = s.drive = 0.0f;
    s.wobble = 0.5f; // noon: still
    auto peakIn = [](const Stereo& o, double from, double len) {
        float p = 0.0f;
        for (size_t i = size_t(from * kFs); i < size_t((from + len) * kFs) && i < o.l.size(); ++i)
            p = std::max({p, std::fabs(o.l[i]), std::fabs(o.r[i])});
        return 20.0 * std::log10(double(p) + 1e-30);
    };
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, s);
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) t.reset(); // a restart: must behave like power-up
        const Stereo o = render(t, sk.channels[0], 48);
        // Chords every 0.8 s from 1.4 s (tools/make_stimulus.py skank()).
        const double c1 = peakIn(o, 1.4, 0.8);
        const double later = std::max({peakIn(o, 2.2, 0.8), peakIn(o, 3.0, 0.8), peakIn(o, 3.8, 0.8)});
        std::snprintf(msg, sizeof msg,
                      "First-hit level, 04_skank %s: chord 1 peaks %.1f dBFS, chords 2-4 %.1f (limit +1 dB)",
                      pass == 0 ? "after power-up" : "after reset()", c1, later);
        check(c1 <= later + 1.0, msg);
    }
    // 02_hits: the first -6 dBFS snare vs the same snare after a warm-up
    // (the file played once before), wet peak.
    {
        const Buf& h = hits.channels[0];
        const Stereo cold = renderWith(s, h);
        Buf twice(h.size() * 2);
        std::copy(h.begin(), h.end(), twice.begin());
        std::copy(h.begin(), h.end(), twice.begin() + std::ptrdiff_t(h.size()));
        const Stereo warm = renderWith(s, twice);
        const double first = peakIn(cold, 1.0, 1.0), again = peakIn(warm, double(h.size()) / kFs + 1.0, 1.0);
        std::snprintf(msg, sizeof msg,
                      "First-hit level, 02_hits: first snare %.1f dBFS after power-up vs %.1f warm (%+.1f dB; 0 .. -1)",
                      first, again, first - again);
        check(first <= again + 0.1 && first >= again - 1.0, msg);
    }
}

// Level follows the intended rise on quiet sustained material too (ADR 0022:
// no loudness cue beyond ADR 0033's +6 dB): steady noise ~ -25 dBFS RMS, a
// pad-like input that the saturators hardly squash, so a fixed makeup tuned
// on hits would make it louder.
void driveLevelHeld()
{
    const Buf in = noise(size_t(3.0f * kFs), 0.1f, 3u);
    for (int a = 0; a < 3; ++a) {
        double lev[6], dev = 0;
        for (int i = 0; i <= 5; ++i) {
            Settings s;
            s.att = a;
            s.drive = float(i) / 5.0f;
            const Stereo o = renderWith(s, in);
            lev[i] = db(power(o.l, size_t(1.5f * kFs), size_t(2.9f * kFs)));
            dev = std::max(dev, std::fabs(lev[i] - lev[0] - heardDb(s.drive)));
        }
        std::snprintf(msg, sizeof msg,
                      "DRIVE 0->1 %s, steady noise at -25 dBFS RMS: level %+.2f / %+.2f / %+.2f / %+.2f / %+.2f dB re "
                      "DRIVE 0 at .2 / .4 / .6 / .8 / 1 (intended %+.1f at 1); max %.2f off the ADR 0033 curve (limit 2)",
                      kAttName[a], lev[1] - lev[0], lev[2] - lev[0], lev[3] - lev[0], lev[4] - lev[0], lev[5] - lev[0],
                      heardDb(1.0f), dev);
        check(dev <= 2.0, msg);
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
// "Worst" = the product closest to failing: the one whose smaller excess
// over the two limits (rel + 60, abs + 100) is largest. A loud relative
// reading that sits far under -100 dBFS (noise floor at 15 kHz through a
// dark band-limit) is not the one that matters.
// The Tank's wet at DRIVE d comes back heardDb(d) louder on purpose (ADR
// 0033: +6 dB at DRIVE 1, applied after every saturator), and so does
// everything in it, the Tank's own float-rounding floor included (~-104
// dBFS around 30-100 Hz with a 0 dBFS 11-12 kHz tone, at DRIVE 0 already:
// not aliasing). So for the Tank's wet the absolute floor is read on the
// DRIVE-0 scale: -100 dBFS + heardDb(DRIVE). The relative limit is unchanged.
struct AliasResult {
    double worst = -300, atHz = 0, fromHz = 0, absDb = -300, excess = -1e9;
    bool ok = true;
};

void accumulate(AliasResult& r, const Buf& y, float f0, double absFloorDb = -100.0)
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
    const bool good = rel <= -60.0 || absDb <= absFloorDb;
    r.ok &= good;
    const double excess = std::min(rel + 60.0, absDb - absFloorDb);
    if (excess > r.excess) {
        r.excess = excess;
        r.worst = rel;
        r.atHz = at;
        r.fromHz = f0;
        r.absDb = absDb;
    }
}

// The whole Tank: what its nonlinear stages add, judged against the same
// Tank, same settings, same tone played 40 dB quieter (where every stage is
// linear), relative to the fundamental. F's darker tank (ADR 0038) keeps the
// lows and loses the 5-15 kHz tone by ~30 dB, so a product the tank only
// shapes (the same share of the tone at -40 dB) is not counted; anything the
// drive, coil, Loop or pickup stages add is, at the same -60 dB / absolute
// bars. (ADR 0038 Round F2: with the gentler low cut and the coupled wire
// gauges together, a 104 Hz product read -53.7 dB re a 7 kHz tone the tank
// had darkened, -93.9 dBFS.)
void accumulateVsLinear(AliasResult& r, const Buf& y, const Buf& yLin, float f0, double absFloorDb)
{
    constexpr size_t N = 32768;
    const auto p = spectrum(y, y.size() - N, N), q = spectrum(yLin, yLin.size() - N, N);
    const double binHz = kFs / double(N);
    double fp = 0, fq = 0;
    for (long k = long(f0 / binHz) - 3; k <= long(f0 / binHz) + 3; ++k) fp = std::max(fp, p[size_t(k)]), fq = std::max(fq, q[size_t(k)]);
    const double fundDbfs = db(fp) - 20.0 * std::log10(0.35875 * double(N) / 2.0);
    for (size_t k = size_t(20.0 / binHz); k <= size_t(20000.0 / binHz); ++k) {
        const double hz = double(k) * binHz;
        bool harmonic = false;
        for (int h = 1; h * f0 < kFs / 2 + 7 * binHz; ++h)
            if (std::fabs(hz - h * f0) <= 6.5 * binHz) harmonic = true;
        if (harmonic) continue;
        const double added = p[k] / fp - q[k] / fq; // re the fundamental, power: what the loud tone adds
        if (added <= 0.0) continue;
        const double rel = db(added), absDb = fundDbfs + rel;
        const bool good = rel <= -60.0 || absDb <= absFloorDb;
        r.ok &= good;
        const double excess = std::min(rel + 60.0, absDb - absFloorDb);
        if (excess > r.excess) r.excess = excess, r.worst = rel, r.atHz = hz, r.fromHz = f0, r.absDb = absDb;
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
                      "Aliasing %s: deciding non-harmonic product %.1f dB re fundamental (%.0f Hz, from %.0f Hz; %.0f dBFS "
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
    // SPLASH 0 and WOBBLE noon = still (M7): WOBBLE's pitch movement and KICKED's
    // energy-dependent rattle put modulation sidebands within ~10-20 Hz of
    // the tone (-41 dB at the old WOBBLE 0.2 / SPLASH 0.3 in KICKED). Those are
    // not aliasing; this measures the drive stages.
    // Read before the output's mu-law box (ADR 0042; output_bits_voicing 0 as
    // a test hook): this measures the drive chain's oversampling. The box's own
    // products (mu-law's expansion is slightly curved, and it runs at 24 kHz,
    // so the highest fold) sit ~30 dB under its grain; test_output_bits checks
    // the box for new pitches. Its number is printed below as INFO.
    {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 1000.0f) {
            Settings s;
            s.att = 1;
            s.drive = 1.0f;
            s.decay = 0.3f;
            s.tension = 0.0f; // loosest tank
            s.springs = 2;
            s.splash  = 0.0f;
            s.wobble  = 0.5f; // noon: still
            accumulateVsLinear(r, renderWith(s, fadedSine(size_t(3.0f * kFs), f0, 1.0f), 0).l,
                               renderWith(s, fadedSine(size_t(3.0f * kFs), f0, 0.01f), 0).l, f0, -100.0 + heardDb(s.drive));
        }
        report("Tank wet DRIVEN DRIVE 1 (3 Springs), 5-15 kHz at 0 dBFS, what it adds over the same tone at -40 dBFS (abs floor -100 dBFS + the +6 dB DRIVE adds)", r);
    }
    for (float amp : {1.0f, 0.5f}) {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 1000.0f) {
            Settings s;
            s.att = 2;
            s.drive = 1.0f;
            s.decay = 0.3f;
            s.tension = 0.0f; // loosest tank
            s.springs = 2;
            s.splash  = 0.0f;
            s.wobble  = 0.5f; // noon: still
            accumulateVsLinear(r, renderWith(s, fadedSine(size_t(3.0f * kFs), f0, amp), 0).l,
                               renderWith(s, fadedSine(size_t(3.0f * kFs), f0, 0.01f * amp), 0).l, f0, -100.0 + heardDb(s.drive));
        }
        report(amp == 1.0f ? "Tank wet KICKED DRIVE 1 (3 Springs), 5-15 kHz at 0 dBFS, what it adds over the same tone 40 dB down"
                           : "Tank wet KICKED DRIVE 1 (3 Springs), 5-15 kHz at -6 dBFS, what it adds over the same tone 40 dB down",
               r);
    }
    // INFO: the same at 0 dBFS with the output's mu-law box in (what ships).
    for (int a = 1; a < 3; ++a) {
        AliasResult r;
        for (float f0 = 5000.0f; f0 <= 15000.0f; f0 += 1000.0f) {
            Settings s;
            s.att = a, s.drive = 1.0f, s.decay = 0.3f, s.tension = 0.0f, s.springs = 2, s.splash = 0.0f, s.wobble = 0.5f;
            accumulateVsLinear(r, renderWith(s, fadedSine(size_t(3.0f * kFs), f0, 1.0f)).l,
                               renderWith(s, fadedSine(size_t(3.0f * kFs), f0, 0.01f)).l, f0, -100.0 + heardDb(s.drive));
        }
        std::printf("INFO  Aliasing Tank wet %s DRIVE 1, 5-15 kHz at 0 dBFS, with the output's mu-law box (ADR 0042): worst non-harmonic "
                    "product %.1f dB re fundamental (%.0f Hz, from %.0f Hz; %.0f dBFS abs)\n",
                    a == 1 ? "DRIVEN" : "KICKED", r.worst, r.atHz, r.fromHz, r.absDb);
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
            // First echo: from the pickups (kPickupArrival x L, ADR 0029) to
            // just before the next round trip.
            const size_t end = size_t((rv::modes::kPickupArrival + 0.98f) * rv::map::tensionLoopDelaySeconds(tension) * kFs);
            // High band per map::kChirpDirection, as test_spring.
            const float hiLo = rv::map::kHighsLater ? 0.8f : 0.5f, hiHi = rv::map::kHighsLater ? 0.97f : 0.85f;
            const double tHi = centroidSeconds(bandpass(m, hiLo * fC, hiHi * fC), end);
            const double tLo = centroidSeconds(bandpass(m, 200.0f, 500.0f), end);
            // Late band per map::kChirpDirection (lows for LowsLater).
            const double dir = rv::map::kHighsLater ? -1.0 : 1.0;
            std::snprintf(msg, sizeof msg, "TONE 0 chirp, %s TENSION %.0f: highs at %.1f ms, lows at %.1f ms (%s later)",
                          kAttName[a], tension, tHi * 1e3, tLo * 1e3, rv::map::kHighsLater ? "highs" : "lows");
            check(dir * (tLo - tHi) > 0.001, msg);
        }

    // Level across TONE, energy above 10 kHz at CW vs noon, and (owner, 29
    // Sep) the bright side's low cut: energy below 150 Hz, snare hits.
    const Buf in = snareHits(size_t(7.0f * kFs), 0.5f);
    for (int a = 0; a < 3; ++a) {
        double lev[5], hf[5], lf[5];
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
            const Buf ll = onePoleLp(onePoleLp(onePoleLp(onePoleLp(o.l, 150.0f), 150.0f), 150.0f), 150.0f);
            const Buf lr = onePoleLp(onePoleLp(onePoleLp(onePoleLp(o.r, 150.0f), 150.0f), 150.0f), 150.0f);
            lf[i] = db(power(ll, 0, ll.size()) + power(lr, 0, lr.size()));
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
        std::snprintf(msg, sizeof msg,
                      "TONE CW %s thins the lows: energy < 150 Hz %+.1f dB at 3 o'clock, %+.1f dB at full CW vs noon "
                      "(full CW <= -8, 3 o'clock between)",
                      kAttName[a], lf[3] - lf[2], lf[4] - lf[2]);
        check(lf[4] - lf[2] <= -8.0 && lf[3] < lf[2] && lf[3] > lf[4], msg);
    }
}

// ---- 5b. Big Knob TONE voicings (prototype, ADR 0036 Proposed) ------------------------------
// TONE's right half as King Tubby's Big Knob (DriveVoicing.h "Big Knob TONE
// voicings"; docs/research/big-knob.md). Renderer-only key tone_voicing:
// 0 = today, 1 = steep (18 dB/oct), 2 = steep + bump, 3 = + ringier when
// driven. Checks the guarantees the brief keeps (ADR 0017, SPEC §2.3.4):
//   response   the low cut's shape per voicing at TONE 0.5 / 0.7 / 0.85 / 1
//              (cutoff, bump, slope): INFO, plus the voicings' defining
//              traits (slope >= 16 dB/oct, the bump's size).
//   identical  hits and skank at TONE 0 / 0.25 / 0.5 are bit for bit
//              voicing 0's in every voicing (CLEAN and KICKED, DRIVE 0.25;
//              voicing 3 also DRIVE 0.8).
//   loudness   hits / skank / held chords at TONE 0.5 / 0.7 / 0.85 / 1
//              within ±3 dB of noon, CLEAN and KICKED, the owner's settings
//              (2 Springs, DECAY / TENSION noon, SPLASH 0.3, DRIVE 0.25, MIX 1;
//              voicing 3 also DRIVE 0.8).
//   chirp      the Chirp at full CW, every voicing (same measure as TONE 0).
//   cpu        Tilt alone, ns/sample, voicing 0 vs 2 (INFO).
double sectionDb(const rv::drive::BigKnob& b, double hz)
{
    rv::dsp::Biquad bq;
    bq.setHighpass(b.hz, b.q, kFs);
    rv::dsp::OnePoleLowpass lp;
    lp.setCutoff(b.hz1, kFs);
    const double w = 2.0 * rv::map::kPi * hz / kFs;
    const std::complex<double> z1 = std::polar(1.0, -w);
    // y = x - k LP(x): LP(z) = c / (1 - (1-c) z^-1)
    const std::complex<double> h1 = 1.0 - double(b.k) * double(lp.c) / (1.0 - (1.0 - double(lp.c)) * z1);
    return 10.0 * std::log10(double(bq.magnitudeSquared(float(std::cos(w))))) + 20.0 * std::log10(std::abs(h1));
}

void bigKnob()
{
    const float tones[] = {0.5f, 0.7f, 0.85f, 1.0f};
    const char* const vName[6] = {"0 today", "1 steep", "2 bump", "3 driven", "4 gentle", "5 hits (default)"};
    for (int v = 0; v < 5; ++v)
        for (float tn : tones) {
            const rv::drive::BigKnob b = rv::drive::bigKnob(v, tn);
            const double u = std::max(0.0f, 2.0f * tn - 1.0f);
            const double fc = v == 0 ? b.hz : (tn > 0.5f ? rv::drive::bigKnobHz(float(u)) : b.hz);
            double peak = -100, peakHz = 0, f3 = 0;
            for (double f = 10.0; f < 20000.0; f *= 1.005) {
                const double d = sectionDb(b, f);
                if (d > peak) { peak = d; peakHz = f; }
                if (f3 == 0 && d >= -3.0) f3 = f;
            }
            const double slope = sectionDb(b, fc / 4) - sectionDb(b, fc / 8);
            std::printf("INFO  Big Knob voicing %-8s TONE %.2f: cutoff %6.0f Hz (-3 dB at %6.0f Hz), bump %+5.1f dB at "
                        "%5.0f Hz, slope %4.1f dB/oct (fc/8 -> fc/4), DriveIn lows push %+4.1f dB\n",
                        vName[v], tn, fc, f3, std::max(0.0, peak), peak > 0.05 ? peakHz : 0.0, slope, b.pushDb);
            if (v >= 1 && tn >= 0.7f) {
                std::snprintf(msg, sizeof msg, "Big Knob voicing %s TONE %.2f: slope %.1f dB/oct (>= 16, the Altec's 18)",
                              vName[v], tn, slope);
                check(slope >= 16.0, msg);
            }
            if (v == 1 && tn > 0.5f) {
                std::snprintf(msg, sizeof msg, "Big Knob voicing 1 TONE %.2f: no bump (%+.2f dB, <= 0.1)", tn, peak);
                check(peak <= 0.1, msg);
            }
            if (v >= 2 && tn == 1.0f) {
                std::snprintf(msg, sizeof msg, "Big Knob voicing %s TONE 1: bump %+.1f dB at %.2f x cutoff (4-8 dB, 1.2-1.6 x)",
                              vName[v], peak, peakHz / fc);
                check(peak >= 4.0 && peak <= 8.0 && peakHz / fc >= 1.2 && peakHz / fc <= 1.6, msg);
            }
        }

    // TONE <= 0.5: every voicing is today's, bit for bit.
    rv::wav::Audio hitsA, skankA, heldA;
    if (!readStimulus("02_hits.wav", hitsA) || !readStimulus("04_skank.wav", skankA) || !readStimulus("08_held_tones.wav", heldA)) {
        check(false, "Big Knob: 02_hits / 04_skank / 08_held_tones readable");
        return;
    }
    auto mono = [](const rv::wav::Audio& a, double seconds) {
        Buf m(std::min(a.frames(), size_t(seconds * kFs)));
        for (size_t i = 0; i < m.size(); ++i) {
            float acc = 0;
            for (const auto& c : a.channels) acc += c[i];
            m[i] = acc / float(a.channels.size());
        }
        return m;
    };
    const Buf hits = mono(hitsA, 8.0), skank = mono(skankA, 8.0), held = mono(heldA, 12.0);
    auto owner = [](int att, float drive, float tn, int v) {
        Settings s;
        s.att = att;
        s.springs = 1;
        s.decay = s.tension = 0.5f;
        s.splash = 0.3f;
        s.drive = drive;
        s.mix = 1.0f;
        s.tone = tn;
        s.toneVoicing = v;
        return s;
    };
    {
        int cells = 0, diff = 0;
        for (int att : {0, 2})
            for (float drive : {0.25f, 0.8f})
                for (float tn : {0.0f, 0.25f, 0.5f})
                    for (const Buf* in : {&hits, &skank}) {
                        const Stereo ref = renderWith(owner(att, drive, tn, 0), *in);
                        for (int v = 1; v < 4; ++v) {
                            if (drive > 0.5f && v != 3) continue;
                            const Stereo o = renderWith(owner(att, drive, tn, v), *in);
                            ++cells;
                            if (o.l != ref.l || o.r != ref.r) ++diff;
                        }
                    }
        std::snprintf(msg, sizeof msg,
                      "Big Knob: hits and skank at TONE 0 / 0.25 / 0.5 are bit for bit today's in voicings 1-3 "
                      "(CLEAN, KICKED; DRIVE 0.25, voicing 3 also 0.8): %d of %d differ",
                      diff, cells);
        check(diff == 0, msg);
    }

    // Loudness across the right half, per material.
    const char* const matName[3] = {"hits", "skank", "held"};
    const Buf* mats[3] = {&hits, &skank, &held};
    for (int v = 0; v < 4; ++v)
        for (int att : {0, 1, 2})
            for (float drive : {0.25f, 0.8f}) {
                double worst = 0;
                char line[200] = {}, at[64] = {};
                std::future<double> fut[3][4];
                for (int m = 0; m < 3; ++m)
                    for (int i = 0; i < 4; ++i)
                        fut[m][i] = std::async(std::launch::async, [&, m, i] {
                            return loudness(renderWith(owner(att, drive, tones[i], v), *mats[m]));
                        });
                for (int m = 0; m < 3; ++m) {
                    double lev[4];
                    for (int i = 0; i < 4; ++i) lev[i] = fut[m][i].get();
                    char part[64];
                    std::snprintf(part, sizeof part, " %s %+.1f/%+.1f/%+.1f", matName[m], lev[1] - lev[0], lev[2] - lev[0], lev[3] - lev[0]);
                    std::strncat(line, part, sizeof line - std::strlen(line) - 1);
                    for (int i = 1; i < 4; ++i)
                        if (std::fabs(lev[i] - lev[0]) > std::fabs(worst)) {
                            worst = lev[i] - lev[0];
                            std::snprintf(at, sizeof at, "%s TONE %.2f", matName[m], tones[i]);
                        }
                }
                std::snprintf(msg, sizeof msg,
                              "Big Knob voicing %s %s DRIVE %.2f: loudness vs noon at TONE 0.7/0.85/1 (dB):%s; worst %+.1f (%s; limit ±3)",
                              vName[v], kAttName[att], drive, line, worst, at);
                check(std::fabs(worst) <= 3.0, msg);
            }

    // The Chirp at full CW (as tone(): highs after the 200-500 Hz band in the first echo).
    // Also the default voicing 5, at the default placement (the Big Knob
    // after the Springs, ADR 0036 amendment: it thins the 200-500 Hz band on
    // the wet, so the Chirp is measured through it).
    for (int v : {0, 1, 2, 3, 5})
        for (float tension : {0.0f, 1.0f}) {
            Settings s;
            s.decay = 0.5f;
            s.tension = tension;
            s.tone = 1.0f;
            s.att = 1;
            s.springs = 0;
            s.toneVoicing = v;
            Buf imp(size_t(0.5f * kFs), 0.0f);
            imp[0] = 1.0f;
            const Stereo o = renderWith(s, imp);
            Buf m(o.l.size());
            for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
            const float fC = rv::map::tensionTransitionHz(tension);
            const size_t end = size_t((rv::modes::kPickupArrival + 0.98f) * rv::map::tensionLoopDelaySeconds(tension) * kFs);
            const float hiLo = rv::map::kHighsLater ? 0.8f : 0.5f, hiHi = rv::map::kHighsLater ? 0.97f : 0.85f;
            const double tHi = centroidSeconds(bandpass(m, hiLo * fC, hiHi * fC), end);
            const double tLo = centroidSeconds(bandpass(m, 200.0f, 500.0f), end);
            const double dir = rv::map::kHighsLater ? -1.0 : 1.0;
            std::snprintf(msg, sizeof msg, "Big Knob voicing %s TONE 1 chirp, TENSION %.0f: highs at %.1f ms, lows at %.1f ms (%s later)",
                          vName[v], tension, tHi * 1e3, tLo * 1e3, rv::map::kHighsLater ? "highs" : "lows");
            check(dir * (tLo - tHi) > 0.001, msg);
        }

    // CPU: the Tilt alone at full CW, voicing 0 vs 2 (best of 5).
    {
        const size_t n = size_t(4.0f * kFs);
        const Buf in = noise(n, 0.3f, 7u);
        double ns[2];
        for (int k = 0; k < 2; ++k) {
            double best = 1e9;
            for (int rep = 0; rep < 5; ++rep) {
                rv::dsp::Tilt t;
                t.prepare(kFs);
                t.setVoicing(k ? rv::drive::kToneVoicingBump : rv::drive::kToneVoicingToday);
                t.set(1.0f, true, 48);
                volatile float sink = 0;
                const auto t0 = std::chrono::steady_clock::now();
                float acc = 0;
                for (size_t i = 0; i < n; ++i) {
                    if (i % 48 == 0) t.set(1.0f, false, 48);
                    acc += t.process(in[i]);
                }
                sink = acc;
                (void)sink;
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n));
            }
            ns[k] = best;
        }
        std::printf("INFO  Big Knob CPU: Tilt alone %.2f ns/sample (voicing 0) vs %.2f (voicing 2) desktop\n", ns[0], ns[1]);
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
    float worstPeak = 0, shippedPeak = 0;
    for (int a = 0; a < 3; ++a)
        for (float dr : {0.0f, 0.5f, 1.0f})
            for (float d : {0.0f, 0.5f, 0.89f, 1.0f})
                for (int m = 0; m < 3; ++m)
                    for (int input = 0; input < 2; ++input) {
                        const bool howl = a == 2 && d >= rv::drive::kHowlZoneStart;
                        // CLEAN / DRIVEN from DECAY 0.9: the Hold (ADR 0040). It
                        // holds by design, so it must only never grow; in the
                        // layer voicing (the default) the input gets in.
                        const bool hold = a < 2 && d > rv::throwhold::kZoneStart;
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
                        s.tension = 0.0f; // loosest tank
                        // Rendered here (not renderWith) to log the output
                        // limiter's gain per 48-sample block.
                        // Read before the output's mu-law box (ADR 0042;
                        // output_bits_voicing 0 as a test hook): the Tank's
                        // stability, not the converter's steps. The shipped
                        // output is checked below (finite, ends no louder).
                        rv::Tank tank;
                        tank.prepare(kFs, 48);
                        if (hold) tank.setHoldVoicing(rv::throwhold::kVoicingLayer);
                        apply(tank, s);
                        tank.setOutputBitsVoicing(0);
                        Stereo o{Buf(in.size()), Buf(in.size())};
                        std::vector<float> limGain(in.size() / 48 + 1, 1.0f);
                        for (size_t pos = 0; pos < in.size(); pos += 48) {
                            const int k = int(std::min<size_t>(48, in.size() - pos));
                            tank.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
                            limGain[pos / 48] = tank.limiterGain();
                        }
                        // True if the limiter pulled anywhere in [from, to).
                        auto limited = [&](size_t from, size_t to) {
                            for (size_t b = from / 48; b <= std::min(to / 48, limGain.size() - 1); ++b)
                                if (limGain[b] < 0.999f) return true;
                            return false;
                        };
                        const float pk = std::max(peakAbs(o.l), peakAbs(o.r));
                        worstPeak = std::max(worstPeak, pk);
                        bool good = allFinite(o.l) && allFinite(o.r) && pk < 1.0f;
                        {
                            // The shipped output (box in): finite; outside the
                            // Howl and the Hold its last second no louder than
                            // the first after the input (<=: the box's tail can
                            // be exact silence).
                            rv::Tank shT;
                            shT.prepare(kFs, 48);
                            apply(shT, s);
                            if (hold) shT.setHoldVoicing(rv::throwhold::kVoicingLayer);
                            const Stereo sh = render(shT, in, 48);
                            shippedPeak = std::max({shippedPeak, peakAbs(sh.l), peakAbs(sh.r)});
                            const size_t st = input == 0 ? sec / 2 : sec + sec / 2;
                            good &= allFinite(sh.l) && allFinite(sh.r)
                                 && (howl || hold || power(sh.l, 5 * sec, 6 * sec) <= power(sh.l, st, st + sec));
                        }
                        if (!howl) {
                            // Energy after the input stops falls (+1 dB slack), and ends lower.
                            // A window compared with one the output limiter was
                            // still pulling down is skipped: the limiter letting
                            // go reads as rising energy, not a Loop running away
                            // (since ADR 0033 the wet is up to 6 dB louder at
                            // DRIVE 1, so full-scale noise at DECAY 1 meets the
                            // limiter). The "ends lower" check below still holds.
                            // In the Hold the ducking lets go of the bed over
                            // ~1.5 s after the noise stops (up to +12 dB, by
                            // design): judge the bed from 3 s.
                            const size_t start = hold && input == 1 ? 3 * sec : input == 0 ? sec / 2 : sec + sec / 2;
                            bool falls = true;
                            for (const Buf* ch : {&o.l, &o.r}) {
                                double prev = power(*ch, start, start + sec / 2);
                                for (size_t w = start + sec / 2; w + sec / 2 <= ch->size(); w += sec / 2) {
                                    const double e = power(*ch, w, w + sec / 2);
                                    if (e > prev * 1.26 && e > 1e-16 && !limited(w - sec / 2, w)) falls = false;
                                    prev = e;
                                }
                            }
                            good &= falls && (hold || power(o.l, 5 * sec, 6 * sec) < power(o.l, start, start + sec));
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
                  "noise, TENSION 0 (loose)): finite, peak before the output box < 1 (worst %.3f), decaying outside the Howl zone, never "
                  "growing in the Hold (CLEAN / DRIVEN DECAY 1, layer voicing) (%d bad); shipped output peak %.3f (Versio after "
                  "kOutputTrim %.3f)",
                  cells, worstPeak, bad, shippedPeak, shippedPeak * 0.874f);
    check(bad == 0, msg);

    // CLEAN / DRIVEN at the top of DECAY below the Hold (ADR 0001, 0040):
    // 3 s after a noise burst the level is well below where it started.
    for (int a = 0; a < 2; ++a) {
        Buf in = noise(10 * sec, 0.5f, 5u);
        std::fill(in.begin() + long(sec), in.end(), 0.0f);
        Settings s;
        s.att = a;
        s.decay = rv::throwhold::kZoneStart;
        s.drive = 1.0f;
        s.springs = 2;
        const Stereo o = renderWith(s, in);
        const double e1 = db(power(o.l, 1 * sec + sec / 2, 2 * sec)), e9 = db(power(o.l, 9 * sec, 10 * sec));
        std::snprintf(msg, sizeof msg, "%s DECAY 0.9 DRIVE 1: tail falls %.1f dB from 1.5 s to 9.5 s (fades, ADR 0001)",
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
    // normal tail, which then fades at DECAY 1's T60 (reported). Unchanged
    // by the Hold (ADR 0040, owner's pick): it arms only when DECAY enters
    // its zone outside KICKED, so this flip is bit for bit as before.
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
    // Worst-case knobs: DECAY / TONE / DRIVE 1, TENSION 0 (loosest: most stages), MIX 0.5, noise in.
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
                s.tension = 0.0f; // loosest tank
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

// ---- 2f. DRIVE doesn't shorten the tail (ADR 0033) ---------------------------------------
// Owner (30 Sep 2026): "the clean signal seems to decay longer than the
// driven and kicked variants, which feels counter to driving the tanks
// harder". Cause: ADR 0022's DRIVE push on the LoopSat, removed. Measure:
// 02_hits (all six isolated hits), MIX 1, 2 Springs, DECAY 0.6, TENSION /
// TONE noon, SPLASH 0, WOBBLE 0; mono sum band-passed 300 Hz-4 kHz; per hit,
// the 100 ms RMS window starting 0.6 s after the hit, dB re the hit's
// loudest 100 ms window in its first 0.2 s; mean over the hits. `main` before
// ADR 0033: CLEAN -18.9 -> -18.7 dB, DRIVEN -19.0 -> -20.9, KICKED -19.2 ->
// -22.6 from DRIVE 0 to 1 (and -27.3 -> -31.0 at 1 s). Gate: at DRIVE 1 no
// ATTITUDE's tail sits more than 1 dB under its DRIVE-0 tail or under CLEAN's.
void driveTail()
{
    rv::wav::Audio a;
    if (!readStimulus("02_hits.wav", a)) {
        check(false, "DRIVE tail (ADR 0033): 02_hits.wav found");
        return;
    }
    const Buf& in = a.channels[0];
    auto tailDb = [&](int att, float drive, double at) {
        Settings s;
        s.att = att;
        s.drive = drive;
        s.decay = 0.6f;
        s.splash = 0.0f;
        s.wobble = 0.5f; // noon: still
        const Stereo o = renderWith(s, in);
        Buf m(o.l.size());
        for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
        m = bandpass(m, 300.0f, 4000.0f);
        const size_t w = size_t(0.1f * kFs), step = size_t(0.005f * kFs);
        double sum = 0;
        int hits = 0;
        for (double t0 : {1.0, 7.0, 13.0, 19.0, 25.0, 31.0}) {
            const size_t o0 = size_t(t0 * kFs);
            double peak = 0;
            for (size_t p = o0; p < o0 + size_t(0.2f * kFs); p += step) peak = std::max(peak, power(m, p, p + w));
            const size_t p6 = o0 + size_t(at * kFs);
            sum += db(power(m, p6, p6 + w) / peak);
            ++hits;
        }
        return sum / hits;
    };
    double t0[3], t1[3], s0[3], s1[3];
    for (int att = 0; att < 3; ++att) {
        t0[att] = tailDb(att, 0.0f, 0.6);
        t1[att] = tailDb(att, 1.0f, 0.6);
        s0[att] = tailDb(att, 0.0f, 1.0);
        s1[att] = tailDb(att, 1.0f, 1.0);
    }
    for (int att = 0; att < 3; ++att) {
        std::snprintf(msg, sizeof msg,
                      "DRIVE doesn't shorten the tail, %s (ADR 0033): 0.6 s after a hit %.1f dB at DRIVE 0 -> %.1f at 1 "
                      "(1 s: %.1f -> %.1f); CLEAN at DRIVE 1 %.1f (no more than 1 dB under either)",
                      kAttName[att], t0[att], t1[att], s0[att], s1[att], t1[0]);
        check(t1[att] >= t0[att] - 1.0 && t1[att] >= t1[0] - 1.0, msg);
    }
}

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
                       {"drive-sweetspot", driveSweetSpot}, {"wet-level", wetLevelVsMaterial}, {"first-hit", firstHit}, {"drive-tail", driveTail},
                       {"drive-held", driveLevelHeld}, {"audible", audibleAtDriveZero}, {"alias", aliasing},
                       {"tone", tone},             {"bigknob", bigKnob},          {"morph", morphClickFree},     {"determinism", determinism},
                       {"howl", howl},             {"stability", stabilityGrid},  {"performance", performance}};
    for (const T& t : tests)
        if (only.empty() || std::string(t.name).find(only) != std::string::npos) t.fn();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
