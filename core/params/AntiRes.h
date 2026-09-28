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
// Hook for M7's WOBBLE (ADR 0008): SpringSettings::modDepth is the *total*
// random depth (floor + WOBBLE) and lfoDepth / lfoHz the sine part. At M6 the
// Tank sets modDepth = kMicroModDepth and the sine only in the Howl zone.
// WOBBLE adds on top; the floor stays underneath at WOBBLE 0.

namespace rv::antires {

constexpr float kMicroModDepth       = 0.0005f; // 0.05 % of L, peak
constexpr float kMicroModHoldSeconds = 0.6f;    // new random target this often (~0.2 Hz glide)
constexpr float kMicroModNorm        = 1.35f;   // smoothed-random peaks -> ~+-1 (measured, test_antires)

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
