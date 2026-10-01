// Metrics + sidecar test suite (docs/m1-contracts.md Stream B).
// Dependency-free, synthetic signals only (the Tank's sound is still
// changing under Stream A) except for the click_count stimulus checks,
// which read the real WAVs contract-required as reference behaviour.

#include "Base64.h"
#include "Fft.h"
#include "Json.h"
#include "Metrics.h"
#include "Sidecar.h"
#include "Spectral.h"
#include "Spectrogram.h"
#include "Wav.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

double db(double x) { return std::pow(10.0, x / 20.0); }

// Tries a couple of relative locations so the test works whether ctest's
// working directory is the build dir (the normal case, one level under
// the repo root) or the repo root itself.
bool readStimulus(const std::string& name, rv::wav::Audio& out)
{
    std::string error;
    for (const std::string& prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"}) {
        if (rv::wav::read(prefix + name, out, error)) return true;
    }
    std::fprintf(stderr, "could not read stimulus %s: %s\n", name.c_str(), error.c_str());
    return false;
}

void jsonRoundTrips()
{
    rv::json::Value root = rv::json::Value::makeObject();
    root.set("name", rv::json::Value::makeString("m1_grid"));
    root.set("count", rv::json::Value::makeNumber(3));
    root.set("ratio", rv::json::Value::makeNumber(1.5));
    root.set("ok", rv::json::Value::makeBool(true));
    root.set("nothing", rv::json::Value::makeNull());
    rv::json::Value arr = rv::json::Value::makeArray();
    arr.push(rv::json::Value::makeNumber(0.0));
    arr.push(rv::json::Value::makeNumber(0.5));
    arr.push(rv::json::Value::makeString("escape \"me\"\nplease"));
    root.set("grid", arr);
    rv::json::Value nested = rv::json::Value::makeObject();
    nested.set("decay", rv::json::Value::makeNumber(0.8));
    root.set("params", nested);

    const std::string text = rv::json::write(root);
    rv::json::Value back;
    std::string error;
    const bool parsed = rv::json::parse(text, back, error);
    check(parsed && back == root, "JSON: write() -> parse() round-trips");
}

void base64RoundTrips()
{
    std::vector<uint8_t> data;
    for (int i = 0; i < 257; ++i) data.push_back(uint8_t((i * 37 + 11) & 0xFF)); // odd length, exercises padding
    const std::string encoded = rv::base64::encode(data);
    std::vector<uint8_t> back;
    const bool ok = rv::base64::decode(encoded, back) && back == data;
    check(ok, "base64: encode() -> decode() round-trips");
}

void fftMatchesDirectDft()
{
    const size_t n = 64;
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> re(n), im(n, 0.0f);
    for (auto& v : re) v = dist(rng);
    std::vector<float> reRef = re;

    // Direct O(n^2) DFT reference.
    std::vector<double> expectRe(n, 0.0), expectIm(n, 0.0);
    for (size_t k = 0; k < n; ++k) {
        for (size_t t = 0; t < n; ++t) {
            const double ang = -2.0 * M_PI * double(k) * double(t) / double(n);
            expectRe[k] += double(reRef[t]) * std::cos(ang);
            expectIm[k] += double(reRef[t]) * std::sin(ang);
        }
    }

    rv::fft::transform(re, im, false);
    double maxErr = 0.0;
    for (size_t k = 0; k < n; ++k) {
        maxErr = std::max(maxErr, std::fabs(double(re[k]) - expectRe[k]));
        maxErr = std::max(maxErr, std::fabs(double(im[k]) - expectIm[k]));
    }
    check(maxErr < 1e-2, "FFT: radix-2 transform matches a direct DFT on random data");
}

// Exponentially decaying white noise with a known T60: amplitude envelope
// 10^(-3t/T60) gives a 60 dB power decay at t = T60.
std::vector<float> decayingNoise(double t60, double totalSeconds, float sr, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const size_t n = size_t(totalSeconds * double(sr));
    std::vector<float> x(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = double(i) / double(sr);
        const double env = std::pow(10.0, -3.0 * t / t60);
        x[i] = float(env * dist(rng));
    }
    return x;
}

