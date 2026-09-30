#pragma once
// AntiRes numbers (SPEC §4.10, ADR 0010, docs/m6-metric-calibration.md).
// Layers 1 (even Loop gain), 3 (Spring detuning, SpringModes.h) and 5
// (LoopSat, DriveVoicing.h) live with the parts they shape. This file holds
// layer 2, the Micro-mod floor, and the Howl-zone movement that rides on the
// same modulation hook (ADR 0019).
//
// Layer 4 (the adaptive suppressor) is NOT built: the M6 grid passes the
// Ringing metric with layers 1-3 + 5 (ADR 0010, docs/m6-metric-calibration.md).
//
// ---- Micro-mod floor (layer 2) ---------------------------------------------
// Plain version: each Spring's tank length L drifts by a tiny, slow, random
// amount all the time, even with WOBBLE at 0, and each Spring drifts on its
// own. A Loop's resonances sit at the frequencies whose round trip is a
// whole number of cycles; if L never moves, those frequencies never move
// either, and any one that is slightly favoured can lock in and build up.
// Moving L keeps them moving. The drift is far too small and too slow to
// hear as pitch on a held chord (measured by test_antires: well under the
// ~5 cent pitch-change threshold).
//
// Shape: a new random target every kMicroModHoldSeconds, smoothed by two
// one-pole low-passes with that same time constant (so it glides, never
// steps: no zipper, no clicks). Scaled so its peaks are about +-1, then by
// the depth (a fraction of L). Seeded per Spring and reset by Tank::reset(),
// so renders stay deterministic.
//
// Independent drifts have a cost with 2 and 3 Springs (M8, after TENSION):
// where A's and B's modes line up, the drifts slide each pair in and out of
// phase, and a pair that slides into phase late in the tail can swell out
// of it as one note. WOBBLE does the same, harder. What keeps that quiet is
// the damping / decay spread between Springs (SpringModes.h "Detune"), not
// this floor; a floor shared by every Spring was tried and measured no
// better once the spread was in (it also leaves WOBBLE's drift untouched).
//
// WOBBLE (ADR 0008, M7) rides on top of this: the Tank's dsp::Wobble per
// Spring gives a per-sample offset in samples, added to the same Loop delay
// read (Spring::process()); the floor stays underneath at WOBBLE 0. (M6
// planned WOBBLE through modDepth/lfoDepth; M7 uses its own generator, which
// specifies depth in cents per pass, so these fields stay floor + Howl.)

namespace rv::antires {

constexpr float kMicroModDepth       = 0.0005f; // 0.05 % of L, peak
constexpr float kMicroModHoldSeconds = 0.6f;    // new random target this often (~0.2 Hz glide)
constexpr float kMicroModNorm        = 1.35f;   // smoothed-random peaks -> ~+-1 (measured, test_antires)
// TENSION (ADR 0026): a tight tank (L 33 ms) has its resonances ~30 Hz
// apart, three times wider than a loose one's; a drift that is a fixed
// fraction of L moves every resonance by the same few tenths of a Hz, so on
// a tight tank it moves them a much smaller share of their spacing, and at
// max DECAY (~270 round trips) one of them could outlive its neighbours
// (M6 grid: 4 of 90 tightest-tank cells at ringing_db 15-20). Below
// kMicroModRefLoopSeconds the depth grows as 1/L, so the drift in samples
// stays what it is at the reference tank.
constexpr float kMicroModRefLoopSeconds = 0.069f; // TENSION noon's L
inline float microModDepth(float loopDelaySeconds)
{
    return loopDelaySeconds >= kMicroModRefLoopSeconds ? kMicroModDepth
                                                       : kMicroModDepth * kMicroModRefLoopSeconds / loopDelaySeconds;
}

// ---- LoopSat quiet-tail fade (layer 5, M8) ---------------------------------
// Plain version: the Loop's saturator (LoopSat, DriveVoicing.h) bends even a
// quiet tail a tiny bit, and that bend makes faint overtones of the tail's
// strongest, longest-lived notes (3 x, and sums of three). Wherever one
// lands right on one of the Loop's own high resonances, that resonance is
// kept fed by the long-lived note underneath and dies at the note's pace
// instead of its own much faster one: one high pitch outliving everything
// around it (M6 grid, KICKED / 1 Spring / tightest TENSION / TONE 1:
// 3902 Hz = 3 x a 1301 Hz mode). The LoopSat is there for loud tails
// (squash, thickness, the Howl's ceiling), not quiet ones, so its blend
// fades out as the tail gets quiet: full above kLoopSatQuietDb +
// kLoopSatFadeDb, none at and below kLoopSatQuietDb (linear in power in
// between; the level is the wet mid's, the smoothed one the Splash rattle
// already uses, or the last control tick's if louder, so a hit into a quiet
// tank is bent at once). Control rate, no Loop redesign: the LoopSat's slope
// at rest is 1 whatever its blend. Details: docs/m8-tuning-backlog.md
// "Tight-tank ringing (M6 corner)" and "Tight-tank ringing: owner listen".
// The level is measured in the saturator's own terms, u = k x (k = the
// harder half's hardness; fixed per ATTITUDE since ADR 0033), so KICKED's
// harder curve keeps its bend on quieter tails than DRIVEN's.
// Floor -30 dB (owner, 30 Sep 2026: the fade was "very subtle"; gentler, so
// quiet skank tails keep more of the LoopSat's colour). At the floor (u RMS
// -30 dB, peaks ~0.1) the curve bends the signal by well under 1 %. Margin:
// the M6 corner sweeps pass with the floor anywhere from -20 to -30 dB and
// fail at -35 (proto/tight-ringing). Needed once the SPLASH noise burst is
// gone (ADR 0032): the burst had been masking the corner (23.5 dB without).
constexpr float kLoopSatQuietDb = -30.0f; // u = k x, RMS, dB
constexpr float kLoopSatFadeDb  = 12.0f;

// ---- Howl movement (ADR 0019: "rough, moving, never a steady sine") --------
// In the KICKED Howl zone the Loop self-oscillates, and a self-oscillating
// Loop settles on one resonance: without help it is a steady tone (the M6
// grid measured < 0.1 % pitch movement). So the Howl rides the same hook
// harder: a slow sine sweep of L plus a stronger random drift, both scaled by
// the Howl amount (SpringSettings::howl, 0 outside the zone). The pitch then
// wanders like a dub siren leaning on the feedback. Each Spring gets its own
// rate, so 2 and 3 Springs beat against each other.
constexpr float kHowlLfoDepth = 0.006f;  // sine: +-0.6 % of L at full Howl
constexpr float kHowlModDepth = 0.003f;  // extra random drift at full Howl
constexpr float kHowlLfoHz    = 0.35f;   // Spring A; B and C use the ratios below
constexpr float kHowlLfoRatio[3] = {1.0f, 1.27f, 0.83f};

} // namespace rv::antires
