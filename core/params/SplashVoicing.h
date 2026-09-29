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

// Level-adaptive detection (M8, backlog item 2). A Hit is judged against a
// reference R, not a fixed threshold, so it measures how much a hit stands
// out from what is playing, not how loud it is:
//   R = max(T, q · P),   Hit = curve(d / R)
// P = a slow program-level tracker: a follower of the fast envelope with a
// kProgAttackMs attack and kProgReleaseMs release (control rate). One
// isolated hit lifts it to ~0.1-0.2 of its envelope peak, a groove or a pad
// holds it up, silence lets it fall back within a few seconds.
//   T = absolute floor (SPLASH's threshold; isolated hits after silence,
//       P ≈ 0). SPLASH 0: 0.30 (only hard hits register); SPLASH 1: 0.015,
//       so an isolated rimshot or snare at −18 dBFS (d ≈ 0.035-0.06) gives
//       Hit ≈ 0.9-1. (0.10 at SPLASH 1 in M7's absolute detector.)
//   q = relative threshold: a hit needs d ≥ q · P for Hit ≥ 0.5. SPLASH 1:
//       1.5, SPLASH 0: 4.0. In a groove of −6 dBFS backbeats 1 s apart P
//       sits near a quarter of a backbeat's d, so backbeats get Hit ≈ 0.9
//       at any level, while a −18 dBFS ghost note half way between gets
//       Hit ≈ 0.1: it barely triggers (test_m7_tank ghostGroove: its
//       Clatter −22..−28 dB under its own bright part, −41 dB under a
//       backbeat's). A snare over a loud pad is judged against the pad.
// Reference d values (DRIVEN, DRIVE 0.25, post-DriveIn): the −6 dBFS snare
// of 02_hits d ≈ 0.15-0.22, −12 dBFS 0.07-0.11, −18 dBFS 0.04-0.055.
constexpr float kHitThresholdSplash0 = 0.30f;
constexpr float kHitThresholdSplash1 = 0.015f;
inline float hitThreshold(float splash) { return map::expLerp(kHitThresholdSplash0, kHitThresholdSplash1, splash); }
constexpr float kRelThresholdSplash0 = 4.0f;
constexpr float kRelThresholdSplash1 = 1.5f;
inline float relThreshold(float splash) { return map::expLerp(kRelThresholdSplash0, kRelThresholdSplash1, splash); }
constexpr float kProgAttackMs  = 100.0f;
constexpr float kProgReleaseMs = 1500.0f;

