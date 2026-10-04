#pragma once
// SPRINGS position 3 = echo mode (ADR 0041; dub-lens critique direction D,
// docs/research/dub-lens-critique.md §3.4 / §6 D / §8 "Echo mode design").
// Every number for the echo lives here; tuning by ear edits this file.
//
// What it is: what nearly every dub rig had, a tape echo feeding a spring
// tank (Tubby's homemade tape delay, Perry's Space Echo, Sherwood, Pole,
// Echospace; Sylvan Morris tuned a tape loop to each tune's tempo). The
// owner picked the prototype's voicing B, "series" (branch proto/echo-
// springs, c2fb5b3), on 4 Oct 2026:
//
//   input -> tape echo -> the 2-Spring tank
//
// The tape's output (the dry hit plus its repeats) is what the springs hear,
// on the mono input before the Splash: every repeat lands in the tank as its
// own splash (the Clang, the Bite, DRIVE and TONE act on each one like a
// fresh hit). The feedback stays on the tape.
//
// The springs behind it are fixed (owner, 4 Oct): today's noon-TENSION tank
// (kSpringsTension) with one classic medium tail (kSpringsT60Seconds), like
// a Space Echo's built-in spring. Positions 1 and 2's Springs A and B play
// it, with position 2's mix and level; Spring C is never heard in echo mode
// and its audio doesn't run (Tank.h "SPRINGS switching").
//
// The panel in position 3 (owner, 4 Oct):
//   DECAY   = the echo's feedback (kFeedback*): 0 = one repeat; up to long
//             builds just short of runaway in CLEAN and DRIVEN. In KICKED the
//             top of the knob tips into a runaway (kKicked*), a dub
//             self-oscillation held by the tape's saturator and the output
//             limiter, which dies away naturally when DECAY comes back down.
//   TENSION = the echo time. Unclocked: free, kFreeLongSeconds (TENSION 0)
//             to kFreeShortSeconds (TENSION 1), log. Clocked: TENSION picks
//             one of kDivisionBeats. Turning TENSION up is tighter either
//             way (a tighter tank today, shorter repeats here).
//   gate    = a clock (one pulse = one beat, a quarter note; dsp/EchoClock.h).
//             Positions 1-2 keep the gate's own job there.
//   button  = the Kick, into the tank, as in every position.
//   WOBBLE  = moves the tape too (its own Transport-role generator, ADR 0034:
//             Drift left of noon = wow and flutter, Warble right = a sine,
//             still at noon), so each repeat wavers a little more than the one
//             before (it passes the head again).
//
// The tape (dsp/Echo.h): one delay line, read with linear interpolation; the
// playback head darkens every pass (2-pole low-pass, kHeadLpHz) and thins it
// (1-pole high-pass, kHeadHpHz: no bass in the reverb, Bovell); the record
// head loses the top (kRecLpHz) and saturates (softClip, kTapeDrive) what goes on the tape (input +
// feedback), so repeats grit up as they build and can never exceed
// 1 / kTapeDrive. A time change (TENSION, a new division, a new clock tempo,
// the clock lost) swoops like tape: the tape speed follows over
// kTimeGlideSeconds, at most kMaxSlew samples of delay per sample, so the
// repeats bend in pitch (a Space Echo's rate knob) and never click.

#include <cmath>

