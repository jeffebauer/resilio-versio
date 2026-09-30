#pragma once
// WOBBLE voicing, bipolar (SPEC §3 P6, §4.7; ADR 0034, which supersedes ADR
// 0008's one-way zones; ADR 0015 Gliding tier; CONTEXT.md: Drift, Warble,
// wow, flutter, Micro-mod floor).
//
// PROTOTYPE (branch proto/bipolar-wobble). Every WOBBLE number lives here,
// so tuning by ear edits constants, not DSP code (the old one-way numbers
// in SplashVoicing.h's WOBBLE section are no longer used).
//
// The knob (0..1, CV added to it in hardware) is bipolar, like other Versio
// firmwares:
//
//   fully left ◄──── Drift: smooth random wow + flutter ────┐
//   noon           still (only the Micro-mod floor)          ├ dead zone ±kDeadZone
//   fully right ───► Warble: a sine LFO, getting stronger ───┘
//
// Each side has an "amount" a = 0 at the dead zone's edge → 1 at the end
// stop. Only one side is ever active, so the knob (smoothed, Gliding tier)
// crossing noon fades one side out to exactly 0 before the other fades in.
//
// Per generator (dsp::Wobble), in samples of Loop delay (or pickup offset
// for the Transport):
//
//   m(t) = Dw·wow(t) + Df·flutter(t)       left  (random, never repeats)
//        = Ds·sin(2π ∫f(t) dt + φ)         right (sine; f wanders by ±kLfoRateWander)
//
// wow and flutter are random lines: seeded random points, Catmull-Rom
// interpolated (smooth value and slope, no stops at the points), each
// segment with its own random length, so the rate itself wanders.
//
// Depth is specified in peak cents per pass (as before, SplashVoicing.h
// "Pitch maths"): a delay read with a moving delay m plays at 1 − dm/dn, so
// peak cents c needs a peak slope of (2^(c/1200) − 1)·fs samples/s. For a
// sine that slope is 2π f D; for a random line it is ~kRandSlope·f·D
// (measured over 120 s, test_wobble). The tail multiplies the per-pass
// shift (a "tail factor", SplashVoicing.h), slow movement most; the heard
// numbers per knob step are in docs/m8-tuning-backlog.md "Bipolar WOBBLE".

#include "params/Mappings.h"

#include <array>
#include <cmath>

