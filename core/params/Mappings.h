#pragma once
// Parameter mappings: Normalised value (0–1) -> internal units.
// Pure functions shared by every Host (docs/m1-contracts.md). The DSP in
// core/dsp/ only ever sees the numbers these return.
//
// All times are in seconds and all frequencies in Hz, so a mapping means
// the same thing at 44.1, 48 and 96 kHz. Conversion to samples happens in
// the DSP (e.g. stretchK()).
//
// Numbers are M1 starting guesses (SPEC §4.4 "starting guesses, not from
// literature"); the ADR ranges they must respect are noted per mapping.

#include <cmath>

namespace rv::map {

constexpr float kPi = 3.14159265358979f;

// lo * (hi/lo)^v: equal knob turns give equal *ratios*, which is how we
// hear time and frequency (SPEC §4.4 "exponential curves on L and T60").
// Written as exp(v·log(hi/lo)): log of constants folds at compile time, and
// expf is much smaller than powf in the Firmware's 128 KB flash.
inline float expLerp(float lo, float hi, float v) { return lo * std::exp(v * std::log(hi / lo)); }

// ---- DECAY (ADR 0001, 0006, 0012; SPEC §4.4) ------------------------------

// Target tail length. 0.4 s tight slap (ADR 0006: 0.3–0.5 s) -> 9 s (ADR 0001:
// 8–10 s, always fades).
constexpr float kT60MinSeconds = 0.4f;
constexpr float kT60MaxSeconds = 9.0f;
inline float decayT60Seconds(float v) { return expLerp(kT60MinSeconds, kT60MaxSeconds, v); }

// Loop delay L: the plain delay line inside the Loop. Short tank ~30 ms ->
// long tank ~100 ms. The full round trip is L plus the allpass chain's group
// delay, which depends on frequency (that is what makes the Chirp); see
// Spring::roundTripSamples().
constexpr float kLoopDelayMinSeconds = 0.030f;
constexpr float kLoopDelayMaxSeconds = 0.100f;
inline float decayLoopDelaySeconds(float v) { return expLerp(kLoopDelayMinSeconds, kLoopDelayMaxSeconds, v); }

// Transition frequency fC: the Chirp lives below it. The stretched allpass
// with stretch K repeats its behaviour every fs/K Hz, so its first
// "descending chirp" band is 0 .. fs/(2K). We pick fC and derive K = fs/(2 fC),
// so the Chirp band is the same in Hz at every sample rate. Larger tank
// (DECAY CW) = lower fC = larger K (SPEC §4.4 "stretch K larger at CW").
constexpr float kTransitionMaxHz = 4200.0f; // DECAY 0: K ≈ 5.7 at 48 kHz
constexpr float kTransitionMinHz = 2700.0f; // DECAY 1: K ≈ 8.9 at 48 kHz
inline float decayTransitionHz(float v) { return expLerp(kTransitionMaxHz, kTransitionMinHz, v); }
inline float stretchK(float transitionHz, float sampleRate) { return sampleRate / (2.0f * transitionHz); }

// ---- BOING (ADR 0007) ------------------------------------------------------

// Allpass coefficient a of each stretched section H(z) = (a + z^-K)/(1 + a z^-K).
// Negative a delays low frequencies more than high ones, so highs arrive
// first: a descending Chirp. |a| larger = steeper, longer Chirp.
// Floor -0.45 keeps a clear Chirp at BOING 0 (ADR 0007: never "no dispersion").
constexpr float kBoingCoeffMin = -0.45f; // BOING 0: soft, washy, still a boing
constexpr float kBoingCoeffMax = -0.72f; // BOING 1: exaggerated Chirp
inline float boingCoefficient(float v) { return kBoingCoeffMin + (kBoingCoeffMax - kBoingCoeffMin) * v; }

// Number of active stretched sections M. Each section adds the same amount
// of dispersion, so M scales Chirp length. Floor of 24 (ADR 0007).
// M_max = 64 is sized for the Daisy budget (SPEC §5): ~12 cycles per section
// per sample -> ~800 cycles/sample per Spring at 48 kHz, ~2.4k for three
// Springs, about a quarter of the 10k cycles/sample budget. Decimating the
// low-chirp path x2 (Parker 2011) would halve that; see Spring.h.
constexpr int kMinStages = 24;
constexpr int kMaxStages = 64;
inline int boingStages(float v)
{
    return kMinStages + static_cast<int>(static_cast<float>(kMaxStages - kMinStages) * v + 0.5f);
}

// ---- TONE (ADR 0017) -------------------------------------------------------
// TONE = pre-tank Tilt (M5, params/DriveVoicing.h) + Loop damping + high
// path level (below).

// Loop damping low-pass cutoff. Floor 1.6 kHz keeps Chirps audible fully CCW
// (ADR 0017 "not through a wall"); noon ≈ 3.8 kHz (neutral, dub spring tails
// are dark); CW ceiling 9 kHz so the tail never turns fizzy.
constexpr float kDampingMinHz = 1600.0f;
constexpr float kDampingMaxHz = 9000.0f;
inline float toneDampingHz(float v) { return expLerp(kDampingMinHz, kDampingMaxHz, v); }

// Level of the high path (faster wideband echoes) mixed into the Spring out.
// 0.10 CCW, 0.225 noon (as M1), 0.36 CW. M5: the CW end came down from M1's
// 0.60 because the Tilt now brightens CW too; together they keep the energy
// above 10 kHz within +6 dB of noon (ADR 0017 "never harsh", test_drive).
inline float toneHighPathLevel(float v) { return 0.10f + 0.24f * v + 0.02f * v * v; }

// ---- MIX -------------------------------------------------------------------

// Equal-power dry/wet: dry² + wet² = 1 at every setting, -3 dB each at noon.
// sqrt law (not sin/cos) so both ends are exact: MIX 0 is bit-identical dry,
// MIX 1 is wet only.
struct MixGains {
    float dry;
    float wet;
};
inline MixGains mixGains(float v) { return {std::sqrt(1.0f - v), std::sqrt(v)}; }

// ---- Loop analysis ---------------------------------------------------------

// Group delay (samples) of one stretched allpass section at freqHz.
// First-order allpass A(z) = (a + z^-1)/(1 + a z^-1) has group delay
// (1 - a²)/(1 + a² + 2a cos w). Stretching z^-1 -> z^-K multiplies it by K and
// evaluates it at w*K. At DC this is K(1-a)/(1+a); at fC it is K(1+a)/(1-a).
inline float stretchedAllpassGroupDelaySamples(float a, float K, float freqHz, float sampleRate)
{
    const float theta = 2.0f * kPi * freqHz * K / sampleRate;
    return K * (1.0f - a * a) / (1.0f + a * a + 2.0f * a * std::cos(theta));
}

} // namespace rv::map