void t60MeasuresKnownDecays()
{
    for (double t60 : {0.5, 8.0}) {
        const float sr = 48000.0f;
        const double total = t60 * 1.5 + 1.0; // enough tail to see -35 dB
        std::vector<float> mono = decayingNoise(t60, total, sr, 42);
        rv::metrics::Metrics m = rv::metrics::compute({mono}, sr);
        const bool ok = !std::isnan(m.t60S) && std::fabs(m.t60S - t60) / t60 < 0.05;
        char what[128];
        std::snprintf(what, sizeof what, "T60: %.1fs decay measured within 5%% (got %s)",
                      t60, std::isnan(m.t60S) ? "null" : std::to_string(m.t60S).c_str());
        check(ok, what);
    }
}

// A repeated stimulus: the same tail measured alone must read the same
// when a second event follows it. The second event builds up quietly
// under -40 dBFS first (like a spring's output before the next click
// crosses the event threshold); that build-up must not reach the fit.
void t60IgnoresNextEventBuildUp()
{
    const float sr = 48000.0f;
    const double t60 = 1.0;
    std::vector<float> tail = decayingNoise(t60, 2.5, sr, 7);
    for (float& x : tail) x *= 0.1f; // -20 dBFS peak: a quiet take
    std::mt19937 floorRng(99); // plus a -80 dBFS noise floor, like a recording
    std::uniform_real_distribution<float> floorDist(-1.0f, 1.0f);
    for (float& x : tail) x += 1e-4f * floorDist(floorRng);
    std::vector<float> alone = tail;
    alone.resize(alone.size() + size_t(1.0 * sr), 0.0f);

    std::vector<float> repeated = tail;
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const size_t buildUp = size_t(0.080 * sr); // -70 -> -41 dBFS peak over 80 ms
    for (size_t i = 0; i < buildUp; ++i) {
        const double db = -70.0 + 29.0 * double(i) / double(buildUp);
        repeated.push_back(float(std::pow(10.0, db / 20.0) * dist(rng)));
    }
    std::vector<float> next = decayingNoise(t60, 2.5, sr, 8);
    for (float x : next) repeated.push_back(0.1f * x);

    const double a = rv::metrics::compute({alone}, sr).t60S;
    const double r = rv::metrics::compute({repeated}, sr).t60S;
    const bool ok = !std::isnan(a) && !std::isnan(r) && std::fabs(r / a - 1.0) < 0.01
                    && std::fabs(a - t60) / t60 < 0.05;
    char what[192];
    std::snprintf(what, sizeof what,
                  "T60: a tail reads the same alone (%.3f s) and followed by a second event with an 80 ms build-up "
                  "(%.3f s), within 1%% (true %.1f s, -80 dBFS floor)", a, r, t60);
    check(ok, what);
}

// A recording's noise floor: a 1 s tail (-10 dBFS start) sinks into hiss
// `floorDb` below its start and the hiss runs on for seconds, then a gap of
// digital silence, then a second event. Integrating the hiss would read the
// tail far too long (a real take read 14 s for ~3.5 s).
double t60WithFloor(double floorDb)
{
    const float sr = 48000.0f;
    const float amp = 0.3f;
    std::vector<float> x = decayingNoise(1.0, 5.0, sr, 21);
    for (float& v : x) v *= amp;
    std::mt19937 floorRng(22);
    std::uniform_real_distribution<float> floorDist(-1.0f, 1.0f);
    const float floorAmp = amp * float(std::pow(10.0, floorDb / 20.0));
    for (float& v : x) v += floorAmp * floorDist(floorRng);
    x.resize(x.size() + size_t(0.5 * sr), 0.0f);
    for (float v : decayingNoise(1.0, 3.0, sr, 23)) x.push_back(amp * v);
    return rv::metrics::compute({x}, sr).t60S;
}

void t60HandlesNoiseFloor()
{
    for (double floorDb : {-50.0, -60.0}) {
        const double t = t60WithFloor(floorDb);
        char what[160];
        std::snprintf(what, sizeof what, "T60: a 1 s tail over a noise floor %.0f dB down, then silence and a second event, "
                      "reads within 5%% (got %s)", -floorDb, std::isnan(t) ? "null" : std::to_string(t).c_str());
        check(!std::isnan(t) && std::fabs(t - 1.0) < 0.05, what);
    }
    const double buried = t60WithFloor(-25.0);
    char what[160];
    std::snprintf(what, sizeof what, "T60: a tail that never gets clear of the noise (floor 25 dB down) reads null (got %s)",
                  std::isnan(buried) ? "null" : std::to_string(buried).c_str());
    check(std::isnan(buried), what);
}

