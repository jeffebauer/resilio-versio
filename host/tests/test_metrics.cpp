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

void sidecarMetricsRoundTrip()
{
    rv::metrics::Metrics m;
    m.peakDbfs = -3.1; m.rmsDbfs = -24.0; m.t60S = 8.7; m.resonancePeakDb = 7.5;
    m.steadyTone = false; m.nanInfCount = 0; m.clipCount = 0; m.clickCount = 2;
    rv::json::Value v = rv::sidecar::metricsToJson(m);
    rv::metrics::Metrics back = rv::sidecar::jsonToMetrics(v);
    bool ok = back.peakDbfs == m.peakDbfs && back.rmsDbfs == m.rmsDbfs && back.t60S == m.t60S
           && back.resonancePeakDb == m.resonancePeakDb && back.steadyTone == m.steadyTone
           && back.nanInfCount == m.nanInfCount && back.clipCount == m.clipCount && back.clickCount == m.clickCount;
    check(ok, "sidecar: metrics -> JSON -> metrics round-trips");

    rv::metrics::Metrics withNulls;
    rv::json::Value v2 = rv::sidecar::metricsToJson(withNulls);
    const rv::json::Value* t60 = v2.find("t60_s");
    check(t60 && t60->isNull(), "sidecar: unmeasurable t60_s is written as JSON null");
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
    resonancePeakDistinguishesTone();
    steadyToneDetectsSustainedSine();
    clickCountOnStimulusFiles();
    clickCountFlagsInjectedStep();
    sidecarMetricsRoundTrip();
    sidecarSpectrogramRoundTrip();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