// Hit = r³ / (1 + r³), r = d / R. A knee: a hit at half the reference gives
// 0.11, at the reference 0.5, at twice 0.89. Multiplies only.
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
// Burst peak at Hit 1, amount 1 (before the band-pass), for a hit of the
// reference level. 0.6 in the stand-alone build; 3.0 at M7 integration.
// M8: the crash follows the hit's size. A burst's peak is scaled by the
// stroke's level λ = d / kClatterLevelRef (d of the −6 dBFS snare, so λ = 1
// there), capped at kClatterLevelMax: with the level-adaptive Hit, a hit
// that stands out gets the same crash *relative to itself* at −18 or −3
// dBFS (the owner's DAW levels vs the module's), and a quiet hit can never
// get a crash louder than a hard one. A Kick's forced strike: kKickClatterLevel.
// M7's 3.0 left the crash inaudible to the owner: Clatter −8 dB (DRIVEN) /
// −2 dB (KICKED) under the hit's own 1–6 kHz, and 0.0 dB of brightening on
// a rimshot in DRIVEN. M8 (test_m7_tank splashAudible):
//  - kClatterGain 3 → 3.5 into the Springs' high path (more than that and
//    the common burst through the detuned Springs combs the mono sum:
//    test_tank's mono-notch margin);
//  - kClatterWet: a share of the Clatter also goes straight to the wet,
//    after the pickups (Tank.cpp): the pickup hearing the springs clatter,
//    on top of the tail rather than only through the quiet high path.
// Crash (1–6 kHz, SPLASH 1 vs 0, first 150 ms) on a rimshot at −18 / −9 /
// −3 dBFS: DRIVEN +4.5 / +5.7 / +5.8 dB (M7: 0.0), KICKED +8 / +10 / +10.
constexpr float kClatterGain     = 3.5f;
constexpr float kClatterWet      = 0.45f;
// The direct share's side copy: delayed kClatterSideMs, at kClatterSide of
// the mid's level (L/R correlation of the direct crash (1 − 0.8²)/(1 + 0.8²)
// ≈ 0.22; the mono sum is the plain burst; test_tank's L/R correlation
// margin on hits).
constexpr float kClatterSideMs   = 1.3f;
constexpr float kClatterSide     = 0.8f;
constexpr float kClatterLevelRef = 0.22f;
constexpr float kClatterLevelMax = 2.5f;
// A Kick's forced strike: its crash level λ. With the direct share the M7
// value (1) put the Kick's crash so far over its thud that the limiter
// ducked the thud (KICKED Kick low end, test_kick); 0.5 keeps the Kick's
// crash about where M7 had it (high path −3 dB, plus the direct share).
constexpr float kKickClatterLevel = 0.5f;
// The jitter counts from the Hit's peak (the countdown restarts while the
// stroke is still growing, for at most kMaxRiseMs), so the burst takes the
// stroke's full strength and level (M8; before, a short jitter could fire
// on a hit's first millisecond with a fraction of its strength).
constexpr float kMaxRiseMs    = 2.0f;
constexpr float kJitterMinMs  = 0.3f;
constexpr float kJitterMaxMs  = 3.0f;
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
// The allpass Jolt (Δa) is scaled the same way per Spring (M8): a common Δa
// pulled the Springs' responses together, and with the more sensitive M8
// detector at the default SPLASH it cost test_tank's mono-notch margin on
// chord stabs (−5.3 dB at 2 Springs, margin −4.5; per Spring: −2.9).
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
};

inline constexpr std::array<Voice, 3> kVoice{{
    //  clat0  clat1  dMin   dMax   ratt  jolt0  jolt1  jDec    jL      jA     rattle
    {  0.00f, 0.35f,  4.0f, 10.0f, 0.0f, 0.00f, 0.50f,  60.0f, 0.002f, 0.005f, 0.0000f}, // CLEAN
    {  0.18f, 0.55f,  6.0f, 18.0f, 1.0f, 0.10f, 0.50f,  90.0f, 0.006f, 0.015f, 0.0000f}, // DRIVEN
    {  0.25f, 0.80f,  8.0f, 30.0f, 3.0f, 0.20f, 1.00f, 180.0f, 0.011f, 0.12f, 0.0015f}, // KICKED
}};

// CLEAN (ADR 0025, replaces SPEC §4.5's "mild HF emphasis only", which
// measured as no change: the knob's only dead range): a real but gentle
// splash, a polite tank getting nudged. A light, short Clatter with no
// rattle, and a tiny Jolt (Loop and allpass lurch a third of DRIVEN's).
// Nothing at SPLASH 0: CLEAN stays hi-fi unless asked. Same level-adaptive
// detector as DRIVEN / KICKED, so ghost notes barely trigger here too.

// DRIVEN's |Δa| was 0.05 in the stand-alone build; halved at integration.
// The Δa is common to all Springs, and at 0.05 it pulled their responses
// together enough to break the M4 stereo checks at the default SPLASH 0.3
// (test_tank: mono notch −6.4 dB on chord stabs, 2 Springs, DECAY 0 BOING 1;
// L/R correlation 0.47 → 0.49 on hits). At 0.025 all M4 checks keep their
// margin. KICKED keeps 0.12: a big smear is part of "full chaos".
// M8: Δa is now scaled per Spring by kJoltSpringScale (see above), and
// DRIVEN's is 0.015: the M8 detector gives stabs at the default SPLASH a
// full-strength Jolt, which at 0.025 left test_tank's mono-notch margin at
// −5.0 dB (3 Springs, chord stabs; margin −4.5). At 0.015: −3.9 dB.

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

