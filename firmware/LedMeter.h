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
// Values returned are what DaisyVersio::SetLed() takes. libDaisy cubes each
// channel before its software PWM (hid/led.cpp), so these are "drive"
// values, not light output: 0.35 cubed is ~4 % duty, about the dimmest step
// the 1 kHz PWM shows, hence kMinGlow.

#include <algorithm>
#include <cmath>

namespace rvled {

struct Rgb {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

// ---- Tuning (starting values; the owner judges them on the module) --------
constexpr float kFloorDb        = -48.0f; // at or below: LED off
constexpr float kWarmFromDb     = -18.0f; // pure green up to here
constexpr float kAmberDb        = -6.0f;  // fully amber from here up
constexpr float kMinGlow        = 0.35f;  // drive at the floor (see header note)
constexpr float kOffPosition    = 0.01f;  // meter position treated as dark (half a dB above the floor)
constexpr float kFallSeconds    = 0.3f;   // fall time constant
constexpr float kRedHoldSeconds = 0.5f;   // red stays this long after the last trigger
constexpr float kInputRedDb     = -1.0f;  // input peak at/above this (dBFS): near clip
constexpr float kLimiterRedDb   = 0.5f;   // limiter gain reduction at/above this (dB): red

// Amber is (1, kAmberGreen, 0) before libDaisy's cube: 0.7 -> duty 1 : 0.34,
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

} // namespace rvled
