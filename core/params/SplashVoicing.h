#pragma once
// SPLASH / KICK / WOBBLE voicing (SPEC §3 K3/K5, button/gate, §4.5–4.7;
// ADRs 0005, 0008, 0013, 0015, 0016, 0020; CONTEXT.md: Hit, Clatter, Jolt,
// Splash, Kick, Drift, Warble, Micro-mod floor).
//
// One place for every number that shapes the M7 sound, so M8 tuning by ear
// edits constants here, not DSP code (same role as DriveVoicing.h). Pure
// constants + tiny mapping functions of Normalised values. No platform code,
// no powf (flash, Mappings.h): exp/log/sin only, and only at control rate.
//
// The DSP that uses these: dsp/Splash.h (HitDetector, Clatter, Jolt),
// dsp/Kick.h (KickVoice), dsp/Wobble.h. How they hook into the Tank:
// docs/m7-integration.md.

#include "params/Mappings.h"

#include <array>
#include <cmath>

namespace rv::splash {

// The M7 components step their control-rate logic on a fixed grid of this
// many samples, counted internally from reset(), so their output does not
// depend on the host block size. Equal to Tank::kControlInterval, so after
// integration the grids line up (static_assert there, docs/m7-integration.md).
constexpr int kControlInterval = 32;

// ---- Hit detector (SPEC §4.5 step 1) ------------------------------------------
// Two envelope followers on the driven (post-DriveIn) mono signal, first
// high-passed at kDetectorHpHz: fast = peak follower on |x| (1 ms attack),
// slow = follower of the fast one (50 ms attack). Their difference
// d = fast − slow is large only at the start of a transient (the slow one
// has not caught up yet) and ~0 on sustained sound. Hit = curve(d / T),
// T = SPLASH's threshold. The high-pass keeps a bass note's waveform ripple
// (the fast follower drooping between 110 Hz peaks) from reading as hits;
// transients are broadband, so a kick drum's click still registers.
constexpr float kDetectorHpHz  = 200.0f;
constexpr float kFastAttackMs  = 1.0f;
constexpr float kFastReleaseMs = 20.0f;
constexpr float kSlowAttackMs  = 50.0f;
constexpr float kSlowReleaseMs = 80.0f;

// Threshold T (linear amplitude of d) where Hit = 0.5. SPLASH 0: 0.30 (only
// hard hits register), SPLASH 1: 0.10. The −6 dBFS snare of 02_hits gives
// d ≈ 0.22 (high-passed fast peak minus the slow follower already rising),
// the −12 dBFS one d ≈ 0.11, the −18 dBFS ghost d ≈ 0.055.
constexpr float kHitThresholdSplash0 = 0.30f;
constexpr float kHitThresholdSplash1 = 0.10f;
inline float hitThreshold(float splash) { return map::expLerp(kHitThresholdSplash0, kHitThresholdSplash1, splash); }

// Hit = r³ / (1 + r³), r = d / T. A level-dependent knee: a hit at half the
// threshold gives 0.11, at the threshold 0.5, at twice 0.89. A hit 12 dB
// below a hard one (the ghost) lands far down the curve, and Clatter energy
// goes with Hit², so ghosts barely register and hard hits clearly do (SPEC
// §7 M7). Snare Hit at −6 / −12 / −18 dBFS (test_splash): SPLASH 0:
// 0.27 / 0.05 / 0.006; SPLASH 0.5: 0.66 / 0.20 / 0.03; SPLASH 1: 0.91 / 0.56 /
// 0.14 (Clatter −11 and −25 dB re the hard hit). Multiplies only.
inline float hitCurve(float d, float threshold)
{
    const float r = d / threshold, r3 = r * r * r;
    return r3 / (1.0f + r3);
}

// Stroke detection (one primary impact per stroke): the detector re-arms
// once Hit has fallen below kRearmRatio × the last stroke's peak and
// kMinStrokeMs have passed; armed, a stroke starts when Hit rises above
// kOnsetHit + kRetriggerRatio × the lowest Hit since re-arming (the valley).
// So a snare roll gives one impact per stroke (Hit falls between strokes),
// while the ragged decay of one noisy stroke, a slow swell or a bass note's
// envelope ripple give none extra.
constexpr float kOnsetHit       = 0.02f;
constexpr float kRetriggerRatio = 2.0f;
constexpr float kRearmRatio     = 0.5f;
constexpr float kMinStrokeMs    = 20.0f;

// ---- Clatter (SPEC §4.5 step 2) -------------------------------------------------
// Band-passed noise bursts, 1–6 kHz (2nd-order HP + 2nd-order LP), fed into
// each Spring's high path. Each impact fires a seeded timing jitter of
// kJitterMin..MaxMs after the Hit peaked, with the peak Hit as its strength.
constexpr float kClatterHpHz  = 1000.0f;
constexpr float kClatterLpHz  = 6000.0f;
// Burst peak at Hit 1, amount 1 (before the band-pass). 0.6 in the
// stand-alone build; ×5 (+14 dB) at integration: through the Tank the
// Clatter goes into the high path, whose HPF (0.8 fC) and level (TONE,
// 0.225 at noon) leave little of it, so at 0.6 KICKED SPLASH 1 on a hard
// snare added only +0.2 dB of 1-6 kHz (Clatter −16 dB under the snare's own
// bright part). At 3.0 it is −2 dB (KICKED) / −8 dB (DRIVEN), a clear crash
// (test_m7_tank). Ratios (ghost vs hard hit) do not depend on it.
constexpr float kClatterGain  = 3.0f;
constexpr float kJitterMinMs  = 0.5f;
constexpr float kJitterMaxMs  = 4.0f;
// Secondary impacts ("rattle": springs bouncing against each other/the
// housing) follow the first at seeded intervals, each weaker.
constexpr float kRattleIntervalMinMs = 7.0f;
constexpr float kRattleIntervalMaxMs = 22.0f;
constexpr float kRattleStrengthRatio = 0.6f;

// ---- Jolt (SPEC §4.5 step 3) ------------------------------------------------------
// Envelope j (0..1): at an impact its target jumps to strength × jolt amount
// and decays over joltDecayMs; j follows the target with a kJoltAttackMs
// one-pole, so the lurch has a rise (the tank stretches: L grows, pitch
// dips) and a slow return (pitch slightly sharp while L shrinks back).
// Outputs: Loop delay offset = j × joltLoopFrac × L, allpass offset
// Δa = kChirpSign × j × joltAllpass (larger |a| = more dispersion: chirp
// smear; −j × joltAllpass for the LowsLater Chirp, Mappings.h).
// 18 ms: the fastest lurch (KICKED, Kick, L = 108 ms) stays under the
// Spring's Loop slew limit of 0.08 samples/sample (test_splash).
constexpr float kJoltAttackMs = 18.0f;
// Each Spring lurches its own way (a tank knock does not stretch every
// spring the same): Spring A, B, C scale the Loop-delay Jolt by these.
// B moves the other way, so a big hit also spreads the stereo image.
inline constexpr std::array<float, 3> kJoltSpringScale{{1.0f, -0.75f, 0.9f}};
// KICKED rattle: a seeded random jitter (~kRattleHz, smoothed) riding on the
// Jolt, depth ∝ (j + kRattleEnergyGain × tank level): energy-dependent.
constexpr float kRattleHz         = 18.0f;
constexpr float kRattleEnergyGain = 2.0f;
// Tank level fed to the rattle (Tank integration): RMS of the wet mid,
// smoothed with this time constant, times the smoothed SPLASH (so SPLASH 0
// has no tail rattle; the Kick's forced Splash rattles through the Jolt).
constexpr float kTankLevelSmoothMs = 50.0f;
// Hard ceiling on |a| after the Jolt is added, so a Jolt can never push the
// allpass toward |a| = 1. Only bites at BOING max on the most-detuned Spring
// (−0.72 × 1.07 − 0.12 = −0.89 → −0.85; HighsLater tops out at
// 0.55 × 1.07 + 0.12 = 0.71, so the clamp never bites there).
constexpr float kMaxAllpassMagnitude = 0.85f;

// ---- CLEAN: SPLASH = mild HF emphasis only (SPEC §4.5 table) -------------------
// High path input gain = 1 + hitEnv × SPLASH × hfEmphasis, hitEnv = Hit held
// with a kHfEmphasisReleaseMs release. CLEAN 0.41 = +3 dB on transients at
// SPLASH max, nothing at rest.
constexpr float kHfEmphasisReleaseMs = 40.0f;

// ---- Per-ATTITUDE table (SPEC §4.5) -----------------------------------------------
// Blended by the ATTITUDE Morph weights exactly like drive::Voice.
struct Voice {
    float clatterFloor;   // Clatter amount at SPLASH 0 ("polite tank" natural splash)
    float clatterMax;     // at SPLASH 1
    float clatterDecayMinMs; // burst decay (1/e) for a weak impact
    float clatterDecayMaxMs; // for a Hit-1 impact
    float rattleImpacts;  // secondary impacts after a Hit-1 impact (∝ strength)
    float joltFloor;      // Jolt amount at SPLASH 0
    float joltMax;        // at SPLASH 1 (a Kick always uses this)
    float joltDecayMs;    // Jolt target decay (1/e)
    float joltLoopFrac;   // Loop delay offset at j = 1, fraction of L
    float joltAllpass;    // |Δa| at j = 1
    float rattleDepth;    // KICKED rattle, fraction of L at full rattle
    float hfEmphasis;     // CLEAN HF emphasis (see above)
};

inline constexpr std::array<Voice, 3> kVoice{{
    //  clat0  clat1  dMin   dMax   ratt  jolt0  jolt1  jDec    jL      jA     rattle   hf
    {  0.00f, 0.00f,  5.0f, 10.0f, 0.0f, 0.00f, 0.00f,  60.0f, 0.000f, 0.00f, 0.0000f, 0.41f}, // CLEAN
    {  0.18f, 0.55f,  6.0f, 18.0f, 1.0f, 0.10f, 0.50f,  90.0f, 0.006f, 0.025f, 0.0000f, 0.00f}, // DRIVEN
    {  0.25f, 1.00f,  8.0f, 30.0f, 3.0f, 0.20f, 1.00f, 180.0f, 0.011f, 0.12f, 0.0015f, 0.00f}, // KICKED
}};

// DRIVEN's |Δa| was 0.05 in the stand-alone build; halved at integration.
// The Δa is common to all Springs, and at 0.05 it pulled their responses
// together enough to break the M4 stereo checks at the default SPLASH 0.3
// (test_tank: mono notch −6.4 dB on chord stabs, 2 Springs, DECAY 0 BOING 1;
// L/R correlation 0.47 → 0.49 on hits). At 0.025 all M4 checks keep their
// margin. KICKED keeps 0.12: a big smear is part of "full chaos".

inline Voice blendVoice(const std::array<float, 3>& w)
{
    Voice v{};
    auto mix = [&](float Voice::*m) {
        v.*m = w[0] * (kVoice[0].*m) + w[1] * (kVoice[1].*m) + w[2] * (kVoice[2].*m);
    };
    mix(&Voice::clatterFloor);
    mix(&Voice::clatterMax);
    mix(&Voice::clatterDecayMinMs);
    mix(&Voice::clatterDecayMaxMs);
    mix(&Voice::rattleImpacts);
    mix(&Voice::joltFloor);
    mix(&Voice::joltMax);
    mix(&Voice::joltDecayMs);
    mix(&Voice::joltLoopFrac);
    mix(&Voice::joltAllpass);
    mix(&Voice::rattleDepth);
    mix(&Voice::hfEmphasis);
    return v;
}

// Clatter / Jolt amount for SPLASH v: floor at 0 (a DRIVEN tank still splashes
// a little on hard hits), max at 1, linear between (the Hit curve is already
// steep; a linear amount keeps the knob even).
inline float clatterAmount(const Voice& vc, float splash) { return vc.clatterFloor + (vc.clatterMax - vc.clatterFloor) * splash; }
inline float joltAmount(const Voice& vc, float splash) { return vc.joltFloor + (vc.joltMax - vc.joltFloor) * splash; }

// ---- KICK (SPEC §4.6, ADR 0005, 0013, 0016) --------------------------------------
// A Kick = low thump (decaying sine with a downward pitch glide, like a
// knuckle on the tank) + ~10 ms broadband burst + a forced maximal Splash
// (Clatter + Jolt at Hit 1, SPLASH 1: the "big crash"). Fixed strength,
// scaled by ATTITUDE only.
struct KickParams {
    float thumpGain;     // thump peak
    float thumpStartHz;  // pitch at the strike
    float thumpEndHz;    // pitch it glides down to
    float thumpDecayMs;  // amplitude 1/e time (−40 dB after ~4.6×)
    float burstGain;     // broadband burst peak
    float burstDecayMs;  // 1/e: −40 dB after ~10 ms
};
inline constexpr std::array<KickParams, 3> kKick{{
    // gain  f0     f1     tau    burst  tau
    {  0.35f, 80.0f, 55.0f, 22.0f, 0.20f, 2.2f}, // CLEAN: polite knock
    {  0.50f, 75.0f, 50.0f, 28.0f, 0.35f, 2.5f}, // DRIVEN
    {  0.70f, 70.0f, 45.0f, 35.0f, 0.50f, 3.0f}, // KICKED: big thud
}};
inline KickParams blendKick(const std::array<float, 3>& w)
{
    KickParams k{};
    auto mix = [&](float KickParams::*m) {
        k.*m = w[0] * (kKick[0].*m) + w[1] * (kKick[1].*m) + w[2] * (kKick[2].*m);
    };
    mix(&KickParams::thumpGain);
    mix(&KickParams::thumpStartHz);
    mix(&KickParams::thumpEndHz);
    mix(&KickParams::thumpDecayMs);
    mix(&KickParams::burstGain);
    mix(&KickParams::burstDecayMs);
    return k;
}
constexpr float kThumpGlideMs = 15.0f;  // pitch glide 1/e time
// The part of the Kick fed into the Loop is high-passed here (4th order:
// two 2nd-order sections, ~−30 dB at the thump's 50 Hz) so
// the thump cannot ring in the tail (ADR 0016); the full thump goes straight
// to the wet bus (the pickup hears the tank body move).
constexpr float kKickLoopHpHz = 120.0f;
// Two gate/button edges closer than this are one Kick (bounce guard: a
// 12/s gate train is 83 ms apart, far above it).
constexpr float kKickMergeMs = 5.0f;
constexpr float kBurstHpHz   = 150.0f; // keeps the burst's own lows out

// ---- WOBBLE (SPEC §4.7, ADR 0008, 0020) ---------------------------------------------
// Per Spring: Loop delay modulation m(t) = D · (wS · sin(2π f t + φ) + wR · r(t)),
// r = smoothed seeded random (smoothstep between random points at f_r).
// It rides on top of M6's Micro-mod floor; at WOBBLE 0 it is exactly 0.
//
// Pitch maths: a delay line read with a time-varying delay m(t) (samples)
// plays back at rate 1 − dm/dn, so one pass through the Loop shifts pitch by
//   cents(t) = 1200 · log2(1 − dm/dn),   dm/dn = (dm/dt) / fs.
// For the sine alone, peak |dm/dt| = 2π f D, so the per-pass peak deviation is
//   c = 1200 · log2(1 + 2π f D / fs).
// SPEC §4.7's guess "D = 0.5–1 % of L" gives, at f = 1 Hz, fs-independent:
//   L = 30 ms: D = 0.15–0.3 ms → 2π·1·0.0003 = 0.0019 → 3.3 cents
//   L = 100 ms: D = 0.5–1 ms  →                          10.9 cents
// i.e. 2–11 cents: fine for Drift, far too small for "clearly out of tune"
// (ADR 0008), and it would make WOBBLE's pitch depend on DECAY. So depth is
// specified in cents per pass and converted to samples with the rate:
//   D = (2^(c/1200) − 1) · fs / (wS · 2π f + wR · 2 f_r)
// (the random part's typical peak slope is ~2 f_r: smoothstep slope 1.5·Δ·f_r
// with a large step Δ ≈ 4/3). Then WOBBLE sounds the same at every DECAY.
//
// The tail multiplies it. A held tone in a Loop of gain g sits on the Loop's
// resonances, where the phase slope (group delay) is ~1/(1 − g) round trips:
// modulating L then moves the tail's phase that many times faster, so the
// heard deviation of the wet tail is a "tail factor" G times the per-pass one
// (G ≈ 1/(1 − g) while the modulation is slow against the tail; it saturates
// once n·L approaches a modulation period). Measured with a held 1 kHz tone
// through a modulated feedback Loop (test_wobble, 10-cycle averaged pitch):
//   DECAY 0 (L 30 ms, T60 0.4 s): G ≈ 1.5–1.8
//   DECAY noon (L 55 ms, T60 1.9 s): G ≈ 1–7 (depends on where the tone sits
//                                     against the Loop's resonances), typ. 4–6
//   DECAY max (L 100 ms, T60 9 s): G ≈ 2–4.5
// So with G ≈ 5 at the default DECAY, SPEC §4.7's 0.5–1 % of L (1.7–3.4
// cents per pass at 1 Hz, L = 55 ms) is ~10–17 cents heard: not as far off
// as it looks per pass. The zones below are set for the *heard* tail at
// DECAY noon, and specified per pass (so WOBBLE does not depend on L).
//
// Depth curve (ADR 0008, exponential-ish): c(w) = cMax (e^{k w} − 1)/(e^k − 1),
// cMax = 12 cents per pass, k chosen so c(0.5) = 0.75: 1/(e^{k/2} + 1) =
// 0.75/12 → k = 5.42. Per pass → heard in the tail at DECAY noon (×G):
//   Drift      0 – 0.50:  0 → 0.75 cents → ≲ 5 cents (felt; slow drift under
//                                          the pitch JND: held chords in tune)
//   transition 0.50–0.75: 0.75 → 3.0 cents → 5 → ~15 cents (becoming audible)
//   Warble     0.75–1.00: 3.0 → 12 cents  → ~15 → ~50 cents (worn tape: clearly
//                                          out of tune on held chords)
// As a fraction of L at WOBBLE 1 (D ≈ 41 samples at 48 kHz): 2.8 % at DECAY 0,
// 1.5 % at noon, 0.85 % at DECAY max.
// Placeholder cMax until the Magneto WOW & FLUTTER takes (ADR 0020) calibrate
// it on 08_held_tones; per-DECAY numbers: docs/m7-integration.md.
constexpr float kWobbleMaxCents  = 12.0f;
constexpr float kWobbleCurve     = 5.42f;
constexpr float kDriftEnd        = 0.50f;
constexpr float kWarbleStart     = 0.75f;
inline float wobbleCents(float w)
{
    if (w <= 0.0f) return 0.0f;
    return kWobbleMaxCents * (std::exp(kWobbleCurve * w) - 1.0f) / (std::exp(kWobbleCurve) - 1.0f);
}
// Rate rises gently with depth (SPEC §3 K5): slow drift 0.12 Hz → wow 1.4 Hz.
constexpr float kWobbleRateMinHz = 0.12f;
constexpr float kWobbleRateMaxHz = 1.4f;
inline float wobbleRateHz(float w) { return map::expLerp(kWobbleRateMinHz, kWobbleRateMaxHz, w); }
// Random part runs a little faster than the sine, unrelated ratio.
constexpr float kWobbleRandomRateRatio = 1.37f;
// Sine share: Drift is mostly random (alive, not periodic), Warble mostly
// the periodic wow of a worn capstan.
constexpr float kWobbleSineMin = 0.30f;
constexpr float kWobbleSineMax = 0.90f;
inline float wobbleSineWeight(float w) { return kWobbleSineMin + (kWobbleSineMax - kWobbleSineMin) * w; }
// Independent per Spring: own seed/phase, and slightly different rates so
// they never lock (unrelated ratios).
inline constexpr std::array<float, 3> kWobbleSpringRate{{0.87f, 1.0f, 1.13f}};

// Modulation amplitude D in samples for WOBBLE w at sample rate fs (and a
// Spring rate scale). 0 at w = 0.
inline float wobbleDepthSamples(float w, float sampleRate, float rateScale = 1.0f)
{
    const float c = wobbleCents(w);
    if (c <= 0.0f) return 0.0f;
    const float f = wobbleRateHz(w) * rateScale, ws = wobbleSineWeight(w);
    const float ratio = std::exp(c * (0.693147181f / 1200.0f)) - 1.0f;
    return ratio * sampleRate / (ws * 2.0f * map::kPi * f + (1.0f - ws) * 2.0f * kWobbleRandomRateRatio * f);
}
// Upper bound of |m| (samples) over the whole knob, for sizing delay memory:
// D·(wS + wR) = D. Largest at mid-knob where the rate is still low.
inline float wobbleMaxDepthSamples(float sampleRate)
{
    float m = 0.0f;
    for (int i = 1; i <= 20; ++i) m = std::fmax(m, wobbleDepthSamples(0.05f * float(i), sampleRate, kWobbleSpringRate[0]));
    return m;
}

} // namespace rv::splash
