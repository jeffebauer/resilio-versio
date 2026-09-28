#pragma once
// Renderer metrics (docs/m1-contracts.md Stream B, SPEC §4.10 / §6.2, plus
// the stereo metrics added by docs/m4-contracts.md Stream E).
// All analysis metrics (t60, resonance, steady_tone, click_count) run on
// the mono downmix (sum of channels / channel count); nan_inf_count and
// clip_count scan every channel. peak/rms are also over the mono downmix
// per the contract. Use NaN in t60S / resonancePeakDb to mean "null" /
// not measurable (written as JSON null).
//
// Stereo metrics (stereoCorrelation, monoLossDb, monoNotchDb) are NaN
// ("null" / "mono" in the sidecar and review page) whenever the file has
// fewer than 2 channels -- they need a real L/R pair. maxStepDb100ms is a
// mono metric like t60/resonance and is always attempted regardless of
// channel count.

#include <cmath>
#include <string>
#include <vector>

namespace rv::metrics {

struct Metrics {
    double peakDbfs        = -200.0;
    double rmsDbfs          = -200.0;
    double t60S             = std::nan("");
    double resonancePeakDb  = std::nan("");
    bool   steadyTone       = false;
    long   nanInfCount      = 0;
    long   clipCount        = 0;
    long   clickCount       = 0;
    double stereoCorrelation = std::nan(""); // Pearson r of L/R over the T60 segment; null for mono
    double monoLossDb        = std::nan(""); // 10*log10(power(L+R) / (power(L)+power(R))); null for mono
    double monoNotchDb       = std::nan(""); // deepest mono-sum-vs-stereo-avg dip, 200Hz-5kHz; null for mono
    double maxStepDb100ms    = std::nan(""); // largest |RMS dB| step between consecutive 100ms windows
};

// `channels` is [channel][frame], any channel count >= 1. The stereo
// metrics (stereoCorrelation/monoLossDb/monoNotchDb) read `channels[0]`
// and `channels[1]` directly when `channels.size() >= 2`, unless
// `stereoSource` is given, in which case they read `(*stereoSource)[0]`
// and `[1]` instead -- this lets a caller downmix/select a channel for
// the classic mono-based metrics (e.g. --analyze's --channel L|R|mix)
// while still measuring the file's real stereo image.
Metrics compute(const std::vector<std::vector<float>>& channels, float sampleRate,
                 const std::vector<std::vector<float>>* stereoSource = nullptr);

// One-line human summary printed to stdout per render.
std::string summaryLine(const Metrics& m);

} // namespace rv::metrics
