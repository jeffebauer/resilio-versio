#pragma once
// SPLASH / WOBBLE voicing (SPEC §3 K3/K5, §4.5, §4.7; ADRs 0008, 0015,
// 0020, 0025, 0032, 0043; CONTEXT.md: Hit, Clang, Bite, Jolt, Splash, Drift,
// Warble, Micro-mod floor).
//
// One place for every number that shapes the M7 sound, so M8 tuning by ear
// edits constants here, not DSP code (same role as DriveVoicing.h). Pure
// constants + tiny mapping functions of Normalised values. No platform code,
// no powf (flash, Mappings.h): exp/log/sin only, and only at control rate.
//
// The DSP that uses these: dsp/Splash.h (HitDetector, HitEnvelope, Jolt),
// dsp/Wobble.h. How they hook into the Tank:
// docs/m7-integration.md (M7) and Tank.h (since ADR 0032).
//
// SPLASH comes from the hit itself (ADR 0032, owner 30 Sep 2026): a loud
// hit's own highs clang the springs harder (the Clang), and in DRIVEN /
// KICKED a short, cracking hit also bites the input transducer harder (the
// Bite). Nothing is added on a hit; the Jolt stays. Everything listens to
// the input after the INPUT gain (DRIVE, ADR 0033), before any saturation.
// (The Kick and its noise-burst crash, the Clatter, went in ADR 0043.)

#include "params/Mappings.h"
#include "dsp/SizeOpt.h"

#include <array>
#include <cmath>

