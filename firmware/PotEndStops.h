#pragma once
// Release firmware: knob end stops (owner, 4 Oct 2026). Pure maths, tested
// on desktop in host/tests/test_pot_endstops.cpp.
//
// The Versio's pots (each summed with its CV jack in hardware) rarely reach
// exactly 0 or 1: M0 only showed "within 2 %". MIX is equal-power, so a pot
// that tops out at 0.98 still lets the dry through at about -30 dB, heard on
// the module as an un-TONE'd dry at MIX fully right (the plugin, whose knob
// reaches exactly 1, had none). A small dead zone at each end makes fully
// CCW exactly 0 and fully CW exactly 1 for every knob; in between the
// reading is stretched linearly, so noon stays noon (WOBBLE's own ±3 %
// dead zone at noon is unchanged). The Plugin and Renderer don't use this:
// their values already reach both ends.

namespace rvpot {

// Readings within this distance of either end snap to it.
constexpr float kEndZone = 0.025f;

inline float endStops(float raw)
{
    const float v = (raw - kEndZone) / (1.0f - 2.0f * kEndZone);
    return v <= 0.0f ? 0.0f : (v >= 1.0f ? 1.0f : v);
}

} // namespace rvpot