void resonancePeakDistinguishesTone()
{
    const float sr = 48000.0f;
    const double total = 3.0;
    const size_t n = size_t(total * double(sr));
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<float> noiseOnly(n), noisePlusTone(n);
    for (size_t i = 0; i < n; ++i) {
        const float w = dist(rng) * 0.1f; // -20 dBFS-ish white noise
        noiseOnly[i] = w;
        const double t = double(i) / double(sr);
        // Tone ~20 dB above the noise floor.
        noisePlusTone[i] = w + float(0.1 * db(20.0)) * std::sin(2.0f * float(M_PI) * 1000.0f * float(t));
    }
    rv::metrics::Metrics low = rv::metrics::compute({noiseOnly}, sr);
    rv::metrics::Metrics high = rv::metrics::compute({noisePlusTone}, sr);

    check(!std::isnan(low.resonancePeakDb) && low.resonancePeakDb < 6.0, "resonance_peak_db: white noise stays below 6 dB");
    check(!std::isnan(high.resonancePeakDb) && high.resonancePeakDb > 12.0, "resonance_peak_db: noise + 20 dB tone exceeds 12 dB");
}

void steadyToneDetectsSustainedSine()
{
    const float sr = 48000.0f;
    const size_t n = size_t(3.0 * double(sr));
    std::vector<float> sine(n), noise(n), sweep(n);
    std::mt19937 rng(9);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < n; ++i) {
        const double t = double(i) / double(sr);
        sine[i] = float(db(-20.0)) * std::sin(2.0f * float(M_PI) * 440.0f * float(t));
        noise[i] = dist(rng) * float(db(-20.0));
        // Instantaneous freq 500 + 833*t Hz: sweeps fast enough to move many FFT bins per 0.5 s frame.
        sweep[i] = float(db(-20.0)) * std::sin(2.0 * M_PI * (500.0 * t + 0.5 * 833.0 * t * t));
    }
    check(rv::metrics::compute({sine}, sr).steadyTone, "steady_tone: true for a 3 s -20 dBFS sine");
    check(!rv::metrics::compute({noise}, sr).steadyTone, "steady_tone: false for white noise");
    check(!rv::metrics::compute({sweep}, sr).steadyTone, "steady_tone: false for a fast sine sweep");
}

void clickCountOnStimulusFiles()
{
    struct Expect { const char* file; bool expectAny; };
    const Expect files[] = {
        {"01_clicks.wav", true},
        {"02_hits.wav", false},
        {"03_sweep.wav", false},
        {"04_skank.wav", false},
        {"05_silence_for_kicks.wav", false},
        {"06_noise_bursts.wav", false},
    };
    for (const auto& f : files) {
        rv::wav::Audio a;
        if (!readStimulus(f.file, a)) { check(false, f.file); continue; }
        rv::metrics::Metrics m = rv::metrics::compute(a.channels, float(a.sampleRate));
        char what[128];
        std::snprintf(what, sizeof what, "click_count: %s -> %ld (expected %s)", f.file, m.clickCount,
                      f.expectAny ? "> 0" : "0");
        check(f.expectAny ? (m.clickCount > 0) : (m.clickCount == 0), what);
    }
}

void clickCountFlagsInjectedStep()
{
    const float sr = 48000.0f;
    const size_t n = size_t(2.0 * double(sr));
    std::vector<float> sine(n);
    for (size_t i = 0; i < n; ++i) sine[i] = 0.5f * std::sin(2.0f * float(M_PI) * 440.0f * float(i) / sr);
    sine[n / 2] += 0.3f; // single-sample step injected mid-file
    rv::metrics::Metrics m = rv::metrics::compute({sine}, sr);
    check(m.clickCount > 0, "click_count: > 0 for a sine with an injected single-sample step");

    std::vector<float> clean(n);
    for (size_t i = 0; i < n; ++i) clean[i] = 0.5f * std::sin(2.0f * float(M_PI) * 440.0f * float(i) / sr);
    check(rv::metrics::compute({clean}, sr).clickCount == 0, "click_count: 0 for a clean sine (no injected step)");
}

