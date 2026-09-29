#pragma once
// Sweep JSON: a cartesian grid over params rendered against one input,
// with tail silence appended so tails ring out (docs/m1-contracts.md
// Stream B).
//
// { "name": "m1_grid", "input": "...", "base": {preset}, "tail_seconds": 12,
//   "grid": { "decay": [0, 0.5, 1], "tension": [0, 0.5, 1] },
//   "ignore_flags": ["click_count"] }   // optional: metrics the review page shouldn't flag
//   (e.g. click_count for impulse inputs, whose own echoes read as clicks)

#include "Json.h"

#include <string>
#include <utility>
#include <vector>

namespace rv::sweep {

struct Axis {
    std::string key;
    std::vector<double> values;
};

struct Config {
    std::string name;
    std::string input;
    json::Value base = json::Value::makeObject();
    std::vector<Axis> grid; // insertion order preserved
    double tailSeconds = 0.0;
    json::Value ignoreFlags = json::Value::makeArray(); // copied into manifest.json
};

bool parse(const json::Value& root, Config& out, std::string& error);
bool loadFile(const std::string& path, Config& out, std::string& error);

using Combo = std::vector<std::pair<std::string, double>>; // one grid point, axis order

// Full cartesian product over `grid`, axis order preserved (first axis
// varies slowest), deterministic.
std::vector<Combo> cartesian(const std::vector<Axis>& grid);

// "<name>__decay0.50_tension1.00" (no extension).
std::string fileBaseName(const std::string& name, const Combo& combo);

} // namespace rv::sweep
