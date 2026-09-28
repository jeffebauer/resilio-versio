#pragma once
// SPRINGS switch voicing (SPEC §4.3, §5; docs/m4-contracts.md Stream D).
// One place for everything that differs between 1, 2 and 3 Springs:
//   - per-Spring detuning (fixed, never random),
//   - per-mode allpass stage caps (the CPU knob M3 profiling retunes),
//   - the stereo output matrix per mode (placement, cross-feed, level match).
// Pure constants + tiny helpers, no platform code.

#include "params/Mappings.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace rv::modes {

constexpr int kNumSprings = 3; // A, B, C
constexpr int kNumModes   = 3; // SPRINGS left / centre / right = 1 / 2 / 3 Springs

// ---- Detuning (SPEC §4.3, AntiRes layer 3, ADR 0010) -----------------------
// Each Spring is the same design with its own small offsets, like three real
// springs that are never quite the same length or tension:
//   loopDelay  multiplies L       -> different echo repeat time
//   transition multiplies fC      -> different stretch K = fs/(2 fC), so the
//                                    Chirp has a different pitch spacing
//   allpassCoeff multiplies a     -> different Chirp steepness
// Why: a feedback loop "prefers" frequencies whose round trip is a whole
// number of cycles (its modes). If two Springs had the same modes they would
// reinforce each other into a Ringing tone. With 3–8 % offsets and ratios
// that are not simple fractions (A:B:C loop lengths 0.965 : 1.05 : 0.925),
// the Springs' modes interleave instead of lining up, which also gives the
// slow beating/density that makes 2 and 3 Springs sound richer than one.
// Spring A is what 1-Spring mode plays; it is detuned too (by the smallest
// amounts) so the three are symmetric around the ParamSpec mapping.
struct Detune {
    float loopDelay;
    float transition;
    float allpassCoeff;
};
inline constexpr std::array<Detune, kNumSprings> kDetune{{
    {0.965f, 1.040f, 1.030f}, // A: -3.5 % L, +4 % fC, +3 % a  (left)
    {1.050f, 0.955f, 0.960f}, // B: +5 % L, -4.5 % fC, -4 % a   (right)
    {0.925f, 1.075f, 1.070f}, // C: -7.5 % L, +7.5 % fC, +7 % a (centre)
}};
// Extremes of the table, so the Spring sizes its delay memory for the
// longest detuned L and largest detuned K (checked by static_asserts below).
constexpr float kMaxLoopDelayDetune  = 1.05f;
constexpr float kMinTransitionDetune = 0.955f;

// ---- Stage caps per mode (SPEC §5 mitigation 3, §7 M3 tradeoff order) -----
// BOING maps to a stage count M between the floor (map::kMinStages = 24,
// ADR 0007) and the mode's cap. The cap scales the whole BOING range rather
// than clipping it, so BOING has no dead zone in any mode.
//
// Idle Springs (not heard in this mode) keep running at the floor count so
// they always hold a live tail, ready for a click-free SPRINGS change (see
// Tank.h "SPRINGS switching"). Rule that keeps CPU safe: every mode's total
// stage count must be <= the 3-Spring total, so 3 Springs stays the worst
// case the SPEC §5 budget is written for:
//   1 Spring : 64 + 24 + 24 = 112
//   2 Springs: 64 + 64 + 24 = 152
//   3 Springs: 52 + 52 + 52 = 156  <- worst case
// M1/M2 behaviour (one Spring, 64 stages at BOING 1) is unchanged.
constexpr int kIdleStages = map::kMinStages;
inline constexpr std::array<int, kNumModes> kStageCap{{64, 64, 52}};

constexpr int modeTotalStages(int mode)
{
    return mode == 0 ? kStageCap[0] + 2 * kIdleStages
         : mode == 1 ? 2 * kStageCap[1] + kIdleStages
                     : 3 * kStageCap[2];
}
static_assert(modeTotalStages(0) <= modeTotalStages(2) && modeTotalStages(1) <= modeTotalStages(2),
              "1- and 2-Spring modes (with idle Springs) must not cost more than 3-Spring mode");
static_assert(kStageCap[0] <= map::kMaxStages && kStageCap[1] <= map::kMaxStages && kStageCap[2] <= map::kMaxStages,
              "stage cap above the Spring's allocated maximum");

// Stage count for BOING v under a cap: floor + (cap - floor)·v, rounded.
// With cap = map::kMaxStages this is exactly map::boingStages(v).
inline int boingStages(float v, int cap)
{
    return map::kMinStages + static_cast<int>(static_cast<float>(cap - map::kMinStages) * v + 0.5f);
}

// Is Spring s heard in this mode? A in all, B in 2 and 3, C in 3 only.
constexpr bool springActive(int mode, int s) { return s <= mode; }

