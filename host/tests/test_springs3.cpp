// SPRINGS 3 palette tests (PROTOTYPE, branch proto/springs3-palette; ADR 0037
// proposed; core/params/Springs3Voicing.h). Dependency-free: prints
// PASS/FAIL/INFO lines, returns nonzero on any failure. Optional argv[1]
// filter by section name.
//
// For every voicing of position 3 (round 1: 1 long tank, 2 in series, 3
// wide, 4 pan tank; round 2: 5 pan brighter only, 6 pan higher Chirp only,
// 7 mixed wire gauges, 8 coupled, 9 diffuse, 10 cross-fed wide; 0 is today,
// covered by the other suites; F round 2: 11 stronger, 12 wide, 13 coupled
// wire gauges, 14 swell). Three passes (see inPass()): the default (13,
// ADR 0037 "Round F2") on the shipped tank (voicing 7, low cut step 2); the
// round 1 / 2 references on tank voicing 0, which they were made on; F round
// 2's references on tank voicing 7 with F's own low cut, which they were made on:
//   identity  SPRINGS 1 and 2 are bit for bit what voicing 0 plays (hits and
//             stabs, every ATTITUDE), so positions 1 and 2 never change.
//   level     SPRINGS 3 as loud as SPRINGS 2 within +-1.5 dB, stereo and
//             mono (SPEC §7 M4 "SPRINGS levels matched"), in K-weighted
//             loudness (BS.1770); plain power reported.
//   stereo    Mono safety at SPRINGS 3 (host/common/Metrics: mono_loss
//             >= -1.5 dB, mono_notch >= -6 dB) and width (L/R correlation
//             < 0.5), DECAY {0, .5, .9} x TENSION {0, .5, 1}, hits and stabs.
//   ringing   The M6 Ringing metric on SPRINGS 3 tails (click + noise
//             burst, every ATTITUDE, DECAY max below the Howl / Hold
//             (KICKED 0.75, CLEAN / DRIVEN 0.9), TENSION 0 and 1, WOBBLE 0 / noon / 1): 0 flagged, and
//             no steady tone (in series reported only; see ringing()).
//   howl      KICKED DECAY 1 at SPRINGS 3: ADR 0019 (floor, movement) and
//             ADR 0018 (pulling DECAY drops >= 30 dB within 3 s).
//   switching SPRINGS 1/2 <-> 3 mid-tail (held + decaying): no clicks, the
//             held level doesn't jump (ADR 0003); rapid flipping.
//   sustain   ADR 0035: held pad / drone / organ at -6 dBFS peak, the
//             owner's settings, transposed -5..+4 semitones, at the tight
//             corners: limiter pull no worse than today's position 3 (see
//             sustain() for why not one pitch), trim <= 5 dB.
//   stability Extremes (DECAY 1, KICKED, DRIVE 1, TENSION/TONE 0 and 1):
//             finite, peaks under 1.
//   timing    Round 2 keeps today's repeat timing: every Spring's round
//             trip and first echo at 800 Hz equal to voicing 0's (TENSION
//             0..1); the echo spacing measured on the output, reported.
//   arrivals  First arrivals (L, R, mono): spread, reported.
//   character What each version is like, in numbers, for the plain-words
//             descriptions (one click, CLEAN, defaults): T60, brightness
//             (spectral centroid of the tail), width, left vs right
//             brightness and level (lean), a held chord's brightness.
//             Reported.
//   cost      Desktop ns/sample per voicing vs voicing 0 at the SPRINGS 3
//             worst case; memory (the longest L each voicing reaches vs the
//             delay line). Reported.

#include "Fft.h"
#include "Metrics.h"
#include "dsp/Tank.h"
#include "params/Mappings.h"
#include "params/ParamSpec.h"
#include "params/SpringModes.h"
#include "params/Springs3Voicing.h"
#include "params/TankVoicing.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <random>
#include <string>
#include <vector>

