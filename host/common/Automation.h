#pragma once
// Automation JSON: parameter breakpoints over time (docs/m1-contracts.md
// Stream B) + gate events (THROW, ADR 0039) + button events (ADR 0043).
// Linear interpolation between breakpoints per key.
//
// { "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.2}, ... ],
//   "gates": [[1.0, 1.25], [3.0, 3.5]],
//   "buttons": [[5.0, 5.1], [5.3, 7.5]], // press, release (seconds)
//   "throw_exits": [6.0],
//   "clocks": [0.0, 0.5, 1.0],  // gate rising edges (seconds): echo mode's clock (ADR 0041)
//   "clock_bpm": 120 }          // or a steady clock: a number (the whole file) or
//                               // {"bpm": 120, "start": 0, "end": 8} (seconds)
//
// "gates": the gate input, as [rise, fall] pairs in seconds: high from rise
// to fall, sample-accurate (Tank::gate). The first rise switches the throw
// on (until then the send is open, as unpatched). The "throw_gate" key in
// "breakpoints" does the same at the Renderer's 16-sample granularity.
// "buttons": the panel button, as [press, release] pairs in seconds,
// sample-accurate (Tank::button): a hand throw in SPRINGS 1-2 (the example
// above is a tap, then a double tap held 2.2 s: throw mode off), tap tempo
// in SPRINGS 3.
// "throw_exits": throw mode off at each time (Tank::exitThrowMode(), what
// the button's double-tap-and-hold does), at the start of the Renderer's
// 16-sample block holding it.
// "kicks" is refused: the Kick was removed (ADR 0043).

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
    // Gate changes (seconds, high?), sorted by time; from "gates" [rise, fall] pairs.
    std::vector<std::pair<double, bool>> gateEvents;
    // Button changes (seconds, down?), sorted by time; from "buttons" [press, release] pairs.
    std::vector<std::pair<double, bool>> buttonEvents;
    std::vector<double> throwExitsSeconds; // sorted ascending
    std::vector<double> clocksSeconds; // gate rising edges ("clocks"), sorted
    double clockBpm = 0.0, clockStart = 0.0, clockEnd = -1.0; // "clock_bpm" (end < 0 = to the end of the file)
};

// Linear interpolation; clamps to the first/last point outside the range.
double valueAt(const Track& track, double t);

bool parse(const json::Value& root, Automation& out, std::string& error);
bool loadFile(const std::string& path, Automation& out, std::string& error);

} // namespace rv::automation