// Modulation amplitude D in samples that gives a peak shift of c cents at
// WOBBLE w's rate and sine share (the formula above), sample rate fs.
inline float wobbleDepthForCents(float c, float w, float sampleRate, float rateScale)
{
    if (c <= 0.0f) return 0.0f;
    const float f = wobbleRateHz(w) * rateScale, ws = wobbleSineWeight(w);
    const float ratio = std::exp(c * (0.693147181f / 1200.0f)) - 1.0f;
    return ratio * sampleRate / (ws * 2.0f * map::kPi * f + (1.0f - ws) * 2.0f * kWobbleRandomRateRatio * f);
}
// Loop modulation amplitude D in samples for WOBBLE w at sample rate fs (and
// a Spring rate scale). 0 at w = 0.
inline float wobbleDepthSamples(float w, float sampleRate, float rateScale = 1.0f)
{
    return wobbleDepthForCents(wobbleCents(w), w, sampleRate, rateScale);
}

// ---- WOBBLE on the first echoes: the transport (M8, backlog item 4) ----------
// The Loop wobble above builds up over repeats (per pass × tail factor), so a
// drum's first echoes barely move: at DECAY noon the first echo carried only
// the tap's share (~half a pass, ~6 cents at WOBBLE 1), inaudible on a snare.
// Tape wobble moves the first echo too. So one more generator, the transport,
// shared by all Springs (like one tape transport feeding the tank, so the
// Springs' first echoes move together and never flange against each other),
// moves every Spring's pickup read. It is heard once (a read of the delay
// line, not inside the Loop), so it does not accumulate: the first echo
// wavers by c_e(w) cents at once and the tail adds the Loop's build-up on top.
//
// Early depth c_e(w), set by the zone edges of ADR 0008 (peak cents on the
// first echo, per zone edge; same rate and sine share as the Loop):
//   Drift      0 – 0.50:  0 → 2 cents, linear   (under the pitch JND: in tune)
//   transition 0.50–0.75: 2 → 14 cents, geometric (becoming audible)
//   Warble     0.75–1.00: 14 → 36 cents, geometric (a snare's echoes audibly
//                                     waver, hit to hit and echo to echo)
// An exponential curve through 0 can't be both this flat in the Drift zone
// and this steep in the transition, hence the three anchors. The slope
// changes at the zone edges, the value never jumps; WOBBLE glides anyway.
// Placeholders until the Magneto WOW & FLUTTER takes (ADR 0020) calibrate
// them: with kWobbleMaxCents above, these are all the WOBBLE depth numbers.
constexpr float kWobbleEarlyDriftCents  = 2.0f;  // at kDriftEnd
constexpr float kWobbleEarlyWarbleCents = 14.0f; // at kWarbleStart
constexpr float kWobbleEarlyMaxCents    = 36.0f; // at WOBBLE 1
inline float wobbleEarlyCents(float w)
{
    if (w <= 0.0f) return 0.0f;
    if (w <= kDriftEnd) return kWobbleEarlyDriftCents * w / kDriftEnd;
    if (w <= kWarbleStart)
        return map::expLerp(kWobbleEarlyDriftCents, kWobbleEarlyWarbleCents, (w - kDriftEnd) / (kWarbleStart - kDriftEnd));
    return map::expLerp(kWobbleEarlyWarbleCents, kWobbleEarlyMaxCents, std::fmin(1.0f, (w - kWarbleStart) / (1.0f - kWarbleStart)));
}
// The transport's rate ratio (unrelated to the Springs' 0.87 / 1.0 / 1.13).
constexpr float kWobbleTransportRate = 0.94f;
// Transport amplitude in samples (pickup read offset). 0 at w = 0.
inline float wobbleEarlyDepthSamples(float w, float sampleRate, float rateScale = kWobbleTransportRate)
{
    return wobbleDepthForCents(wobbleEarlyCents(w), w, sampleRate, rateScale);
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