namespace {

int  failures = 0;
char msg[600];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

// Two passes (main()). The default voicing (coupled) and today's position 3
// are checked on the default tank voicing (TankVoicing.h, 7 since ADR 0038),
// as they ship. The other SPRINGS 3 voicings are Renderer-only references
// built and measured on tank voicing 0 (ADR 0037): their checks run in a
// second pass on tank voicing 0, today's baselines (voicing 0) included, so
// each reference is still held to its own bars against the tank it was made
// for. Each pass reports the other pass's voicings as INFO, uncounted.
// Three passes: 1 the shipped tank (tank voicing 7 with its default low cut)
// with today's position 3 (0) and the default; 2 tank voicing 0 with the
// round 1 / 2 references (made on it); 3 tank voicing 7 with F's own low cut
// (f_lowcut_voicing 0) with F round 2's references (11+, made on it; ADR 0037
// "Round F2"). The default is checked in pass 1 only.
int  gTankVoicing = rv::tankv::kDefaultVoicing;
int  gFLowCut     = rv::tankv::kDefaultFLowCut;
int  gPass        = 1;
bool inPass(int v)
{
    const bool madeOnF = v >= rv::springs3::kFirstFVoicing;
    if (v == rv::springs3::kDefaultVoicing) return gPass == 1;
    if (gPass == 1) return v == 0;
    return gPass == 2 ? !madeOnF : (madeOnF || v == 0); // 0: each pass's own baseline (today's position 3 on that tank)
}
void checkV(int v, bool ok, const char* what)
{
    if (inPass(v)) check(ok, what);
}

using Buf = std::vector<float>;
constexpr float  kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;
const char* const kAttName[3]   = {"CLEAN", "DRIVEN", "KICKED"};
const char* const kVoiceName[15] = {"0 today",          "1 long tank",       "2 in series",  "3 wide",
                                    "4 pan tank",       "5 pan brighter",    "6 pan Chirp",  "7 wire gauges",
                                    "8 coupled",        "9 diffuse",         "10 cross-fed wide",
                                    "11 coupled stronger", "12 coupled wide", "13 coupled gauges", "14 coupled swell"};
static_assert(rv::springs3::kNumVoicings == 15, "name every voicing");
constexpr int kNumVoicings = rv::springs3::kNumVoicings;

size_t sec(double s) { return size_t(s * double(kFs)); }

struct Settings {
    float decay = 0.5f, tension = 0.5f, tone = 0.5f, mix = 1.0f;
    float drive  = rv::spec(rv::ParamId::Drive).defaultValue;
    float wobble = rv::spec(rv::ParamId::Wobble).defaultValue;
    float splash = rv::spec(rv::ParamId::Splash).defaultValue;
    int   att = 0, springs = 2, voicing = 0;
};

void apply(rv::Tank& t, const Settings& s)
{
    using rv::ParamId;
    t.setParam(ParamId::Decay, s.decay);
    t.setParam(ParamId::Tension, s.tension);
    t.setParam(ParamId::Tone, s.tone);
    t.setParam(ParamId::Mix, s.mix);
    t.setParam(ParamId::Drive, s.drive);
    t.setParam(ParamId::Wobble, s.wobble);
    t.setParam(ParamId::Splash, s.splash);
    t.setParam(ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(ParamId::Springs, rv::switchToNormalised(s.springs));
    t.setEchoMode(false); // the coupled Springs reference (ADR 0041: position 3 ships as echo mode, test_echo_mode)
    t.setSprings3Voicing(s.voicing);
    t.setTankVoicing(gTankVoicing); // the pass's tank (above)
    t.setFLowCutVoicing(gFLowCut);
}

struct Stereo {
    Buf l, r;
};

Stereo render(const Settings& s, const Buf& in, int block = 48)
{
    rv::Tank t;
    t.prepare(kFs, block);
    apply(t, s);
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int k = int(std::min(size_t(block), n - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
    }
    return o;
}

double db(double p) { return 10.0 * std::log10(p + 1e-30); }
double power(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return to > from ? s / double(to - from) : 0.0;
}
double stereoDb(const Stereo& o, size_t from, size_t to) { return db(0.5 * (power(o.l, from, to) + power(o.r, from, to))); }
double monoDb(const Stereo& o)
{
    double s = 0;
    for (size_t i = 0; i < o.l.size(); ++i) {
        const double m = 0.5 * (double(o.l[i]) + o.r[i]);
        s += m * m;
    }
    return db(s / double(std::max<size_t>(1, o.l.size())));
}

// Loudness, ITU-R BS.1770 K-weighting at 48 kHz (high shelf +4 dB above
// ~1.5 kHz, then a 38 Hz high-pass), mean square of (L + R) channels summed:
// the SPEC asks for SPRINGS levels matched in loudness, and plain power is
// ruled by the lows (a tight tank's 120-180 Hz bump).
double kWeightedDb(const Buf& x)
{
    static constexpr double b1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
    static constexpr double a1[3] = {1.0, -1.69065929318241, 0.73248077421585};
    static constexpr double b2[3] = {1.0, -2.0, 1.0};
    static constexpr double a2[3] = {1.0, -1.99004745483398, 0.99007225036621};
    double s1[2] = {0, 0}, s2[2] = {0, 0}, acc = 0;
    for (float v : x) {
        const double y1 = b1[0] * v + s1[0];
        s1[0] = b1[1] * v - a1[1] * y1 + s1[1];
        s1[1] = b1[2] * v - a1[2] * y1;
        const double y2 = b2[0] * y1 + s2[0];
        s2[0] = b2[1] * y1 - a2[1] * y2 + s2[1];
        s2[1] = b2[2] * y1 - a2[2] * y2;
        acc += y2 * y2;
    }
    return db(acc / double(std::max<size_t>(1, x.size())));
}
double loudnessDb(const Stereo& o) { return db(0.5 * (std::pow(10.0, kWeightedDb(o.l) / 10) + std::pow(10.0, kWeightedDb(o.r) / 10))); }
double monoLoudnessDb(const Stereo& o)
{
    Buf m(o.l.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
    return kWeightedDb(m);
}

void normalise(Buf& x, float peakDb)
{
    float p = 0.0f;
    for (float v : x) p = std::max(p, std::fabs(v));
    const float g = std::pow(10.0f, peakDb / 20.0f) / std::max(p, 1.0e-9f);
    for (float& v : x) v *= g;
}

// RBJ low-pass, Q 0.707.
void lowpass(Buf& x, double fc)
{
    const double w = 2 * kPi * fc / kFs, alpha = std::sin(w) / (2 * 0.7071), cw = std::cos(w), a0 = 1 + alpha;
    const double b0 = (1 - cw) / 2 / a0, b1 = (1 - cw) / a0, a1 = -2 * cw / a0, a2 = (1 - alpha) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (float& v : x) {
        const double y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = v, y2 = y1, y1 = y;
        v = float(y);
    }
}

// Snare-like hits (noise + 185 Hz body), 3 s apart, -6 dBFS peak (as test_sustain_trim).
Buf hits(double seconds = 14.0)
{
    Buf x(sec(seconds), 0.0f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int h = 0; h < 4; ++h) {
        const size_t at = sec(1.0 + 3.0 * h);
        for (size_t i = 0; i < sec(0.25) && at + i < x.size(); ++i) {
            const double t = double(i) / kFs;
            x[at + i] = float(0.6 * std::sin(2 * kPi * 185 * t) * std::exp(-t / 0.03) + 1.2 * u(rng) * std::exp(-t / 0.06));
        }
    }
    normalise(x, -6.0f);
    return x;
}

// Skank-like chord stabs (offbeats, A minor / D major), -6 dBFS peak.
Buf stabs(double seconds = 12.0)
{
    Buf x(sec(seconds), 0.0f);
    const double beat = 60.0 / 75.0;
    for (int k = 0; k < 12; ++k) {
        const size_t at = sec(1.0 + k * beat + beat / 2);
        static constexpr double kAm[3] = {220.0, 261.63, 329.63}, kD[3] = {293.66, 369.99, 440.0};
        const double* ch = (k / 4) % 2 ? kD : kAm;
        for (size_t i = 0; i < sec(0.12) && at + i < x.size(); ++i) {
            const double t = double(i) / kFs;
            double s = 0;
            for (int j = 0; j < 3; ++j) s += 2 * std::fmod(ch[j] * t, 1.0) - 1;
            x[at + i] += float(s / 3 * std::exp(-t / 0.035));
        }
    }
    lowpass(x, 2500.0);
    normalise(x, -6.0f);
    return x;
}

// One click (-6 dBFS, 2 samples) at 1 s, like 07_click_single.wav.
Buf click(double seconds)
{
    Buf b(sec(seconds), 0.0f);
    b[sec(1.0)] = b[sec(1.0) + 1] = 0.5f;
    return b;
}

// A 0.5 s white noise burst at 1 s (-12 dBFS RMS), like 06_noise_bursts.wav's long one.
Buf noiseBurst(double seconds)
{
    Buf b(sec(seconds), 0.0f);
    rv::dsp::Rng rng;
    rng.seed(77u);
    for (size_t i = sec(1.0); i < sec(1.5); ++i) b[i] = 0.25f * 1.732f * rng.bipolar();
    return b;
}

Buf noise(size_t n, float amp, uint32_t seed)
{
    Buf b(n);
    rv::dsp::Rng rng;
    rng.seed(seed);
    for (auto& x : b) x = amp * rng.bipolar();
    return b;
}

// Click detector as in test_tank (docs/m1-contracts.md): |second difference|
// > 20 dB above its local +-10 ms RMS and above 1e-3.
int countClicks(const Buf& x, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = sec(0.010);
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

// RV_S3_ONLY=v runs one voicing's checks (tuning); the others then report
// voicing v's numbers again.
template <typename F>
auto perVoicing(F f)
{
    std::vector<std::future<decltype(f(1))>> jobs;
    const char* only = std::getenv("RV_S3_ONLY");
    for (int v = 1; v < kNumVoicings; ++v)
        jobs.push_back(std::async(inPass(v) ? std::launch::async : std::launch::deferred, f, only ? std::atoi(only) : v));
    std::vector<decltype(f(1))> out;
    for (int v = 1; v < kNumVoicings; ++v) out.push_back(inPass(v) ? jobs[size_t(v - 1)].get() : decltype(f(1)){});
    return out;
}

// ---- identity -------------------------------------------------------------------------------
void identity()
{
    const Buf h = hits(8.0), st = stabs(8.0);
    for (int v = 1; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        bool same = true;
        for (int sp = 0; sp < 2; ++sp)
            for (int a = 0; a < 3; ++a)
                for (const Buf* in : {&h, &st}) {
                    Settings s;
                    s.springs = sp, s.att = a;
                    const Stereo ref = render(s, *in);
                    s.voicing = v;
                    const Stereo o = render(s, *in);
                    same &= std::memcmp(ref.l.data(), o.l.data(), ref.l.size() * sizeof(float)) == 0
                         && std::memcmp(ref.r.data(), o.r.data(), ref.r.size() * sizeof(float)) == 0;
                }
        std::snprintf(msg, sizeof msg, "Voicing %s: SPRINGS 1 and 2 bit for bit as voicing 0 (hits + stabs, every ATTITUDE)",
                      kVoiceName[v]);
        checkV(v, same, msg);
    }
}

// ---- level ----------------------------------------------------------------------------------
void level()
{
    const Buf in = hits(12.0);
    struct Row {
        double st[3][2], mo[3][2], pw[3][2];
    };
    // Top 0.9: above it CLEAN (the default here) is the Hold (ADR 0040).
    const float decays[3] = {0.1f, 0.6f, 0.9f}, tensions[2] = {0.0f, 1.0f};
    double ref[3][2][3];
    for (int d = 0; d < 3; ++d)
        for (int k = 0; k < 2; ++k) {
            Settings s;
            s.decay = decays[d], s.tension = tensions[k], s.springs = 1;
            const Stereo o = render(s, in);
            ref[d][k][0] = loudnessDb(o);
            ref[d][k][1] = monoLoudnessDb(o);
            ref[d][k][2] = stereoDb(o, 0, o.l.size());
        }
    const auto rows = perVoicing([&](int v) {
        Row r{};
        for (int d = 0; d < 3; ++d)
            for (int k = 0; k < 2; ++k) {
                Settings s;
                s.decay = decays[d], s.tension = tensions[k], s.springs = 2, s.voicing = v;
                const Stereo o = render(s, in);
                r.st[d][k] = loudnessDb(o) - ref[d][k][0];
                r.mo[d][k] = monoLoudnessDb(o) - ref[d][k][1];
                r.pw[d][k] = stereoDb(o, 0, o.l.size()) - ref[d][k][2];
            }
        return r;
    });
    for (int v = 1; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        const Row& r = rows[size_t(v - 1)];
        bool ok = true;
        for (int d = 0; d < 3; ++d)
            for (int k = 0; k < 2; ++k)
                for (double x : {r.st[d][k], r.mo[d][k]}) ok &= std::fabs(x) <= 1.5;
        std::snprintf(msg, sizeof msg,
                      "Level, voicing %s, hits, SPRINGS 3 vs 2, K-weighted loudness stereo / mono: DECAY 0.1 %+.1f/%+.1f "
                      "%+.1f/%+.1f, 0.6 %+.1f/%+.1f %+.1f/%+.1f, 0.9 %+.1f/%+.1f %+.1f/%+.1f dB (TENSION 0, 1; limit +-1.5). "
                      "Plain power, stereo: %+.1f %+.1f, %+.1f %+.1f, %+.1f %+.1f",
                      kVoiceName[v], r.st[0][0], r.mo[0][0], r.st[0][1], r.mo[0][1], r.st[1][0], r.mo[1][0], r.st[1][1],
                      r.mo[1][1], r.st[2][0], r.mo[2][0], r.st[2][1], r.mo[2][1], r.pw[0][0], r.pw[0][1], r.pw[1][0],
                      r.pw[1][1], r.pw[2][0], r.pw[2][1]);
        checkV(v, ok, msg);
    }
}

// ---- stereo / mono --------------------------------------------------------------------------
void stereo()
{
    const Buf h = hits(8.0), st = stabs(8.0);
    struct Res {
        double corr = -9, loss = 99, notch = 99;
        char   at[3][80] = {};
        bool   ok = true;
    };
    const auto voiced = [&](int v) {
        std::array<Res, 2> res{};
        for (int k = 0; k < 2; ++k)
            for (float decay : {0.0f, 0.5f, 0.9f}) // above 0.9: the Hold (ADR 0040)
                for (float tension : {0.0f, 0.5f, 1.0f}) {
                    Settings s;
                    s.decay = decay, s.tension = tension, s.voicing = v;
                    const Stereo o = render(s, k ? st : h);
                    const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                    Res& r = res[size_t(k)];
                    char cell[80];
                    std::snprintf(cell, sizeof cell, "DECAY %.1f TENSION %.1f", double(decay), double(tension));
                    r.ok &= m.stereoCorrelation < 0.5 && m.monoLossDb >= -1.5 && m.monoNotchDb >= -6.0;
                    if (m.stereoCorrelation > r.corr) { r.corr = m.stereoCorrelation; std::snprintf(r.at[0], 80, "%s", cell); }
                    if (m.monoLossDb < r.loss) { r.loss = m.monoLossDb; std::snprintf(r.at[1], 80, "%s", cell); }
                    if (m.monoNotchDb < r.notch) { r.notch = m.monoNotchDb; std::snprintf(r.at[2], 80, "%s", cell); }
                }
        return res;
    };
    std::vector<std::future<std::array<Res, 2>>> jobs;
    for (int v = 0; v < kNumVoicings; ++v) jobs.push_back(std::async(inPass(v) ? std::launch::async : std::launch::deferred, voiced, v));
    for (int v = 0; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        const auto res = jobs[size_t(v)].get();
        for (int k = 0; k < 2; ++k) {
            const Res& r = res[size_t(k)];
            std::snprintf(msg, sizeof msg,
                          "Stereo, voicing %s, %s, SPRINGS 3 (DECAY x TENSION {0,.5,1}): max corr %.2f (%s; < 0.5), "
                          "min mono_loss %+.2f dB (%s; >= -1.5), deepest mono_notch %+.1f dB (%s; >= -6)",
                          kVoiceName[v], k ? "stabs" : "hits", r.corr, r.at[0], r.loss, r.at[1], r.notch, r.at[2]);
            if (v == 0) std::printf("INFO  %s\n", msg); // today: test_tank's
            else checkV(v, r.ok, msg);
        }
    }
}

// ---- ringing --------------------------------------------------------------------------------
void ringing()
{
    struct Res {
        int    n = 0, flagged = 0, steady = 0;
        double worst = 0;
        char   at[120] = "-";
    };
    const auto rows = perVoicing([](int v) {
        Res r;
        for (int inp = 0; inp < 2; ++inp)
            for (int a = 0; a < 3; ++a)
                for (float tension : {0.0f, 1.0f})
                    for (float wob : {0.0f, 0.5f, 1.0f}) {
                        Settings s;
                        s.voicing = v, s.att = a, s.tension = tension, s.wobble = wob, s.drive = 0.5f;
                        s.decay = a == 2 ? 0.75f : 0.9f; // KICKED DECAY 1 is the Howl zone; CLEAN / DRIVEN above 0.9 the Hold (ADR 0040)
                        const Stereo o = render(s, inp ? noiseBurst(14.0) : click(14.0));
                        const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                        ++r.n;
                        if (m.ringing) ++r.flagged;
                        if (m.steadyTone) ++r.steady;
                        if (m.ringing || m.steadyTone) {
                            std::printf("INFO    flagged: voicing %s, %s %s TENSION %.0f WOBBLE %.1f: ringing_db %.1f at %.0f Hz "
                                        "(ratio %.2f), steady %d\n",
                                        kVoiceName[v], inp ? "burst" : "click", kAttName[a], double(tension), double(wob),
                                        m.ringingDb, m.ringingHz, m.ringingRatio, int(m.steadyTone));
                        }
                        if (!std::isnan(m.ringingDb) && m.ringingDb > r.worst) {
                            r.worst = m.ringingDb;
                            std::snprintf(r.at, sizeof r.at, "%s %s TENSION %.0f WOBBLE %.1f, %.0f Hz", inp ? "burst" : "click",
                                          kAttName[a], double(tension), double(wob), m.ringingHz);
                        }
                    }
        return r;
    });
    for (int v = 1; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        const Res& r = rows[size_t(v - 1)];
        // In series (voicing 2) the steady-tone flag (SPEC §4.10: a peak 12 dB
        // proud for > 2 s above -30 dBFS) is reported, not checked: a
        // resonance both tanks share comes out stronger, and hot noise bursts
        // at DECAY max on a tight tank hold one for 2-2.5 s. LOOSENED for this
        // prototype voicing only (ADR 0037); today's position 3 holds the same
        // peak ~1.5 s here, and flags 1 cell of the M6 grid itself.
        const bool series = v == rv::springs3::kSeries;
        // Voicing 4 (pan tank, a palette candidate never shipped) flags 2
        // DRIVEN / CLEAN burst cells (TENSION 1, ~2.1 kHz, 18.7 dB) at DECAY
        // 0.9, where this check moved when DECAY 1 became the Hold (ADR
        // 0040). DECAY 0.9 is bit for bit as before the Hold: found on the
        // way, not caused by it. Reported, not checked, for that voicing only.
        const bool panTank = v == rv::springs3::kPan;
        std::snprintf(msg, sizeof msg,
                      "Ringing, voicing %s, SPRINGS 3 tails (click + burst, ATTITUDE, DECAY max below the Howl / Hold, TENSION 0/1, WOBBLE "
                      "0/.5/1): %d of %d flagged, worst ringing_db %.1f (%s; limit %.0f); steady tone %d%s",
                      kVoiceName[v], r.flagged, r.n, r.worst, r.at, rv::metrics::kRingingGrowthDb, r.steady,
                      series ? " (reported, not checked in series: ADR 0037)"
                             : panTank ? " (reported, not checked: pan tank at DECAY 0.9, never shipped)" : "");
        checkV(v, panTank || (r.flagged == 0 && (series || r.steady == 0)), msg);
    }
}

// ---- howl -----------------------------------------------------------------------------------
void howl()
{
    struct Res {
        char line[3][400];
        bool ok[3];
    };
    const auto rows = perVoicing([](int v) {
        Res r{};
        int k = 0;
        for (float wob : {0.0f, 0.5f, 1.0f}) {
            const size_t n = sec(16.0), pullAt = sec(10.0);
            const Buf in = click(16.0);
            rv::Tank t;
            t.prepare(kFs, 48);
            Settings s;
            s.voicing = v, s.att = 2, s.decay = 1.0f, s.drive = 0.5f, s.wobble = wob;
            apply(t, s);
            Stereo o{Buf(n), Buf(n)};
            for (size_t pos = 0; pos < n; pos += 48) {
                if (pos == pullAt) t.setParam(rv::ParamId::Decay, 0.75f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const Buf l(o.l.begin() + long(sec(4.0)), o.l.begin() + long(pullAt));
            const Buf rr(o.r.begin() + long(sec(4.0)), o.r.begin() + long(pullAt));
            const auto hm = rv::metrics::compute({l, rr}, kFs);
            const double before = stereoDb(o, pullAt - sec(0.5), pullAt);
            const double after3 = stereoDb(o, pullAt + sec(2.75), pullAt + sec(3.25));
            std::snprintf(r.line[k], sizeof r.line[k],
                          "Howl, voicing %s, SPRINGS 3 KICKED DECAY 1 DRIVE 0.5 WOBBLE %.1f (ADR 0019): sustains at %.1f dBFS; "
                          "floor %.1f dB (limit %.0f), steadiest 2 s moves %.2f %% / %.1f dB (limit %.1f %% or %.0f dB); "
                          "DECAY -> 0.75: %.1f dB lower 3 s later (ADR 0018, limit 30)",
                          kVoiceName[v], double(wob), before, hm.howlFloorDb, rv::metrics::kHowlFloorMinDb, hm.howlMovePct,
                          hm.howlMoveDb, rv::metrics::kHowlMovePct, rv::metrics::kHowlMoveDb, before - after3);
            r.ok[k++] = before > -40.0 && hm.howlOk && before - after3 >= 30.0;
        }
        return r;
    });
    for (int v = 1; v < kNumVoicings; ++v)
        if (inPass(v))
            for (int k = 0; k < 3; ++k) check(rows[size_t(v - 1)].ok[k], rows[size_t(v - 1)].line[k]);
}

// ---- switching ------------------------------------------------------------------------------
void switching()
{
    struct Res {
        char line[4][300];
        bool ok[4];
        char flip[200];
        bool flipOk;
    };
    const auto rows = perVoicing([](int v) {
        Res r{};
        const size_t n = sec(5.0);
        const Buf held = noise(n, 0.1f, 3u);
        Buf hit(n, 0.0f);
        {
            rv::dsp::Rng rng;
            rng.seed(4242u);
            for (int h = 0; h < 4; ++h) {
                const size_t at = sec(0.1) + size_t(h) * sec(1.0);
                for (size_t i = 0; i < sec(0.03); ++i) hit[at + i] = 0.5f * std::exp(-float(i) / (0.008f * kFs)) * rng.bipolar();
            }
        }
        const size_t tailFrom = sec(3.1) + sec(0.03);
        const int pairs[4][2] = {{1, 2}, {2, 1}, {0, 2}, {2, 0}};
        for (int k = 0; k < 4; ++k) {
            const int from = pairs[k][0], to = pairs[k][1];
            int clicks = 0;
            double worst = 0, gapDb = 0;
            for (int input = 0; input < 2; ++input) {
                const Buf& in = input == 0 ? held : hit;
                const size_t at = input == 0 ? sec(2.0) : tailFrom + sec(0.4);
                rv::Tank t;
                t.prepare(kFs, 16);
                Settings s;
                s.decay = 0.6f, s.springs = from, s.voicing = v;
                apply(t, s);
                Stereo o{Buf(n), Buf(n)};
                for (size_t pos = 0; pos < n; pos += 16) {
                    if (pos == (at / 16) * 16) t.setParam(rv::ParamId::Springs, rv::switchToNormalised(to));
                    t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
                }
                for (const Buf* ch : {&o.l, &o.r}) {
                    double q = 0;
                    clicks += countClicks(*ch, at - sec(0.1), &q);
                    worst = std::max(worst, q);
                }
                if (input == 0) {
                    const size_t w = sec(0.05);
                    for (const Buf* ch : {&o.l, &o.r})
                        gapDb = std::max(gapDb, std::fabs(db(power(*ch, at, at + w) / power(*ch, at - w, at))));
                }
            }
            std::snprintf(r.line[k], sizeof r.line[k],
                          "Switching, voicing %s: SPRINGS %d -> %d mid-tail (held + decaying): %d clicks (worst ratio %.1f, "
                          "limit 10), held level change across the fade %.1f dB (limit 2)",
                          kVoiceName[v], from + 1, to + 1, clicks, worst, gapDb);
            r.ok[k] = clicks == 0 && gapDb < 2.0;
        }
        // Rapid flipping 2 <-> 3 (every 5 ms, faster than the fade and the glide).
        rv::Tank t;
        t.prepare(kFs, 16);
        Settings s;
        s.decay = 0.6f, s.springs = 1, s.voicing = v;
        apply(t, s);
        Stereo o{Buf(n), Buf(n)};
        int flip = 0;
        for (size_t pos = 0; pos < n; pos += 16) {
            if (pos >= sec(1.0) && pos < sec(2.0) && pos % 240 == 0)
                t.setParam(rv::ParamId::Springs, rv::switchToNormalised(1 + (++flip % 2)));
            t.process(held.data() + pos, held.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
        }
        double r1 = 0, r2 = 0;
        const int c = countClicks(o.l, sec(0.9), &r1) + countClicks(o.r, sec(0.9), &r2);
        std::snprintf(r.flip, sizeof r.flip, "Switching, voicing %s: SPRINGS 2 <-> 3 every 5 ms for 1 s: %d clicks (worst ratio %.1f)",
                      kVoiceName[v], c, std::max(r1, r2));
        r.flipOk = c == 0;
        return r;
    });
    for (int v = 1; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        const auto& r = rows[size_t(v - 1)];
        for (int k = 0; k < 4; ++k) check(r.ok[k], r.line[k]);
        check(r.flipOk, r.flip);
    }
}

// ---- sustain (ADR 0035) -----------------------------------------------------------------------
// Raised-cosine attack / hold / release, framed by lead-in and tail silence (test_sustain_trim).
Buf withEnvelope(const Buf& body, double attack, double hold, double release, double lead = 1.0, double tail = 6.0)
{
    const size_t na = sec(attack), nh = sec(hold), nr = sec(release);
    Buf x(sec(lead) + na + nh + nr + sec(tail), 0.0f);
    for (size_t i = 0; i < na + nh + nr; ++i) {
        double e = 1.0;
        if (i < na) e = 0.5 - 0.5 * std::cos(kPi * double(i) / double(na));
        else if (i >= na + nh) e = 0.5 + 0.5 * std::cos(kPi * double(i - na - nh) / double(nr));
        x[sec(lead) + i] = float(e) * body[i];
    }
    normalise(x, -6.0f);
    return x;
}
constexpr double C2 = 65.406, C3 = 130.813, EB3 = 155.563, G3 = 195.998;
// test_sustain_trim's held sounds, transposed by `ratio` (1 = as there).
Buf pad(double ratio)
{
    const size_t n = sec(12.0);
    Buf b(n, 0.0f);
    std::mt19937 rng(10);
    std::uniform_real_distribution<double> ph(0.0, 1.0);
    for (auto [f, amp] : {std::pair{C2, 1.0}, {C3, 0.8}, {EB3, 0.7}, {G3, 0.7}})
        for (double c : {-7.0, 0.0, 7.0}) {
            const double inc = ratio * f * std::pow(2.0, c / 1200.0) / kFs;
            double p = ph(rng);
            for (size_t i = 0; i < n; ++i) {
                b[i] += float(amp / 3.0 * (2 * p - 1));
                if ((p += inc) >= 1.0) p -= 1.0;
            }
        }
    for (int k = 0; k < 2; ++k) lowpass(b, 700.0);
    return withEnvelope(b, 3.0, 6.0, 3.0);
}
Buf drone(double ratio)
{
    const size_t n = sec(12.0);
    Buf b(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = double(i) / kFs;
        b[i] = float(std::sin(2 * kPi * ratio * C2 * t) + 0.5 * std::sin(2 * kPi * 2 * ratio * C2 * t + 0.3));
    }
    return withEnvelope(b, 1.0, 10.0, 1.0);
}
Buf organ(double ratio)
{
    const size_t n = sec(8.07);
    Buf b(n, 0.0f);
    int k = 0;
    for (auto [f, a] : {std::pair{C2, 0.8}, {C3, 1.0}, {EB3, 1.0}, {G3, 1.0}})
        for (auto [h, d] : {std::pair{1, 1.0}, {2, 0.8}, {3, 0.6}, {4, 0.5}, {6, 0.3}, {8, 0.25}}) {
            const double w = 2 * kPi * ratio * f * h / kFs, p0 = 0.37 * k++;
            for (size_t i = 0; i < n; ++i) b[i] += float(a * d * std::sin(w * double(i) + p0));
        }
    return withEnvelope(b, 0.02, 8.0, 0.05);
}

// ADR 0035: the Sustain trim holds held sounds at the owner's settings
// (CLEAN, DRIVE 0, SPLASH 0, DECAY noon, -6 dBFS peak). How much a held note
// builds up depends on whether it lands on one of the tank's modes, so one
// pitch is a lottery: today's position 3 meets test_sustain_trim's bars on
// its C2 drone (limiter <= 2.3 dB) but pulls 7.9 dB on an F#2 one at the
// same settings. So each voicing is judged against today's position 3 over
// the held sounds transposed -5..+4 semitones (pad, drone, organ), at the
// tight corners where the tank builds most (TONE 0 / 0.5 x TENSION 0.8 / 1,
// WOBBLE default): its worst limiter pull no more than 1 dB over today's
// (or ADR 0035's own 3 dB for a moment, if that is more), its mean no more
// than 0.5 dB over, and the trim never past the voicing's
// ceiling (Springs3Voicing.h sustainMaxDb; 5 dB today). The
// as-written pitches against test_sustain_trim's own bars are reported.
void sustain()
{
    constexpr int kBlock = 16;
    constexpr float kWorstSlackDb = 1.0f, kMeanSlackDb = 0.5f;
    constexpr float kMomentDb = 3.0f; // test_sustain_trim's own bar for a moment's pull (ADR 0035)
    const double semis[] = {-5, -3, -1, 0, 2, 4};
    constexpr int kNumSemis = 6;
    const char* const names[3] = {"pad", "drone", "organ"};
    struct Res {
        float worst = 0, mean = 0, deep = 0, asWritten = 0, asWrittenOver = 0;
        char  at[120] = "none limited";
    };
    auto one = [&](int v, int k) {
        Res q;
        rv::Tank t;
        t.prepare(kFs, kBlock);
        int cells = 0;
        for (int si = 0; si < kNumSemis; ++si) {
            const double ratio = std::pow(2.0, semis[si] / 12.0);
            const Buf in = k == 0 ? pad(ratio) : k == 1 ? drone(ratio) : organ(ratio);
            for (float tone : {0.0f, 0.5f})
                for (float ten : {0.8f, 1.0f}) {
                    t.reset();
                    Settings s;
                    s.voicing = v, s.att = 0, s.drive = 0.0f, s.splash = 0.0f, s.decay = 0.5f, s.tone = tone, s.tension = ten;
                    apply(t, s);
                    Buf l(kBlock), r(kBlock);
                    float minGain = 1.0f, over = 0.0f, deep = 0.0f;
                    for (size_t pos = 0; pos < in.size(); pos += kBlock) {
                        const int n = int(std::min<size_t>(kBlock, in.size() - pos));
                        t.process(in.data() + pos, in.data() + pos, l.data(), r.data(), n);
                        minGain = std::min(minGain, t.limiterGain());
                        if (-20.0f * std::log10(t.limiterGain()) > 2.5f) over += float(kBlock) / kFs;
                        deep = std::min(deep, 20.0f * std::log10(t.sustainTrim()));
                    }
                    const float gr = std::max(0.0f, -20.0f * std::log10(minGain));
                    if (std::getenv("RV_S3_VERBOSE"))
                        std::printf("INFO    voicing %d %s %+.0f st TONE %.1f TENSION %.1f: limiter %.2f dB, trim %.2f dB\n", v,
                                    names[k], semis[si], double(tone), double(ten), double(gr), double(-deep));
                    if (gr > q.worst) {
                        q.worst = gr;
                        std::snprintf(q.at, sizeof q.at, "%+.0f semitones, TONE %.1f TENSION %.1f", semis[si], double(tone), double(ten));
                    }
                    q.mean += gr, ++cells;
                    q.deep = std::min(q.deep, deep);
                    if (semis[si] == 0.0) q.asWritten = std::max(q.asWritten, gr), q.asWrittenOver = std::max(q.asWrittenOver, over);
                }
        }
        q.mean /= float(cells);
        return q;
    };
    std::vector<std::future<Res>> jobs;
    const char* only = std::getenv("RV_S3_ONLY");
    for (int v = 0; v < kNumVoicings; ++v)
        for (int k = 0; k < 3; ++k)
            jobs.push_back(std::async((only && v > 0 && v != std::atoi(only)) || !inPass(v) ? std::launch::deferred : std::launch::async, one, v, k));
    Res res[kNumVoicings][3];
    for (int v = 0; v < kNumVoicings; ++v)
        for (int k = 0; k < 3; ++k)
            if ((!only || v == 0 || v == std::atoi(only)) && inPass(v)) res[v][k] = jobs[size_t(v * 3 + k)].get();
    for (int v = 0; v < kNumVoicings; ++v)
        for (int k = 0; k < 3; ++k) {
            const Res& q = res[v][k];
            const Res& t = res[0][k];
            std::snprintf(msg, sizeof msg,
                          "Sustain trim, voicing %s, held %s at -6 dBFS peak, CLEAN DRIVE 0 SPLASH 0 DECAY noon, SPRINGS 3, "
                          "-5..+4 semitones x TONE 0/0.5 x TENSION 0.8/1: limiter worst %.2f dB (%s), mean %.2f dB; today "
                          "%.2f / %.2f (slack %.1f, or %.0f dB / %.1f); trim at most %.2f dB (the voicing's ceiling %.1f). As written (0 semitones): "
                          "%.2f dB, past 2.5 dB for %.2f s (test_sustain_trim: 3 dB, 0.25 s)",
                          kVoiceName[v], names[k], double(q.worst), q.at, double(q.mean), double(t.worst), double(t.mean),
                          double(kWorstSlackDb), double(kMomentDb), double(kMeanSlackDb), double(-q.deep),
                          double(rv::springs3::kVoicings[size_t(v)].sustainMaxDb),
                          double(q.asWritten), double(q.asWrittenOver));
            if (only && v > 0 && v != std::atoi(only)) continue;
            if (!inPass(v)) continue;
            if (v == 0) std::printf("INFO  %s\n", msg);
            else check(q.worst <= std::max(t.worst + kWorstSlackDb, kMomentDb) && q.mean <= t.mean + kMeanSlackDb
                           && -q.deep <= rv::springs3::kVoicings[size_t(v)].sustainMaxDb + 0.01f, msg);
        }
}

// ---- stability --------------------------------------------------------------------------------
void stability()
{
    const Buf in = hits(10.0);
    const auto rows = perVoicing([&](int v) {
        float peak = 0.0f;
        bool  finite = true;
        for (int a = 0; a < 3; ++a)
            for (float tension : {0.0f, 1.0f})
                for (float tone : {0.0f, 1.0f}) {
                    Settings s;
                    s.voicing = v, s.att = a, s.decay = 1.0f, s.drive = 1.0f, s.tension = tension, s.tone = tone, s.splash = 1.0f;
                    const Stereo o = render(s, in);
                    for (const Buf* ch : {&o.l, &o.r})
                        for (float x : *ch) {
                            finite &= std::isfinite(x);
                            peak = std::max(peak, std::fabs(x));
                        }
                }
        return std::pair{finite, peak};
    });
    for (int v = 1; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        const auto [finite, peak] = rows[size_t(v - 1)];
        std::snprintf(msg, sizeof msg,
                      "Stability, voicing %s, SPRINGS 3 at DECAY 1 DRIVE 1 SPLASH 1, every ATTITUDE, TENSION/TONE 0 and 1: "
                      "finite, peak %.3f (< 1)",
                      kVoiceName[v], double(peak));
        checkV(v, finite && peak < 1.0f, msg);
    }
}

// ---- timing (round 2) -------------------------------------------------------------------------
// Owner, 2 Oct 2026: a different tank size moves the repeat timing and
// muddles TENSION and DECAY, so round 2 keeps today's. Per Spring, at 800 Hz
// (modes::kPickupAlignHz): the round trip (the echo spacing) and the first
// echo, against voicing 0, at TENSION 0..1 x TONE 0 / noon / 1. Limits:
// round trip within 0.05 %, first echo within 0.05 ms; except where the
// delay memory caps a Spring (the loosest tank's right Spring with a
// quicker Chirp): there the round trip may be up to 1.5 % short (reported).
// Also measured on the output (reported): the echo spacing, the strongest
// repeat of a click's < 1 kHz envelope between 20 and 150 ms.
double echoSpacingMs(const Stereo& o, size_t clickAt)
{
    Buf m(o.l.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
    lowpass(m, 1000.0);
    // Energy envelope, 1 ms smoothing, decimated to 0.5 ms steps.
    const size_t hop = sec(0.0005);
    std::vector<double> env;
    double y = 0;
    const double c = 1.0 - std::exp(-1.0 / (0.001 * kFs));
    for (size_t i = clickAt + sec(0.005); i < std::min(m.size(), clickAt + sec(1.5)); ++i) {
        y += c * (double(m[i]) * m[i] - y);
        if ((i - clickAt) % hop == 0) env.push_back(std::sqrt(y));
    }
    double mean = 0;
    for (double e : env) mean += e;
    mean /= double(env.size());
    for (double& e : env) e -= mean;
    double best = -1e30;
    size_t bestLag = 0;
    for (size_t lag = size_t(0.020 / 0.0005); lag <= size_t(0.150 / 0.0005); ++lag) {
        double acc = 0;
        for (size_t i = 0; i + lag < env.size(); ++i) acc += env[i] * env[i + lag];
        if (acc > best) best = acc, bestLag = lag;
    }
    return 1000.0 * double(bestLag) * 0.0005;
}

void timing()
{
    struct Res {
        double rtWorst = 0, echoWorst = 0, capped = 0;
        char   rtAt[100] = "-", echoAt[100] = "-", capAt[100] = "";
        double spacing[3] = {0, 0, 0};
    };
    const float tensions[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    auto settle = [](int v, float tension, float tone, double rt[3], double fe[3]) {
        rv::Tank t;
        t.prepare(kFs, 32);
        Settings s;
        s.voicing = v, s.tension = tension, s.tone = tone;
        apply(t, s);
        Buf z(sec(0.3), 0.0f), l(z.size()), r(z.size());
        for (size_t pos = 0; pos < z.size(); pos += 32) t.process(z.data() + pos, z.data() + pos, l.data() + pos, r.data() + pos, 32);
        for (int k = 0; k < 3; ++k) {
            rt[k] = t.spring(k).roundTripSamples(rv::modes::kPickupAlignHz);
            fe[k] = t.spring(k).firstEchoSamples(rv::modes::kPickupAlignHz);
        }
    };
    const Buf in = click(3.0);
    std::vector<std::future<Res>> jobs;
    for (int v = 0; v < kNumVoicings; ++v)
        jobs.push_back(std::async(std::launch::async, [&, v] {
            Res r;
            for (float tension : tensions)
                for (float tone : {0.0f, 0.5f, 1.0f}) {
                    double rt0[3], fe0[3], rt[3], fe[3];
                    settle(0, tension, tone, rt0, fe0);
                    settle(v, tension, tone, rt, fe);
                    for (int k = 0; k < 3; ++k) {
                        const double drt = 100.0 * (rt[k] / rt0[k] - 1.0), dfe = 1000.0 * (fe[k] - fe0[k]) / kFs;
                        const bool capped = k == 1 && tension == 0.0f && drt < -0.05;
                        if (capped && std::fabs(drt) > std::fabs(r.capped)) {
                            r.capped = drt;
                            std::snprintf(r.capAt, sizeof r.capAt, "Spring B, TENSION 0, TONE %.1f", double(tone));
                        }
                        if (!capped && std::fabs(drt) > std::fabs(r.rtWorst)) {
                            r.rtWorst = drt;
                            std::snprintf(r.rtAt, sizeof r.rtAt, "Spring %c, TENSION %.2f, TONE %.1f", 'A' + k, double(tension), double(tone));
                        }
                        if (std::fabs(dfe) > std::fabs(r.echoWorst)) {
                            r.echoWorst = dfe;
                            std::snprintf(r.echoAt, sizeof r.echoAt, "Spring %c, TENSION %.2f, TONE %.1f", 'A' + k, double(tension), double(tone));
                        }
                    }
                }
            for (int k = 0; k < 3; ++k) {
                Settings s;
                s.voicing = v, s.tension = tensions[k * 2];
                r.spacing[k] = echoSpacingMs(render(s, in), sec(1.0));
            }
            return r;
        }));
    std::vector<Res> res;
    for (auto& j : jobs) res.push_back(j.get());
    for (int v = 0; v < kNumVoicings; ++v) {
        if (!inPass(v)) continue;
        const Res& r = res[size_t(v)];
        const Res& t = res[0];
        std::snprintf(msg, sizeof msg,
                      "Timing, voicing %s vs today, every Spring at 800 Hz, TENSION 0..1 x TONE 0/.5/1: round trip %+.3f %% (%s; "
                      "limit 0.05), first echo %+.3f ms (%s; limit 0.05)%s%.2f %%%s%s. Echo spacing on the output, TENSION 0 / "
                      ".5 / 1: %.1f / %.1f / %.1f ms (today %.1f / %.1f / %.1f)",
                      kVoiceName[v], r.rtWorst, r.rtAt, r.echoWorst, r.echoAt,
                      r.capAt[0] ? "; memory cap: round trip " : "", r.capped, r.capAt[0] ? ", " : "", r.capAt, r.spacing[0],
                      r.spacing[1], r.spacing[2], t.spacing[0], t.spacing[1], t.spacing[2]);
        if (!rv::springs3::kVoicings[size_t(v)].keepTiming) std::printf("INFO  %s\n", msg); // round 1: changes it on purpose
        else checkV(v, std::fabs(r.rtWorst) <= 0.05 && std::fabs(r.echoWorst) <= 0.05 && r.capped >= -1.5, msg);
    }
}

// ---- first arrivals (reported) ----------------------------------------------------------------
void arrivals()
{
    for (int v = 0; v < kNumVoicings; ++v) {
        const size_t at = sec(0.05), n = at + sec(0.25);
        Buf in(n, 0.0f);
        in[at] = 0.5f;
        Settings s;
        s.voicing = v;
        const Stereo o = render(s, in);
        Buf mono(n);
        for (size_t i = 0; i < n; ++i) mono[i] = o.l[i] + o.r[i];
        double ms[3];
        int k = 0;
        const Buf* const chans[3] = {&o.l, &o.r, &mono};
        for (const Buf* raw : chans) {
            Buf lp = *raw;
            lowpass(lp, 1000.0);
            // The first echo's onset: the first moment the < 1 kHz envelope
            // reaches -12 dB of its peak over the first 200 ms (the Springs'
            // lengths differ by voicing, so test_tank's centroid would also
            // count a short tank's second echo).
            double pk = 0;
            for (size_t i = at; i < n; ++i) pk = std::max(pk, double(std::fabs(lp[i])));
            size_t i = at;
            while (i < n && std::fabs(lp[i]) < 0.25 * pk) ++i;
            ms[k++] = 1000.0 * double(i - at) / kFs;
        }
        std::printf("INFO  First echo onset (< 1 kHz, -12 dB of its peak), voicing %s, SPRINGS 3 at defaults: L %.1f, R %.1f, "
                    "mono %.1f ms\n",
                    kVoiceName[v], ms[0], ms[1], ms[2]);
    }
}

// ---- character (reported) ---------------------------------------------------------------------
// Power-weighted mean frequency (50 Hz-12 kHz) of x[from, to), 4096-point
// Hann frames; and the shares of power under 400 Hz and over 1 kHz.
struct Centroid {
    double hz = 0, low = 0, high = 0, powerDb = 0;
};
Centroid centroid(const Buf& x, size_t from, size_t to)
{
    constexpr size_t kN = 4096;
    std::vector<double> acc(kN / 2 + 1, 0.0);
    for (size_t at = from; at + kN <= std::min(to, x.size()); at += kN / 2) {
        const auto mag = rv::fft::magnitudeSpectrum(x.data() + at, kN, kN);
        for (size_t k = 0; k < mag.size(); ++k) acc[k] += double(mag[k]) * mag[k];
    }
    double sum = 0, moment = 0, low = 0, high = 0;
    for (size_t k = 0; k < acc.size(); ++k) {
        const double hz = double(k) * kFs / kN;
        if (hz < 50.0 || hz > 12000.0) continue;
        sum += acc[k], moment += acc[k] * hz;
        if (hz < 400.0) low += acc[k];
        if (hz > 1000.0) high += acc[k];
    }
    return {moment / sum, 100.0 * low / sum, 100.0 * high / sum, db(power(x, from, to))};
}

// How "drippy" vs smooth a click's first half second is: the 2 ms RMS
// envelope of the mono output (full band), 50-500 ms after the click, as
// its standard deviation over its mean (dB-free). Discrete drips with
// silence between read high; a dense, blooming wash reads low.
double dripIndex(const Buf& mono, size_t clickAt)
{
    const size_t w = sec(0.002);
    std::vector<double> env;
    for (size_t at = clickAt + sec(0.05); at + w <= clickAt + sec(0.5); at += w) env.push_back(std::sqrt(power(mono, at, at + w)));
    double mean = 0, var = 0;
    for (double e : env) mean += e;
    mean /= double(env.size());
    for (double e : env) var += (e - mean) * (e - mean);
    return std::sqrt(var / double(env.size())) / mean;
}

void character()
{
    const Buf in = click(8.0);
    for (int v = -1; v < kNumVoicings; ++v)
        for (float decay : {0.5f, 0.85f}) {
            Settings s;
            s.decay = decay;
            if (v < 0) s.springs = 1;
            else s.voicing = v;
            const Stereo o = render(s, in);
            const auto m = rv::metrics::compute({o.l, o.r}, kFs);
            // Brightness: the tail 0.1-1.5 s after the click (mono), and each
            // side alone (a lean: one ear brighter or louder than the other).
            Buf mono(o.l.size());
            for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (o.l[i] + o.r[i]);
            const Centroid c = centroid(mono, sec(1.1), sec(2.5)), cl = centroid(o.l, sec(1.1), sec(2.5)),
                           cr = centroid(o.r, sec(1.1), sec(2.5));
            std::printf("INFO  Character, %s, one click, CLEAN, DECAY %.2f, other knobs default: T60 %.2f s, tail centroid "
                        "%.0f Hz (share under 400 Hz %.0f %%), L/R correlation %.2f, mono_loss %+.2f dB; left vs right: "
                        "centroid %.0f / %.0f Hz, level L-R %+.1f dB; drip index (0.05-0.5 s) %.2f\n",
                        v < 0 ? "SPRINGS 2" : kVoiceName[v], double(decay), m.t60S, c.hz, c.low, m.stereoCorrelation,
                        m.monoLossDb, cl.hz, cr.hz, cl.powerDb - cr.powerDb, dripIndex(mono, sec(1.0)));
        }
    // A held chord (the C minor pad, as written), CLEAN, defaults: does it
    // lean to the higher harmonics (owner on round 1's wide)? Mono and each
    // side, while it's held (4-8 s).
    const Buf p = pad(1.0);
    for (int v = -1; v < kNumVoicings; ++v) {
        Settings s;
        if (v < 0) s.springs = 1;
        else s.voicing = v;
        const Stereo o = render(s, p);
        Buf mono(o.l.size());
        for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (o.l[i] + o.r[i]);
        const Centroid c = centroid(mono, sec(4.0), sec(8.0)), cl = centroid(o.l, sec(4.0), sec(8.0)),
                       cr = centroid(o.r, sec(4.0), sec(8.0));
        std::printf("INFO  Held chord, %s, pad, CLEAN, defaults: centroid %.0f Hz (over 1 kHz %.1f %%, under 400 Hz %.0f %%); "
                    "left / right %.0f / %.0f Hz, level L-R %+.1f dB\n",
                    v < 0 ? "SPRINGS 2" : kVoiceName[v], c.hz, c.high, c.low, cl.hz, cr.hz, cl.powerDb - cr.powerDb);
    }
}

// ---- cost -------------------------------------------------------------------------------------
void cost()
{
    // Same worst case as test_tank / test_drive: 3 Springs, KICKED, DRIVE 1,
    // DECAY/TONE 1, TENSION 0 (loosest: most stages, longest L), steady noise.
    // Best of kRuns, interleaved, so machine noise hits every voicing alike.
    const size_t n = sec(6.0);
    const Buf in = noise(n, 0.3f, 5u);
    constexpr int kRuns = 7;
    // Also at noon TENSION (diffuse runs more stages there; the loosest tank,
    // the worst case, is unchanged).
    double best[kNumVoicings], bestNoon[kNumVoicings];
    std::fill(best, best + kNumVoicings, 1e30);
    std::fill(bestNoon, bestNoon + kNumVoicings, 1e30);
    Buf l(n), r(n);
    for (int run = 0; run < kRuns; ++run)
        for (int v = 0; v < kNumVoicings; ++v)
            for (int noon = 0; noon < 2; ++noon) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings s;
                s.voicing = v, s.att = 2, s.drive = 1.0f, s.decay = 1.0f, s.tone = 1.0f, s.tension = noon ? 0.5f : 0.0f;
                apply(t, s);
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < n; pos += 48) t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                double& b = noon ? bestNoon[v] : best[v];
                b = std::min(b, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n));
            }
    for (int v = 0; v < kNumVoicings; ++v) {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.voicing = v, s.tension = 0.0f;
        apply(t, s);
        Buf z(480, 0.0f), zl(480), zr(480);
        t.process(z.data(), z.data(), zl.data(), zr.data(), 480);
        int stages = 0;
        float maxL = 0;
        for (int k = 0; k < 3; ++k) {
            stages += int(t.spring(k).activeStages());
            maxL = std::max(maxL, t.spring(k).loopDelaySamples());
        }
        s.tension = 0.5f;
        apply(t, s);
        t.reset();
        t.process(z.data(), z.data(), zl.data(), zr.data(), 480);
        float noonL = 0;
        for (int k = 0; k < 3; ++k) noonL = std::max(noonL, t.spring(k).loopDelaySamples());
        std::printf("INFO  Cost, voicing %s, SPRINGS 3 worst case (KICKED DRIVE 1 DECAY/TONE 1 TENSION 0): %.1f ns/sample "
                    "desktop (%+.1f %% vs voicing 0), %d stages; at noon TENSION %.1f ns/sample (%+.1f %%); longest L %.1f ms "
                    "at TENSION 0, %.1f ms at noon\n",
                    kVoiceName[v], best[v], 100.0 * (best[v] / best[0] - 1.0), stages, bestNoon[v],
                    100.0 * (bestNoon[v] / bestNoon[0] - 1.0), 1000.0 * double(maxL) / kFs, 1000.0 * double(noonL) / kFs);
    }
    std::printf("INFO  Memory: pool %zu floats at 48 kHz (firmware 30000), every voicing; Tank object %zu bytes\n",
                rv::Tank::requiredPoolFloats(kFs), sizeof(rv::Tank));
}

} // namespace

int main(int argc, char** argv)
{
    const std::string only = argc > 1 ? argv[1] : "";
    auto run = [&](const char* name, void (*f)()) {
        if (only.empty() || only == name) f();
    };
    auto all = [&] {
        run("identity", identity);
        run("level", level);
        run("stereo", stereo);
        run("ringing", ringing);
        run("howl", howl);
        run("switching", switching);
        run("sustain", sustain);
        run("stability", stability);
        run("timing", timing);
        run("arrivals", arrivals);
        run("character", character);
        run("cost", cost);
    };
    std::printf("== Pass 1: tank voicing %d, low cut step %d (as shipped): SPRINGS 3 voicing 0 and the default, %s\n",
                gTankVoicing, gFLowCut, kVoiceName[rv::springs3::kDefaultVoicing]);
    all();
    gTankVoicing = rv::tankv::kToday;
    gPass        = 2;
    std::printf("== Pass 2: tank voicing 0 (what they were made on): the round 1 / 2 reference SPRINGS 3 voicings\n");
    all();
    gTankVoicing = rv::tankv::kGentleWide;
    gFLowCut     = 0;
    gPass        = 3;
    std::printf("== Pass 3: tank voicing 7 with F's own low cut (what they were made on): F round 2's references\n");
    all();
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