// Synthetic stereo signals per docs/m4-contracts.md Stream E's test list.
void stereoMetricsOnSyntheticSignals()
{
    const float sr = 48000.0f;
    const size_t n = size_t(2.0 * double(sr));
    std::mt19937 rngL(11), rngR(22);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // identical L/R -> correlation 1, mono loss +3 dB, no notch.
    {
        std::vector<float> L(n);
        for (size_t i = 0; i < n; ++i) L[i] = dist(rngL) * 0.5f;
        std::vector<float> R = L;
        rv::metrics::Metrics m = rv::metrics::compute({L, R}, sr);
        check(!std::isnan(m.stereoCorrelation) && std::fabs(m.stereoCorrelation - 1.0) < 1e-6,
              "stereo_correlation: identical L/R -> 1.0");
        check(!std::isnan(m.monoLossDb) && std::fabs(m.monoLossDb - 3.0) < 0.1,
              "mono_loss_db: identical L/R -> +3 dB");
        check(!std::isnan(m.monoNotchDb) && m.monoNotchDb > -1.0,
              "mono_notch_db: identical L/R -> no notch (~0 dB)");
    }

    // independent noise L/R -> correlation ~= 0, mono loss ~= 0 dB.
    {
        std::vector<float> L(n), R(n);
        for (size_t i = 0; i < n; ++i) { L[i] = dist(rngL) * 0.5f; R[i] = dist(rngR) * 0.5f; }
        rv::metrics::Metrics m = rv::metrics::compute({L, R}, sr);
        char what[128];
        std::snprintf(what, sizeof what, "stereo_correlation: independent noise -> ~0 (got %.3f)",
                      std::isnan(m.stereoCorrelation) ? -9.0 : m.stereoCorrelation);
        check(!std::isnan(m.stereoCorrelation) && std::fabs(m.stereoCorrelation) < 0.1, what);
        std::snprintf(what, sizeof what, "mono_loss_db: independent noise -> ~0 dB (got %.2f)",
                      std::isnan(m.monoLossDb) ? -999.0 : m.monoLossDb);
        check(!std::isnan(m.monoLossDb) && std::fabs(m.monoLossDb) < 0.5, what);
    }

    // R = -L -> mono loss < -40 dB.
    {
        std::vector<float> L(n), R(n);
        for (size_t i = 0; i < n; ++i) { L[i] = dist(rngL) * 0.5f; R[i] = -L[i]; }
        rv::metrics::Metrics m = rv::metrics::compute({L, R}, sr);
        check(!std::isnan(m.monoLossDb) && m.monoLossDb < -40.0, "mono_loss_db: R = -L -> < -40 dB");
    }

    // R = L delayed 1 ms -> a deep notch near 500 Hz (comb filter, first
    // null at 1 / (2 * delay)). White noise gives a broadband comb.
    {
        std::vector<float> L(n);
        for (size_t i = 0; i < n; ++i) L[i] = dist(rngL) * 0.5f;
        const size_t delaySamples = size_t(0.001 * double(sr)); // 1 ms
        std::vector<float> R(n, 0.0f);
        for (size_t i = delaySamples; i < n; ++i) R[i] = L[i - delaySamples];
        rv::metrics::Metrics m = rv::metrics::compute({L, R}, sr);
        check(!std::isnan(m.monoNotchDb) && m.monoNotchDb < -6.0,
              "mono_notch_db: R = L delayed 1ms -> deep notch (< -6 dB)");
    }

    // A 6 dB step in level -> max_step >= 5. Independent noise so the
    // step is a level change, not a stereo-image change; step lands
    // exactly on a 100ms window boundary.
    {
        const size_t winLen = size_t(0.1 * double(sr));
        const size_t stepAt = winLen * 5; // 500 ms in
        std::vector<float> L(n), R(n);
        for (size_t i = 0; i < n; ++i) {
            const float level = i < stepAt ? 0.1f : 0.2f; // +6.02 dB step
            L[i] = dist(rngL) * level;
            R[i] = dist(rngR) * level;
        }
        rv::metrics::Metrics m = rv::metrics::compute({L, R}, sr);
        char what[128];
        std::snprintf(what, sizeof what, "max_step_db_100ms: 6 dB level step -> >= 5 dB (got %s)",
                      std::isnan(m.maxStepDb100ms) ? "null" : std::to_string(m.maxStepDb100ms).c_str());
        check(!std::isnan(m.maxStepDb100ms) && m.maxStepDb100ms >= 5.0, what);
    }

    // A true mono file (single channel) -> all three stereo metrics null.
    {
        std::vector<float> mono(n);
        for (size_t i = 0; i < n; ++i) mono[i] = dist(rngL) * 0.5f;
        rv::metrics::Metrics m = rv::metrics::compute({mono}, sr);
        check(std::isnan(m.stereoCorrelation), "stereo_correlation: null for a mono file");
        check(std::isnan(m.monoLossDb), "mono_loss_db: null for a mono file");
        check(std::isnan(m.monoNotchDb), "mono_notch_db: null for a mono file");
    }
}