namespace rv::splash {

// The M7 components step their control-rate logic on a fixed grid of this
// many samples, counted internally from reset(), so their output does not
// depend on the host block size. Equal to Tank::kControlInterval, so after
// integration the grids line up (static_assert there, docs/m7-integration.md).
constexpr int kControlInterval = 32;

// ---- Hit detector (SPEC §4.5 step 1) ------------------------------------------
// Two envelope followers on the mono input after the INPUT gain G (ADR 0033:
// x · G, before any saturation; until then it was the post-DriveIn signal,
// whose makeup took DRIVE's gain back out, so DRIVE could only dampen the
// splash), first high-passed at kDetectorHpHz: fast = peak follower on |x| (1 ms attack),
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
//       Hit ≈ 0.1: it barely triggers (test_m7_tank ghostGroove, measured
//       when hits still fired the Clatter: −22..−28 dB under its own bright
//       part, −41 dB under a backbeat's). A snare over a loud pad is judged against the pad.
// Reference d values (measured post-DriveIn at DRIVE 0.25, ≈ x · G at DRIVE
// 0 now): the −6 dBFS snare of 02_hits d ≈ 0.15-0.22, −12 dBFS 0.07-0.11,
// −18 dBFS 0.04-0.055. DRIVE raises them by G (+9.7 dB at noon, +24 at 1):
// a −24 dBFS send at DRIVE ~0.8 gets the Hit a −6 dBFS hit gets at DRIVE 0.
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

// ---- Hit envelope e: the Clang and the Bite (ADR 0032; SPLASH round 4) -----
// A second, faster pair of followers on the same high-passed x · G, per
// sample (round 4's detector, which the owner picked by ear):
//   fast   = peak follower on |x| (kEnvFastAttackMs, kEnvFastReleaseMs)
//   slow   = follower of fast (kEnvSlowAttackMs, kEnvSlowReleaseMs)
//   sudden = (fast − slow) / fast        level-free, 0 on steady sound
//   loud   = min(1, (fast / R)²),  R = max(kLoudRef, kLoudRel · P)
//   e      = min(1, SPLASH × sudden × loud)
// e lasts a hit's first ~10–25 ms, 0 on sustained sound, small on quiet
// (ghost) hits. kLoudRef sits on the input after the INPUT gain (ADR 0033:
// the floor scales with the gain), calibrated as the send-level study
// asked: a −6 dBFS DAW-level hit (high-passed peak ~0.45) reaches it at
// DRIVE 0 (a gentle splash there: e is up only at the very peak), and 12 /
// 18 dB of INPUT (DRIVE ~0.65 / ~0.8) put a −18 / −24 dBFS mixer send
// there. By DRIVE ~0.5 a DAW-level hit is fully loud through its attack,
// as on the owner's round-4 page. P = the Hit detector's program level
// (above): in a groove the loud reference rises with the backbeats
// (kLoudRel × P), so a ghost note between them stays small at any DRIVE;
// an isolated quiet hit after silence is judged against kLoudRef only, so
// with DRIVE up it comes up to full (a tank with its INPUT cranked),
// clanging with its own, quieter highs.
constexpr float kEnvFastAttackMs  = 0.3f;
constexpr float kEnvFastReleaseMs = 20.0f;
constexpr float kEnvSlowAttackMs  = 40.0f;
constexpr float kEnvSlowReleaseMs = 400.0f;
constexpr float kLoudRef          = 0.45f;
constexpr float kLoudRel          = 3.0f;
//
// The owner's round-4 picks (page at DRIVE 0.8), all at the "clear"
// strength: CLEAN hits C2, DRIVEN and KICKED hits T2, the skank C2 in every
// ATTITUDE; "C2 on longer sounds like the skank, and T2 on shorter
// impulses". So every loud hit clangs, in every ATTITUDE, and in DRIVEN /
// KICKED a short one also bites: with w = Voice::bite (0 CLEAN, 1 DRIVEN /
// KICKED, blended by the Morph),
//   Clang amount = Voice::clang × e
//   Bite amount  = kBiteGain × w · short × e
// A drum hit in DRIVEN / KICKED gets both (round 4's TC), a chord stab the
// Clang and little Bite, CLEAN the Clang. (Tried and dropped: the Bite
// replacing the Clang on drum hits, T2 alone as picked. Below DRIVE ~0.5 the
// transducer is hardly driven, so the Bite alone makes little grit and
// little splash: KICKED rimshots got +1.3 dB of crash at the default DRIVE.)
//
// Clang: the Springs' input gets its own highs (above kClangHz, a one-pole
// split) fed harder while it lasts:
//   springs in += Clang amount × (x − LP(x))
// so the splash is the hit's own sound, chirped and coloured by the tank and
// dying with the tail: brightness, no grit. Voice::clang 5 = round 4's C2,
// in every ATTITUDE for chord-like hits. KICKED's drum-like hits get more,
// Voice::clangShort 12 (round 4's C3 strength), blended by "short" (below):
// owner, 30 Sep, "a more even spread of intensity", KICKED clearly the most
// intense, and with the Bite taking more back, KICKED's dark transducer
// (5 kHz) left its rimshots little crash at C2. On chords C3 read "hotter
// and brighter than before" (KICKED skank, owner's second listen), so there
// it stays C2. CLEAN and DRIVEN: 5 for both.
constexpr float kClangHz = 2000.0f;
//
// Bite: a short, cracking hit is pushed harder into DriveIn (transducer +
// tape) and part of the push taken back after:
//   DriveIn(x × (1 + Bite amount)) ÷ (1 + Bite amount)^kBiteTakeBack
// so the hit gets grit and reaches the springs a little harder (a harder
// hit on a real tank). CLEAN has none (its transducer stays clean; round 4
// measured T in CLEAN as only a louder hit).
// Owner, after the build page (30 Sep 2026): DRIVEN / KICKED hits and skank
// "a bit hot/distorted". Round 4's T2 (x(1 + 4e), half the push heard) made
// DRIVEN drum hits +9.3 dB louder at DRIVE 1 (default SPLASH) against
// CLEAN's +6.9; now x(1 + 2.5e) with three quarters of the push taken back
// (inside DriveIn, so its makeup gives back only the saturators' squash):
// +7.5. (Taking all of it back left KICKED rimshots only +1.2 dB of crash at
// the default DRIVE, and the level the push lends a hit at DRIVE noon, gone
// once KICKED's transducer saturates, dips its splash between noon and 3/4.)
constexpr float kBiteGain     = 2.5f;
constexpr float kBiteTakeBack = 0.75f; // share of the push (in dB) taken back (was 0.5, round 4's T2)
// 1 / push^0.75 = 1 / (s·sqrt(s)), s = sqrt(push): two square roots, cheaper
// than exp/log on the M7 (per bitten sample only).
static_assert(kBiteTakeBack == 0.75f, "biteBack is written for a three-quarter take-back");
inline float biteBack(float push)
{
    const float s = std::sqrt(push);
    return 1.0f / (s * std::sqrt(s));
}
//
// DRIVE's top half (ADR 0032): past noon DRIVE saturates the input harder
// and pushes the pickups, and both squash the drips with everything else
// (on DAW-level hits and the skank, the splash measured +8 dB at noon and
// +5 dB at DRIVE 1 in KICKED: "DRIVE dampens SPLASH" again, from noon up).
// So above noon the Clang and the Bite grow with DRIVE, along driveCurve:
//   gain = (1 + kSplashDriveBoost × u) / (1 + kSplashDriveBoost × u(0.8)),
//   u = (driveCurve(DRIVE) − driveCurve(0.5)) / (1 − driveCurve(0.5)), >= 0
// normalised to 1 at DRIVE 0.8, where the owner picked C2 / T2 (round 4's
// page), so there they are as picked: 0.50 up to noon, 1.37 at 1 (1.75: the
// Bite now lends a hit less level, so the Clang has more to make up above
// noon; 1.0 before the owner's 30 Sep "less hot" listen).
// Then the splash never falls as DRIVE rises (test_m7_tank).
constexpr float kSplashDriveBoost = 1.75f;
constexpr float kSplashRefDrive   = 0.8f;
// dc = drive::driveCurve(DRIVE), dcNoon = driveCurve(0.5), dcRef = driveCurve(kSplashRefDrive).
inline float splashDriveGain(float dc, float dcNoon, float dcRef)
{
    const float u  = dc > dcNoon ? (dc - dcNoon) / (1.0f - dcNoon) : 0.0f;
    const float u0 = (dcRef - dcNoon) / (1.0f - dcNoon);
    return (1.0f + kSplashDriveBoost * u) / (1.0f + kSplashDriveBoost * u0);
}
//
// ---- SPLASH stronger (1 Oct 2026, branch proto/splash-stronger; ADR 0032
// amendment, Proposed) ------------------------------------------------------
// Owner, 1 Oct (plugin and module): "the splash knob feels very subtle".
// Asked what matters most: stronger at the top (turned up, unmistakable on
// any hit) and less tied to DRIVE (they often play with DRIVE fully down).
// Four voicings on one page (hidden Renderer key splash_voicing; the
// firmware and plugin use kDefaultVoicing):
//   0 today (reference).
//   1 stronger top: over the top quarter of SPLASH the Clang (and a little
//     of the Bite) grows, (1 + topClang · t²), t = (SPLASH − kTopStart) /
//     (1 − kTopStart), and lasts longer (held up to topHoldMs × t²): no step
//     at 3/4 (t² starts flat), the rest of the knob bit for bit today's.
//     The hold adds splash without adding peaks (the ceiling below caps the
//     level, so length is where the top's extra comes from on loud hits).
//   2 = 1, and DRIVE-free: the Clang / Bite gain is held at its DRIVE-0.8
//     value (1, the round-4 pick) at every DRIVE, and the detector's level
//     references (the Hit floor T and kLoudRef) follow the INPUT gain, so
//     the splash hears every hit as it would at DRIVE 0.8 (+18 dB of INPUT):
//     DRIVE 0 splashes like DRIVE 0.8. The relative parts (q · P, kLoudRel
//     · P) are level-free already, so ghost notes in a groove stay down.
//   3 = 2, bolder: a bigger top, a lower Clang split (more of the hit's
//     body rings the springs) and a held Clang at every SPLASH (it lets go
//     over holdMs instead of with the hit's first 10–25 ms): a longer,
//     more metallic splash.
// The Clang's ceiling (voicings 1-3): uncapped, the bigger top and the
// DRIVE-free splash on clean, bright hits pushed the output limiter hard
// (02_hits, SPLASH 1, MIX 1: up to 23.5 dB of pull, KICKED at DRIVE 0;
// today's worst there is 7.2, CLEAN at DRIVE 0.8). So the Tank follows the
// springs' input highs (x − LP(x), a peak follower with e's fast times) and
// caps the Clang where the added highs would pass clangCeil (× KICKED's
// pickup push, kCeilPushShare: KICKED at DRIVE 0.8 keeps its whole top,
// its limiter at 1.3 dB), and never below the Clang today's voicing would
// give (HitEnvelope computes it alongside), so no voicing splashes less than
// today anywhere: the cap only trims what a voicing adds. A first cap on the detector's
// level (before DRIVE's saturation) took KICKED's whole top away at DRIVE
// 0.8, where the limiter never needed it. 0.2: at SPLASH 1, MIX 1 the
// limiter pulls about as hard and as long as today's (backlog "SPLASH
// stronger"); 0.3-0.4 kept hits near today but held the skank down 0.5-2 s
// longer at DRIVE 0.8-1.
struct Strong {
    float topClang;  // extra Clang at SPLASH 1 (× (1 + topClang))
    float topBite;   // extra Bite at SPLASH 1
    float driveFree; // 1 = the splash ignores DRIVE (voicings 2, 3)
    float clangHz;   // the Clang's split (kClangHz today)
    float holdMs;    // the Clang lets go over this (1/e, ms); 0 = with e
    float topHoldMs; // + this much hold at SPLASH 1 (× t², the top quarter)
    float clangCeil; // the Clang's ceiling (see above); 0 = none
};
inline constexpr std::array<Strong, 4> kStrong{{
    //  top   bite  free  split     hold  +hold  ceil
    {  0.0f, 0.0f, 0.0f, kClangHz,  0.0f,  0.0f, 0.0f}, // 0 today
    {  2.0f, 0.5f, 0.0f, kClangHz,  0.0f, 40.0f, 0.2f}, // 1 stronger top
    {  2.0f, 0.5f, 1.0f, kClangHz,  0.0f, 40.0f, 0.2f}, // 2 stronger top, DRIVE-free
    {  3.0f, 0.5f, 1.0f, 1200.0f, 40.0f, 40.0f, 0.2f}, // 3 bolder
}};
constexpr int   kDefaultVoicing = 2; // owner pick, 2 Oct 2026: C (stronger top + DRIVE-free)
constexpr float kTopStart       = 0.75f;
// The Clang's ceiling rises with the pickups' push, by this share of it (in
// dB) per ATTITUDE (CLEAN, DRIVEN, KICKED; blended by the Morph): KICKED's
// pickups squash a big splash on their own (its limiter stays under 2 dB
// uncapped). DRIVEN's don't (a quarter of its push left the skank at DRIVE
// 0.8 limiting 8 dB for 2.3 s), CLEAN has none.
inline constexpr std::array<float, 3> kCeilPushShare{{0.0f, 0.0f, 1.0f}};
// DRIVE-free voicings: the level references ease from today's (SPLASH 0:
// the Jolt floor, bit for bit today's) to DRIVE 0.8's by this SPLASH.
constexpr float kFreeRampSplash = 0.1f;
constexpr const Strong& strong(int v) { return kStrong[size_t(v < 0 ? 0 : (v > 3 ? 3 : v))]; }
// The firmware plays kDefaultVoicing only and, while that is 0 (today), the
// voicing code compiles out of it (flash: release 126.5 of 128 KB). The
// Renderer, the plugin and the tests build every voicing.
#if defined(RV_MODE_RELEASE) || defined(RV_MODE_PROFILE) || defined(RV_MODE_M0TEST)
constexpr bool kVoicingsBuilt = kDefaultVoicing != 0;
#else
constexpr bool kVoicingsBuilt = true;
#endif
// (1 + amount · t²) over the top quarter, 1 below.
inline float topBoost(float splash, float amount)
{
    const float t = splash > kTopStart ? (splash - kTopStart) * (1.0f / (1.0f - kTopStart)) : 0.0f;
    return 1.0f + amount * t * t;
}
//
// "Short" (0..1): how much of the hit is crack rather than notes, the share
// of its high-passed peak envelope above kClangHz (a peak follower on the
// highs over the fast follower), mapped kShortLo -> 0 .. kShortHi -> 1.
// Owner: "C2 on longer sounds like the skank, and T2 on shorter impulses".
// Why this measure and not the decay or the crest: on the stimuli the owner
// judged, a skank stab decays *faster* than the snare (its level 20-40 ms
// after the onset -6 dB re the first 10 ms; the snare's -4 dB) and the
// onset crest is the same (2.4 vs 2.5), so neither can tell them apart. What
// does is what the hit is made of: a drum's crack is noise, mostly above
// 2 kHz (share: snare 0.62-0.72, rim 0.53-0.58, a click 0.96), a chord's
// attack is its notes (skank 0.21-0.27). So drum hits get the full bite,
// chord stabs the clang and no bite; a bright guitar chop in between gets
// part of it. Cheap: one follower and a multiply (the divide is sudden's).
constexpr float kShortLo = 0.35f;
constexpr float kShortHi = 0.55f;

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

// ---- Impact timing ------------------------------------------------------------------
// Each stroke fires one impact (its Jolt) after a seeded timing jitter of
// kJitterMin..MaxMs from the Hit's peak, with the peak Hit as its strength.
// (Until ADR 0043 the Kick's crash, the Clatter, lived here too: a burst of
// sparse band-passed knocks into every Spring, plus rattle impacts. Its
// history is in ADR 0016 and git.)
// The jitter counts from the Hit's peak (the countdown restarts while the
// stroke is still growing, for at most kMaxRiseMs), so the impact (since
// ADR 0032 a hit's Jolt) takes the stroke's full strength (M8; before, a
// short jitter could fire on a hit's first millisecond with a fraction of
// its strength).
constexpr float kMaxRiseMs    = 2.0f;
constexpr float kJitterMinMs  = 0.3f;
constexpr float kJitterMaxMs  = 3.0f;
// ---- Jolt (SPEC §4.5 step 3) ------------------------------------------------------
// Envelope j (0..1): at an impact its target jumps to strength × jolt amount
// and decays over joltDecayMs; j follows the target with a kJoltAttackMs
// one-pole, so the lurch has a rise (the tank stretches: L grows, pitch
// dips) and a slow return (pitch slightly sharp while L shrinks back).
// Outputs: Loop delay offset = j × joltLoopFrac × L, allpass offset
// Δa = kChirpSign × j × joltAllpass (larger |a| = more dispersion: chirp
// smear; −j × joltAllpass for the LowsLater Chirp, Mappings.h).
// 18 ms: the fastest lurch (KICKED, SPLASH 1, L = 108 ms) stays under the
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
// has no tail rattle).
constexpr float kTankLevelSmoothMs = 50.0f;
// Hard ceiling on |a| after the Jolt is added, so a Jolt can never push the
// allpass toward |a| = 1. With the tuned HighsLater Chirp |a| tops out at
// 0.55 × 1.07 + 0.12 = 0.71 (loosest TENSION, KICKED Jolt, most-detuned Spring),
// so the clamp never bites; it guards any future retune of a.
constexpr float kMaxAllpassMagnitude = 0.85f;

// ---- Per-ATTITUDE table (SPEC §4.5) -----------------------------------------------
// Blended by the ATTITUDE Morph weights exactly like drive::Voice.
struct Voice {
    float clang;          // Clang: highs fed into the springs at e = 1 (ADR 0032), chord-like hits (short 0)
    float clangShort;     // the same for drum-like hits (short 1); blended by "short" in between
    float bite;           // Bite weight 0..1: short hits bite (kBiteGain) instead of clanging (DRIVEN, KICKED)
    float joltFloor;      // Jolt amount at SPLASH 0
    float joltMax;        // at SPLASH 1
    float joltDecayMs;    // Jolt target decay (1/e)
    float joltLoopFrac;   // Loop delay offset at j = 1, fraction of L
    float joltAllpass;    // |Δa| at j = 1
    float rattleDepth;    // KICKED rattle, fraction of L at full rattle
};

inline constexpr std::array<Voice, 3> kVoice{{
    //  clang  clSh   bite  jolt0  jolt1  jDec    jL      jA     rattle
    {  5.0f,  5.0f, 0.0f, 0.00f, 0.50f,  60.0f, 0.002f, 0.005f, 0.0000f}, // CLEAN
    {  5.0f,  5.0f, 1.0f, 0.10f, 0.50f,  90.0f, 0.006f, 0.015f, 0.0000f}, // DRIVEN
    {  5.0f, 12.0f, 1.0f, 0.20f, 1.00f, 180.0f, 0.011f, 0.12f, 0.0015f}, // KICKED
}};

// SPLASH 0 has no Clang and no Bite in any ATTITUDE (M8 round 2 took the
// old noise burst's "faint natural splash" floor out: the owner heard it as
// a click on hard hits). The Jolt floor stays: a slight pitch lurch, no
// transient.

// CLEAN (ADR 0025; ADR 0032): a real splash on hard hits, the same Clang as
// DRIVEN (the owner picked round 4's C2 for CLEAN too; KICKED's is bigger) with no Bite
// and a tiny Jolt (Loop and allpass lurch a third of DRIVEN's): the hit's
// own sparkle, no grit. Nothing at SPLASH 0: CLEAN stays hi-fi unless asked.

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

RV_SIZE_OPT inline Voice blendVoice(const std::array<float, 3>& w)
{
    Voice v{};
    auto mix = [&](float Voice::*m) {
        v.*m = w[0] * (kVoice[0].*m) + w[1] * (kVoice[1].*m) + w[2] * (kVoice[2].*m);
    };
    mix(&Voice::clang);
    mix(&Voice::clangShort);
    mix(&Voice::bite);
    mix(&Voice::joltFloor);
    mix(&Voice::joltMax);
    mix(&Voice::joltDecayMs);
    mix(&Voice::joltLoopFrac);
    mix(&Voice::joltAllpass);
    mix(&Voice::rattleDepth);
    return v;
}

// Jolt amount for SPLASH v: floor at 0, max at 1, linear between (the Hit
// curve is already steep; a linear amount keeps the knob even). (SPLASH
// scales the Clang and the Bite through e.)
inline float joltAmount(const Voice& vc, float splash) { return vc.joltFloor + (vc.joltMax - vc.joltFloor) * splash; }

// ---- WOBBLE (SPEC §4.7, ADR 0008, 0020) ---------------------------------------------
// UNUSED since ADR 0034 (bipolar WOBBLE): see params/WobbleVoicing.h. Delete this section at merge.
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
// (Those G values are from the DECAY-sized tank before TENSION; since ADR
// 0026 L comes from TENSION, 69 ms at noon at every DECAY, so at DECAY max
// the tail now holds more trips per second and multiplies a little more:
// whole-Tank Drift at DECAY max went 5.9 cents with c(0.5) = 0.75.)
//
// Depth curve (ADR 0008, exponential-ish): c(w) = cMax (e^{k w} − 1)/(e^k − 1),
// cMax = 12 cents per pass, k chosen so c(0.5) = 0.6: 1/(e^{k/2} + 1) =
// 0.6/12 → k = 5.89 (0.75 / 5.42 before TENSION). Per pass → heard in the
// tail at DECAY noon (×G):
//   Drift      0 – 0.50:  0 → 0.6 cents  → ≲ 5 cents at every DECAY (felt;
//                                          slow drift under the pitch JND:
//                                          held chords in tune)
//   transition 0.50–0.75: 0.6 → 2.7 cents → 5 → ~15 cents (becoming audible)
//   Warble     0.75–1.00: 3.0 → 12 cents  → ~15 → ~50 cents (worn tape: clearly
//                                          out of tune on held chords)
// As a fraction of L at WOBBLE 1 (D ≈ 41 samples at 48 kHz): 2.8 % at DECAY 0,
// 1.5 % at noon, 0.85 % at DECAY max.
// Placeholder cMax until the Magneto WOW & FLUTTER takes (ADR 0020) calibrate
// it on 08_held_tones; per-DECAY numbers: docs/m7-integration.md.
constexpr float kWobbleMaxCents  = 12.0f;
constexpr float kWobbleCurve     = 5.89f;
constexpr float kDriftEnd        = 0.50f;
constexpr float kWarbleStart     = 0.75f;
inline float wobbleCents(float w)
{
    if (w <= 0.0f) return 0.0f;
    return kWobbleMaxCents * (std::exp(kWobbleCurve * w) - 1.0f) / (std::exp(kWobbleCurve) - 1.0f);
}
// Long DECAYs (ADR 0033 re-check, 30 Sep 2026): above noon the tail
// multiplies the Loop wobble more than the zones above were set for (a held
// tone sits longer in the Loop), and whole-Tank Drift at DECAY 1, WOBBLE 0.5
// read 6.6-6.8 cents p95 on 08_held_tones (CLEAN at any DRIVE, DRIVEN at
// DRIVE 0), over ADR 0008's 5 for "held chords in tune". DRIVEN at the
// default DRIVE read 3.9 only because ADR 0022's LoopSat push compressed the
// held tone, and ADR 0033 removed that push. So above DECAY noon the Loop
// depth eases down to kWobbleDecayMaxScale at DECAY 1 (the transport, heard
// once, is not scaled). Warble at DECAY 1 stays clearly out of tune.
constexpr float kWobbleDecayMaxScale = 0.65f;
inline float wobbleDecayScale(float decay)
{
    return decay <= 0.5f ? 1.0f : map::expLerp(1.0f, kWobbleDecayMaxScale, std::fmin(1.0f, 2.0f * decay - 1.0f));
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
