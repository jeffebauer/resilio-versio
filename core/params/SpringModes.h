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
//
//   damping      multiplies the Loop's damping cutoff (TONE) -> each Spring
//                loses its highs at its own speed
//   decay        multiplies T60 -> each Spring dies away at its own speed
//                (never longer than DECAY asks)
// Why (M8, after TENSION, ADR 0026): interleaving is not enough on its own.
// Two combs with slightly different spacings still line up every
// 1/(RT_B - RT_A) Hz (a Vernier: ~330 Hz apart on the tightest tank, where
// the round trips differ by ~3 ms). Near each line-up a mode of A and a mode
// of B sit a fraction of a Hz apart and beat slowly. If they also die at the
// same speed, a pair that drifts into phase late in the tail swells up out
// of it as one singing note ("resonances creeping in at mid DECAY", owner):
// ringing_db 12-23 on 2 and 3 Springs at DECAY 0.3-0.75, worst on the tight
// tanks, where the modes are ~30 Hz apart and one pair has few neighbours to
// hide among. One Spring alone never does this (ringing_db <= 4). Any slow
// drift that moves the Springs apart (WOBBLE, the Micro-mod floor) makes
// more pairs slide into phase, at random. The cure is to let the two
// modes of a pair die at different speeds, as the springs in a real tank do:
// by the late tail one of them has faded and the pair is one mode, not a
// beat. Both spreads point the same way (a darker Spring is also a shorter
// one), so the two modes of a pair part at every frequency, from the low
// mids (decay) to the top (damping); darker-but-longer cancels out near
// 1-2 kHz and made it worse. A, B, C each a step darker and shorter than
// the one before, so every pair differs: A x1 / x1 (1 Spring renders
// exactly as before), B x0.85 damping (about 0.09 of a TONE turn darker)
// and x0.93 T60, C x0.72 and x0.865. No Spring rings longer than DECAY asks
// (AntiRes layer 1); the longest, A, still sets the tail length you hear.
// Tank scan (WOBBLE 0.2, TONE noon, TENSION 0-1 x DECAY 0.3-0.75, clicks /
// hits / bursts, 2 and 3 Springs): cells >= 12 dB 49 -> 5, >= 15 dB 19 -> 0.
// Twice the spread (B x0.8 / x0.9, C x0.64 / x0.81) scored 0 there, but at
// long DECAY it thinned a 3-Spring chord tail into Spring A alone and left
// one partial singing over it (+9 dB), so the gentler step won.
// The Loop gain design sees the damping (it checks every band), so a
// Spring's decay factor is exactly its T60 change.
struct Detune {
    float loopDelay;
    float transition;
    float allpassCoeff;
    float damping;
    float decay;
};
inline constexpr std::array<Detune, kNumSprings> kDetune{{
    {0.965f, 1.040f, 1.030f, 1.00f, 1.000f}, // A: -3.5 % L, +4 % fC, +3 % a                      (left)
    {1.050f, 0.955f, 0.960f, 0.85f, 0.930f}, // B: +5 % L, -4.5 % fC, -4 % a; darker, shorter     (right)
    {0.925f, 1.075f, 1.070f, 0.72f, 0.865f}, // C: -7.5 % L, +7.5 % fC, +7 % a; darkest, shortest (centre)
}};
// Extremes of the table, so the Spring sizes its delay memory for the
// longest detuned L and largest detuned K (checked by static_asserts below).
constexpr float kMaxLoopDelayDetune  = 1.05f;
constexpr float kMinTransitionDetune = 0.955f;

// ---- Stage caps per mode (SPEC §5 mitigation 3, §7 M3 tradeoff order) -----
// TENSION maps to a stage count M between the floor (map::kMinStages = 24,
// ADR 0007) and the mode's cap. The cap scales the whole TENSION range rather
// than clipping it, so TENSION has no dead zone in any mode.
//
// Idle Springs (not heard in this mode) keep running at the floor count so
// they always hold a live tail, ready for a click-free SPRINGS change (see
// Tank.h "SPRINGS switching"). Rule that keeps CPU safe: every mode's total
// stage count must be <= the 3-Spring total, so 3 Springs stays the worst
// case the SPEC §5 budget is written for:
//   1 Spring : 64 + 24 + 24 = 112
//   2 Springs: 64 + 64 + 24 = 152
//   3 Springs: 52 + 52 + 52 = 156  <- worst case
// One Spring: 64 stages at TENSION 1.
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

