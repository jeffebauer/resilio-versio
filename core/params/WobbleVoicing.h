#pragma once
// WOBBLE voicing, bipolar (SPEC §3 P6, §4.7; ADR 0034, which supersedes ADR
// 0008's one-way zones; ADR 0015 Gliding tier; CONTEXT.md: Drift, Warble,
// wow, flutter, Micro-mod floor).
//
// PROTOTYPE (branch proto/bipolar-wobble-2, round 2). Every WOBBLE number lives here,
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
// side) grows the heard depth by a clear step and the first step off noon is
// already a few cents on a held tone (the old k = 5.89 spread the whole
// lower half over 0–0.75 cents per pass: "9 o'clock ≈ noon" by ear).
// Its middle, s(0.5) = 1/(e^{k/2} + 1), fixes k: to lower the end stop by a
// factor T while the middle only drops by a factor M, the new middle share
// is r = M·s_old(0.5)/T and k = 2·ln(1/r − 1) (the Voicing table below).
inline float shape(float a, float k)
{
    if (a <= 0.0f) return 0.0f;
    if (k > -1e-3f && k < 1e-3f) return a > 1.0f ? 1.0f : a; // k = 0: a straight line
    return (std::exp(k * a) - 1.0f) / (std::exp(k) - 1.0f);
}

// ---- Voicings: round 1 (A) and two strengths of toning down (B, C) ---------------
// Owner, 1 Oct 2026, after round 1's page: "slightly tone down the amount of
// modulation at the top end of each side … on the smooth side, we could make
// it feel more like a vibrato or flutter, and the left remains for wow and
// flutter." So B and C (not A):
//  - right side is a vibrato: 1.5 Hz just right of noon → 5.5 Hz fully right
//    (A: a slow sway, 0.6 → 1.4 Hz);
//  - its rate rises mostly in the first half of the side (lfoRateEase): a
//    faster vibrato builds up less in the tail than a slow sway, so with an
//    even rise the top steps (rate up, depth up) cancelled by ear-proxy
//    (held tone at DECAY noon, 0.9 → 1.0: 24.9 → 22.3 cents); now the top
//    steps are mostly depth;
//  - both end stops lower: B by a quarter (×0.75), C by nearly half (×0.55),
//    first echo scaled the same; the middle of each side (9 and 3 o'clock)
//    stays about where it is per pass (the curve is re-bent straighter:
//    B left ×0.94 / right ×1.03, C left ×0.80 / right ×0.89 of A's);
//  - flutter tremolo: a small volume wobble on the whole wet sound that
//    follows the Transport's flutter line, left side only (≤ 1 dB peak at
//    the end stop): flutter sounds like a tape transport, not only a pitch
//    effect (Wear & Tear manual's idea);
//  - the flutter's speed follows the wow (±20 % on the wow line), so it
//    never settles on one rate.
// A is round 1 exactly (the page's reference). The firmware and plugin use
// kDefaultVoicing; the Renderer can pick another (Tank::setWobbleVoicing,
// sweep key "wobble_voicing": 0 = A, 1 = B, 2 = C, 3 = D). Hidden: no panel control.
struct Voicing {
    // Right side (Warble): peak cents per pass at the end stop (Loop / first
    // echo), rate just right of noon → fully right, depth curve k.
    float lfoLoopCents, lfoEarlyCents, lfoRateMinHz, lfoRateMaxHz, curveLfo;
    // How the rate rises across the side: 0 = evenly in ratio (A: exp), 1 =
    // mostly in the first half (1 − (1 − a)²), so the top steps are depth.
    float lfoRateEase;
    // Left side (Drift): peak cents per pass at the end stop per layer (Loop
    // wow / flutter, first echo wow / flutter), depth curve k.
    float wowLoopCents, flutterLoopCents, wowEarlyCents, flutterEarlyCents, curveRandom;
    // Flutter tremolo: dB per unit of the flutter line at the end stop (the
    // line peaks at ~1, rarely 1.25: kRandOvershoot), on the same curve as
    // the flutter's depth. 0 = none.
    float tremoloDb;
    // Flutter speed follows the wow: flutter rate × (1 + this × wow line).
    float flutterFollow;
};
// k for a voicing whose end stop is T × A's and middle M × A's (see "Depth
// shape"), then trimmed by the held-tone check (test_m7_tank: every step
// heard, first step off noon >= 1.5 cents):
//   left  (A: k 1.0, s(0.5) 0.3775):  B T 0.75, k 0.2 (M 0.94);  C T 0.55, k −0.394 (M 0.80)
//   right (A: k 1.6, s(0.5) 0.3100):  B T 0.75, k 0.6 (M 1.03);  C T 0.55, k 0 (M 0.89)
//   D (owner's pick): T 0.65, B's middles: left k −0.368, right k 0.07
inline constexpr std::array<Voicing, 4> kVoicings{{
    // A: round 1 (proto/bipolar-wobble)
    {10.0f, 36.0f, 0.6f, 1.4f, 1.6f, 0.0f, /**/ 10.0f, 7.0f, 28.0f, 18.0f, 1.0f, /**/ 0.0f, 0.0f},
    // B: gentle (end stops ×0.75)
    {7.5f, 27.0f, 1.5f, 5.5f, 0.6f, 1.0f, /**/ 7.5f, 5.25f, 21.0f, 13.5f, 0.2f, /**/ 0.8f, 0.2f},
    // C: more (end stops ×0.55)
    {5.5f, 19.8f, 1.5f, 5.5f, 0.0f, 1.0f, /**/ 5.5f, 3.85f, 15.4f, 9.9f, -0.394f, /**/ 0.8f, 0.2f},
    // D: the owner's pick (1 Oct 2026, round 2 page): B everywhere, but C at
    // both end stops on held tones ("slightly more resonance buildup with
    // B"), while skank kept B there. So B's middles with end stops halfway
    // between B and C (×0.65 of A).
    {6.5f, 23.4f, 1.5f, 5.5f, 0.07f, 1.0f, /**/ 6.5f, 4.55f, 18.2f, 11.7f, -0.368f, /**/ 0.8f, 0.2f},
}};
constexpr int kVoicingA = 0, kVoicingB = 1, kVoicingC = 2, kVoicingD = 3;
constexpr int kDefaultVoicing = kVoicingD;
inline const Voicing& voicing(int v) { return kVoicings[size_t(v < 0 ? 0 : (v > 3 ? 3 : v))]; }

