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

// Hidden, Renderer-only keys (not panel controls, not in ParamSpec): used by
// prototype pages to compare voicings in one sweep. Today only
// "wobble_voicing" (0 = A, 1 = B, 2 = C; core/params/WobbleVoicing.h,
// ADR 0034 round 2). Returns false if `key` isn't one.
inline constexpr const char* kWobbleVoicingKey = "wobble_voicing";
bool applyHidden(Tank& tank, const std::string& key, double value);
std::string wobbleVoicingLabel(const Tank& tank); // "A" / "B" / "C"

// Applies every key in a preset object (hidden keys included), e.g. { "decay": 0.8, "springs": "2" }.
bool applyPreset(Tank& tank, const json::Value& preset, std::string& error);

} // namespace rv::paramsjson
