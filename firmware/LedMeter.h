#pragma once
// LED level meters for the release firmware (ADR 0031, SPEC §3 LEDs).
//
// The owner's Noise Engineering habit: the left LED pair shows the input
// (In L, In R), the right pair the output (Out L, Out R). Each LED is one
// LevelMeter:
//   - Brightness follows level on a dB scale, so quiet signals still glow:
//     kFloorDb (-48 dBFS) and below is off, 0 dBFS is full.
//   - Colour warms from green to amber as the level gets hot (kWarmFromDb ->
//     kAmberDb). Level alone never makes red.
//   - Red is a warning, held kRedHoldSeconds after the last trigger:
//     input LEDs when the input is near the ADC's full scale (the jack's
//     analog clip point), output LEDs when the Tank's output safety limiter
//     is pulling the wet down (e.g. a loud Howl).
//   - Ballistics: rises at once to a new peak, falls back with a
//     kFallSeconds time constant (about 1 s from full to dark).
//
// Pure maths, no libDaisy: the firmware's audio callback only takes a
// per-block abs peak per channel (and the limiter gain); everything here
// runs in the main loop. host/tests/test_led_meter.cpp tests it on desktop.
//
// Colour values are "drive" values, not light output: the PWM cubes each
// channel (pwmCount() below, the same curve libDaisy's hid/led.cpp uses), so
// drive tracks perceived brightness roughly linearly (lightness ~ cube root
// of light). kMinGlow 0.3 cubed is 2.7 % duty, 14 of the 512 PWM steps.
//
// The PWM itself (30 Sep 2026 fix, "LEDs flicker rather than dim"): the
// release firmware no longer uses libDaisy's software PWM, which assumes
// Update() runs samplerate times a second but got ~1 kHz from the main loop,
// i.e. ~8 uneven steps per 120 Hz period; dim values became sparse 1 ms
// flashes. It can't simply run faster either: the audio callback holds the
// CPU for ~60 % of every 1 ms block at top interrupt priority, freezing any
// CPU-driven PWM. So main.cpp has a timer clock precomputed pin patterns
// (fillPwmWords() below) out to the GPIO pins by DMA, which the CPU load
// can't disturb: kPwmSteps steps per period, ~1 kHz.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rvled {

struct Rgb {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

// ---- Tuning (starting values; the owner judges them on the module) --------
constexpr float kFloorDb        = -48.0f; // at or below: LED off
constexpr float kWarmFromDb     = -18.0f; // pure green up to here
constexpr float kAmberDb        = -6.0f;  // fully amber from here up
constexpr float kMinGlow        = 0.3f;   // drive at the floor: 2.7 % duty (see header note)
constexpr float kOffPosition    = 0.01f;  // meter position treated as dark (half a dB above the floor)
constexpr float kFallSeconds    = 0.3f;   // fall time constant
constexpr float kRedHoldSeconds = 0.5f;   // red stays this long after the last trigger
constexpr float kInputRedDb     = -1.0f;  // input peak at/above this (dBFS): near clip
constexpr float kLimiterRedDb   = 0.5f;   // limiter gain reduction at/above this (dB): red

// Amber is (1, kAmberGreen, 0) before the PWM's cube: 0.7 -> duty 1 : 0.34,
// a warm amber. (1, 0.5, 0) would cube to 1 : 0.125, which reads as
// red-orange. The warm-up goes green -> yellow (red rises) -> amber (green
// eases off), so the brightest channel always equals the level's drive.
constexpr float kAmberGreen = 0.7f;
constexpr Rgb   kRed{1.0f, 0.0f, 0.0f};

inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }

// Linear abs peak (1 = digital full scale) -> meter position 0..1 on the dB
// scale: 0 at kFloorDb and below, 1 at 0 dBFS and above.
inline float levelToPosition(float peak)
{
    if (!(peak > 0.0f)) return 0.0f;
    const float db = 20.0f * std::log10(peak);
    return std::clamp((db - kFloorDb) / -kFloorDb, 0.0f, 1.0f);
}

// Red triggers.
inline bool inputNearClip(float peak) { return peak >= dbToGain(kInputRedDb); }
inline bool limiterReducing(float limiterGain) { return limiterGain <= dbToGain(-kLimiterRedDb); }

// Colour for a meter position (no red: that is LevelMeter's hold).
inline Rgb positionColour(float pos)
{
    if (pos <= kOffPosition) return {};
    const float drive  = kMinGlow + (1.0f - kMinGlow) * std::min(pos, 1.0f);
    const float db     = kFloorDb + pos * -kFloorDb;
    const float warmth = std::clamp((db - kWarmFromDb) / (kAmberDb - kWarmFromDb), 0.0f, 1.0f);
    const float red    = std::min(1.0f, 2.0f * warmth);
    const float green  = 1.0f - std::max(0.0f, 2.0f * warmth - 1.0f) * (1.0f - kAmberGreen);
    return {drive * red, drive * green, 0.0f};
}

class LevelMeter {
public:
    // peak: largest abs sample since the last update (linear). red: a red
    // trigger happened since the last update. dt: seconds since the last update.
    void update(float peak, bool red, float dt)
    {
        const float target = levelToPosition(peak);
        if (target >= pos_) pos_ = target; // fast rise
        else pos_ = target + (pos_ - target) * std::exp(-dt / kFallSeconds);
        if (red) redLeft_ = kRedHoldSeconds;
        else redLeft_ = std::max(0.0f, redLeft_ - dt);
    }

    float position() const { return pos_; }
    bool  isRed() const { return redLeft_ > 0.0f; }
    Rgb   colour() const { return isRed() ? kRed : positionColour(pos_); }

private:
    float pos_     = 0.0f;
    float redLeft_ = 0.0f;
};

// ---- PWM tables (release firmware, main.cpp streams them to the pins) -----
// One PWM period is kPwmSteps steps. For each GPIO port the firmware keeps a
// table of kPwmSteps 32-bit words, one per step, written by DMA to the port's
// BSRR register (low half sets pins, high half resets them). Every word
// drives every LED pin on that port, on or off, so the table alone defines
// the pins' state; a pin is on for the first `count` steps of each period.
constexpr int kPwmSteps = 512;

// Drive 0..1 -> on-steps per period: cubed (as libDaisy did), rounded.
inline int pwmCount(float drive)
{
    const float d = std::clamp(drive, 0.0f, 1.0f);
    return int(d * d * d * float(kPwmSteps) + 0.5f);
}

// BSRR bits that turn one pin on / off. The Versio's LEDs are wired active
// low (libDaisy inits them inverted): on = drive the pin low (reset bit).
struct PwmPin {
    uint32_t onBits = 0, offBits = 0;
};
inline PwmPin pwmPin(int pin, bool activeLow)
{
    const uint32_t set = 1u << pin, reset = 1u << (pin + 16);
    return activeLow ? PwmPin{reset, set} : PwmPin{set, reset};
}

// Fill one port's table: words[s] turns pins[p] on while s < counts[p], off
// after. Stores each word once, in runs between thresholds (the table sits in
// uncached RAM that the DMA reads; no read-modify-write).
inline void fillPwmWords(uint32_t* words, int steps, const PwmPin* pins, const int* counts, int numPins)
{
    int s = 0;
    while (s < steps) {
        uint32_t w    = 0;
        int      next = steps;
        for (int p = 0; p < numPins; ++p) {
            const bool on = s < counts[p];
            w |= on ? pins[p].onBits : pins[p].offBits;
            if (on && counts[p] < next) next = counts[p];
        }
        for (; s < next; ++s) words[s] = w;
    }
}

} // namespace rvled
