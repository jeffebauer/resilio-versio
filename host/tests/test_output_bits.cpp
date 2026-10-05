// The output's bit depth (ADR 0042, owner 4 Oct 2026; core/dsp/OutputBits.h,
// core/params/OutputVoicing.h, docs/prototypes/output-mulaw/README.md).
// Renderer key output_bits_voicing: 0 = before the box (the "today" reference), 1 = the default, DRIVEN 24 kHz / 12-bit
// mu-law and KICKED 24 kHz / 10-bit mu-law (8-bit until the 5 Oct 2026 amendment) on the whole output after MIX
// (dry and wet), CLEAN untouched.
//
// Checks:
//  - the default is voicing 1, and an untouched Tank renders bit for bit as it;
//  - voicing 1 in CLEAN renders bit for bit as today (every SPRINGS, MIX 0 /
//    0.4 / 1), and CLEAN MIX 0 is still the input exactly;
//  - level: K-weighted loudness within +-0.5 dB of today per ATTITUDE
//    (hits, skank, pad; every SPRINGS; MIX 0 / 0.4 / 1);
//  - no new pitches: no narrow peak in the tail that today doesn't have (a
//    rim, a 1 kHz and an 1130 Hz tone burst), with a positive control (the
//    same quantiser without the half-band filters) to show the check sees
//    folding; M6 (Ringing, steady tone) on long tails and the SPRINGS 3 grid;
//  - silence in -> exact silence out; the dry alone falls to exact 0 within
//    a few ms; a tail ends in exact 0;
//  - ATTITUDE flips mid-tail and on a dry tone: no clicks;
//  - deterministic, block-size free;
//  - INFO: latency (samples / ms), noise floor per mode (a 1 kHz tone at
//    -6 ... -80 dBFS, dry only), desktop cost.

#include "../../firmware/LedMeter.h"

#include "dsp/Filters.h"
#include "dsp/Tank.h"
#include "params/Mappings.h"
#include "params/OutputVoicing.h"
#include "params/ParamSpec.h"
#include "Fft.h"
#include "Metrics.h"
#include "Wav.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

int  failures = 0;
char msg[700];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}
void info(const char* what) { std::printf("INFO  %s\n", what); }

using Buf = std::vector<float>;
constexpr float  kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;
size_t sec(double s) { return size_t(s * double(kFs)); }
const char* const kAtt[3] = {"CLEAN", "DRIVEN", "KICKED"};

// The page's settings: DECAY noon, 2 Springs, TONE / TENSION noon, SPLASH 0.3, DRIVE 0.4.
struct Settings {
    float decay = 0.5f, tension = 0.5f, tone = 0.5f, mix = 1.0f, drive = 0.4f, splash = 0.3f;
    int   att = 0, springs = 1;
};

struct Stereo {
    Buf l, r;
};