// ---- M6 Ringing metric + Howl criterion (docs/m6-metric-calibration.md) ----
// Small synthetic cases: things that must pass (noise, dense decaying tails,
// a plain feedback loop's comb) and things that must be flagged (a feedback
// loop with a narrow resonance inside it, a sine buried under a tail, a
// self-oscillating loop). Deterministic seeds.
namespace syn {

using Buf = std::vector<float>;
constexpr double kFs = 48000.0;

Buf noiseBurst(double totalS, double dur, double peak, unsigned seed)
{
    Buf x(size_t(totalS * kFs), 0.0f);
    std::mt19937 r(seed);
    std::uniform_real_distribution<float> u(-1, 1);
    const size_t a = size_t(kFs), n = size_t(dur * kFs);
    float y = 0;
    const float c = 1.0f - std::exp(-2.0f * float(M_PI) * 3000.0f / float(kFs));
    for (size_t i = 0; i < n; ++i) { y += c * (u(r) - y); x[a + i] = float(peak) * 2.0f * y; }
    return x;
}

// Feedback delay loop: delay L, one-pole damping at 6 kHz, broadband gain for
// T60, an RBJ peaking resonance (+peakDb at f0, Q) inside the loop, tanh
// (slope 1 at rest) so a loop gain above 1 settles into a steady tone.
Buf loop(const Buf& in, double L, double t60, double f0, double q, double peakDb)
{
    const size_t D = size_t(L * kFs);
    std::vector<double> buf(D, 0.0);
    size_t w = 0;
    const double g = std::pow(10.0, -3.0 * L / t60), c = 1.0 - std::exp(-2 * M_PI * 6000.0 / kFs);
    const double A = std::pow(10.0, peakDb / 40.0), wv = 2 * M_PI * f0 / kFs, al = std::sin(wv) / (2 * q), a0 = 1 + al / A;
    const double b0 = (1 + al * A) / a0, b1 = -2 * std::cos(wv) / a0, b2 = (1 - al * A) / a0, a2 = (1 - al / A) / a0;
    double lp = 0, s1 = 0, s2 = 0;
    Buf out(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        const double fb = buf[w];
        lp += c * (double(in[i]) + g * fb - lp);
        const double y = b0 * lp + s1;
        s1 = b1 * lp - b1 * y + s2;
        s2 = b2 * lp - a2 * y;
        buf[w] = std::tanh(y);
        w = (w + 1) % D;
        out[i] = float(fb);
    }
    return out;
}

// The same loop with WOBBLE-like wow on its delay (ADR 0034 round 2): two
// slow unrelated sines, +-depth samples each (~8 cents per pass at 60),
// linear-interpolated read. A tank's modes drift by about a bin.
Buf loopWow(const Buf& in, double L, double t60, double f0, double q, double peakDb, double depth)
{
    const size_t size = 1 << 15;
    std::vector<double> buf(size, 0.0);
    const double g = std::pow(10.0, -3.0 * L / t60), c = 1.0 - std::exp(-2 * M_PI * 6000.0 / kFs);
    const double A = std::pow(10.0, peakDb / 40.0), wv = 2 * M_PI * f0 / kFs, al = std::sin(wv) / (2 * q), a0 = 1 + al / A;
    const double b0 = (1 + al * A) / a0, b1 = -2 * std::cos(wv) / a0, b2 = (1 - al * A) / a0, a2 = (1 - al / A) / a0;
    double lp = 0, s1 = 0, s2 = 0;
    Buf out(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        const double t = double(i) / kFs;
        const double d = L * kFs + depth * (2.0 + std::sin(2 * M_PI * 0.37 * t) + std::sin(2 * M_PI * 0.61 * t + 1.0));
        const double rp = double(i) - d;
        double fb = 0.0;
        if (rp >= 0) {
            const size_t i0 = size_t(rp);
            const double fr = rp - double(i0);
            fb = buf[i0 & (size - 1)] + fr * (buf[(i0 + 1) & (size - 1)] - buf[i0 & (size - 1)]);
        }
        lp += c * (double(in[i]) + g * fb - lp);
        const double y = b0 * lp + s1;
        s1 = b1 * lp - b1 * y + s2;
        s2 = b2 * lp - a2 * y;
        buf[i & (size - 1)] = std::tanh(y);
        out[i] = float(fb);
    }
    return out;
}

Buf decayingNoise(double totalS, double t60, double rmsDb, unsigned seed)
{
    Buf x(size_t(totalS * kFs), 0.0f);
    std::mt19937 r(seed);
    std::normal_distribution<double> n(0, 1);
    const double amp = std::pow(10.0, rmsDb / 20.0);
    for (size_t i = size_t(kFs); i < x.size(); ++i)
        x[i] = float(amp * n(r) * std::pow(10.0, -3.0 * (double(i) / kFs - 1.0) / t60));
    return x;
}

// Dense modal tail: many decaying modes, T60 smooth in frequency (longer at
// lows) with +-15 % random spread per mode, like a real resonator.
Buf modalTail(double totalS, double t60mid, unsigned seed, int modes)
{
    Buf x(size_t(totalS * kFs), 0.0f);
    std::mt19937 r(seed);
    std::uniform_real_distribution<double> u(0, 1);
    const size_t a = size_t(kFs);
    for (int m = 0; m < modes; ++m) {
        const double f = 80.0 * std::pow(12000.0 / 80.0, u(r));
        const double t60 = t60mid * std::pow(1000.0 / f, 0.3) * (0.85 + 0.3 * u(r));
        const double dec = std::exp(-6.9078 / (t60 * kFs)), ph = 2 * M_PI * u(r), amp = (u(r) - 0.5) / std::sqrt(double(modes));
        const double wv = 2 * M_PI * f / kFs, cr = std::cos(wv) * dec, ci = std::sin(wv) * dec;
        double re = std::cos(ph), im = std::sin(ph), e = 1.0;
        for (size_t i = a; i < x.size() && e > 1e-6; ++i) {
            x[i] += float(amp * re);
            const double nr = re * cr - im * ci;
            im = re * ci + im * cr;
            re = nr;
            e *= dec;
        }
    }
    return x;
}

void addSine(Buf& x, double f, double rmsDb, double from)
{
    const double a = std::pow(10.0, rmsDb / 20.0) * std::sqrt(2.0);
    for (size_t i = size_t(from * kFs); i < x.size(); ++i) x[i] += float(a * std::sin(2 * M_PI * f * double(i) / kFs));
}

} // namespace syn

