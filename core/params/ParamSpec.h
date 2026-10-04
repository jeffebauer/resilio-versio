#pragma once
// Single source of truth for every parameter (SPEC §6.1).
// Every Host passes Normalised values (0–1). Mapping curves to internal
// units are added per milestone as each DSP block lands; until then a
// parameter only carries its default and smoothing tier.

#include <array>
#include <cstddef>
#include <cstdint>

namespace rv {

enum class ParamId : uint8_t {
    Decay,
    Tone,
    Tension,
    Splash,
    Drive,
    Wobble,
    Mix,
    Springs,
    Attitude,
    Throw, // not a panel knob: the gate on hardware, an automatable switch in the Plugin (ADR 0039)
    Count
};

enum class ParamKind : uint8_t {
    Knob,   // continuous 0–1 (Versio knob + CV)
    Switch3, // 3-position toggle, Normalised as 0 / 0.5 / 1
    Toggle   // on / off, Normalised as 0 / 1 (>= 0.5 reads on)
};

// Smoothing tiers (ADR 0015).
enum class Smoothing : uint8_t {
    Snappy,  // ~5 ms: level and colour, safe to move fast
    Gliding, // ~50–100 ms: moves delay lengths / allpass coefficients
    Switch   // handled by Morph / crossfade (ADR 0003)
};

struct ParamSpec {
    ParamId     id;
    // Stable identifier for presets and plugin params. Never rename one: hosts
    // save a device's parameters by it (VST3 ID = a hash of the key), so a
    // rename leaves a dead slot in every saved set (BOING -> TENSION, 29 Sep
    // 2026: the owner's Ableton set kept a dead BOING). Change `name` instead.
    const char* key;
    const char* name; // panel label
    ParamKind   kind;
    float       defaultValue; // Normalised
    Smoothing   smoothing;
    float       smoothingMs;
    const char* choices[3]; // Switch3 labels, left / centre / right
};

inline constexpr std::array<ParamSpec, static_cast<size_t>(ParamId::Count)> kParams{{
    {ParamId::Decay,    "decay",    "DECAY",    ParamKind::Knob,    0.5f, Smoothing::Gliding, 80.0f, {}},
    {ParamId::Tone,     "tone",     "TONE",     ParamKind::Knob,    0.5f, Smoothing::Snappy,   5.0f, {}},
    {ParamId::Tension,  "tension",  "TENSION",  ParamKind::Knob,    0.5f, Smoothing::Gliding, 60.0f, {}},
    {ParamId::Splash,   "splash",   "SPLASH",   ParamKind::Knob,    0.3f, Smoothing::Snappy,   5.0f, {}},
    {ParamId::Drive,    "drive",    "DRIVE",    ParamKind::Knob,    0.25f, Smoothing::Snappy,  5.0f, {}},
    {ParamId::Wobble,   "wobble",   "WOBBLE",   ParamKind::Knob,    0.45f, Smoothing::Gliding, 80.0f, {}},
    {ParamId::Mix,      "mix",      "MIX",      ParamKind::Knob,    0.5f, Smoothing::Snappy,   5.0f, {}},
    {ParamId::Springs,  "springs",  "SPRINGS",  ParamKind::Switch3, 0.5f, Smoothing::Switch,  20.0f, {"1", "2", "3"}},
    {ParamId::Attitude, "attitude", "ATTITUDE", ParamKind::Switch3, 0.5f, Smoothing::Switch,  20.0f, {"CLEAN", "DRIVEN", "KICKED"}},
    // THROW (ADR 0039): the Plugin's and Renderer's stand-in for the gate.
    // On = gate high (the send open), off = gate low. Like the gate, it only
    // takes effect from the first time it goes on: until then the send is
    // open (Tank::gate). Not smoothed here: the Tank ramps the send itself.
    {ParamId::Throw,    "throw_gate", "THROW",  ParamKind::Toggle,  0.0f, Smoothing::Switch,   2.0f, {}},
}};

constexpr const ParamSpec& spec(ParamId id) { return kParams[static_cast<size_t>(id)]; }

// Switch3 position (0, 1, 2) <-> Normalised value.
constexpr float switchToNormalised(int position) { return static_cast<float>(position) * 0.5f; }
constexpr int normalisedToSwitch(float v) { return v < 0.25f ? 0 : (v < 0.75f ? 1 : 2); }

} // namespace rv