// ---- Pickup position per Spring (mono safety + width, M4 follow-up) --------
// Where each Spring's pickup taps its delay line, as a fraction of L (M1
// used 0.5). Staggering them makes each Spring's *first* echo arrive at a
// clearly different time (at DECAY 0: C ~7 ms, A ~19 ms, B ~30 ms; at
// DECAY 1 three times that). Why it matters: with equal taps the three
// first echoes land within ~1–2 ms of each other and look alike, so
// (a) L and R were strongly correlated at short DECAY (narrow), and
// (b) summing two near-copies a millisecond apart is a comb filter: deep
// notches in the mono sum. Arrivals 10+ ms apart are different echoes, not
// near-copies, so both problems go away. C (centre) speaks first, which
// also anchors the image in the middle, then A (left), then B (right).
inline constexpr std::array<float, kNumSprings> kPickupTap{{0.65f, 0.95f, 0.25f}};

// ---- Stereo output per mode (SPEC §4.3) ------------------------------------
// Built as mid/side, which makes mono safety a matter of construction:
//
//   mid  = sum(mid gain  × Spring)        what a mono listener hears
//   side = sum(side gain × Spring)        the L/R difference
//   D    = decorrelator(mid)              two short allpasses: same spectrum
//                                         as mid, scrambled phase
//   L = mid + side + w·D,   R = mid - side - w·D
//
// Mono (L + R) = 2·mid exactly: side and D cancel completely, so whatever
// makes the stereo wide can never comb-filter or thin out the mono sum.
// The mono sum is simply the Springs added together.
//
//   1 Spring : mid = A,              side = 0,             w = 0.65
//              (M1 put D alone on R; L + R = A + D then had allpass comb
//              notches down to -10 dB. Now mono is exactly Spring A.)
//   2 Springs: mid = (A + B)/2,      side = k·(A - B),     w = 0.40
//              = A left, B right (L = 0.93 A + 0.07 B with k = 0.43), plus
//              decorrelated cross-feed: D carries some of each Spring to
//              both sides without adding correlation or combs, so neither
//              side is ever empty.
//   3 Springs: mid = (A + B)/2 + c·C, side = k·(A - B),    w = 0.50
//              C in the centre; D keeps the centre from making L and R
//              too alike (C alone in both sides would be correlation 1).
//
// Width: L·R = mid² - (side + w·D)², so the more side and D energy
// relative to mid, the lower the L/R correlation. D is a scrambled copy of
// mid with the same level, so with w = 0.65 even a single Spring gets
// correlation ~ (1 - w²)/(1 + w²) ≈ 0.4.
//
// Level match: with the Springs treated as independent (they add in power),
//   stereo power (L² + R²)/2 = mid²·(1 + w²) + side²
//   mono power ((L + R)/2)²  = mid²
// Modes spend different shares on side/D, so one scale can't make both
// exactly equal across modes; each mode is scaled so the *average* of the
// two is 1 (mixPower). Measured (test_tank "Level"): stereo and mono
// loudness of 1/2/3 Springs both within ±1.5 dB. kModeTrim is left for M8
// tuning by ear.
constexpr int kNumSources = kNumSprings; // A, B, C

struct StereoMix {
    float mid[kNumSources];
    float side[kNumSources];
    float decorr; // w
};

constexpr float kDecorr1 = 0.65f; // w, 1 Spring
constexpr float kDecorr2 = 0.40f; // w, 2 Springs
constexpr float kDecorr3 = 0.50f; // w, 3 Springs
constexpr float kCentre3 = 0.40f; // c, 3 Springs
constexpr float kSide2   = 0.43f; // k, 2 Springs (0.5 = hard pan)
constexpr float kSide3   = 0.45f; // k, 3 Springs
// HighsLater Chirp (Mappings.h): the lows' round trip is ~20 ms shorter (the
// allpass chain barely delays them), so with the same T60 they make more
// trips per second and build up ~1 dB louder on low, tonal material (held
// chords, snare bodies); the highs near fC get quieter. This trim puts the
// Tank's level, and the limiter's headroom, back where M5-M7 tuned them.
// LowsLater: exactly 1 (unchanged).
constexpr float kChirpDirectionTrim = map::kHighsLater ? 0.89f : 1.0f; // -1 dB
inline constexpr std::array<float, kNumModes> kModeTrim{{kChirpDirectionTrim, kChirpDirectionTrim, kChirpDirectionTrim}};

// Loudness power of a mix: average of stereo and mono power (see "Level
// match"), treating the Springs (and D vs mid) as independent.
inline float mixPower(const StereoMix& m)
{
    float mid = 0.0f, side = 0.0f;
    for (int k = 0; k < kNumSources; ++k) {
        mid += m.mid[k] * m.mid[k];
        side += m.side[k] * m.side[k];
    }
    return mid * (1.0f + 0.5f * m.decorr * m.decorr) + 0.5f * side;
}

// The mix for a mode, before normalisation and trim (see Tank: it scales by
// 1/sqrt(mixPower) continuously, also through a SPRINGS fade).
inline StereoMix stereoMix(int mode)
{
    StereoMix m{};
    if (mode == 0) {
        m.mid[0] = 1.0f;
        m.decorr = kDecorr1;
    } else {
        m.mid[0] = m.mid[1] = 0.5f;
        m.side[0] = mode == 2 ? kSide3 : kSide2;
        m.side[1] = -m.side[0];
        m.mid[2]  = mode == 2 ? kCentre3 : 0.0f;
        m.decorr  = mode == 2 ? kDecorr3 : kDecorr2;
    }
    return m;
}

} // namespace rv::modes
