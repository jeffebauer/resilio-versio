#pragma once
// Applies JSON parameter values (preset files, sweep grids, --set) to a
// Tank via the shared ParamSpec table (docs/m1-contracts.md Stream B).
// Knobs take a normalised number 0-1; Switch3 params take either a
// ParamSpec::choices label or a number 0 / 0.5 / 1.

#include "Json.h"
#include "dsp/Tank.h"

#include <string>

namespace rv::paramsjson {

bool findParamId(const std::string& key, ParamId& id);

bool applyValue(Tank& tank, ParamId id, const json::Value& val, std::string& error);

// Applies every key in a preset object, e.g. { "decay": 0.8, "springs": "2" }.
bool applyPreset(Tank& tank, const json::Value& preset, std::string& error);

} // namespace rv::paramsjson