void apply(rv::Tank& t, const Settings& s)
{
    using rv::ParamId;
    t.setParam(ParamId::Decay, s.decay);
    t.setParam(ParamId::Tension, s.tension);
    t.setParam(ParamId::Tone, s.tone);
    t.setParam(ParamId::Mix, s.mix);
    t.setParam(ParamId::Drive, s.drive);
    t.setParam(ParamId::Splash, s.splash);
    t.setParam(ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(ParamId::Springs, rv::switchToNormalised(s.springs));
}

// voicing < 0: never call the hook (an untouched Tank). flips: (sample, ATTITUDE).
Stereo render(const Settings& s, const Buf& inL, const Buf& inR, int voicing, int block = 48,
              const std::vector<std::pair<size_t, int>>& flips = {})
{
    rv::Tank t;
    t.prepare(kFs, block);
    apply(t, s);
    if (voicing >= 0) t.setOutputBitsVoicing(voicing);
    const size_t n = inL.size();
    Stereo o{Buf(n), Buf(n)};
    size_t f = 0;
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        while (f < flips.size() && flips[f].first <= pos) {
            t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(flips[f].second));
            ++f;
        }
        const int m = int(std::min<size_t>(size_t(block), n - pos));
        t.process(inL.data() + pos, inR.data() + pos, o.l.data() + pos, o.r.data() + pos, m);
    }
    return o;
}
Stereo render(const Settings& s, const Buf& in, int voicing, int block = 48, const std::vector<std::pair<size_t, int>>& flips = {})
{
    return render(s, in, in, voicing, block, flips);
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
bool same(const Stereo& a, const Stereo& b)
{
    return a.l.size() == b.l.size() && std::memcmp(a.l.data(), b.l.data(), a.l.size() * sizeof(float)) == 0
        && std::memcmp(a.r.data(), b.r.data(), a.r.size() * sizeof(float)) == 0;
}
bool finite(const Stereo& o)
{
    for (size_t i = 0; i < o.l.size(); ++i)
        if (!std::isfinite(o.l[i]) || !std::isfinite(o.r[i])) return false;
    return true;
}

// BS.1770 K-weighting at 48 kHz (shelf + RLB high-pass), both channels, ungated.
double kLoudnessDb(const Stereo& s)
{
    double sum = 0;
    for (const Buf* ch : {&s.l, &s.r}) {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0, z1 = 0, z2 = 0, w1 = 0, w2 = 0;
        for (float xv : *ch) {
            const double x = xv;
            const double y = 1.53512485958697 * x - 2.69169618940638 * x1 + 1.19839281085285 * x2 + 1.69065929318241 * y1 - 0.73248077421585 * y2;
            x2 = x1, x1 = x, y2 = y1, y1 = y;
            const double w = y - 2.0 * z1 + z2 + 1.99004745483398 * w1 - 0.99007225036621 * w2;
            z2 = z1, z1 = y, w2 = w1, w1 = w;
            sum += w * w;
        }
    }
    return db(sum / double(s.l.size()));
}

void normalise(Buf& x, float peakDb)
{
    float p = 0.0f;
    for (float v : x) p = std::max(p, std::fabs(v));
    const float g = std::pow(10.0f, peakDb / 20.0f) / std::max(p, 1.0e-9f);
    for (float& v : x) v *= g;
}
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

// ---- material ----------------------------------------------------------------------------------
// Snare-like hits (185 Hz body + noise), 1.5 s apart, -6 dBFS peak. The
// noise as test_mix / test_drive (800 Hz - 7 kHz, one-pole), or full band
// (bright: white noise up to 24 kHz, a hi-hat's worth of top octave).
Buf hits(double seconds, bool bright = false)
{
    Buf x(sec(seconds), 0.0f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    rv::dsp::OnePoleLowpass lp, hp;
    lp.setCutoff(7000.0f, kFs);
    hp.setCutoff(800.0f, kFs);
    for (double at = 0.5; at + 0.25 < seconds; at += 1.5) {
        const size_t a = sec(at);
        for (size_t i = 0; i < sec(0.25); ++i) {
            const double t = double(i) / kFs;
            float nz = u(rng);
            if (!bright) {
                nz = lp.process(nz);
                nz = 2.0f * (nz - hp.process(nz));
            }
            x[a + i] = float(0.6 * std::sin(2 * kPi * 185 * t) * std::exp(-t / 0.03) + 1.2 * nz * std::exp(-t / 0.06));
        }
    }
    normalise(x, -6.0f);
    return x;
}
// Skank stabs (offbeats at 75 bpm, A minor / D major), -6 dBFS peak.
Buf stabs(double seconds)
{
    Buf x(sec(seconds), 0.0f);
    const double beat = 60.0 / 75.0;
    for (int k = 0; 0.5 + k * beat + beat / 2 + 0.12 < seconds; ++k) {
        const size_t at = sec(0.5 + k * beat + beat / 2);
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
// A C minor pad (C3 Eb3 G3 saws, 1.2 kHz low-pass), 0.6 s swell, held, 0.5 s release; -6 dBFS peak.
Buf pad(double seconds, double hold = 4.0)
{
    Buf x(sec(seconds), 0.0f);
    static constexpr double kF[3] = {130.81, 155.56, 196.0};
    for (size_t i = sec(0.3); i < std::min(x.size(), sec(0.3 + hold + 0.5)); ++i) {
        const double t = double(i - sec(0.3)) / kFs;
        const double env = t < 0.6 ? t / 0.6 : (t < hold ? 1.0 : std::max(0.0, 1.0 - (t - hold) / 0.5));
        double s = 0;
        for (int j = 0; j < 3; ++j) s += 2 * std::fmod(kF[j] * (1.0 + 0.002 * j) * t, 1.0) - 1;
        x[i] = float(env * s / 3);
    }
    lowpass(x, 1200.0);
    normalise(x, -6.0f);
    return x;
}
// A rim-like hit (click + 420 / 1150 / 2300 Hz partials), at `at` seconds, -6 dBFS.
void addRim(Buf& b, double at)
{
    rv::dsp::Rng rng;
    rng.seed(99u);
    Buf r(sec(0.12));
    for (size_t i = 0; i < r.size(); ++i) {
        const double t = double(i) / kFs;
        r[i] = float(0.5 * std::exp(-t / 0.002) * rng.bipolar() + 0.35 * std::exp(-t / 0.03) * std::sin(2 * kPi * 420 * t)
                     + 0.3 * std::exp(-t / 0.02) * std::sin(2 * kPi * 1150 * t) + 0.2 * std::exp(-t / 0.012) * std::sin(2 * kPi * 2300 * t));
    }
    normalise(r, -6.0f);
    for (size_t i = 0; i < r.size() && sec(at) + i < b.size(); ++i) b[sec(at) + i] += r[i];
}
Buf rims(double seconds, std::initializer_list<double> at)
{
    Buf b(sec(seconds), 0.0f);
    for (double a : at) addRim(b, a);
    return b;
}
// A tone burst (5 ms ramps), peak `peakDb`.
Buf toneBurst(double seconds, double hz, double from, double len, double peakDb = -6.0)
{
    Buf b(sec(seconds), 0.0f);
    const double amp = std::pow(10.0, peakDb / 20.0), ramp = 0.005;
    for (size_t i = 0; i < sec(len); ++i) {
        const double t = double(i) / kFs;
        const double e = std::min({1.0, t / ramp, (len - t) / ramp});
        b[sec(from) + i] = float(amp * std::max(0.0, e) * std::sin(2 * kPi * hz * t));
    }
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

// Click detector as test_tank / test_springs3: |second difference| > 10x its
// local +-10 ms RMS and above 1e-3.
int countClicks(const Buf& x, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = sec(0.010);
    int    clicks = 0;
    double worst  = 0;
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
int clicksBoth(const Stereo& o, size_t from, double* worst)
{
    double a = 0, b = 0;
    const int c = countClicks(o.l, from, &a) + countClicks(o.r, from, &b);
    if (worst) *worst = std::max(a, b);
    return c;
}

// ---- spectra -----------------------------------------------------------------------------------
constexpr size_t kWelch = 4096; // 11.7 Hz bins
double binHz(size_t k) { return double(k) * kFs / double(kWelch); }

// Welch power spectrum (dB) of the L+R sum over [from, to): 4096-point Hann, half overlap.
std::vector<double> welchDb(const Stereo& o, size_t from, size_t to)
{
    Buf m(to - from);
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[from + i] + o.r[from + i]);
    std::vector<double> acc(kWelch / 2 + 1, 0.0);
    int frames = 0;
    for (size_t a = 0; a + kWelch <= m.size(); a += kWelch / 2, ++frames) {
        const auto mag = rv::fft::magnitudeSpectrum(m.data() + a, kWelch, kWelch);
        for (size_t k = 0; k < acc.size(); ++k) acc[k] += double(mag[k]) * mag[k];
    }
    for (auto& v : acc) v = db(v / std::max(1, frames));
    return acc;
}
constexpr double kTopHz = 10000.0;
// Prominence of bin k over its +-1/3-octave median (bins more than 3 away, none past the box's passband).
double prominence(const std::vector<double>& v, size_t k)
{
    const double hz = binHz(k), lo = hz / 1.26, hi = hz * 1.26;
    std::vector<double> nb;
    for (size_t m = 1; m < v.size(); ++m) {
        const double f = binHz(m);
        if (f >= lo && f <= hi && f <= kTopHz + 400.0 && (m + 3 < k || m > k + 3)) nb.push_back(v[m]);
    }
    if (nb.size() < 4) return 0.0;
    std::nth_element(nb.begin(), nb.begin() + long(nb.size() / 2), nb.end());
    return v[k] - nb[nb.size() / 2];
}
// New narrow peaks in `b` that today's `a` doesn't have, 100 Hz - 10 kHz
// (the box is flat to 10.6 kHz; above, its filters' slope reads as a "peak"), over [from, to): a bin counts when it stands above its 1/3-octave median
// both in b itself and in b's spectrum relative to a's (so a broad change of
// colour or a raised noise floor doesn't count; a new pitch, narrow, does).
// Bins more than 70 dB under b's loudest are ignored (the float floor).
// harmonicOf > 0: bins within 2 bins of a multiple of it are skipped (a tone's
// own harmonics are allowed; inharmonic peaks are what folding makes).
double newPeakDb(const Stereo& a, const Stereo& b, size_t from, size_t to, double* atHz, double harmonicOf = 0.0)
{
    const auto sa = welchDb(a, from, to), sb = welchDb(b, from, to);
    std::vector<double> rel(sb.size());
    for (size_t k = 0; k < sb.size(); ++k) rel[k] = sb[k] - sa[k];
    const double floorDb = *std::max_element(sb.begin(), sb.end()) - 70.0;
    double worst = 0.0;
    for (size_t k = 1; k < sb.size(); ++k) {
        const double hz = binHz(k);
        if (hz < 100.0 || hz > kTopHz || sb[k] < floorDb) continue;
        if (harmonicOf > 0.0) {
            const double h = std::round(hz / harmonicOf) * harmonicOf;
            if (std::fabs(hz - h) <= 2.0 * kFs / double(kWelch)) continue;
        }
        const double p = std::min(prominence(sb, k), prominence(rel, k));
        if (p > worst) {
            worst = p;
            if (atHz) *atHz = hz;
        }
    }
    return worst;
}

// The positive control: today's output through the same mu-law quantiser
// WITHOUT the half-band filters or the dither (every other sample kept and
// held, rounded to nearest): what the box would sound like if it folded.
Stereo naiveBox(const Stereo& o, float bits)
{
    const float q = std::exp2(bits - 1.0f), lnMu1 = std::log1p(255.0f);
    auto qz = [&](float x) {
        const float s = x < 0 ? -1.0f : 1.0f, a = std::min(1.0f, std::fabs(x));
        const float v = std::min(q - 1.0f, std::nearbyint(std::log1p(255.0f * a) / lnMu1 * q));
        return s * std::expm1(v / q * lnMu1) / 255.0f;
    };
    Stereo r = o;
    for (Buf* ch : {&r.l, &r.r})
        for (size_t i = 0; i + 1 < ch->size(); i += 2) (*ch)[i] = (*ch)[i + 1] = qz((*ch)[i]);
    return r;
}

// ---- checks ------------------------------------------------------------------------------------
void identity()
{
    std::snprintf(msg, sizeof msg, "the default output_bits_voicing is 1 (mu-law, ADR 0042): %d", rv::outbits::kOutputBitsDefault);
    check(rv::outbits::kOutputBitsDefault == 1, msg);
    const Buf h = hits(4.0);
    int ok = 0, cells = 0;
    for (int sp = 0; sp < 3; ++sp)
        for (int a = 0; a < 3; ++a) {
            Settings s;
            s.springs = sp, s.att = a, s.mix = 0.4f;
            ok += same(render(s, h, -1), render(s, h, 1)) ? 1 : 0;
            ++cells;
        }
    std::snprintf(msg, sizeof msg, "voicing 1 = an untouched Tank (the default), bit for bit (hits, MIX 0.4, SPRINGS x ATTITUDE): %d of %d", ok, cells);
    check(ok == cells, msg);
}

void cleanUntouched()
{
    const Buf h = hits(4.0), st = stabs(4.0);
    int ok = 0, cells = 0;
    for (int sp = 0; sp < 3; ++sp)
        for (float mix : {0.0f, 0.4f, 1.0f})
            for (const Buf* in : {&h, &st}) {
                Settings s;
                s.springs = sp, s.mix = mix;
                ok += same(render(s, *in, 0), render(s, *in, 1)) ? 1 : 0;
                ++cells;
            }
    std::snprintf(msg, sizeof msg, "voicing 1, CLEAN = today bit for bit (hits + skank, every SPRINGS, MIX 0 / 0.4 / 1): %d of %d", ok, cells);
    check(ok == cells, msg);
    // CLEAN MIX 0 is still the input exactly (stereo, different L and R).
    const Buf l = noise(sec(2.0), 0.5f, 7u), r = noise(sec(2.0), 0.5f, 8u);
    Settings s;
    s.mix = 0.0f;
    const Stereo o = render(s, l, r, 1);
    std::snprintf(msg, sizeof msg, "voicing 1, CLEAN MIX 0 = the input, bit for bit: %d", int(o.l == l && o.r == r));
    check(o.l == l && o.r == r, msg);
}

// Today's output through the box's two half-band filters alone (no bits):
// the reference for what the bits themselves do to the level.
Stereo filtersOnly(const Stereo& o)
{
    Stereo r = o;
    for (Buf* ch : {&r.l, &r.r}) {
        rv::dsp::Halfband dn, up;
        float held = 0.0f, pend = 0.0f;
        for (size_t i = 0; i < ch->size(); ++i) {
            const float x = (*ch)[i];
            if (i % 2 == 0) {
                held     = x;
                (*ch)[i] = pend;
            } else {
                float f, sc;
                up.up(dn.down(held, x), f, sc);
                (*ch)[i] = f;
                pend     = sc;
            }
        }
    }
    return r;
}

bool readStimulus(const char* name, Buf& out)
{
    rv::wav::Audio a;
    std::string    error;
    for (const std::string prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (rv::wav::read(prefix + name, a, error)) {
            out.assign(a.frames(), 0.0f);
            for (const auto& ch : a.channels)
                for (size_t i = 0; i < out.size(); ++i) out[i] += ch[i] / float(a.channels.size());
            return true;
        }
    std::fprintf(stderr, "could not read stimulus %s: %s\n", name, error.c_str());
    return false;
}

void level()
{
    // The page's material (02_hits, 04_skank, 10_pad_cminor; first 8 s), every
    // SPRINGS, MIX 0 / 0.4 / 1: K-weighted loudness vs today (the whole box:
    // the 24 kHz rate and the bits), and the bits alone (vs today through the
    // same filters).
    const char* const kMat[3] = {"02_hits.wav", "04_skank.wav", "10_pad_cminor.wav"};
    Buf mats[3];
    for (int m = 0; m < 3; ++m) {
        if (!readStimulus(kMat[m], mats[m])) {
            check(false, "level: stimulus WAVs (copy test_audio/stimulus into the checkout)");
            return;
        }
        mats[m].resize(std::min(mats[m].size(), sec(8.0)));
    }
    for (int a = 1; a < 3; ++a) {
        // Worst vs today per material x MIX (over SPRINGS), and the bits alone.
        double worst[3][3] = {}, worstBits = 0, sum = 0, worstChecked = 0;
        int    n = 0;
        char   whereBits[120] = "";
        const float kMix[3] = {0.0f, 0.4f, 1.0f};
        for (int m = 0; m < 3; ++m)
            for (int sp = 0; sp < 3; ++sp)
                for (int x = 0; x < 3; ++x) {
                    Settings s;
                    s.att = a, s.springs = sp, s.mix = kMix[x];
                    const Stereo o0 = render(s, mats[m], 0);
                    const double l1 = kLoudnessDb(render(s, mats[m], 1));
                    const double d = l1 - kLoudnessDb(o0), dBits = l1 - kLoudnessDb(filtersOnly(o0));
                    sum += d, ++n;
                    if (std::fabs(d) > std::fabs(worst[m][x])) worst[m][x] = d;
                    // Checked vs today everywhere but the dry drums alone (MIX
                    // 0 on 02_hits): their 11-24 kHz is the 24 kHz rate's loss.
                    if (!(m == 0 && x == 0) && std::fabs(d) > std::fabs(worstChecked)) worstChecked = d;
                    if (std::fabs(dBits) > std::fabs(worstBits)) {
                        worstBits = dBits;
                        std::snprintf(whereBits, sizeof whereBits, "%s, %d Spring%s, MIX %.1f", kMat[m], sp + 1, sp ? "s" : "", double(kMix[x]));
                    }
                }
        std::snprintf(msg, sizeof msg,
                      "level, %s (K-weighted, worst over SPRINGS, vs today at MIX 0 / 0.4 / 1): hits %+.2f / %+.2f / %+.2f, skank %+.2f / %+.2f / "
                      "%+.2f, pad %+.2f / %+.2f / %+.2f dB (mean %+.2f over %d cells). Limit +-0.5 except dry hits (the top octave the 24 kHz "
                      "rate takes off, by design): worst %+.2f. The bits alone (vs today through the same filters): worst %+.2f dB (%s), limit +-0.5",
                      kAtt[a], worst[0][0], worst[0][1], worst[0][2], worst[1][0], worst[1][1], worst[1][2], worst[2][0], worst[2][1], worst[2][2],
                      sum / n, n, worstChecked, worstBits, whereBits);
        check(std::fabs(worstChecked) <= 0.5 && std::fabs(worstBits) <= 0.5, msg);
    }
    // The 24 kHz rate takes the top octave (11-24 kHz) off: on bright
    // material (white-noise snares, a hi-hat's worth of air) that is heard,
    // and K-weighting (which lifts the highs) reads it as quieter. The bits
    // alone still don't change the level.
    {
        const Buf b = hits(7.0, true);
        std::printf("INFO  level on bright synthetic hits (white noise to 24 kHz), K-weighted, vs today / the bits alone:");
        for (int a = 1; a < 3; ++a)
            for (float mix : {0.0f, 1.0f}) {
                Settings s;
                s.att = a, s.mix = mix;
                const Stereo o0 = render(s, b, 0);
                const double l1 = kLoudnessDb(render(s, b, 1));
                std::printf(" %s MIX %.0f %+.2f / %+.2f dB;", kAtt[a], double(mix), l1 - kLoudnessDb(o0), l1 - kLoudnessDb(filtersOnly(o0)));
            }
        std::printf("\n");
    }
}

void latency()
{
    // The box's delay per frequency (dry only, DRIVEN: 12 bits, so the phase
    // reads clean): fit a sinusoid to input and output, delay = phase / w.
    std::printf("INFO  latency of the box (DRIVEN / KICKED only; CLEAN is never delayed), dry tone at -6 dBFS:");
    for (double hz : {100.0, 1000.0, 5000.0, 10000.0}) {
        const Buf in = toneBurst(1.0, hz, 0.0, 1.0);
        Settings s;
        s.att = 1, s.mix = 0.0f;
        const Stereo o = render(s, in, 1);
        double ci = 0, si = 0, co = 0, so = 0;
        for (size_t i = sec(0.2); i < sec(0.9); ++i) {
            const double w = 2 * kPi * hz * double(i) / kFs;
            ci += in[i] * std::cos(w), si += in[i] * std::sin(w);
            co += o.l[i] * std::cos(w), so += o.l[i] * std::sin(w);
        }
        double dph = std::atan2(ci, si) - std::atan2(co, so);
        while (dph < 0) dph += 2 * kPi;
        const double d = dph / (2 * kPi * hz) * kFs;
        std::printf(" %.0f Hz %.2f samples (%.3f ms);", hz, d, d / kFs * 1000.0);
    }
    std::printf("\n");
}

void noiseFloor()
{
    // A 1 kHz tone, dry only (MIX 0): what's left after fitting the tone is
    // the box's grain (quantisation + dither), in dBFS, and its SNR.
    for (int a = 1; a < 3; ++a) {
        std::printf("INFO  noise floor, %s, dry 1 kHz tone (residual dBFS / SNR dB):", kAtt[a]);
        for (double lv : {-6.0, -20.0, -40.0, -60.0, -80.0}) {
            const Buf in = toneBurst(1.2, 1000.0, 0.0, 1.2, lv);
            Settings s;
            s.att = a, s.mix = 0.0f;
            const Stereo o = render(s, in, 1);
            const size_t f = sec(0.2), t = sec(1.0);
            double c = 0, sn = 0;
            for (size_t i = f; i < t; ++i) {
                const double w = 2 * kPi * 1000.0 * double(i) / kFs;
                c += o.l[i] * std::cos(w), sn += o.l[i] * std::sin(w);
            }
            c *= 2.0 / double(t - f), sn *= 2.0 / double(t - f);
            double res = 0, sig = 0;
            for (size_t i = f; i < t; ++i) {
                const double w = 2 * kPi * 1000.0 * double(i) / kFs, fit = c * std::cos(w) + sn * std::sin(w);
                res += (o.l[i] - fit) * (o.l[i] - fit), sig += fit * fit;
            }
            std::printf(" %.0f dBFS: %.1f / %.1f;", lv, db(res / double(t - f)), db(sig / std::max(res, 1e-30)));
        }
        std::printf("\n");
    }
}

void silence()
{
    // Nothing in: exactly nothing out, every SPRINGS x DRIVEN/KICKED x MIX.
    {
        const Buf z(sec(3.0), 0.0f);
        int ok = 0, cells = 0;
        for (int sp = 0; sp < 3; ++sp)
            for (int a = 1; a < 3; ++a)
                for (float mix : {0.0f, 0.4f, 1.0f}) {
                    Settings s;
                    s.springs = sp, s.att = a, s.mix = mix;
                    const Stereo o = render(s, z, 1);
                    bool zero = true;
                    for (size_t i = 0; i < o.l.size(); ++i) zero = zero && o.l[i] == 0.0f && o.r[i] == 0.0f;
                    ok += zero, ++cells;
                }
        std::snprintf(msg, sizeof msg, "silence in, exact silence out (no idle hiss): %d of %d cells", ok, cells);
        check(ok == cells, msg);
    }
    // The dry alone (MIX 0): noise for 1 s, then nothing: exact 0 within a few ms.
    for (int a = 1; a < 3; ++a) {
        Buf in(sec(2.0), 0.0f);
        const Buf nz = noise(sec(1.0), 0.3f, 3u);
        std::copy(nz.begin(), nz.end(), in.begin());
        Settings s;
        s.att = a, s.mix = 0.0f;
        const Stereo o = render(s, in, 1);
        size_t last = 0;
        for (size_t i = 0; i < o.l.size(); ++i)
            if (o.l[i] != 0.0f || o.r[i] != 0.0f) last = i;
        const double ms = (double(last) - double(sec(1.0))) / kFs * 1000.0;
        std::snprintf(msg, sizeof msg, "%s, dry only: the output is exactly 0 from %.1f ms after the input stops (limit 15)", kAtt[a], ms);
        check(ms <= 15.0, msg);
    }
    // A rim's tail (MIX 1, DECAY noon and 0.85): ends in exact silence, and when.
    for (int a = 1; a < 3; ++a)
        for (float dc : {0.5f, 0.85f}) {
            const Buf in = rims(30.0, {0.5});
            Settings s;
            s.att = a, s.decay = dc;
            const Stereo o1 = render(s, in, 1), o0 = render(s, in, 0);
            size_t last = 0;
            for (size_t i = 0; i < o1.l.size(); ++i)
                if (o1.l[i] != 0.0f || o1.r[i] != 0.0f) last = i;
            const double t = double(last) / kFs;
            const double todayAt = stereoDb(o0, std::min(last, sec(29.0)), std::min(last, sec(29.0)) + sec(0.5));
            std::snprintf(msg, sizeof msg,
                          "%s, DECAY %.2f, a rim's tail at MIX 1: exact silence from %.2f s after the hit (today's tail there: %.1f dBFS; want "
                          "silence within the 30 s render)",
                          kAtt[a], double(dc), t - 0.5, todayAt);
            check(last < sec(29.5), msg);
        }
}

void artefacts()
{
    // New narrow peaks vs today (DECAY noon / 0.85, MIX 0.4 / 1): a rim's
    // tail (0.15-1.15 s and 1-2 s after it, and the last second before the
    // box's tail reaches silence, where the bits run out undithered), and a
    // 1 s tone burst (1 kHz, and 1130 Hz whose folds would land between its
    // harmonics) while it plays and in its tail. Positive control: today's
    // output through the same bits without the half-band filters or the
    // dither (folding and granulation both): the check has to see it.
    for (int a = 1; a < 3; ++a) {
        const float bits = rv::outbits::kDepth[a].bits; // DRIVEN 12, KICKED 10
        double worstRim = 0, hzRim = 0, worstEnd = 0, hzEnd = 0, worstTone = 0, hzTone = 0, ctrl = 0, hzCtrl = 0;
        for (float dc : {0.5f, 0.85f})
            for (float mix : {0.4f, 1.0f}) {
                Settings s;
                s.att = a, s.decay = dc, s.mix = mix;
                const Buf in = rims(12.0, {0.3});
                const Stereo o0 = render(s, in, 0), o1 = render(s, in, 1);
                for (double w0 : {0.45, 1.3}) {
                    double hz = 0;
                    const double p = newPeakDb(o0, o1, sec(w0), sec(w0 + 1.0), &hz);
                    if (p > worstRim) worstRim = p, hzRim = hz;
                }
                size_t last = 0;
                for (size_t i = 0; i < o1.l.size(); ++i)
                    if (o1.l[i] != 0.0f || o1.r[i] != 0.0f) last = i;
                if (last > sec(1.6)) {
                    double hz = 0;
                    const double p = newPeakDb(o0, o1, last - sec(1.2), last - sec(0.2), &hz);
                    std::printf("INFO    %s DECAY %.2f MIX %.1f: last second %.1f dB (%.0f Hz)\n", kAtt[a], double(dc), double(mix), p, hz);
                    if (p > worstEnd) worstEnd = p, hzEnd = hz;
                }
                for (double f0 : {1000.0, 1130.0}) {
                    const Buf tb = toneBurst(4.0, f0, 0.3, 1.0);
                    const Stereo t0 = render(s, tb, 0), t1 = render(s, tb, 1);
                    for (double w0 : {0.4, 1.4, 2.4}) {
                        double hz = 0;
                        const double p = newPeakDb(t0, t1, sec(w0), sec(w0 + 0.85), &hz, f0);
                        if (p > worstTone) worstTone = p, hzTone = hz;
                        const double pc = newPeakDb(t0, naiveBox(t0, bits), sec(w0), sec(w0 + 0.85), &hz, f0);
                        if (pc > ctrl) ctrl = pc, hzCtrl = hz;
                    }
                }
            }
        // A quiet dry tone (MIX 0) a few steps above the bottom: where
        // quantisation turns a tone into a pitched buzz unless dithered.
        double quiet = 0, hzQuiet = 0;
        {
            // ~20 dB over the bottom step (12-bit -99.5 dBFS, 10-bit -87.4: -80 / -68 dBFS).
            const double q = std::exp2(double(bits) - 1.0), bottom = 20.0 * std::log10((std::pow(256.0, 1.0 / q) - 1.0) / 255.0);
            const double lv = std::floor(bottom + 20.0);
            const Buf tb = toneBurst(2.0, 1130.0, 0.1, 1.8, lv);
            Settings s;
            s.att = a, s.mix = 0.0f;
            const Stereo t0 = render(s, tb, 0), t1 = render(s, tb, 1);
            double hz = 0;
            quiet = newPeakDb(t0, t1, sec(0.3), sec(1.8), &hzQuiet, 1130.0);
            const double pc = newPeakDb(t0, naiveBox(t0, bits), sec(0.3), sec(1.8), &hz, 1130.0);
            if (pc > ctrl) ctrl = pc, hzCtrl = hz;
            worstTone = std::max(worstTone, quiet);
        }
        std::snprintf(msg, sizeof msg,
                      "%s, no new pitches vs today (DECAY noon / 0.85, MIX 0.4 / 1; limit 6 dB): rim tail %.1f dB (%.0f Hz); tone bursts 1 kHz / "
                      "1130 Hz, inharmonic %.1f dB (%.0f Hz). Control, the same bits without filters or dither: %.1f dB (%.0f Hz), must exceed 6. "
                      "Quiet dry tone (a few steps): %.1f dB (%.0f Hz). The last second before the tail's silence: %.1f dB (%.0f Hz)",
                      kAtt[a], worstRim, hzRim, worstTone, hzTone, ctrl, hzCtrl, quiet, hzQuiet, worstEnd, hzEnd);
        check(worstRim <= 6.0 && worstTone <= 6.0 && worstEnd <= 6.0 && ctrl > 6.0, msg);
    }
    // M6 on long tails: a rim, 30 s, SPRINGS 2, each ATTITUDE.
    for (int a = 0; a < 3; ++a) {
        const Buf in = rims(30.0, {0.5});
        Settings s;
        s.att = a, s.decay = a == 2 ? 0.8f : 0.85f;
        const auto m0 = rv::metrics::compute({render(s, in, 0).l, render(s, in, 0).r}, kFs);
        const Stereo o1 = render(s, in, 1);
        const auto m1 = rv::metrics::compute({o1.l, o1.r}, kFs);
        std::snprintf(msg, sizeof msg, "M6, %s rim tail (DECAY %.2f, 30 s): steady tone %d (today %d), ringing_db %.1f at %.0f Hz (today %.1f at %.0f Hz)",
                      kAtt[a], double(s.decay), int(m1.steadyTone), int(m0.steadyTone), m1.ringingDb, m1.ringingHz, m0.ringingDb, m0.ringingHz);
        check(!m1.steadyTone && (!m1.ringing || m0.ringing), msg);
    }
    // M6 grid at SPRINGS 3 (click + burst, ATTITUDE x DECAY .85/1 x TENSION 0/.5; KICKED not at DECAY 1).
    {
        Buf clk(sec(14.0), 0.0f);
        clk[sec(0.5)] = clk[sec(0.5) + 1] = 0.5f;
        Buf nb(sec(14.0), 0.0f);
        const Buf z = noise(sec(0.5), 0.43f, 77u);
        std::copy(z.begin(), z.end(), nb.begin() + long(sec(0.5)));
        int    flagged[2] = {0, 0}, steady[2] = {0, 0}, cells = 0;
        double worst[2] = {0, 0};
        for (int a = 0; a < 3; ++a)
            for (float dc : {0.85f, 1.0f})
                for (float tn : {0.0f, 0.5f})
                    for (const Buf* in : {&clk, &nb}) {
                        if (a == 2 && dc > 0.9f) continue;
                        ++cells;
                        for (int v = 0; v < 2; ++v) {
                            Settings s;
                            s.att = a, s.decay = dc, s.tension = tn, s.springs = 2, s.mix = 1.0f;
                            const Stereo o = render(s, *in, v);
                            const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                            if (m.ringing || m.steadyTone) ++flagged[v];
                            if (m.steadyTone) ++steady[v];
                            if (!std::isnan(m.ringingDb)) worst[v] = std::max(worst[v], m.ringingDb);
                        }
                    }
        std::snprintf(msg, sizeof msg,
                      "M6 at SPRINGS 3 (%d cells: click + burst, ATTITUDE x DECAY .85/1 x TENSION 0/.5): flagged (steady tone) / worst ringing_db: "
                      "today %d (%d) / %.1f, mu-law %d (%d) / %.1f",
                      cells, flagged[0], steady[0], worst[0], flagged[1], steady[1], worst[1]);
        check(flagged[1] <= flagged[0] && steady[1] == 0, msg);
    }
}

void flips()
{
    // ATTITUDE flips every 0.6 s through every transition, mid-tail (skank +
    // pad, MIX 0.4) and on a dry tone (1 kHz, MIX 0, where the box is most exposed).
    const std::vector<std::pair<size_t, int>> seq = {{sec(1.2), 1}, {sec(1.8), 2}, {sec(2.4), 1}, {sec(3.0), 0}, {sec(3.6), 2},
                                                     {sec(4.2), 0}, {sec(4.8), 1}, {sec(5.4), 2}, {sec(6.0), 0}};
    Buf mat = stabs(7.0);
    {
        const Buf p = pad(7.0, 5.0);
        for (size_t i = 0; i < mat.size(); ++i) mat[i] = 0.6f * mat[i] + 0.6f * p[i];
    }
    const Buf tone = toneBurst(7.0, 1000.0, 0.2, 6.6, -12.0);
    for (int which = 0; which < 2; ++which) {
        Settings s;
        s.mix = which == 0 ? 0.4f : 0.0f;
        const Buf& in = which == 0 ? mat : tone;
        double w0 = 0, w1 = 0;
        const int c0 = clicksBoth(render(s, in, 0, 48, seq), sec(1.0), &w0);
        const int c1 = clicksBoth(render(s, in, 1, 48, seq), sec(1.0), &w1);
        std::snprintf(msg, sizeof msg, "ATTITUDE flips every 0.6 s (%s): clicks today %d (worst ratio %.1f), mu-law %d (worst ratio %.1f)",
                      which == 0 ? "skank + pad mid-tail, MIX 0.4" : "dry 1 kHz tone, MIX 0", c0, w0, c1, w1);
        check(c1 == 0 && c1 <= c0, msg);
    }
    // The fade takes ~20 ms: halfway through a CLEAN -> KICKED flip the weights are about half.
    {
        rv::Tank t;
        t.prepare(kFs, 48);
        t.setOutputBitsVoicing(1);
        Buf z(48, 0.0f), l(48), r(48);
        for (int k = 0; k < 10; ++k) t.process(z.data(), z.data(), l.data(), r.data(), 48);
        t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(2));
        for (int k = 0; k < 10; ++k) t.process(z.data(), z.data(), l.data(), r.data(), 48); // 10 ms
        const float half = t.outputBits().weights()[2];
        for (int k = 0; k < 12; ++k) t.process(z.data(), z.data(), l.data(), r.data(), 48);
        const float full = t.outputBits().weights()[2];
        std::snprintf(msg, sizeof msg, "the box fades in over ~20 ms on a flip: KICKED weight %.2f at 10 ms, %.2f at 22 ms", double(half), double(full));
        check(half > 0.3f && half < 0.7f && full == 1.0f, msg);
    }
}

// The release firmware's output LEDs (ADR 0031) with the box in. Red comes
// only from the limiter (Tank::limiterGain()), which sits before the box: it
// must read exactly as without the box. The level meters read the box's
// output (the per-block peak of what leaves the Tank), so they show what the
// box does (INFO: on bright material the 24 kHz rate lowers a transient's
// peak). test_led_meter's "limiter pulls" case (a held chord at -3 dBFS into
// 3 Springs, DECAY 0.62, the Sustain trim off, fully wet; and at MIX 0.5) in
// DRIVEN and KICKED, per 48-sample block as the firmware does.
void leds()
{
    Buf in(sec(4.0), 0.0f);
    for (size_t n = 0; n < in.size(); ++n) {
        const double env = std::min(1.0, double(n) / 480.0);
        double sum = 0.0;
        for (double hz : {220.0, 261.63, 329.63}) sum += std::sin(2.0 * kPi * hz * double(n) / kFs);
        in[n] = float(std::pow(10.0, -3.0 / 20.0) * env * sum / 3.0);
    }
    for (int a = 1; a < 3; ++a)
    for (float mix : {0.5f, 1.0f}) {
        Settings s;
        s.att = a, s.springs = 2, s.drive = 0.0f, s.splash = 0.0f, s.decay = 0.62f, s.mix = mix;
        std::vector<float> gain[2], peak[2];
        for (int v = 0; v < 2; ++v) {
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            t.setSustainTrimEnabled(false);
            t.setEchoMode(false); // three Springs (SPRINGS 3's reference since ADR 0041), as test_led_meter
            t.setOutputBitsVoicing(v);
            Buf l(48), r(48);
            for (size_t pos = 0; pos + 48 <= in.size(); pos += 48) {
                t.process(in.data() + pos, in.data() + pos, l.data(), r.data(), 48);
                float pk = 0.0f;
                for (int i = 0; i < 48; ++i) pk = std::max({pk, std::fabs(l[size_t(i)]), std::fabs(r[size_t(i)])});
                gain[v].push_back(t.limiterGain());
                peak[v].push_back(pk);
            }
        }
        int red[2] = {0, 0};
        double worstPos = 0.0;
        for (size_t b = 0; b < gain[0].size(); ++b) {
            for (int v = 0; v < 2; ++v) red[v] += rvled::limiterReducing(gain[v][b]) ? 1 : 0;
            const float p0 = std::max(peak[0][b], b ? peak[0][b - 1] : 0.0f), p1 = std::max(peak[1][b], b ? peak[1][b - 1] : 0.0f);
            if (p0 >= 0.063f) worstPos = std::max(worstPos, double(std::fabs(rvled::levelToPosition(p1) - rvled::levelToPosition(p0))));
        }
        std::snprintf(msg, sizeof msg,
                      "LEDs, %s held chord into the limiter (MIX %.1f): limiter gain per block identical with and without the box %d; red in %d / %d blocks "
                      "(without / with: the same; DRIVEN must light it, KICKED stays under the limiter here). INFO the output level meter, worst block (above -24 dBFS) %.3f of its "
                      "scale from before the box",
                      kAtt[a], double(mix), int(gain[0] == gain[1]), red[0], red[1], worstPos);
        check(gain[0] == gain[1] && red[0] == red[1] && (a == 2 || red[0] > 0), msg);
    }
}

void deterministic()
{
    const Buf h = hits(4.0);
    for (int a = 1; a < 3; ++a) {
        Settings s;
        s.att = a, s.mix = 0.4f;
        const Stereo a1 = render(s, h, 1, 48), a2 = render(s, h, 1, 48), b = render(s, h, 1, 333), c = render(s, h, 1, 1);
        std::snprintf(msg, sizeof msg, "%s: the same twice %d, block 48 = 333 = 1: %d %d, finite %d", kAtt[a], int(same(a1, a2)), int(same(a1, b)),
                      int(same(a1, c)), int(finite(a1)));
        check(same(a1, a2) && same(a1, b) && same(a1, c) && finite(a1), msg);
    }
}

void cost()
{
    // Worst case (3 Springs, KICKED, DECAY / TONE / DRIVE 1, TENSION 0), hits + noise, best of 7 interleaved.
    Buf in = hits(4.0);
    {
        const Buf nz = noise(in.size(), 0.05f, 5u);
        for (size_t i = 0; i < in.size(); ++i) in[i] += nz[i];
    }
    double best[2] = {1e30, 1e30};
    for (int run = 0; run < 7; ++run)
        for (int v = 0; v < 2; ++v) {
            Settings s;
            s.att = 2, s.springs = 2, s.decay = 1.0f, s.tone = 1.0f, s.drive = 1.0f, s.tension = 0.0f, s.splash = 1.0f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            t.setOutputBitsVoicing(v);
            Buf l(in.size()), r(in.size());
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t pos = 0; pos < in.size(); pos += 48)
                t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
            const auto t1 = std::chrono::steady_clock::now();
            best[v] = std::min(best[v], std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
        }
    std::snprintf(msg, sizeof msg, "cost (desktop, worst case KICKED 3 Springs, best of 7): today %.1f, mu-law %.1f ns/sample (%+.1f %%)", best[0],
                  best[1], 100.0 * (best[1] / best[0] - 1.0));
    info(msg);
}

} // namespace

int main(int argc, char** argv)
{
    const bool only = argc > 1;
    auto want = [&](const char* name) {
        if (!only) return true;
        for (int i = 1; i < argc; ++i)
            if (std::strcmp(argv[i], name) == 0) return true;
        return false;
    };
    if (want("identity")) identity();
    if (want("clean")) cleanUntouched();
    if (want("level")) level();
    if (want("latency")) latency();
    if (want("noise")) noiseFloor();
    if (want("silence")) silence();
    if (want("artefacts")) artefacts();
    if (want("flips")) flips();
    if (want("leds")) leds();
    if (want("deterministic")) deterministic();
    if (want("cost")) cost();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
