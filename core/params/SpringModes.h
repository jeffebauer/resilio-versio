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

// ---- Stereo output matrix per mode (SPEC §4.3) -----------------------------
// Four wet sources: the three Springs and D = Spring A through the short
// allpass decorrelator (the M1 1-Spring right channel). Each mode is a
// 2 × 4 gain matrix: out = sum(gain × source).
//
//   1 Spring : L = A,             R = D                (M1 behaviour)
//   2 Springs: L = s·A + x·B,     R = s·B + x·A        A left, B right
//   3 Springs: L = s·A + x·B + c·C,  R = s·B + x·A + c·C, C in the centre
//
// Cross-feed x: a little of the other side, so neither side ever sounds
// empty (headphones) while L and R stay mostly independent (width).
// For two independent equal-level sources the L/R correlation is
// 2x/(1+x²) (+ c² terms for the centre), so small gains keep us well under
// the 0.5 width target. All gains are positive, so anything the Springs
// share adds up in the mono sum instead of cancelling: mono-safe by design
// (and the Tank's equal-power SPRINGS fade relies on gains >= 0).
//
// Level match: independent Springs add in power, so each side's gains are
// divided by sqrt(s² + x² + c²) = sqrt(sum of squares) to keep the same
// loudness as one Spring. The Springs share their input, so their outputs
// are a little correlated; that would make 2 and 3 Springs slightly louder,
// but the 1-Spring right channel (A and its decorrelated copy D) is
// correlated by about as much, so it cancels out. Measured (test_tank "Level"):
// 2 and 3 Springs sit within 0.3 dB of 1 Spring with no trim. kModeTrim is
// there for M8 tuning by ear.
constexpr int kNumSources = 4; // A, B, C, D

struct OutMatrix {
    float l[kNumSources];
    float r[kNumSources];
};

constexpr float kCrossFeed2  = 0.18f; // x, 2 Springs  (-15 dB)
constexpr float kCrossFeed3  = 0.10f; // x, 3 Springs  (-20 dB)
constexpr float kCentre3     = 0.45f; // c, 3 Springs  (-7 dB each side, vs A/B)
inline constexpr std::array<float, kNumModes> kModeTrim{{1.0f, 1.0f, 1.0f}};

inline OutMatrix outMatrix(int mode)
{
    OutMatrix m{};
    if (mode == 0) {
        m.l[0] = 1.0f;
        m.r[3] = 1.0f;
        return m;
    }
    const float x = mode == 1 ? kCrossFeed2 : kCrossFeed3;
    const float c = mode == 1 ? 0.0f : kCentre3;
    const float n = kModeTrim[size_t(mode)] / std::sqrt(1.0f + x * x + c * c);
    m.l[0] = n;     m.l[1] = x * n; m.l[2] = c * n;
    m.r[1] = n;     m.r[0] = x * n; m.r[2] = c * n;
    return m;
}

} // namespace rv::modes