// Stage count for TENSION v under a cap: floor + (cap - floor)·share(v),
// rounded (share: map::tensionStageFraction). With cap = map::kMaxStages
// this is exactly map::tensionStages(v).
inline int tensionStages(float v, int cap)
{
    return map::kMinStages + static_cast<int>(static_cast<float>(cap - map::kMinStages) * map::tensionStageFraction(v) + 0.5f);
}

// Is Spring s heard in this mode? A in all, B in 2 and 3, C in 3 only.
constexpr bool springActive(int mode, int s) { return s <= mode; }

// ---- Pickup position per Spring (first arrivals, M8 backlog item 5) --------
// Where each Spring's pickup taps its delay line: tapRatio × L plus a fixed
// offset in seconds. The first echo arrives after about that (+ the chain).
//
// M4 staggered the taps (0.65 / 0.95 / 0.25 of L) so the three first echoes
// arrived 10+ ms apart (C 8, A 20, B 31 ms at DECAY 0; 14 / 36 / 56 ms at
// noon; 25 / 64 / 102 ms at max): that fixed L/R correlation and the mono
// comb, but a 20-80 ms spread reads as a flam (the owner heard the right
// Spring land late in 3-Spring mode).
//
// M8: every Spring's first echo lands together, at 0.52 × the *base* L
// (about half a round trip, as M1; 0.52 rather than 0.5 only because it
// kept the ATTITUDE-Morph click check in test_drive clear of its limit):
// tapRatio = 0.52 / (its loopDelay detune), so the detune no longer moves the
// arrival (it still sets each Spring's repeat time, so later echoes spread
// out). The offsets are under a millisecond, fixed (not scaled by DECAY),
// picked by grid searches: B's +0.15 ms nudges the residual A/B phase
// difference (the Chirps differ) so its first mono dip lands high and
// shallow (M4 stereo checks); C's -0.8 ms keeps C's steady partials from
// half-cancelling A and B's on held chords at long DECAY (test_antires
// held-tone pitch). First-arrival spread <= ~1.5 ms in every mode and
// DECAY (was 23 / 42 / 77 ms at DECAY 0 / 0.5 / 1). Mind the lows: two near-copies 2-4 ms apart
// cancel at 125-250 Hz (measured: mono_loss past -1.5 dB), so offsets of
// more than ~0.5 ms between Springs are worse, not better.
//
// Width now comes from D (below, stronger and longer than M4's), which is
// mono-safe by construction, instead of from different arrival times.
//
// TENSION (ADR 0026): the pickup also lines up the Chirp chains. The detune
// of a and fC gives each Spring its own chain delay: on the loosest tank (64
// stages) B's echo body arrives ~1 ms after A's, C's ~0.4 ms before
// (tightest, 24 stages: 0.2 / 0.1 ms). Two near-copies ~1 ms apart cancel
// broadly around 500 Hz in mono. Once the tail is long that washes out over
// many round trips, but at DECAY 0 on the loosest tank (110 ms, ~4 trips)
// the first echoes are most of what you hear, and the mono sum of 2 and 3
// Springs lost -7 to -10 dB around 550-750 Hz. So each Spring's offset
// includes (A's chain delay - its own) at kPickupAlignHz (Tank; recomputed
// as TENSION and SPRINGS move, and glided, so it never clicks): B and C's
// echo bodies land on A's at every TENSION, not only at the tight end.
// 800 Hz: the middle of the band that combed (grid search: 250-1000 Hz all
// fix the loose end; 800 keeps the most margin over DECAY 0-0.25 x TENSION
// 0-1 in steps of 1/8; above ~1 kHz, closer to fC, it gets worse again).
// The trims were re-searched on top: B's +0.15 ms stays; C's moved from
// -0.8 to -1.2 ms (with the alignment adding back 0.1-0.4 ms, C's net
// offset is about what it was at long TENSION and 0.3 ms earlier when tight,
// which keeps the 3-Spring mono sum full at DECAY 0 on the tightest tank).
inline constexpr std::array<float, kNumSprings> kPickupTap{{0.52f / 0.965f, 0.52f / 1.05f, 0.52f / 0.925f}};
inline constexpr std::array<float, kNumSprings> kPickupOffsetSeconds{{0.0f, 0.00015f, -0.0012f}}; // A, B, C
constexpr float kPickupAlignHz = 800.0f;

