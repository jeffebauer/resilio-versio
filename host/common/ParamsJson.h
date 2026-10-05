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
// steps; TankVoicing.h kFLowCutSteps, ADR 0038 "Round F2") and
// "tone_place_voicing" (where the Big Knob acts: 0 = before the Springs, as
// first shipped, for reference; 1 = on the wet, the default since the
// owner's pick of 4 Oct 2026; DriveVoicing.h "TONE placement", ADR 0036).
// "echo_mode" (1 = SPRINGS 3 is echo mode, the default and what ships; 0 =
// the coupled Springs reference, springs3_voicing; ADR 0041) and "host_bpm"
// (echo mode's clock as the Plugin gets it from the DAW: a tempo in bpm, 0 =
// none; Tank::setHostTempo). "hold_voicing" (the Hold at the top of DECAY
// in CLEAN / DRIVEN: 0 = freeze, 1 = layer, the default; core/params/
// ThrowHold.h, ADR 0040) and "duck_voicing" (the Hold's ducking depth: 0 =
// 12 dB, the default, 1 = 18 dB, 2 = none).
// Returns false if `key` isn't one.
inline constexpr const char* kWobbleVoicingKey   = "wobble_voicing";
inline constexpr const char* kSustainVoicingKey  = "sustain_voicing";
inline constexpr const char* kSplashVoicingKey   = "splash_voicing";
inline constexpr const char* kToneVoicingKey     = "tone_voicing";
inline constexpr const char* kSprings3VoicingKey = "springs3_voicing";
inline constexpr const char* kTankVoicingKey     = "tank_voicing";
inline constexpr const char* kFLowCutVoicingKey  = "f_lowcut_voicing";
inline constexpr const char* kOutputBitsKey      = "output_bits_voicing"; // 1 (default) DRIVEN 12-bit / KICKED 10-bit mu-law output, 0 before it (ADR 0042)
inline constexpr const char* kHoldVoicingKey     = "hold_voicing";
inline constexpr const char* kDuckVoicingKey     = "duck_voicing";
inline constexpr const char* kTonePlaceVoicingKey = "tone_place_voicing";
inline constexpr const char* kEchoModeKey        = "echo_mode";
inline constexpr const char* kHostBpmKey         = "host_bpm";
inline constexpr const char* kEchoDiffuseKey     = "echo_diffuse_voicing"; // 0 none ... 3 heavy (EchoVoicing.h kDiffuse)
inline constexpr const char* kEchoWearKey        = "echo_wear_voicing";    // 0 none, 1 worn tape, 2 radio band, 3 BBD grit (default), 4 crushed, 5 tape sat, 6 / 7 + crinkle subtle / obvious
inline constexpr const char* kBbdKey             = "bbd_voicing";          // BBD strength: 0 A today, 1 B, 2 C, 3 D tracks the echo time
inline constexpr const char* kEchoBitsKey        = "echo_bits_voicing";    // 0 none, 1 24 kHz/12-bit, 2 24 kHz/8-bit, 3 24 kHz/8-bit mu-law
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
