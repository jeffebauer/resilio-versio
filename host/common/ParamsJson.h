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
// core/params/DriveVoicing.h, ADR 0035 round 3), "splash_voicing" (0 =
// today, 1 = stronger top, 2 = + DRIVE-free, 3 = bolder; core/params/
// SplashVoicing.h, ADR 0032 "SPLASH stronger"), "tone_voicing" (0 =
// today ... 5 = the bump on hits; the Big Knob, DriveVoicing.h, ADR 0036)
// "springs3_voicing" (what SPRINGS position 3 does: 0 = today, 1 = long
// tank, 2 = in series, 3 = wide, 4 = pan tank, 5-10 round 2; core/params/
// Springs3Voicing.h, ADR 0037) and "tank_voicing" (0 = round 3's today,
// 1-4 round 3, 5-7 round 4; core/params/TankVoicing.h, ADR 0038) and
// "f_lowcut_voicing" (tank voicing 7's low cut: 0 = F's own, 1-3 gentler
// steps; TankVoicing.h kFLowCutSteps, ADR 0038 "Round F2") and "hold_voicing"
// (the Hold at the top of DECAY in CLEAN / DRIVEN: 0 = freeze, 1 = layer, the
// default; core/params/ThrowHold.h, ADR 0040) and "duck_voicing" (the Hold's
// ducking depth on the lows: 0 = 12 dB, the default, 1 = 18 dB).
// Returns false if `key` isn't one.
inline constexpr const char* kWobbleVoicingKey   = "wobble_voicing";
inline constexpr const char* kSustainVoicingKey  = "sustain_voicing";
inline constexpr const char* kSplashVoicingKey   = "splash_voicing";
inline constexpr const char* kToneVoicingKey     = "tone_voicing";
inline constexpr const char* kSprings3VoicingKey = "springs3_voicing";
inline constexpr const char* kTankVoicingKey     = "tank_voicing";
inline constexpr const char* kFLowCutVoicingKey  = "f_lowcut_voicing";
inline constexpr const char* kHoldVoicingKey     = "hold_voicing";
inline constexpr const char* kDuckVoicingKey     = "duck_voicing";
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
