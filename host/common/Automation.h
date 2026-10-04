#pragma once
// Automation JSON: parameter breakpoints over time + Kick events
// (docs/m1-contracts.md Stream B) + gate events (THROW, ADR 0039). Linear
// interpolation between breakpoints per key.
//
// { "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.2}, ... ],
//   "kicks": [1.5, 3.0],
//   "gates": [[1.0, 1.25], [3.0, 3.5]],
//   "throw_exits": [6.0] }
//
// "gates": the gate input, as [rise, fall] pairs in seconds: high from rise
// to fall, sample-accurate (Tank::gate). The first rise switches the throw
// on (until then the send is open, as unpatched). The "throw_gate" key in
// "breakpoints" does the same at the Renderer's 16-sample granularity.
// "throw_exits": a long press of KICK (Tank::exitThrowMode()) at each time,
// at the start of the Renderer's 16-sample block holding it (the press's
// own Kick is a separate "kicks" entry, as on the panel).

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
    // Gate changes (seconds, high?), sorted by time; from "gates" [rise, fall] pairs.
    std::vector<std::pair<double, bool>> gateEvents;
    std::vector<double> throwExitsSeconds; // sorted ascending
};

// Linear interpolation; clamps to the first/last point outside the range.
double valueAt(const Track& track, double t);

bool parse(const json::Value& root, Automation& out, std::string& error);
bool loadFile(const std::string& path, Automation& out, std::string& error);

} // namespace rv::automation
