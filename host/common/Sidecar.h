#pragma once
// Sidecar JSON assembly: the contract with Stream C's review page
// (docs/m1-contracts.md). One sidecar per rendered/analyzed WAV.

#include "Json.h"
#include "Metrics.h"
#include "Spectrogram.h"
#include "dsp/Tank.h"

#include <string>

namespace rv::sidecar {

json::Value metricsToJson(const metrics::Metrics& m);
metrics::Metrics jsonToMetrics(const json::Value& v);

json::Value spectrogramToJson(const spectrogram::Spectrogram& s);
spectrogram::Spectrogram jsonToSpectrogram(const json::Value& v);

// Params object: knobs as numbers (normalised 0-1), switches as their
// label string (e.g. "springs": "1", "attitude": "CLEAN").
json::Value paramsToJson(const Tank& tank);

json::Value build(const std::string& wavFilename, int sampleRate, double durationS,
                   const json::Value& params, const metrics::Metrics& m,
                   const spectrogram::Spectrogram& spec);

} // namespace rv::sidecar