// A Spring's Chirp-chain delay (samples) at kPickupAlignHz: M stretched
// allpass sections plus the chirp low-pass (Butterworth at fC, well below
// cutoff ≈ sqrt(2)/(2 pi fC)). The same terms as Spring::roundTripSamples.
inline float pickupChainSamples(float allpassCoeff, float transitionHz, int stages, float sampleRate)
{
    const float k = map::stretchK(transitionHz, sampleRate);
    return static_cast<float>(stages) * map::stretchedAllpassGroupDelaySamples(allpassCoeff, k, kPickupAlignHz, sampleRate)
         + 1.41421356f * sampleRate / (2.0f * map::kPi * transitionHz);
}

// Decorrelator D (Tank): a cascade of Schroeder allpasses (delays in
// seconds, one coefficient), at unrelated lengths. M4 used two (2.3 and
// 3.7 ms); a third, longer stage scrambles the phase down into the low mids,
// so D is less like mid there and the same w buys lower L/R correlation.
// Still short enough (12 ms in total) to stay a diffusion, never an echo.
inline constexpr std::array<float, 3> kDecorrSeconds{{0.0023f, 0.0037f, 0.0061f}};
constexpr float kDecorrCoeff = 0.5f;

// ---- Stereo output per mode (SPEC §4.3) ------------------------------------
// Built as mid/side, which makes mono safety a matter of construction:
//
//   mid  = sum(mid gain  × Spring)        what a mono listener hears
//   side = sum(side gain × Spring)        the L/R difference
//   D    = decorrelator(mid)              three short allpasses: same spectrum
//                                         as mid, scrambled phase
//   L = mid + side + w·D,   R = mid - side - w·D
//
// Mono (L + R) = 2·mid exactly: side and D cancel completely, so whatever
// makes the stereo wide can never comb-filter or thin out the mono sum.
// The mono sum is simply the Springs added together.
//
//   1 Spring : mid = A,              side = 0,             w = 0.75
//              (M1 put D alone on R; L + R = A + D then had allpass comb
//              notches down to -10 dB. Now mono is exactly Spring A.)
//   2 Springs: mid = (A + B)/2,      side = k·(A - B),     w = 0.65
//              = A left, B right (L = 0.93 A + 0.07 B with k = 0.43), plus
//              decorrelated cross-feed: D carries some of each Spring to
//              both sides without adding correlation or combs, so neither
//              side is ever empty.
//   3 Springs: mid = (A + B)/2 + c·C, side = k·(A - B),    w = 0.65
//              C in the centre; D keeps the centre from making L and R
//              too alike (C alone in both sides would be correlation 1).
//
// Width: L·R = mid² - (side + w·D)², so the more side and D energy
// relative to mid, the lower the L/R correlation. D is a scrambled copy of
// mid with the same level, so with w = 0.75 even a single Spring gets
// correlation ~ (1 - w²)/(1 + w²) ≈ 0.28 (M4: w = 0.65, 0.43; M8 widened
// 1 Spring, which the owner heard as fairly mono). With the first echoes now
// arriving together, A and B start out alike (side small), so 2 and 3
// Springs need w = 0.65 (M4: 0.40 / 0.50, when the stagger did that work).
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

constexpr float kDecorr1 = 0.75f; // w, 1 Spring
constexpr float kDecorr2 = 0.65f; // w, 2 Springs
constexpr float kDecorr3 = 0.65f; // w, 3 Springs
constexpr float kCentre3 = 0.40f; // c, 3 Springs
constexpr float kSide2   = 0.40f; // k, 2 Springs (0.5 = hard pan). 0.43 -> 0.40 (29 Sep): mono-notch margin with SPLASH's clang in the Loops (stabs -4.8 dB at DECAY 0 x tightest TENSION)
constexpr float kSide3   = 0.45f; // k, 3 Springs
// HighsLater Chirp (Mappings.h): the lows' round trip is ~20 ms shorter (the
// allpass chain barely delays them), so with the same T60 they make more
// trips per second and build up ~1 dB louder on low, tonal material (held
// chords, snare bodies); the highs near fC get quieter. This trim puts the
// Tank's level, and the limiter's headroom, back where M5-M7 tuned them.
// 0.89 -> 0.88 at the HighsLater re-tune (the LoopSat flux shelf, Drive.h,
// squashes a little less, so DRIVEN hits came back 0.1 dB hotter and the
// MIX sweep sat exactly on its +-1.5 dB limit; now +2.9 dB wet vs dry).
// LowsLater: exactly 1 (unchanged).
constexpr float kChirpDirectionTrim = map::kHighsLater ? 0.88f : 1.0f; // -1.1 dB
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
