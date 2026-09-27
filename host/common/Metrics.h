#pragma once
// Renderer metrics (docs/m1-contracts.md Stream B, SPEC §4.10 / §6.2).
// All analysis metrics (t60, resonance, steady_tone, click_count) run on
// the mono downmix (sum of channels / channel count); nan_inf_count and
// clip_count scan every channel. peak/rms are also over the mono downmix
// per the contract. Use NaN in t60S / resonancePeakDb to mean "null" /
// not measurable (written as JSON null).

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
};

// `channels` is [channel][frame], any channel count >= 1.
Metrics compute(const std::vector<std::vector<float>>& channels, float sampleRate);

// One-line human summary printed to stdout per render.
std::string summaryLine(const Metrics& m);

} // namespace rv::metrics
