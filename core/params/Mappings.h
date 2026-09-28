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
// TENSION PROTOTYPE: these now bound TENSION's L range (see "TENSION" below);
// kLoopDelayMaxSeconds also sizes the delay memory (Spring.cpp), +10 %.
constexpr float kLoopDelayMinSeconds = 0.033f;
constexpr float kLoopDelayMaxSeconds = 0.110f;
inline float decayLoopDelaySeconds(float v) { return expLerp(kLoopDelayMinSeconds, kLoopDelayMaxSeconds, v); }

// Transition frequency fC: the Chirp lives below it. The stretched allpass
// with stretch K repeats its behaviour every fs/K Hz, so its first
// "descending chirp" band is 0 .. fs/(2K). We pick fC and derive K = fs/(2 fC),
// so the Chirp band is the same in Hz at every sample rate. Larger tank
// (DECAY CW) = lower fC = larger K (SPEC §4.4 "stretch K larger at CW").
// TENSION PROTOTYPE: TENSION's fC range (tight 4.6 kHz, loose 2.7 kHz); the
// minimum also sizes the allpass rings (Spring.cpp).
constexpr float kTransitionMaxHz = 4600.0f; // TENSION 0: K ≈ 5.2 at 48 kHz
constexpr float kTransitionMinHz = 2700.0f; // TENSION 1: K ≈ 8.9 at 48 kHz
inline float decayTransitionHz(float v) { return expLerp(kTransitionMaxHz, kTransitionMinHz, v); }
inline float stretchK(float transitionHz, float sampleRate) { return sampleRate / (2.0f * transitionHz); }

// ---- BOING (ADR 0007) ------------------------------------------------------

// Chirp direction: the one switch (docs/TASKS.md task 7, owner decides by ear).
// Allpass coefficient a of each stretched section H(z) = (a + z^-K)/(1 + a z^-K).
//   LowsLater  (a < 0): lows are delayed more than highs, so highs arrive
//              first: a descending "peeew". The M1-M7 sound (SPEC §2.1).
//   HighsLater (a > 0): the delay grows toward fC, so highs arrive later:
//              a rising Chirp, as every real tank in the IR library does
//              (docs/ir-dispersion-study.md; DAFx-11 fits a = +0.62).
// Both keep the stage range (M 24..64). What follows the switch is constants
// only, no code: a's sign and range here, the Jolt's sign (kChirpSign), a -1 dB
// wet trim (SpringModes.h kModeTrim) and a faster limiter attack (Tank.h),
// both for HighsLater's denser, unsmeared lows. |a| larger = steeper, longer
// Chirp. The floor keeps a clear Chirp at BOING 0 (ADR 0007: never "no
// dispersion"). The Loop gain design already checks T60 up to fC
// (Spring.cpp kDesignFcRatios), where a > 0 puts the longest round trip.
//
// HighsLater tuning (M8, docs/ir-dispersion-study.md "Tuned HighsLater"):
// highs-later per trip, ridge method, Spring A at DECAY 0 / 0.5 / 1:
// BOING 0 ≈ 5 / 7 / 8 ms, noon ≈ 12 / 16 / 19 ms, BOING 1 ≈ 23 / 30 / 37 ms,
// growing steadily with BOING (real tanks: 2 · 15 · 59 ms, most 6-35).
// a stays <= 0.55: above that the Chirp piles up in a narrow band just under
// fC instead of growing (at DECAY 0 it even shrinks), so the extra size comes
// from the stages. Real tanks use a ≈ 0.2-0.4 with 45-300 stages.
enum class ChirpDirection { LowsLater, HighsLater };
constexpr ChirpDirection kChirpDirection = ChirpDirection::HighsLater;
constexpr bool           kHighsLater     = kChirpDirection == ChirpDirection::HighsLater;
// Sign of a (and of the Splash Jolt's Δa, which pushes |a| up: more smear).
constexpr float kChirpSign = kHighsLater ? 1.0f : -1.0f;

constexpr float kBoingCoeffMin = kHighsLater ? 0.40f : -0.45f; // BOING 0: soft, washy, still a boing
constexpr float kBoingCoeffMax = kHighsLater ? 0.55f : -0.72f; // BOING 1: exaggerated Chirp
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

// ---- TENSION (PROTOTYPE, docs/tension-prototype.md) ------------------------
// TENSION replaces BOING (same slot, ParamSpec key "boing"): "which tank is
// fitted". Tight (0) = short tank, small Chirp, quick repeats, bright;
// loose (1) = long tank, big Chirp, slow repeats, darker. L, fC (-> K), a and
// the stage count M all move together, so every position is one plausible
// tank. DECAY no longer touches any of them: it only sets T60.
//
// Three anchors, each a region of the IR library (docs/ir-dispersion-study.md),
// Spring A, measured with tools/ir_dispersion.py (see the prototype doc):
//   tight  0   : L 33 ms, fC 4.6 kHz, a 0.40, M 24  -> short tanks
//                (Space Echo 42 ms); keeps BOING 0's "always a spring" floor
//   noon   0.5 : L 69 ms, fC 3.3 kHz, a 0.47, M 40  -> the median tank
//                (69 ms repeat, ~15 ms Chirp: Amazing Stereo, Amp Spring High)
//   loose  1   : L 110 ms, fC 2.7 kHz, a 0.55, M 64 -> the long tanks
//                (Swissecho 116 ms, big-Chirp Farfi / SNRA500 30-35 ms)
// Between anchors: log-linear for L and fC (equal turns = equal ratios),
// linear for a and M. The anchor at noon is not the geometric middle of the
// ends (60 ms), so each half has its own curve; the kink at noon is a change
// of slope only (no step).
constexpr float kTensionMidLoopDelaySeconds = 0.069f;
constexpr float kTensionMidTransitionHz     = 3300.0f;
constexpr float kTensionCoeffMin = 0.40f, kTensionCoeffMid = 0.47f, kTensionCoeffMax = 0.55f;
constexpr float kTensionStageFracMid = 0.40f; // of (cap - floor): 24 + 0.4*40 = 40 at cap 64

inline float anchorExp(float lo, float mid, float hi, float v)
{
    return v < 0.5f ? expLerp(lo, mid, 2.0f * v) : expLerp(mid, hi, 2.0f * v - 1.0f);
}
inline float anchorLin(float lo, float mid, float hi, float v)
{
    return v < 0.5f ? lo + (mid - lo) * 2.0f * v : mid + (hi - mid) * (2.0f * v - 1.0f);
}
inline float tensionLoopDelaySeconds(float v)
{
    return anchorExp(kLoopDelayMinSeconds, kTensionMidLoopDelaySeconds, kLoopDelayMaxSeconds, v);
}
inline float tensionTransitionHz(float v) { return anchorExp(kTransitionMaxHz, kTensionMidTransitionHz, kTransitionMinHz, v); }
inline float tensionCoefficient(float v)
{
    return kChirpSign * anchorLin(kTensionCoeffMin, kTensionCoeffMid, kTensionCoeffMax, v);
}
// 0..1 share of the stage range (floor .. mode cap), see modes::tensionStages.
inline float tensionStageFraction(float v) { return anchorLin(0.0f, kTensionStageFracMid, 1.0f, v); }

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