void ringingMetricCalibration()
{
    const float sr = float(syn::kFs);
    struct Case {
        const char* what;
        syn::Buf x;
        bool expect;
    };
    std::vector<Case> cases;
    {
        std::mt19937 r(1);
        std::normal_distribution<double> n(0, 0.1);
        syn::Buf w(size_t(8 * syn::kFs)), p(w.size());
        float y = 0;
        for (size_t i = 0; i < w.size(); ++i) { w[i] = float(n(r)); y += 0.05f * (w[i] - y); p[i] = 4.0f * y; }
        cases.push_back({"white noise", w, false});
        cases.push_back({"low-passed (pinkish) noise", p, false});
    }
    cases.push_back({"decaying white noise, T60 3 s", syn::decayingNoise(12, 3.0, -10, 3), false});
    cases.push_back({"dense modal tail (800 modes, +-15 % T60 spread)", syn::modalTail(10, 2.0, 4, 800), false});
    const syn::Buf burst = syn::noiseBurst(12, 0.05, 0.5, 7);
    cases.push_back({"plain feedback loop L 50 ms, T60 2 s (a comb, no resonance)", syn::loop(burst, 0.05, 2.0, 1100, 8, 0.0), false});
    cases.push_back({"feedback loop L 50 ms, T60 2 s, +2 dB Q 8 resonance at 1.1 kHz", syn::loop(burst, 0.05, 2.0, 1100, 8, 2.0), true});
    cases.push_back({"feedback loop L 80 ms, T60 2 s, +2 dB Q 8 resonance at 1.1 kHz", syn::loop(burst, 0.08, 2.0, 1100, 8, 2.0), true});
    cases.push_back({"feedback loop L 100 ms, T60 2 s, +2 dB Q 20 resonance at 300 Hz", syn::loop(burst, 0.1, 2.0, 300, 20, 2.0), true});
    // ADR 0034 round 2: pitch movement alone is not Ringing (a mode drifting
    // into a bin used to read as a jump of 10-20 dB), a resonance still is.
    for (double L : {0.04, 0.05, 0.069})
        cases.push_back({L == 0.05 ? "plain feedback loop L 50 ms, T60 3 s, with slow wow on its delay (~8 cents per pass)"
                                   : (L < 0.05 ? "plain feedback loop L 40 ms, T60 3 s, with slow wow" : "plain feedback loop L 69 ms, T60 3 s, with slow wow"),
                         syn::loopWow(burst, L, 3.0, 1100, 8, 0.0, 60.0), false});
    cases.push_back({"feedback loop L 50 ms, T60 2 s, +2 dB Q 8 resonance at 1.1 kHz, with slow wow",
                     syn::loopWow(burst, 0.05, 2.0, 1100, 8, 2.0, 60.0), true});
    cases.push_back({"self-oscillating loop L 30 ms, +3 dB Q 20 at 3 kHz (steady tone)", syn::loop(burst, 0.03, 2.0, 3000, 20, 3.0), true});
    {
        syn::Buf x = syn::decayingNoise(12, 3.0, -10, 8);
        syn::addSine(x, 440.0, -30.0, 1.0);
        cases.push_back({"sine at -20 dB re a decaying noise tail's start", x, true});
    }
    for (const Case& c : cases) {
        const auto m = rv::metrics::compute({c.x}, sr);
        char what[256];
        std::snprintf(what, sizeof what, "ringing: %s -> %s (ringing_db %.1f at %.0f Hz, limit %.0f)", c.what,
                      c.expect ? "flagged" : "passes", m.ringingDb, m.ringingHz, rv::metrics::kRingingGrowthDb);
        check(m.ringing == c.expect && (c.expect || !std::isnan(m.ringingDb)), what);
    }
    // steady_tone (SPEC) must now also see a saturated self-oscillating loop,
    // whose harmonics are about as prominent as its fundamental.
    const auto so = rv::metrics::compute({syn::loop(burst, 0.03, 2.0, 3000, 20, 3.0)}, sr);
    check(so.steadyTone, "steady_tone: true for a saturated self-oscillating loop (tone + harmonics)");
}

