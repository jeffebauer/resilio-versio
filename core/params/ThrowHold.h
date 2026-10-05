#pragma once
// Throw and Hold (ADR 0039, 0040, 0043; CONTEXT.md "Throw", "Hold"): the numbers.
//
// ---- Throw (ADR 0039) ---------------------------------------------------------
// The throw is dub's main move: open the spring's send for one hit or one
// bar, close it, let the tail ring on. Two things throw (ADR 0043): the gate
// and the button. In throw mode the Springs' input (the send) is open only
// while the gate is high OR the button is held. MIX and the wet are never
// touched, so the tail always rings on after the send closes.
//
// Unpatched the Versio's gate reads low (there is no jack detection), so
// throw mode switches itself on at the FIRST rising edge of the gate, or the
// first press of the button, and stays on until power-off (Tank::reset()) or
// the exit gesture below. Until then the send is open and the Tank is bit
// for bit what it was before the throw existed. So with nothing patched the
// button alone is a hand throw: press = send open, release = closed.
//
// The send's gain is a ramp in front of everything that listens to the
// input: the Splash (a thrown snare splashes; a snare outside the throw
// doesn't), DriveIn, TONE's tilt and the Springs.
//
// Ramp times. The springs smear the send's edges a lot: test_throw_hold's
// click check (a 0 dBFS sustained low chord thrown on and off 8 times,
// every ATTITUDE, the Renderer's click detector on the wet, MIX 1) reads 0
// down to 0.5 ms / 3 ms, and only a hard 1-sample gate reads 1 (KICKED).
// So the times are musical, with a margin:
// - open 2 ms: lands on the hit (a snare's crack is ~2-5 ms) without
//   rounding it off, and keeps the first echo's highs free of a tick.
// - close 15 ms: a sustained chord cut mid-cycle fades rather than snaps
//   (the tail's first echo replays the cut), short enough that the next
//   off-beat doesn't leak in after a one-hit throw.
// The ramp's shape is a smoothstep of a linear position (zero slope at both
// ends), so neither end has a corner.
//
// The gate's and the button's role depend on SPRINGS (gateRole below): in
// positions 1-2 they throw; in position 3 (echo mode, ADR 0041) the gate is
// the echo's clock and the button taps its tempo (ADR 0043).

#include "params/DriveVoicing.h"

#include <algorithm>
#include <cmath>