// ---- Right side: the sine LFO (Warble) -------------------------------------------
// Rate rises with strength (Voicing lfoRateMinHz → lfoRateMaxHz).
inline float lfoRateHz(const Voicing& v, float a)
{
    if (v.lfoRateEase <= 0.0f) return map::expLerp(v.lfoRateMinHz, v.lfoRateMaxHz, a);
    const float e = 1.0f - (1.0f - a) * (1.0f - a);
    return v.lfoRateMinHz + (v.lfoRateMaxHz - v.lfoRateMinHz) * e;
}
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
// Flutter: a smaller, faster random line, ~5–12 Hz (B, C: ±20 % more, on the wow).
constexpr float kFlutterRateHz  = 7.5f;
constexpr float kFlutterSpreadLo = 0.7f;
constexpr float kFlutterSpreadHi = 1.55f;
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
    float tremoloDb = 0.0f;     // dB per unit flutter line (Transport only; 0 elsewhere)
    float flutterFollow = 0.0f; // flutter rate × (1 + this × wow line)
};
inline Depths depths(float knob, bool early, float rateScale, float sampleRate, int voicingIndex = kDefaultVoicing)
{
    const Voicing& v = voicing(voicingIndex);
    Depths d;
    const float aL = randomAmount(knob), aR = lfoAmount(knob);
    d.lfoHz     = lfoRateHz(v, aR) * rateScale;
    d.wowHz     = wowRateHz(aL) * rateScale;
    d.flutterHz = kFlutterRateHz * rateScale;
    const float sL = shape(aL, v.curveRandom), sR = shape(aR, v.curveLfo);
    d.lfo     = sineDepthSamples(sR * (early ? v.lfoEarlyCents : v.lfoLoopCents), d.lfoHz, sampleRate);
    d.wow     = randomDepthSamples(sL * (early ? v.wowEarlyCents : v.wowLoopCents), d.wowHz, sampleRate);
    d.flutter = randomDepthSamples(sL * (early ? v.flutterEarlyCents : v.flutterLoopCents), d.flutterHz, sampleRate);
    d.independence  = independence(aL > aR ? aL : aR);
    d.tremoloDb     = early ? sL * v.tremoloDb : 0.0f; // one tremolo per Tank: the Transport's
    d.flutterFollow = aL > 0.0f ? v.flutterFollow : 0.0f;
    return d;
}

// Upper bound of |m| in the Loop (samples) over the whole knob and every
// voicing, for sizing delay memory. The slowest Spring (kSpringRate[0])
// needs the most, and a sharing Spring follows Spring A scaled by its Loop
// ratio (<= 1.13). (The flutter's rate follow changes its speed, not its
// depth in samples.)
inline float maxDepthSamples(float sampleRate)
{
    float m = 0.0f;
    for (int v = 0; v < int(kVoicings.size()); ++v)
        for (int i = 0; i <= 40; ++i) {
            const Depths d = depths(0.025f * float(i), false, kSpringRate[0], sampleRate, v);
            m = std::fmax(m, std::fmax(d.lfo, kRandOvershoot * (d.wow + d.flutter)));
        }
    return 1.15f * m;
}

} // namespace rv::wobble