void howlCriterion()
{
    const float sr = float(syn::kFs);
    // A bare steady sine: no floor, no movement -> not a Howl.
    syn::Buf sine(size_t(8 * syn::kFs), 0.0f);
    syn::addSine(sine, 300.0, -12.0, 0.0);
    const auto a = rv::metrics::compute({sine}, sr);
    // A rough, moving roar: a gliding tone (+-2 % at 0.4 Hz) with harmonics
    // (tanh) over loud broadband noise (a roar, not a whistle).
    syn::Buf roar(sine.size());
    std::mt19937 r(5);
    std::normal_distribution<double> n(0, 1);
    double ph = 0;
    for (size_t i = 0; i < roar.size(); ++i) {
        const double t = double(i) / syn::kFs;
        ph += 2 * M_PI * 300.0 * (1.0 + 0.02 * std::sin(2 * M_PI * 0.4 * t)) / syn::kFs;
        roar[i] = float(0.3 * std::tanh(2.0 * std::sin(ph)) + 0.2 * n(r));
    }
    const auto b = rv::metrics::compute({roar}, sr);
    char what[256];
    std::snprintf(what, sizeof what, "howl: bare steady sine fails ADR 0019 (floor %.0f dB, moves %.2f %% / %.1f dB)",
                  a.howlFloorDb, a.howlMovePct, a.howlMoveDb);
    check(!a.howlOk, what);
    std::snprintf(what, sizeof what, "howl: rough gliding roar passes ADR 0019 (floor %.0f dB, moves %.2f %% / %.1f dB)",
                  b.howlFloorDb, b.howlMovePct, b.howlMoveDb);
    check(b.howlOk, what);
}

