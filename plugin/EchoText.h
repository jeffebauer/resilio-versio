#pragma once
// TENSION's readout in SPRINGS 3 echo mode (Plugin Host): the note value the
// echo plays (EchoVoicing.h kDivisionBeats) instead of a percentage, both on
// the panel's knob and in the host's parameter text. JUCE-free, so
// plugin_host_test checks it.
//
// Only while the echo is clocked (in a DAW, always: the host's tempo); in
// SPRINGS 1-2, or unclocked, TENSION reads as a percentage as before.
// It names the zone the Tank settles on for that value: the zone playing
// (Tank::echoDivision()) while the value stays within its borders plus the
// hysteresis (kDivisionHysteresis, the rule in Tank::echoTick), else the
// value's own zone. Computed from the value, not read back after the Tank
// has moved (TENSION glides there), so the text is right while dragging.

#include "params/EchoVoicing.h"

#include <algorithm>
#include <atomic>

namespace rv::plugin {

// Long (TENSION 0) -> short (TENSION 1), as kDivisionBeats.
inline constexpr const char* kDivisionNames[echo::kNumDivisions] = {
    "1/2", "1/4 dotted", "1/4", "1/8 dotted", "1/8", "1/16 dotted", "1/16"};
static_assert(echo::kNumDivisions == 7, "kDivisionNames follows kDivisionBeats");

// What the audio thread last played, for the message thread's text.
struct EchoReadout {
    // Audio thread, once per block after Tank::process().
    void note(bool echoMode, int division) { division_.store(echoMode ? division : -1, std::memory_order_relaxed); }
    // The note value's name for TENSION at `value`, or nullptr (show a percentage).
    const char* name(float value) const
    {
        const int playing = division_.load(std::memory_order_relaxed);
        if (playing < 0) return nullptr;
        const float u    = value * float(echo::kNumDivisions);
        const bool  stay = u >= float(playing) - echo::kDivisionHysteresis
                        && u <= float(playing + 1) + echo::kDivisionHysteresis;
        return kDivisionNames[stay ? playing : std::clamp(int(u), 0, echo::kNumDivisions - 1)];
    }

private:
    std::atomic<int>   division_{-1}; // -1: not echo mode, or unclocked
};

} // namespace rv::plugin
