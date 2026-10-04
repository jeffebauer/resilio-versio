#pragma once
// Automation JSON: parameter breakpoints over time + Kick events
// (docs/m1-contracts.md Stream B). Linear interpolation between
// breakpoints per key.
//
// { "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.2}, ... ],
//   "kicks": [1.5, 3.0],
//   "clocks": [0.0, 0.5, 1.0],  // gate rising edges (seconds): echo mode's clock (ADR 0041)
//   "clock_bpm": 120 }          // or a steady clock: a number (the whole file) or
//                               // {"bpm": 120, "start": 0, "end": 8} (seconds)

#include "Json.h"
#include "dsp/Tank.h"

#include <string>
#include <utility>
#include <vector>

namespace rv::automation {

struct Track {
    ParamId key;
    std::vector<std::pair<double, double>> points; // (t seconds, value), sorted by t
};

struct Automation {
    std::vector<Track> tracks;         // one per automated key, insertion order
    std::vector<double> kicksSeconds;  // sorted ascending
    std::vector<double> clocksSeconds; // gate rising edges ("clocks"), sorted
    double clockBpm = 0.0, clockStart = 0.0, clockEnd = -1.0; // "clock_bpm" (end < 0 = to the end of the file)
};

// Linear interpolation; clamps to the first/last point outside the range.
double valueAt(const Track& track, double t);

bool parse(const json::Value& root, Automation& out, std::string& error);
bool loadFile(const std::string& path, Automation& out, std::string& error);

} // namespace rv::automation