namespace rv::wobble {

// ---- Knob → side amounts ---------------------------------------------------------
// A hardware pot's physical noon doesn't read exactly 0.5 (and CV adds
// offset noise), so ±3 % of travel around noon is still.
constexpr float kNoon     = 0.5f;
constexpr float kDeadZone = 0.03f;
constexpr float kSideSpan = kNoon - kDeadZone; // 0.47 of travel per side

// Left side amount (random wow + flutter): 0 inside the dead zone, 1 fully left.
inline float randomAmount(float knob)
{
    const float a = (kNoon - kDeadZone - knob) / kSideSpan;
    return a <= 0.0f ? 0.0f : (a >= 1.0f ? 1.0f : a);
}
// Right side amount (sine LFO): 0 inside the dead zone, 1 fully right.
inline float lfoAmount(float knob)
{
    const float a = (knob - kNoon - kDeadZone) / kSideSpan;
    return a <= 0.0f ? 0.0f : (a >= 1.0f ? 1.0f : a);
}

// ---- Depth shape -----------------------------------------------------------------
// s(a) = (e^{k a} − 1)/(e^k − 1): 0 at the dead-zone edge (so nothing jumps
// there), 1 at the end stop. Set so each 0.1 of knob travel (~5 steps per
// side) grows the heard depth by ~1.5–3x and the first step off noon is
// already a few cents on a held tone (the old k = 5.89 spread the whole
// lower half over 0–0.75 cents per pass: "9 o'clock ≈ noon" by ear). The
// random side is a little softer-curved: its p95 pitch sits further under
// its peak than a sine's, so it needs more depth near noon to be heard.
constexpr float kCurveLfo    = 1.6f;
constexpr float kCurveRandom = 1.0f;
inline float shape(float a, float k)
{
    if (a <= 0.0f) return 0.0f;
    return (std::exp(k * a) - 1.0f) / (std::exp(k) - 1.0f);
}

// ---- Right side: the sine LFO (Warble) -------------------------------------------
// Fully right = the old top end (ADR 0008 "clearly out of tune", kept by
// the owner): 12 cents per pass in the Loop, 36 on the first echo, 1.4 Hz.
// Rate rises gently with strength: a slow sway just right of noon, the
// worn-capstan wow at the end stop.
constexpr float kLfoLoopCents  = 10.0f;
constexpr float kLfoEarlyCents = 36.0f;
constexpr float kLfoRateMinHz  = 0.6f;
constexpr float kLfoRateMaxHz  = 1.4f;
inline float lfoRateHz(float a) { return map::expLerp(kLfoRateMinHz, kLfoRateMaxHz, a); }
// The LFO's rate drifts a little (a slow random line, ±this fraction), so it
// never sounds mechanical. 0 = a pure sine at a fixed rate.
constexpr float kLfoRateWander   = 0.06f;
constexpr float kLfoWanderRateHz = 0.15f; // how fast the rate itself drifts

// ---- Left side: random wow + flutter (Drift) --------------------------------------
// Wow: slow random line, mean rate rising with the amount; each segment
// runs at the mean × a random factor in [kWowSpreadLo, kWowSpreadHi], so
// overall ~0.2–1.5 Hz and never periodic.
constexpr float kWowRateMinHz = 0.35f; // mean rate just left of noon
constexpr float kWowRateMaxHz = 0.9f;  // mean rate fully left
constexpr float kWowSpreadLo  = 0.6f;
constexpr float kWowSpreadHi  = 1.6f;
inline float wowRateHz(float a) { return map::expLerp(kWowRateMinHz, kWowRateMaxHz, a); }
// Flutter: a smaller, faster random line, ~5–12 Hz.
constexpr float kFlutterRateHz  = 7.5f;
constexpr float kFlutterSpreadLo = 0.7f;
constexpr float kFlutterSpreadHi = 1.55f;
// Peak cents per pass (Loop) and on the first echo (Transport) at the end
// stop, per layer. Fully left is set to match fully right by ear-proxy
// (held-tone pitch, test_m7_tank): "roughly as wild as the old top".
constexpr float kWowLoopCents      = 10.0f;
constexpr float kFlutterLoopCents  = 7.0f;
constexpr float kWowEarlyCents     = 28.0f;
constexpr float kFlutterEarlyCents = 18.0f;
// Typical peak slope of a unit random line, per Hz of segment rate
// (Catmull-Rom through uniform points in [-1, 1]).
constexpr float kRandSlope = 2.0f;
// Catmull-Rom can overshoot its points by up to 1/8 of a step (|value| <=
// 1.25 for points in [-1, 1]): used for sizing delay memory.
constexpr float kRandOvershoot = 1.25f;

// ---- Shared Drift at low amounts (proto/wobble-hang "B", owner's pick) -----------
// Independent drift re-rolls the offset between two Springs' nearly
// coincident modes at every chord ending, and sometimes one chord note
// hangs on. So at low amounts Springs B and C take Spring A's movement
// (scaled by their Loop length ratio: every mode moves by the same ratio,
// chords fade evenly), blending to their own over kShareFrom..kShareTo of
// the side's amount (smoothstep). Same on both sides.
constexpr float kShareFrom = 0.10f;
constexpr float kShareTo   = 0.45f;
inline float independence(float a)
{
    float t = (a - kShareFrom) / (kShareTo - kShareFrom);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// ---- Per-generator rate ratios ---------------------------------------------------
// Unrelated ratios so nothing locks (Springs A/B/C; the Transport).
inline constexpr std::array<float, 3> kSpringRate{{0.87f, 1.0f, 1.13f}};
constexpr float kTransportRate = 0.94f;

// ---- Cents → samples -------------------------------------------------------------
inline float slopeForCents(float c, float sampleRate)
{
    return c <= 0.0f ? 0.0f : (std::exp(c * (0.693147181f / 1200.0f)) - 1.0f) * sampleRate;
}
inline float sineDepthSamples(float c, float hz, float sampleRate)
{
    return slopeForCents(c, sampleRate) / (2.0f * map::kPi * hz);
}
inline float randomDepthSamples(float c, float hz, float sampleRate)
{
    return slopeForCents(c, sampleRate) / (kRandSlope * hz);
}

// Everything one generator needs for a knob position.
struct Depths {
    float lfo = 0.0f, wow = 0.0f, flutter = 0.0f;    // samples
    float lfoHz = 0.0f, wowHz = 0.0f, flutterHz = 0.0f;
    float independence = 1.0f;
};
inline Depths depths(float knob, bool early, float rateScale, float sampleRate)
{
    Depths d;
    const float aL = randomAmount(knob), aR = lfoAmount(knob);
    d.lfoHz     = lfoRateHz(aR) * rateScale;
    d.wowHz     = wowRateHz(aL) * rateScale;
    d.flutterHz = kFlutterRateHz * rateScale;
    const float sL = shape(aL, kCurveRandom), sR = shape(aR, kCurveLfo);
    d.lfo     = sineDepthSamples(sR * (early ? kLfoEarlyCents : kLfoLoopCents), d.lfoHz, sampleRate);
    d.wow     = randomDepthSamples(sL * (early ? kWowEarlyCents : kWowLoopCents), d.wowHz, sampleRate);
    d.flutter = randomDepthSamples(sL * (early ? kFlutterEarlyCents : kFlutterLoopCents), d.flutterHz, sampleRate);
    d.independence = independence(aL > aR ? aL : aR);
    return d;
}

// Upper bound of |m| in the Loop (samples) over the whole knob, for sizing
// delay memory. The slowest Spring (kSpringRate[0]) needs the most, and a
// sharing Spring follows Spring A scaled by its Loop ratio (<= 1.13).
inline float maxDepthSamples(float sampleRate)
{
    float m = 0.0f;
    for (int i = 0; i <= 40; ++i) {
        const Depths d = depths(0.025f * float(i), false, kSpringRate[0], sampleRate);
        m = std::fmax(m, std::fmax(d.lfo, kRandOvershoot * (d.wow + d.flutter)));
    }
    return 1.15f * m;
}

} // namespace rv::wobble