namespace rv::echo {

// ---- Echo time -------------------------------------------------------------------------------
// Longest echo time (the tape's length). 2 s at 48 kHz = 96,000 floats
// (375 KB, AXI SRAM on the Daisy: firmware/main.cpp).
constexpr float kMaxSeconds = 2.0f;
// Unclocked, TENSION 0 -> 1, log: 2 s, 0.4 s at noon, 80 ms.
constexpr float kFreeLongSeconds  = 2.0f;
constexpr float kFreeShortSeconds = 0.080f;
// Clocked: TENSION's seven zones, long (TENSION 0) -> short (TENSION 1), in
// beats (one gate pulse = a quarter note): 1/2, dotted 1/4, 1/4, dotted 1/8,
// 1/8, dotted 1/16, 1/16. A time over kMaxSeconds plays at half (and again,
// if need be): the tape is 2 s long.
constexpr int   kNumDivisions = 7;
constexpr float kDivisionBeats[kNumDivisions] = {2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 0.375f, 0.25f};
// A zone changes only once TENSION is this far (a share of a zone) into the
// next, so a knob or CV resting on a border doesn't flicker between two.
constexpr float kDivisionHysteresis = 0.15f;

// ---- The clock (dsp/EchoClock.h) -------------------------------------------------------------
// Tempo from the interval between rising edges: the median of the last three
// intervals; a new reading within kClockJitter of the tempo in use is ignored
// (a gate read once per audio block, 1 ms, is 0.15 % at 90 bpm). Intervals
// outside kClockMinBpm..kClockMaxBpm don't count (a shorter one is a bounce,
// ignored; a longer one starts a new count).
constexpr float kClockMinBpm = 30.0f;
constexpr float kClockMaxBpm = 300.0f;
constexpr float kClockJitter = 0.02f;
// Lost after kClockLostBeats beats without a pulse, at most kClockLostSeconds:
// the echo glides back to free time (a swoop, as any time change).
constexpr float kClockLostBeats   = 2.25f;
constexpr float kClockLostSeconds = 2.5f;

// ---- The swoop -------------------------------------------------------------------------------
// The tape speed follows a new time over this long (one-pole on the delay)
// and never faster than kMaxSlew samples of delay per sample (the repeats
// bend within x0.5 .. x1.5 in pitch while it moves).
constexpr float kTimeGlideSeconds = 0.30f;
constexpr float kMaxSlew          = 0.5f;

// ---- Feedback (DECAY) ------------------------------------------------------------------------
// CLEAN and DRIVEN: kFeedbackMax x DECAY^kFeedbackExp, kFeedbackNoon at noon
// (the prototype's page played 0.45). 0 = one repeat. The heads lose a little
// every pass (the high-pass and low-pass are under 1 everywhere, ~0.965 at
// their peak, ~700 Hz), so kFeedbackMax x 0.965 = 0.92 per pass at the top:
// a long build that still fades (about 15 passes per 10 dB at the peak).
constexpr float kFeedbackMax  = 0.95f;
constexpr float kFeedbackNoon = 0.5f;
// KICKED: above kKickedFrom the feedback rises (smoothstep) to kKickedTop at
// DECAY 1; a pass gains past ~1.04 (1 / the heads' peak), DECAY ~0.88: the
// top ~10 % of the knob is a runaway. The record head's softClip holds it
// (the tape never carries more than 1 / kTapeDrive), the output limiter
// holds the wet; backing DECAY off lets it die away like any repeats.
constexpr float kKickedFrom = 0.75f;
constexpr float kKickedTop  = 1.25f;

// ---- Heads -----------------------------------------------------------------------------------
constexpr float kHeadLpHz  = 3500.0f; // playback: 2-pole low-pass per pass (compounds: each repeat darker)
constexpr float kHeadLpQ   = 0.6f;    // under 0.707: no peak, the head never gains
constexpr float kHeadHpHz  = 140.0f;  // playback: 1-pole high-pass per pass
constexpr float kTapeDrive = 1.5f;    // record head: softClip(k u) / k, ceiling 1/k = 0.67
// Record head: a 2-pole low-pass before the saturation (a real head's high
// loss). The repeats are darker than this anyway (kHeadLpHz); it keeps a hot
// cymbal's highs from folding back down as inharmonic tones when the tape
// saturates (test_echo_mode "hothighs": a 0 dBFS 15 kHz tone's 3 kHz fold
// -48 dBFS without it).
constexpr float kRecLpHz = 6000.0f;
// WOBBLE on the tape: the Transport's first-echo depths (WobbleVoicing.h)
// times this, per pass.
constexpr float kWowScale = 1.0f;
// A tape is never perfectly steady: a slow wander of the delay (two sines,
// kFloorHz apart, kFloorMs deep) runs whatever WOBBLE does. About 5 cents at
// the most.
constexpr float kFloorMs  = 0.8f;
constexpr float kFloorHz1 = 0.43f, kFloorHz2 = 0.71f;
// Room on the tape past kMaxSeconds for WOBBLE's swing, the wander, the
// interpolation and the control grid (samples).
constexpr int kTapeMargin = 1024;

// ---- The springs behind it -------------------------------------------------------------------
// Today's noon tank, one classic medium tail: T60 1.7 s (DECAY ~0.46 on the
// springs' own scale; Mappings.h decayT60Seconds). Measured on a click it
// rings ~1.7-1.9 s (test_echo_mode "springs").
constexpr float kSpringsTension    = 0.5f;
constexpr float kSpringsT60Seconds = 1.7f;

// ---- Level -----------------------------------------------------------------------------------
// The wet's trim in echo mode (x position 2's): the repeats add ~1.5 dB at
// DECAY noon; position 3 within +-2 dB of 2 on hits and skank (K-weighted,
// test_echo_mode "level").
constexpr float kTrim = 0.84f;

// Tape floats a host must provide for sample rate fs (Tank::prepare).
constexpr unsigned tapeFloats(float fs) { return unsigned(kMaxSeconds * fs) + unsigned(kTapeMargin); }

// DECAY -> feedback, per ATTITUDE (the Tank blends them with the Morph).
// exp / log rather than powf: smaller in the firmware's flash.
inline float feedbackClean(float decay)
{
    if (decay <= 0.0f) return 0.0f;
    const float p = std::log(kFeedbackNoon / kFeedbackMax) / std::log(0.5f); // noon -> kFeedbackNoon
    return kFeedbackMax * std::exp(p * std::log(decay < 1.0f ? decay : 1.0f));
}
inline float feedbackKicked(float decay)
{
    const float c = feedbackClean(decay);
    float u = (decay - kKickedFrom) * (1.0f / (1.0f - kKickedFrom));
    if (u <= 0.0f) return c;
    u = u < 1.0f ? u : 1.0f;
    return c + u * u * (3.0f - 2.0f * u) * (kKickedTop - c);
}

} // namespace rv::echo
