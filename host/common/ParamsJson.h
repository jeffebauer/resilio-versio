#pragma once
// Applies JSON parameter values (preset files, sweep grids, --set) to a
// Tank via the shared ParamSpec table (docs/m1-contracts.md Stream B).
// In JSON, knobs take a normalised number 0-1; Switch3 params take either
// a ParamSpec::choices label or a number 0 / 0.5 / 1. `--set` is stricter
// (see applySetArg).

#include "Json.h"
#include "dsp/Tank.h"

#include <string>

namespace rv::paramsjson {

bool findParamId(const std::string& key, ParamId& id);

bool applyValue(Tank& tank, ParamId id, const json::Value& val, std::string& error);

// Hidden, Renderer-only keys (not panel controls, not in ParamSpec): used by
// prototype pages to compare voicings in one sweep: "wobble_voicing" (0 = A,
// 1 = B, 2 = C, 3 = D; core/params/WobbleVoicing.h, ADR 0034 round 2) and
// "sustain_voicing" (0 = off, the limiter hold only; 1 = round 2; 2 = gentle;
// core/params/DriveVoicing.h, ADR 0035 round 3) and "tone_voicing" (0 =
// today, 1 = steep, 2 = steep + bump, 3 = + ringier when driven; the Big
// Knob, DriveVoicing.h, ADR 0036 Proposed). Returns false if `key` isn't one.
inline constexpr const char* kWobbleVoicingKey  = "wobble_voicing";
inline constexpr const char* kSustainVoicingKey = "sustain_voicing";
inline constexpr const char* kToneVoicingKey    = "tone_voicing";
bool applyHidden(Tank& tank, const std::string& key, double value);
std::string wobbleVoicingLabel(const Tank& tank); // "A" / "B" / "C"

// Applies every key in a preset object (hidden keys included), e.g. { "decay": 0.8, "springs": "2" }.
bool applyPreset(Tank& tank, const json::Value& preset, std::string& error);

// Applies one rv_render `--set key=value` argument. Stricter than JSON,
// where the type tells a number from a label: knobs take a number 0-1;
// Switch3 params take only a ParamSpec::choices label (springs=2,
// attitude=KICKED), so a bare number on a switch is an error rather than
// a silently wrong position.
bool applySetArg(Tank& tank, const std::string& arg, std::string& error);

} // namespace rv::paramsjson