namespace rv::throwhold {

constexpr float kThrowOpenSeconds  = 0.002f;
constexpr float kThrowCloseSeconds = 0.015f;

// Leaving throw mode (ADR 0039; gesture ADR 0043, owner 5 Oct 2026):
// unplugging the gate leaves it low, so the send would stay closed until
// power-off. In positions 1-2 a double tap whose second press is held
// switches throw mode off: tap, tap, and keep the second press down for
// kThrowExitHoldSeconds. The send glides open (the open ramp) and the latch
// clears; the next rising edge or press switches it on again. Both taps
// throw like any press until then (the second press holds the send open
// while it counts, and after the exit the send simply stays open).
//
// No single press of any length may exit (the button is played: quick taps,
// slow taps, long held throws), so the gesture asks for all of:
// - the first tap is short: released within kExitTapMaxSeconds;
// - the second press comes within kExitGapSeconds of that release;
// - the first tap stands alone: no release in the kExitGapSeconds before
//   it, so a run of fast taps ending in a long throw is not a double tap;
// - the second press is held kThrowExitHoldSeconds.
// 0.35 s for the gap and the tap: a relaxed deliberate double tap (OS
// double-click defaults are 0.4-0.5 s; a quick one is 0.1-0.2 s) fits with
// room, while slow tapping (quarter notes at 85 bpm or slower, held half
// the beat, leave gaps of 0.35 s or more) never pairs up, and a run of
// faster taps fails the "stands alone" rule. 2 s: the owner's number, far
// longer than any played tap and still quick to do on purpose.
constexpr float kThrowExitHoldSeconds = 2.0f;
constexpr float kExitGapSeconds       = 0.35f;
constexpr float kExitTapMaxSeconds    = 0.35f;
// The confirmation (only if throw mode was on): all four LEDs white this
// long, then the meters again (ADR 0039: the one exception to ADR 0031's
// meters-only LEDs).
constexpr float kThrowExitBlinkSeconds = 0.15f;

enum class GateRole : unsigned char {
    Throw, // positions 1 and 2: the gate and the button throw
    Clock  // position 3 in echo mode (ADR 0041): the gate is the echo's clock (Tank::clock), the button taps it (ADR 0043)
};
// SPRINGS position (0, 1, 2) and echo mode (on: position 3 is the tape
// echo) -> what the gate and the button do there. As the clock the throw
// rests open (the send glides open, and follows the gate and the button
// again back in positions 1-2).
constexpr GateRole gateRole(int springsPosition, bool echoMode)
{
    return springsPosition == 2 && echoMode ? GateRole::Clock : GateRole::Throw;
}

// Smoothstep of a ramp position in [0, 1].
inline float smooth01(float p) { return p * p * (3.0f - 2.0f * p); }

// ---- Hold (ADR 0040) ----------------------------------------------------------
// CLEAN and DRIVEN, top ~10 % of DECAY (the same zone as KICKED's Howl,
// ADR 0002, which is unchanged): the tail becomes near-infinite, for dub
// techno breakdowns and siren beds, and is ducked under new input.
//   z = zone position 0..1 (DECAY 0.9 -> 1) x (CLEAN + DRIVEN Morph weight)
//
// T60: glided in from the plain DECAY curve (map::decayT60Seconds: 6.6 s at
// DECAY 0.9, 9 s at 1) toward kTopT60Seconds, in log time with a smoothstep
// weight, so DECAY stays continuous across the zone's edge:
//   T60 = plain^(1 - s) x kTopT60Seconds^s,  s = smoothstep(z)
// DECAY 0.9: 6.6 s (unchanged), 0.95: ~33 s, 0.97: ~84 s, 1: 240 s at the
// design points (a bed that holds through a 32-bar breakdown and still lets
// go if left alone for minutes). Heard at DECAY 1 (test_throw_hold, a hit
// thrown in): -7 dB over 5-20 s while the damping's highs go, then a
// steady ~-3 dB per 10 s (the Loop's peak capped by kPeakGain: ~190 s).
constexpr float kZoneStart     = drive::kHowlZoneStart;
constexpr float kTopT60Seconds = 240.0f;
inline float zone(float decay)
{
    const float z = (decay - kZoneStart) / (1.0f - kZoneStart);
    return z <= 0.0f ? 0.0f : (z >= 1.0f ? 1.0f : z);
}
inline float t60Weight(float z) { return smooth01(z); }
inline float t60Seconds(float plainT60, float z)
{
    const float s = t60Weight(z);
    return s <= 0.0f ? plainT60 : plainT60 * std::exp(s * std::log(kTopT60Seconds / plainT60));
}

// The Loop's small-signal peak gain P = g x max|H| while held. Outside the
// zone the Spring caps g at Spring::kMaxGain (0.995, ADR 0001); a long T60
// on a short tank needs more than that, so in the zone the cap moves (by
// the zone weight) to P = kPeakGain: always under 1 (never self-oscillates,
// unlike the Howl), with a margin for the gain between the design points:
// max|H| is read at the design points only, and the true peak between them
// sits up to ~0.13 % higher (test_drive's dense sweep read a per-trip 1.0005
// with 0.9992 here); 0.998 keeps it at ~0.9993.
// The high path (fast, undispersed echoes: a comb) keeps the plain DECAY's
// T60, so the held wash is the Loop's dispersed sound, not a metallic ring.
constexpr float kPeakGain = 0.998f;

// How far the zone's other parts are in (the freeze, the ducking, the
// layer's send): all the way from halfway into the zone (DECAY 0.95), so
// most of the zone is a held bed, eased in over its first half.
inline float bedWeight(float z) { return smooth01(std::min(1.0f, 2.0f * z)); }

// Ducking (round 3, owner 4 Oct 2026). The whole held bed dips, like a
// sidechain, but only the input's kick and bass make it dip: snares, hats,
// stabs and chords don't. (Round 2 misread "duck the lows" as ducking only
// the bed's lows; the springs' tail has little bass, so it barely sounded
// ducked. Round 1 ducked the bed on everything and its 300 ms return swelled
// back into each beat.)
// - Key: the dry input (mono) through a 4th-order low-pass at kDuckKeyHz
//   (two Butterworth biquads); a peak follower on it (attack
//   kDuckKeyAttackSeconds, release kDuckKeyReleaseSeconds). Its level maps
//   to the dip in dB: none below kDuckFloorDb, all of the depth from
//   kDuckFullDb up (linear in dB in between), x bedWeight. 120 Hz: kick
//   and bass fundamentals sit at 40-120 Hz and pass; a snare's body
//   (~180-250 Hz) is 15-28 dB down and its rattle, hats, stabs and chords
//   further, so with the floor at -24 dBFS they stay below it
//   (test_throw_hold: a -6 dBFS snare + hats move the bed 0.00 dB at 120 Hz,
//   0.12 dB with a 150 Hz key; 120 keeps the margin for hotter or deeper
//   snares while every kick and bass fundamental still passes).
// - The dip is smoothed on the control tick: falls fast
//   (kDuckAttackSeconds), holds kDuckHoldSeconds, then comes back on a short
//   kDuckReleaseSeconds curve, so it is back well before the next beat (a
//   dip, then flat) instead of still rising into it (round 1's hump).
// - The layer voicing's send dips with it, so kicks and bass don't pile into
//   the bed during the dip (it came back fuller each time: the other half of
//   the hump).
// Applied to the whole wet after the limiter (a gain, so no new peaks).
constexpr float kDuckKeyHz             = 120.0f;
constexpr float kDuckKeyAttackSeconds  = 0.001f;
constexpr float kDuckKeyReleaseSeconds = 0.030f;
constexpr float kDuckFloorDb           = -24.0f; // key (lows) peak dBFS: below this nothing is "playing"
constexpr float kDuckFullDb            = -12.0f;
constexpr float kDuckAttackSeconds     = 0.003f;
constexpr float kDuckHoldSeconds       = 0.020f;
constexpr float kDuckReleaseSeconds    = 0.035f; // time constant (to within 1 dB of 12: ~90 ms)
// Depth: voicing 0 (default) and 1 (deeper), Renderer key
// duck_voicing; 2 = no ducking (a reference for tests and pages). The
// firmware builds the default.
constexpr float kDuckDepthDb[3] = {12.0f, 18.0f, 0.0f};
constexpr int   kNumDuckVoicings = 3;

// Voicings (Renderer key hold_voicing, the firmware compiles only the
// default; the owner picked layer, 4 Oct 2026):
// "freeze": while held the Springs' input closes (nothing new gets in);
//   new hits are heard dry over the ducked bed. Leaving the zone reopens it.
// "layer" (default): new input still enters, kLayerSendDb down (and less
//   while the bed's lows are ducked), and builds into the held bed (bounded
//   by the LoopSat, the Sustain trim and the limiter).
// With the throw on, an OPEN throw overrides the freeze: the gate is how new
// sound gets into a frozen bed (throw a chord into it).
constexpr int   kVoicingFreeze  = 0;
constexpr int   kVoicingLayer   = 1;
constexpr int   kNumVoicings    = 2;
constexpr int   kDefaultVoicing = kVoicingLayer; // owner's pick, 4 Oct 2026 (C on the hold page)
constexpr float kLayerSendDb    = -6.0f;

// The send's gain the Hold asks for (1 = open) at bed weight b.
inline float holdSend(int voicing, float b)
{
    const float closed = voicing == kVoicingLayer ? drive::dbToGain(kLayerSendDb) : 0.0f;
    return 1.0f + b * (closed - 1.0f);
}

} // namespace rv::throwhold
