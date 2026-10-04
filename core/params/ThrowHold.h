#pragma once
// Throw and Hold (ADR 0039, 0040; CONTEXT.md "Throw", "Hold"): the numbers.
//
// ---- Throw (ADR 0039) ---------------------------------------------------------
// The throw is dub's main move: open the spring's send for one hit or one
// bar, close it, let the tail ring on. With the gate patched, the Springs'
// input (the send) is open only while the gate is high. MIX and the wet are
// never touched, so the tail always rings on after the send closes.
//
// Unpatched the Versio's gate reads low (there is no jack detection), so the
// throw switches itself on at the gate's FIRST rising edge after power-up
// and stays on until power-off (Tank::reset()). Until then the send is open
// and the Tank is bit for bit what it was before the throw existed.
//
// The send's gain is a ramp in front of everything that listens to the
// input: the Splash (a thrown snare splashes; a snare outside the throw
// doesn't), DriveIn, TONE's tilt and the Springs. The Kick (button, MIDI)
// is a knock on the tank, not the send: it is never gated.
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
// The gate's role depends on SPRINGS: position 3 will make it the echo's
// clock (another build, docs/research/dub-lens-critique.md §8 "Echo mode
// design"). Until that lands every position throws.

#include "params/DriveVoicing.h"

#include <algorithm>
#include <cmath>

namespace rv::throwhold {

constexpr float kThrowOpenSeconds  = 0.002f;
constexpr float kThrowCloseSeconds = 0.015f;

// Leaving throw mode (ADR 0039, owner 4 Oct 2026): unplugging the gate
// leaves it low, so the send would stay closed until power-off. Holding KICK
// this long switches throw mode off: the send glides open (the open ramp)
// and the latch clears; the next rising edge switches it on again. The Kick
// still fires on the press, as always. 1 s: long enough that no played Kick
// trips it, short enough to feel like a deliberate hold.
constexpr float kThrowExitHoldSeconds = 1.0f;
// The confirmation (only if throw mode was on): all four LEDs white this
// long, then the meters again (ADR 0039: the one exception to ADR 0031's
// meters-only LEDs).
constexpr float kThrowExitBlinkSeconds = 0.15f;

enum class GateRole : unsigned char {
    Throw, // positions 1 and 2 (and, for now, 3)
    Clock  // position 3, once the echo lands (not built)
};
// SPRINGS position (0, 1, 2) -> what the gate does there.
constexpr GateRole gateRole(int springsPosition)
{
    (void)springsPosition; // position 3 becomes GateRole::Clock with the echo build
    return GateRole::Throw;
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

// Ducking: while the input plays, the held wet dips and comes back between
// phrases. A peak follower on the dry input (mono), attack kDuckAttackSeconds,
// release kDuckReleaseSeconds; its level maps to the dip in dB: none below
// kDuckFloorDb, the full kDuckDepthDb from kDuckFullDb up (linear in dB in
// between), x bedWeight. Applied to the wet only, after the limiter.
constexpr float kDuckDepthDb       = 12.0f;
constexpr float kDuckAttackSeconds = 0.006f;
constexpr float kDuckReleaseSeconds = 0.30f;
constexpr float kDuckFloorDb       = -48.0f; // dBFS peak: below this the input isn't "playing"
constexpr float kDuckFullDb        = -30.0f;

// Voicings (owner picks by ear; Renderer key hold_voicing, the firmware
// compiles only the default):
// A "freeze": while held the Springs' input closes (nothing new gets in);
//   new hits are heard dry over the ducked bed. Leaving the zone reopens it.
// B "layer": new input still enters, kLayerSendDb down, and builds into the
//   held bed (bounded by the LoopSat, the Sustain trim and the limiter), ducked.
// With the throw on, an OPEN throw overrides the freeze: the gate is how new
// sound gets into a frozen bed (throw a chord into it).
constexpr int   kVoicingFreeze  = 0;
constexpr int   kVoicingLayer   = 1;
constexpr int   kNumVoicings    = 2;
constexpr int   kDefaultVoicing = kVoicingFreeze;
constexpr float kLayerSendDb    = -6.0f;

// The send's gain the Hold asks for (1 = open) at bed weight b.
inline float holdSend(int voicing, float b)
{
    const float closed = voicing == kVoicingLayer ? drive::dbToGain(kLayerSendDb) : 0.0f;
    return 1.0f + b * (closed - 1.0f);
}

} // namespace rv::throwhold
