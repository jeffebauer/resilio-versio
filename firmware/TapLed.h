#pragma once
// Tap tempo on the LEDs (ADR 0043 amendment, 6 Oct 2026; owner: "flashing
// the In/out LEDs a color that isn't used to denote level ... maybe purple?
// ... flashes with each tap, and then continues to show the tempo for 2
// seconds after the last tap").
//
// SPRINGS 3 (echo mode), the button taps the echo's tempo. On each tap all
// four LEDs flash purple (the meters never make purple: they go green ->
// amber -> red, and white is the throw-exit blink) for kTapFlashSeconds,
// over the meters. Once the taps have set a tempo the echo follows, the
// LEDs keep pulsing purple on each beat of it (one tap interval), counted
// from the last tap, for kTapShowSeconds; then the meters again. A new tap
// starts it over. No tempo (the first tap, or a lone tap that lets the tempo
// go, or the gate clock / the host's tempo setting it): just the flash.
// Tempo gone or SPRINGS off 3: dark at once. If the white blink is ever due
// at the same time, white wins (the hosts check it first).
//
// Pure logic, no libDaisy or JUCE: firmware/main.cpp's main loop and the
// plugin's panel timer both run it on a microsecond clock that may wrap
// (uint32: ~71.6 min); elapsed time is unsigned subtraction, so the wrap
// can't matter. host/tests/test_tap_led.cpp tests it.

#include "LedMeter.h"

#include <cstdint>

namespace rvled {

// ---- Tuning (the owner judges them on the module) -------------------------
constexpr float kTapFlashSeconds = 0.07f; // one flash / one pulse
constexpr float kTapShowSeconds  = 2.0f;  // pulses whose beat lands within this after the last tap
// Purple, as drive values (the PWM cubes them: 0.85 -> 61 % red duty with
// full blue, a violet-purple). Full brightness: the brightest channel is 1.
constexpr Rgb kTapPurple{0.85f, 0.0f, 1.0f};

constexpr uint32_t kTapFlashUs = uint32_t(kTapFlashSeconds * 1.0e6f);
constexpr uint32_t kTapShowUs  = uint32_t(kTapShowSeconds * 1.0e6f);

class TapFlash {
public:
    // nowUs: the host's microsecond clock. taps: Tank::taps() (it moves on
    // each tap). beatUs: Tank::tappedBeatSamples() in microseconds, 0 = no
    // tapped tempo. tapping: Tank::tapping() (SPRINGS 3, echo mode).
    // Returns true while the LEDs are purple. Call it often (the firmware
    // every ~1 ms, the panel at its frame rate).
    bool update(uint32_t nowUs, uint32_t taps, uint32_t beatUs, bool tapping)
    {
        if (taps != seen_) {
            seen_ = taps;
            if (tapping) {
                active_  = true;
                startUs_ = nowUs;
            }
        }
        if (!tapping) active_ = false;
        if (!active_) return false;
        const uint32_t t = nowUs - startUs_; // wrap-safe
        if (t < kTapFlashUs) return true;    // the tap's own flash (the tempo may still be settling)
        if (beatUs == 0 || t >= kTapShowUs + kTapFlashUs) {
            active_ = false; // no tempo (or it went), or the show is over
            return false;
        }
        const uint32_t beatStart = t - t % beatUs; // the latest beat, from the last tap
        return beatStart <= kTapShowUs && t - beatStart < kTapFlashUs;
    }

private:
    uint32_t seen_ = 0, startUs_ = 0;
    bool     active_ = false;
};

// What an LED shows: the throw-exit blink (white) wins, then the tap
// flash (purple), else its meter.
inline Rgb shown(bool white, bool purple, const Rgb& meter)
{
    return white ? Rgb{1.0f, 1.0f, 1.0f} : purple ? kTapPurple : meter;
}

} // namespace rvled