void sidecarMetricsRoundTrip()
{
    rv::metrics::Metrics m;
    m.peakDbfs = -3.1; m.rmsDbfs = -24.0; m.t60S = 8.7; m.resonancePeakDb = 7.5;
    m.steadyTone = false; m.nanInfCount = 0; m.clipCount = 0; m.clickCount = 2;
    m.stereoCorrelation = 0.2; m.monoLossDb = -0.3; m.monoNotchDb = -2.5; m.maxStepDb100ms = 1.1;
    m.ringingDb = 4.5; m.ringingHz = 1234.0; m.ringingRatio = 1.2; m.ringingEndDb = 8.0; m.ringingSpanS = 3.5;
    m.ringing = false; m.howlFloorDb = -12.0; m.howlMovePct = 0.8; m.howlMoveDb = 2.0; m.howlOk = true;
    rv::json::Value v = rv::sidecar::metricsToJson(m);
    rv::metrics::Metrics back = rv::sidecar::jsonToMetrics(v);
    bool ok = back.peakDbfs == m.peakDbfs && back.rmsDbfs == m.rmsDbfs && back.t60S == m.t60S
           && back.resonancePeakDb == m.resonancePeakDb && back.steadyTone == m.steadyTone
           && back.nanInfCount == m.nanInfCount && back.clipCount == m.clipCount && back.clickCount == m.clickCount
           && back.stereoCorrelation == m.stereoCorrelation && back.monoLossDb == m.monoLossDb
           && back.monoNotchDb == m.monoNotchDb && back.maxStepDb100ms == m.maxStepDb100ms
           && back.ringingDb == m.ringingDb && back.ringingHz == m.ringingHz && back.ringingRatio == m.ringingRatio
           && back.ringingEndDb == m.ringingEndDb && back.ringingSpanS == m.ringingSpanS && back.ringing == m.ringing
           && back.howlFloorDb == m.howlFloorDb && back.howlMovePct == m.howlMovePct && back.howlMoveDb == m.howlMoveDb
           && back.howlOk == m.howlOk;
    check(ok, "sidecar: metrics -> JSON -> metrics round-trips");

    rv::metrics::Metrics withNulls;
    rv::json::Value v2 = rv::sidecar::metricsToJson(withNulls);
    const rv::json::Value* t60 = v2.find("t60_s");
    check(t60 && t60->isNull(), "sidecar: unmeasurable t60_s is written as JSON null");
    const rv::json::Value* corr = v2.find("stereo_correlation");
    check(corr && corr->isNull(), "sidecar: mono stereo_correlation is written as JSON null");

    // Old (pre-M4) sidecar JSON that never had the new keys at all must
    // still parse, with the new metrics reported as "not measured".
    rv::json::Value legacy = rv::json::Value::makeObject();
    legacy.set("peak_dbfs", rv::json::Value::makeNumber(-6.0));
    legacy.set("rms_dbfs", rv::json::Value::makeNumber(-20.0));
    legacy.set("t60_s", rv::json::Value::makeNumber(5.0));
    legacy.set("resonance_peak_db", rv::json::Value::makeNumber(4.0));
    legacy.set("steady_tone", rv::json::Value::makeBool(false));
    legacy.set("nan_inf_count", rv::json::Value::makeNumber(0));
    legacy.set("clip_count", rv::json::Value::makeNumber(0));
    legacy.set("click_count", rv::json::Value::makeNumber(0));
    rv::metrics::Metrics legacyBack = rv::sidecar::jsonToMetrics(legacy);
    check(std::isnan(legacyBack.stereoCorrelation) && std::isnan(legacyBack.monoLossDb)
              && std::isnan(legacyBack.monoNotchDb) && std::isnan(legacyBack.maxStepDb100ms),
          "sidecar: pre-M4 JSON missing the new keys entirely -> reported as not measured");
    check(std::isnan(legacyBack.ringingDb) && !legacyBack.ringing && std::isnan(legacyBack.howlFloorDb) && !legacyBack.howlOk,
          "sidecar: pre-M6 JSON without ringing_* / howl_* keys -> not measured");
}

void sidecarSpectrogramRoundTrip()
{
    std::vector<float> mono(48000 * 2);
    for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * std::sin(2.0f * float(M_PI) * 300.0f * float(i) / 48000.0f);
    rv::spectrogram::Spectrogram s = rv::spectrogram::compute(mono, 48000.0f, 64, 32);
    rv::json::Value v = rv::sidecar::spectrogramToJson(s);
    rv::spectrogram::Spectrogram back = rv::sidecar::jsonToSpectrogram(v);
    const bool ok = back.width == s.width && back.height == s.height && back.data == s.data;
    check(ok, "sidecar: spectrogram -> JSON (base64) -> spectrogram round-trips");
}

} // namespace

int main()
{
    jsonRoundTrips();
    base64RoundTrips();
    fftMatchesDirectDft();
    t60MeasuresKnownDecays();
    t60IgnoresNextEventBuildUp();
    t60HandlesNoiseFloor();
    resonancePeakDistinguishesTone();
    steadyToneDetectsSustainedSine();
    clickCountOnStimulusFiles();
    clickCountFlagsInjectedStep();
    stereoMetricsOnSyntheticSignals();
    ringingMetricCalibration();
    howlCriterion();
    sidecarMetricsRoundTrip();
    sidecarSpectrogramRoundTrip();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
