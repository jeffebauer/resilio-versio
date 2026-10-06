#pragma once
// Drive chain + ATTITUDE + TONE tilt voicing (SPEC §3 K1/K4/SW1, §4.5, §4.9;
// ADRs 0002, 0003, 0014, 0015, 0017, 0018, 0019; docs/m5-contracts.md).
//
// One place for every number that shapes the M5 sound, so M8 tuning by ear
// (and M3 CPU profiling) edits constants here, not DSP code. Pure constants
// + tiny helpers, no platform code.
//
// Signal chain these numbers feed (Tank.h has the full diagram):
//
//   mono in ─ DriveIn ─ Tilt ─ Springs (each with a LoopSat in its Loop) ─ mix ─ DriveOut ─ shelf ─ limiter
//
//   DriveIn  = input transducer (band-limit -> soft asymmetric saturator)
//              -> tape (pre-emphasis -> soft saturator -> de-emphasis -> HF smear)
//   Tilt     = TONE's pre-tank tilt EQ
//   LoopSat  = saturator inside each Spring's feedback path
//   DriveOut = output pickup (light soft clip -> band-limit)
//
// Level convention: 0 dBFS (|x| = 1) ≈ 10 Vpp, a hot but typical modular
// signal (ADR 0014 "typical ~10 Vpp").

#include "params/Mappings.h"
#include "dsp/SizeOpt.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rv::drive {

// ---- Oversampling (SPEC §4.9, §5 mitigation 4) ------------------------------
// Every nonlinear stage (transducer + tape, each LoopSat, DriveOut) runs at
// kOversampleFactor x the sample rate between a polyphase IIR halfband
// up-sampler and down-sampler (core/dsp/Oversampler.h). This is THE knob M3
// profiling turns: 1 (off, cheapest, aliases), 2 (default), or 4.
// Why x2 is enough here: every saturator sits after a band-limit (the
// transducer LPF, the Loop's own low-passes), so the harmonics it makes are
// small by the time they could fold back past the doubled Nyquist.
constexpr int kOversampleFactor = 2;
static_assert(kOversampleFactor == 1 || kOversampleFactor == 2 || kOversampleFactor == 4,
              "oversampling factor must be 1, 2 or 4");

// ---- ATTITUDE Morph (ADR 0003) ------------------------------------------------
// A switch flip glides every attitude-dependent number from the old voicing
// to the new one over this time, on the live tail. Never stepped.
constexpr float kMorphSeconds = 0.040f; // M8: 30 -> 40 ms, so a flip into KICKED eases its pushed pickup in without a click

// ---- Per-ATTITUDE voicing (SPEC §4.9 table) ---------------------------------
// Blended linearly between attitudes during a Morph (weights sum to 1).
//
// Saturator shape (Drive.h): sat(x) = S(k·x)/k with S a smooth tanh-like
// curve (slope 1 at 0, never above 1, flat at |k·x| = 3). So k is
// "hardness": the curve starts bending around |x| ≈ 0.5/k and flattens at
// |x| = 3/k. Different k for the positive and negative half makes it
// asymmetric: one side flattens earlier, which adds even harmonics (the
// "valve-ish", "magnetic" colour) and a little DC (removed downstream).
struct Voice {
    // Input transducer (DriveIn)
    float bandHpHz;    // band-limit high-pass: LF loss of a spring driver coil
    float bandLpHz;    // band-limit low-pass (2 x 2nd order = 24 dB/oct): driver HF loss
    float transKPos;   // saturator hardness, positive half
    float transKNeg;   // negative half (different = asymmetric)
    float fluxCutDb;   // HF cut into the saturator (restored after it): see kFluxHz
    // DRIVE -> gain into the saturators, dB at DRIVE 0 and DRIVE 1 (driveCurve
    // in between): the INPUT gain G (below, the same in every ATTITUDE) plus
    // this ATTITUDE's voicing offset, i.e. how hot its transducer and tape
    // run for a given INPUT (ADR 0033). Automatic gain compensation (Drive.h)
    // takes all of it back, so these set colour; the loudness change is G's
    // heard share (kInputHeard), the same in every ATTITUDE.
    float driveDbMin;
    float driveDbMax;
    // Tape (DriveIn, after the transducer)
    float tapeAmount;  // 0 = tape off (CLEAN), 1 = on
    float tapeK;       // tape saturator hardness (symmetric)
    float preEmphDb;   // pre-emphasis high-shelf boost before the tape saturator
    float smearHzMax;  // HF smear low-pass at DRIVE 1 (opens to kSmearOpenHz at DRIVE 0)
    // Loop saturation (per Spring, inside the feedback path)
    float loopAmount;  // 0 = off (CLEAN), 1 = on
    float loopKPos;
    float loopKNeg;
    // DRIVE also pushes the output pickup, the stage *after* the springs
    // (ADR 0022: the springs smear the input's distortion into a dark tail,
    // so the colour has to be made where you hear it too):
    //   outDriveDb: how much harder the pickup (DriveOut) bites at DRIVE 1
    //     (its hardness k is raised by this many dB along pushCurve). It
    //     sits after the springs: its grit is heard directly, not smeared.
    // The LoopSat is no longer pushed (ADR 0033, owner 30 Sep 2026): pushed,
    // it squashed the tail on every round trip and the Loop's damping ate the
    // harmonics it made, so DRIVE up made KICKED's tail *shorter* (-15.4 ->
    // -19.4 dB at 0.6 s after a hit, DECAY 0.6), against "driving the tank
    // harder". It keeps its DRIVE-0 hardness (loopKPos / loopKNeg).
    float outDriveDb;
    //   outFluxOpenDb (M8): the pickup's flux cut (kOutFluxDb: highs cut
    //     going into its saturator, restored after) shrinks by this many dB
    //     at DRIVE 1 (along pushCurve), so a pushed pickup grits the mids
    //     and highs of the finished tail too, not only its lows. That is
    //     what makes a bright one-shot (a rimshot's tail has little below
    //     800 Hz) sound driven. Aliasing stays <= -60 dB (test_drive).
    float outFluxOpenDb;
    //   outAmount0 (M8): the pickup's saturation is blended in parallel,
    //     y = x + a·(sat(x) − x), a rising from outAmount0 at DRIVE 0 to 1
    //     at DRIVE 1 along outAmountCurve (below). A saturator alone changes
    //     the sound mostly at the top of its range (its distortion grows
    //     ~2 dB per dB of drive), so CLEAN, whose only post-tank stage this
    //     is, had dead patches (0–0.4 and 0.5–1, docs/m8-sweetspot.md). The
    //     blend adds tint evenly across the knob: CLEAN 0 = hi-fi pickup,
    //     CLEAN 1 = a gentle, audible transducer tint. DRIVEN/KICKED: 1
    //     (always full, as before).
    float outAmount0;
    // Output pickup (DriveOut)
    float outK;        // soft-clip hardness at DRIVE 0 (light: only bends near full scale)
    float outAsym;     // negative-half hardness = outK * (1 + outAsym)
    float outLpHz;     // pickup band-limit low-pass
    // Wet makeup at DRIVE 1, dB (along pushCurve²): gives back, at once, the
    // level the pushed pickups squash out of the tail. Their automatic makeup
    // does too, but slowly (kAutoMakeupSeconds) and only up to
    // kAutoMakeupMax, so without this a KICKED tail came back ~1.5 dB quiet at
    // DRIVE up and dipped when the Morph flipped into KICKED. (Until ADR 0033
    // it gave back the pushed LoopSat's squash; same numbers, same job.)
    float wetMakeupDb;
    // Level trim so the three ATTITUDEs are equally loud (SPEC §7 M5 ±2 dB).
    float trimDb;
};

// CLEAN: hi-fi, a light transducer tint. Wide band-limit, soft saturator
//        that barely bends at normal levels, no tape, no Loop saturation;
//        DRIVE only warms the pickups a little (a gentle tint at max).
// DRIVEN: the default dub sound. Mid-forward band-limit, medium transducer,
//        tape on (rounded peaks, gentle compression, HF smear with DRIVE),
//        gentle symmetric Loop saturation (tails thicken as they build).
//        DRIVE up: warm, clearly saturated tape; the near-symmetric pickup
//        rounds and squashes the whole tail.
// KICKED: on the edge. Narrow band-limit, hard asymmetric transducer, hot
//        tape, hard asymmetric Loop saturation (and Howl, see below).
//        DRIVE max: a cranked tank (ADR 0022: Wellspring INPUT with the clip
//        light solid, Magneto REC LVL red): thick, squashed, gritty, the
//        asymmetric curves adding even harmonics; still a spring.
// ADR 0022 retune (28 Sep 2026): earlier pre-gain curve, DRIVEN top +5 dB,
// CLEAN +3 dB; the push column (oDrv); KICKED's range shifted down 2 dB
// (-11 dB, 9 o'clock stays clean-ish with the earlier curve) and its top kept
// at +20 dB (aliasing at 10 Vpp, see above). ADR 0033 (30 Sep 2026): the
// LoopSat push (lDrv 24 / 22) is gone; KICKED's pickup push 26 -> 28 dB takes
// back part of the grit it gave at DRIVE 1 (level-matched 0 vs 1 null -6.4
// -> -5.3 dB, ADR 0022 bar -6); dB0 / dB1 now read as INPUT + offset.
// Owner, after the build page (30 Sep 2026): DRIVEN "a bit hot/distorted",
// wanted "a more even spread of intensity across clean/drive/kicked". So
// DRIVEN drives less at the top: dB0 / dB1 -6 / 16 -> -5 / 13, oDrv 24 -> 21,
// and it now sits about midway between CLEAN and KICKED (level-matched null
// vs DRIVE 0 on 02_hits at DRIVE 0.5 / 0.8: CLEAN -23.8 / -19.9, DRIVEN
// -19.1 / -13.5 (was -16.8 / -11.0), KICKED -11.2 / -6.8). KICKED's wet
// makeup 0.8 -> 0.4 dB (its hits no longer run ahead of CLEAN's level).
inline constexpr std::array<Voice, 3> kVoice{{
    //  hp      lp       tK+    tK-    flux   dB0     dB1    tape  tapeK  emph   smear    loop  lK+    lK-    oDrv   oFlx  oAm0   oK     oAs    oLp      wMk   trim
    {  45.0f, 11000.f, 0.30f, 0.38f,  6.0f,  -6.0f, 12.0f, 0.0f, 0.60f, 5.0f,  9000.f, 0.0f, 0.60f, 0.60f,  0.0f,  6.0f, 0.0f, 4.00f, 0.15f, 15000.f,  0.0f, 0.0f}, // CLEAN (outK 4.5 -> 4.0 at the M8 merge: keeps CLEAN mild, 0 vs 1 <= -15 dB)
    {  85.0f,  6500.f, 0.45f, 0.60f,  9.0f,  -5.0f, 13.0f, 1.0f, 0.85f, 3.0f,  6000.f, 1.0f, 0.70f, 0.70f, 21.0f,  6.0f, 1.0f, 0.55f, 0.20f, 11000.f,  0.0f, 0.0f}, // DRIVEN (emph 5 -> 3 at TENSION: aliasing, see below)
    { 130.0f,  5000.f, 0.80f, 1.40f, 15.0f, -11.0f, 20.0f, 1.0f, 1.00f, 4.0f,  4500.f, 1.0f, 1.60f, 2.60f, 28.0f,  9.0f, 1.0f, 0.60f, 0.50f,  8500.f,  0.4f, 0.0f}, // KICKED
}};

// Magnetic transducer (DriveIn): a driver coil saturates on magnetic flux,
// and flux is the *integral* of the drive voltage, so a coil saturates hard
// on lows and hardly at all on highs. Modelled by a shelf that cuts highs by
// fluxCutDb above kFluxHz before the saturator and its exact inverse after
// it: small signals pass unchanged, big lows saturate, highs stay cleaner.
// Musically: warm, not fizzy. Technically: it keeps high-frequency content
// from being clipped into a near-square wave whose high harmonics would
// fold back (alias) even at x2 oversampling.
constexpr float kFluxHz = 400.0f;

// Pre-emphasis / de-emphasis shelf corner (tape).
// DRIVEN's pre-emphasis (Voice::preEmphDb) is 3 dB, not 5 (ADR 0026
// re-check): at DRIVE 1 a 0 dBFS 5 kHz tone got through DRIVEN's 6.5 kHz
// band-limit hot enough for the tape curve to square it off, and its 19th
// harmonic (95 kHz at the doubled rate) folded back to 1 kHz. At TENSION 1
// (fC 2.7 kHz) the Loop keeps ringing at 1 kHz but passes little of the
// 5 kHz tone, so that fold-back read -54 dB re the tone (test_drive, limit
// -60). 3 dB: -66 dB. The emphasis only decides how hard highs above
// ~3 kHz hit the tape (the de-emphasis restores them), and the springs
// darken those anyway: DRIVEN's DRIVE nulls on 02_hits move <= 0.2 dB.
constexpr float kPreEmphHz = 3000.0f;
// HF smear low-pass at DRIVE 0 (effectively open).
constexpr float kSmearOpenHz = 16000.0f;
// Output pickup flux shelf (DriveOut): highs cut by kOutFluxDb above
// kOutFluxHz into its saturator and restored after (same idea as kFluxHz).
constexpr float kOutFluxHz = 800.0f;
constexpr float kOutFluxDb = 18.0f;
// LoopSat flux shelf (M8, HighsLater re-tune; Drive.h LoopSat): highs cut
// by kLoopFluxDb above kLoopFluxHz into each Loop's saturator and restored
// after. The corner sits in the upper part of the Loop's band (fC is
// 2.9-4.4 kHz), so the body of a tail squashes as before (DRIVE nulls on
// 02_hits unchanged to 0.1 dB) while the top saturates less: no folded-back
// harmonics from loud highs at DRIVE 1, and a pushed Loop full of
// broadband noise loses less level (test_drive "steady noise").
constexpr float kLoopFluxHz = 2000.0f;
constexpr float kLoopFluxDb = 12.0f;
// Automatic makeup (DriveIn and DriveOut, see "Automatic gain compensation"
// below): averaging time of the level followers (slow enough not to pump
// on single hits), and the most DriveOut may add (a squashed Howl or a
// very hot tail gets no more, so the Howl stays clear of the limiter).
constexpr float kAutoMakeupSeconds = 0.3f;
constexpr float kAutoMakeupMax     = 2.0f; // linear, +6 dB
// ---- Excitation trim (M8 gain staging, docs/m8-sweetspot.md) ----------------
// The Tank resonates in a band (wet/dry energy per half octave, DECAY noon:
// ~+7 dB from 140 Hz to 1.6 kHz, −1.5 dB at 2.2 kHz re that, −11 dB at
// 4.5 kHz, −6 dB at 70 Hz), so material with its energy in that band came
// back 5–6 dB louder than broadband or bright material (04_skank +8.2 dB
// wet − dry vs pink noise +2.8 dB). The Tank follows the input's power in
// that band (weighting: two one-pole high-passes at kExcHpHz and two
// one-pole low-passes at kExcLpHz, i.e. -3 dB at ~90 Hz and ~2.4 kHz,
// 12 dB/oct each side: cheap, and close to the Tank's own response) and
// its full power, both over kExcSeconds (the auto-makeup time), and trims
// the tank input by
//   trim = (kExcRefShare · broad / band)^(kExcStrength / 2), within ±kExcMaxDb
// so the share of the input the springs can "hear" no longer sets how loud
// they come back. kExcRefShare = the band share of 02_hits (trim ≈ 0 dB on
// it), so the drum calibration (M5, M7) stays where it was.
// It trims the *input* to the springs (after Tilt, before the Loops), never
// the wet: a trim change only reaches new sound, so it cannot pump a tail
// that is already ringing, and it does not touch DECAY's tail length or the
// Howl (which the Loop sets on its own). Below kExcGateDb (broad level,
// silence, a tail ringing out) the trim holds, so it never drifts in a gap.
// The Splash listens before it (Hit does not depend on the trim).
// First-hit fix (owner, hardware, 30 Sep 2026: "the first stab showed the
// output LEDs red"): the trim starts at -kExcMaxDb after power-up and reset.
// It used to start at 0 dB, and a skank's first chord (which wants -3.2 dB)
// reached the Springs untrimmed until the first control tick had heard it;
// its attack, passed almost undispersed by the high path, peaked 3.2 dB over
// every later chord (-3.8 vs -7.0 dBFS; CLEAN, MIX 1, DECAY noon). Starting
// low, a first hit can only come in a little quiet (02_hits' first snare
// 0.4 dB under the same hit later). The followers were never the slow part:
// both start from silence, so their ratio is right from the first tick
// (a fast-down / slow-up trim, the queued idea, measured no different).
constexpr float kExcHpHz      = 58.0f;   // each of two: composite -3 dB at ~90 Hz
constexpr float kExcLpHz      = 3700.0f; // each of two: composite -3 dB at ~2.4 kHz
constexpr float kExcSeconds   = 0.3f;
constexpr float kExcStrength  = 1.0f;
constexpr float kExcRefShare  = 0.4f;
constexpr float kExcMaxDb     = 6.0f;
constexpr float kExcGateDb    = -60.0f;
// New sound (v1.0.45; owner: "the first chord after drums should be as loud
// as the chords that follow it"). The followers above, and the low-cut, TONE
// and Big Knob makeups that share their 0.3 s, average whatever came in
// lately. After a run of drums they still held the drums when the first
// chord came: a kick's lows count as level the springs don't hear, so the
// chord was turned up (and the low-cut makeup, reading snares, turned it
// down a little), and chord 1 peaked up to 3.4 dB over chords 2-4 (CLEAN,
// MIX 1, noon; the backlog's "+2.5 dB"). It was never a one-tick lag: a
// trim set right on the chord's first tick moved it 0.3 dB at most.
// Now, when the input jumps (a tick's power over kExcNewRatio x the
// followers', after its fast level had fallen under kExcNewRearm x them),
// the Tank remembers what the followers held. Once the new sound has been
// heard for kExcNewJudgeTicks, it compares the input trims' gain with and
// without that memory: if they differ by more than kExcNewDiffDb, every
// input-side follower leaves the memory out (it decays in them as it always
// did; the Tank subtracts it until it weighs under kExcNewGoneShare), so the
// new sound is read as if it came from silence. The sustain trim's held
// detector still reads the plain followers. Nothing changes where the old
// weighs under kExcNewMinShare (after a pause, e.g. 02_hits' hits 6 s
// apart) or where the new sound reads like the old (chord after chord,
// pads, drones, sweeps, noise): those render bit for bit as before
// (08_held_tones' chord after the 1 kHz tone moves: +0.4 dB for 0.3 s).
// Trade-off (owner's call): inside a groove with a kick, the snare or rim
// right after a kick is a new sound too, so it loses the lift the kick's
// lows gave it (H2 settings: snares -1.3..-2.1 dB, rims -0.8 dB at the peak).
constexpr float kExcNewRatio      = 2.0f;  // a new sound: a tick's input power over twice (3 dB) the followers'
constexpr int   kExcNewJudgeTicks = 8;      // judged once heard for 8 control ticks (5.3 ms)
constexpr float kExcNewDiffDb     = 1.0f;   // read on its own when that reading differs from today's by more
constexpr float kExcNewMinShare   = 0.02f;  // ... and the old still weighs over 2 % of the followers
constexpr float kExcNewGoneShare  = 0.01f;  // back to the plain followers once it weighs under 1 %
constexpr float kExcNewRearm      = 0.25f;  // ready for the next once the input's fast level (kSusFastSeconds) falls under 1/4 of the followers

// ---- Sustain trim (M8, owner, hardware, 1 Oct 2026; ADR 0035) ----------------
// A held sound (pad, drone, organ) keeps adding to what the Tank is still
// ringing with, so the springs end up louder than the source: a low-mid pad
// at -6 dBFS peak drove the wet 1-4 dB over its own peak, into the output
// limiter (red LEDs, "sounds overdriven ... as if I have the drive turned way
// up"). The Excitation trim above can't help: it fixes which band the input
// sits in, not how long it has been held. The owner picked "the tank tames
// itself on held sounds": hits keep their punch, only pads and drones are
// trimmed.
//
// How (all on the control grid: a handful of one-poles and compares, one
// exp and one log per tick; per sample only a max() on the peak the limiter
// already reads):
// 1. Is the input held? Two ways in, latched until it stops being held:
//    a. its fast power (kSusFastSeconds) stays within kSusHeldDropDb of its
//       slow power (the Excitation trim's kExcSeconds follower) for
//       kSusOnsetSeconds (round 1), or
//    b. sooner: its fast power has stayed within kSusStillDropDb of its own
//       peak since it began for kSusStillSeconds (round 2: "its level has
//       stopped falling"). An organ, a pad or a drone holds its level; a
//       snare, a rimshot or a skank chord has already fallen away by then,
//       so hits and stabs still come out bit for bit as before. A flat stab
//       longer than ~80 ms (an organ bubble) does count as held from there.
// 2. How loud will the tank get? The Tank measures its own build-up gain
//    for this sound, K = the wet's peak envelope (where the limiter reads
//    it, after the pickups and the shelf; DRIVE's heard gain divided out, so
//    DRIVE's deliberate few dB, ADR 0033, ride on top) over the envelope of
//    what went into the Springs (the raw input x the Sustain trim squared,
//    lagged kSusFillScale x the tank's fill time T60 / 13.8), both released
//    over kSusSlowSeconds, so a rise or a fall reads alike on both sides
//    (round 1 averaged the wet over 0.3 s and lagged the input a full fill
//    time: K read high while a sound was still arriving, and too slowly on
//    a sudden swell). K depends on the sound and the settings (TENSION's
//    bump, SPRINGS, TONE, DECAY, WOBBLE), not on the trim, so the trim that
//    puts the peaks on kSusTargetDb is read straight off it: t^2 = target^2
//    / (K x input). Feed-forward: it cannot hunt like a compressor on the wet.
// 3. K is a high-water mark (round 2): the highest build-up met while this
//    sound is held, kept kSusKHoldSeconds, then let down at
//    kSusKReleaseDbPerS. WOBBLE's Drift moves a held note on and off the
//    tank's resonances: fully left a pure drone's wet swings ~10 dB on its
//    own. Round 1 chased each swell (down fast, up over 2 s: the drone's trim
//    moved 2.9 dB at the default WOBBLE, 7.8 fully left); now the trim
//    answers the loudest swell and sits still through the rest.
// While held, the trim eases the Springs' *input* down (after Tilt and the
// Excitation trim, before the Loops; never the wet, the Kick's feed or the
// Clatter), only the part of the held sound that would push the peaks past
// the target: a held sound that stays under it is untouched.
// - Arriving (the first kSusSettleSeconds once held): it aims at the target
//   itself and moves down over kSusOnsetDownSeconds, so a held sound's first
//   peaks are caught (round 1 let the first 0.3 s through like a hit; the
//   organ's attack reached the limiter, 1.5 dB, once WOBBLE's default moved).
// - Settled: kSusSettledLiftDb more room (the high-water K already puts the
//   loudest swell on the target, so the usual peaks sit lower; the lift
//   brings the level back near round 1's, which the owner picked), a steady
//   band of +-kSusSteadyDb, and outside it the trim moves only to the band's
//   edge, down over kSusDownSeconds (at long DECAYs no faster than
//   kSusDownPerFill x the tank's fill time), up over kSusUpSeconds.
// As soon as the input stops being held (it falls away or goes silent) the
// trim lets go over kSusLetGoSeconds, so the next hit arrives at full
// strength. A ringing tail is never touched (only new input is trimmed), so
// a tail cannot pump, DECAY's tail length is unchanged, and the Howl (which
// feeds itself) is as loud as before. At most kSusMaxDb.
// Target: -7 dBFS peaks while arriving, -5 settled (3.3 dB under the
// limiter's knee, 0.82 = -1.7 dBFS): the pad, drone and organ at -6 dBFS
// peak never reach the limiter at the owner's settings at the default
// WOBBLE or right of noon. Left of noon a later, louder swell than any met
// so far can still touch it for a moment (worst 1.4 dB, round 1 4.8).
// Measurements: docs/m8-tuning-backlog.md "Sustain trim".
constexpr float kSusFastSeconds      = 0.02f;
constexpr float kSusHeldDropDb       = 6.0f;
constexpr float kSusOnsetSeconds     = 0.3f;
constexpr float kSusStillSeconds     = 0.08f;
constexpr float kSusStillDropDb      = 3.0f;
constexpr float kSusSlowSeconds      = 0.3f;
constexpr float kSusFillScale        = 0.25f;
constexpr float kSusKHoldSeconds     = 3.0f;
constexpr float kSusKReleaseDbPerS   = 1.0f;
constexpr float kSusTargetDb         = -7.0f;
constexpr float kSusSettleSeconds    = 0.5f;
constexpr float kSusOnsetDownSeconds = 0.03f;
constexpr float kSusSettledLiftDb    = 2.0f;
constexpr float kSusDownSeconds      = 0.1f;
constexpr float kSusDownPerFill      = 0.5f;
constexpr float kSusSteadyDb         = 1.0f;
constexpr float kSusUpSeconds        = 2.0f;
constexpr float kSusLetGoSeconds     = 0.05f;
constexpr float kSusMaxDb            = 12.0f;

// ---- Sustain trim voicings (round 3, owner, 1 Oct 2026; ADR 0035 "Round 3") --
// Round 2 (above) on the owner's real pad: "the limiter is causing an audible
// dip and swell in volume", and the organ "feeling less alive". The output
// never passed -2.0 dBFS there (the knee is -1.7): the dip and swell was the
// trim moving (down over 30 ms to -7 dBFS while a sound arrives, then 2 dB
// back up once settled; the high-water K let down at 1 dB/s), not the
// limiter. The owner asked for a middle ground that keeps things characterful.
// The gentle voicing is a safety net, not a level rider:
// - It reads the same thing (the held detector, the tank's build-up K) and
//   from it the peak the loudest swell met so far would reach untrimmed. Up
//   to kSusGentleFromDb (the limiter, with its 30 ms hold, at most ~0.2 dB
//   in) nothing is trimmed. Past it the trim ramps in, so that from
//   kSusGentleFullDb (the limiter ~1.7 dB in) on the peaks land on
//   kSusGentleTargetDb, just under the knee. At most kSusGentleMaxDb (the
//   C2 drone at TENSION 1, 3 Springs, TONE 0 needs ~5.3 to stay under ~2 dB
//   of limiting; 4 left it at 3.2). A ramp, not an on/off point: a sound
//   that creeps over a threshold mid-hold would otherwise jump by the whole
//   2.5 dB.
// - "The loudest swell so far" is a low-water mark of the trim that swell
//   needs, held kSusGentleNeedHoldSeconds, then let up at
//   kSusGentleNeedReleaseDbPerS: once it has moved it holds, and the
//   sound's own swells under that pass untouched (no chasing, no swell back).
// - It glides down over ~0.3 s (a one-pole of kSusGentleDownSeconds; at long
//   DECAYs no faster than kSusDownPerFill x the fill time) and up over
//   kSusGentleUpSeconds. No separate "arriving" aim, so no dip then lift. A
//   sound that grows into the limiter mid-hold (a pad's beating, WOBBLE
//   moving it onto a resonance) is caught by one glide down; the limiter
//   takes the moment before it (a short, held, clean pull).
// - When the input stops being held it lets go over kSusGentleLetGoSeconds
//   (the next hit meets no trim). Hits and stabs are never held: exactly 0.
// Voicings (Renderer-only key "sustain_voicing"; the firmware and the plugin
// use kSusDefaultVoicing): 0 = off (the limiter hold only), 1 = round 2,
// 2 = gentle. Measurements: docs/m8-tuning-backlog.md "Sustain trim round 3".
constexpr int   kSusVoicingOff       = 0;
constexpr int   kSusVoicingRound2    = 1;
constexpr int   kSusVoicingGentle    = 2;
constexpr int   kSusDefaultVoicing   = kSusVoicingGentle;
constexpr float kSusGentleTargetDb   = -2.5f;
constexpr float kSusGentleFromDb     = -1.5f;
constexpr float kSusGentleFullDb     = 0.0f;
constexpr float kSusGentleMaxDb      = 5.0f;
constexpr float kSusGentleDownSeconds = 0.12f;
constexpr float kSusGentleUpSeconds  = 4.0f;
constexpr float kSusGentleLetGoSeconds = 0.15f;
constexpr float kSusGentleNeedHoldSeconds = 6.0f;
constexpr float kSusGentleNeedReleaseDbPerS = 0.3f;

// Output pickup high-pass (also removes DC made by the asymmetric clip).
constexpr float kOutHpHz = 35.0f;
// DC blocker at the end of DriveIn (asymmetric saturators make DC).
constexpr float kDriveDcHz = 15.0f;

// ---- DRIVE curves (ADR 0014, ADR 0022) ---------------------------------------
// DRIVE works in two places, each with its own curve:
//
// 1. Pre-gain into DriveIn (transducer + tape, before the springs): a power
//    curve. With p = 1.3 the pre-gain has covered 17 % of its dB range at
//    DRIVE 0.25 (9 o'clock: clean-ish), 41 % at noon, 81 % at 0.85. Its top
//    (Voice::driveDbMax) is capped by aliasing: a 10 Vpp input at KICKED's
//    +20 dB is as hot as x2 oversampling stays clean for (test_drive).
//    (Was p = 1.8 before ADR 0022: only 29 % at noon, too late.)
constexpr float kDriveCurvePower = 1.3f;
// exp/log rather than pow: powf is much bigger in the Firmware's flash (Mappings.h).
inline float driveCurve(float v) { return v <= 0.0f ? 0.0f : std::exp(kDriveCurvePower * std::log(v)); }
// dB -> linear gain, via exp for the same reason.
inline float dbToGain(float db) { return std::exp(db * (2.302585093f / 20.0f)); }
//
// DRIVE is the INPUT (ADR 0033, owner 30 Sep 2026; the Wellspring's INPUT
// knob, docs/m8-tuning-backlog.md "Send-level calibration study"): one input
// gain G at the front of the Tank, the same in every ATTITUDE, 0 dB at
// DRIVE 0 up to kInputGainMaxDb at DRIVE 1 along driveCurve (+9.7 dB at
// noon, +18 dB at ~0.8). +18 dB turns a -24 dBFS mixer send into a -6 dBFS
// DAW-level hit; the top leaves room for quieter sends.
//  - The Splash hears the input after G and before any saturation
//    (SplashVoicing.h), so DRIVE up never reduces splash, and a quiet send
//    splashes like a DAW-level hit once DRIVE makes up the difference.
//  - The saturators see G x the ATTITUDE's voicing offset (Voice::driveDbMin
//    / driveDbMax, the pre-gain curve they had before: same colour per
//    ATTITUDE as ADR 0022 tuned it).
//  - Partly louder: DriveIn's automatic gain compensation takes G back, and
//    the Tank gives kInputHeard of it (in dB) back on the Springs' output:
//    the tail grows +6 dB across the knob, the same in every ATTITUDE, a
//    gentle throw by CV, not a volume knob (ADR 0033). On the output, not
//    into the Springs: the LoopSat, the AntiRes fade and the Howl then see
//    the same level at every DRIVE, so DRIVE never changes the tail's length.
//    The pickups' hardness is divided by the same gain (Tank::controlTick),
//    so they bend the louder tail as ADR 0022 voiced them, and a Kick's Loop
//    feed too (a Kick keeps its size, ADR 0005).
constexpr float kInputGainMaxDb = 24.0f;
constexpr float kInputHeard     = 0.25f;
inline float inputGainDb(float drive) { return kInputGainMaxDb * driveCurve(drive); }
//
// DRIVE's colour runs on a stretched knob (owner, 30 Sep 2026, after the
// build pages: "drive80 still feels a bit hot for 80% given there's still
// 20% more we can push it; 100% would yield distortion that wouldn't be
// particularly useful"). Everything that sets how gritty DRIVE gets (the
// saturators' pre-gain, the tape smear, the pickup push and blend, the wet
// makeup) reads colourDrive(DRIVE): unchanged up to noon, then stretched so
// DRIVE 1 has the grit DRIVE 0.92 had (0.8 plays like 0.75 did), as far as
// ADR 0022 allows (KICKED still cranked at max). Curve shapes unchanged.
// The INPUT gain G (the splash's sensitivity, quiet sends) and its heard
// share (+6 dB) keep the real DRIVE.
constexpr float kColourDriveTop = 0.92f;
inline float colourDrive(float drive)
{
    return drive <= 0.5f ? drive : 0.5f + (drive - 0.5f) * ((kColourDriveTop - 0.5f) / 0.5f);
}
inline float drivePreGainDb(const Voice& vc, float drive)
{
    return vc.driveDbMin + (vc.driveDbMax - vc.driveDbMin) * driveCurve(colourDrive(drive));
}
// HF smear cutoff: open at DRIVE 0, closes toward smearHzMax as DRIVE rises
// (tape loses top end when pushed). Exponential so it sweeps evenly by ear.
inline float smearHz(const Voice& vc, float drive)
{
    return map::expLerp(kSmearOpenHz, vc.smearHzMax, driveCurve(colourDrive(drive)));
}
//
// 2. "Push" on the output pickup, the stage after the springs (ADR 0022:
//    most of what DRIVE does to the input is smeared into the dark tail and
//    lost; the pickup makes it heard). An S-curve (logistic, scaled
//    to run 0 -> 1): 20 % of the push at 9 o'clock, 68 % at noon, 95 % at
//    3 o'clock. Why an S: saturation is heard late (a curve twice as hard
//    sounds much more than twice as driven), so the push has to arrive
//    early and level off, or noon would be subtle and the top a cliff.
constexpr float kPushSlope  = 8.0f;
constexpr float kPushCentre = 0.36f;
inline float pushCurve(float v)
{
    auto l = [](float x) { return 1.0f / (1.0f + std::exp(-kPushSlope * (x - kPushCentre))); };
    const float l0 = l(0.0f), l1 = l(1.0f);
    return (l(v) - l0) / (l1 - l0);
}
//
// 3. The pickup blend (Voice::outAmount0; only CLEAN uses it, DRIVEN and
//    KICKED are always at 1): DRIVE^kOutAmountPower, 0 -> 1. Was linear
//    (power 1). CLEAN's pre-gain curve (1. above) does almost nothing below
//    ~10 o'clock, so with a linear blend the bottom three 0.1 steps sat
//    just under the "heard" line (02_hits step nulls about -40.5 dB, a dead
//    patch at TENSION noon, ADR 0026). A slightly concave blend puts a
//    little more of the tint at the bottom and a little less at the top;
//    the end points are unchanged, so DRIVE 0 and DRIVE 1 sound exactly as
//    before (CLEAN 0 vs 1 null still -15.8 dB).
constexpr float kOutAmountPower = 0.8f;
inline float outAmountCurve(float v)
{
    return v <= 0.0f ? 0.0f : (v >= 1.0f ? 1.0f : std::exp(kOutAmountPower * std::log(v)));
}
struct Push {
    float out    = 1.0f; // output pickup hardness factor, linear
    float wet    = 1.0f; // static wet makeup, linear (Voice::wetMakeupDb)
    float outFluxDb = kOutFluxDb; // output pickup flux cut, dB (kOutFluxDb - outFluxOpenDb · c)
    float outAmount = 1.0f;       // output pickup blend (outAmount0 -> 1 along outAmountCurve)
};
// Computed only when DRIVE or the ATTITUDE Morph moved (a few exp calls).
RV_SIZE_OPT inline Push push(const Voice& vc, float drive) // on DRIVE / Morph moves only (controlTick)
{
    const float c = pushCurve(colourDrive(drive));
    Push p;
    p.out    = dbToGain(vc.outDriveDb * c);
    p.outFluxDb = kOutFluxDb - vc.outFluxOpenDb * c;
    p.outAmount = vc.outAmount0 + (1.0f - vc.outAmount0) * outAmountCurve(colourDrive(drive));
    p.wet    = dbToGain(vc.wetMakeupDb * c * c);
    return p;
}

// Automatic gain compensation (SPEC §4.9, ADR 0022, ADR 0033): measured,
// not modelled. DriveIn and DriveOut each follow the slow mean-square level
// (kAutoMakeupSeconds) going into and coming out of their saturators and
// make up the difference (Drive.h). So DRIVE changes the shape (rounded,
// squashed, gritty) and the level only by G's heard share (kInputHeard),
// for loud hits and quiet pads alike (test_drive). Before ADR 0022 this was
// a static model calibrated on -6 dBFS snares, which made quieter material
// up to 5 dB louder once the curves were pushed harder. What the pickups'
// follower gives back late or not at all: Voice::wetMakeupDb.

// ---- TONE tilt (ADR 0017; SPEC §3 K1) ---------------------------------------
// Pre-tank tilt EQ: split at the pivot with a one-pole low-pass, then
// lows × 10^(-T/40), highs × 10^(+T/40): a first-order tilt of T dB end to
// end, 0 dB at noon. CCW tilts to lows (warm, dark: less HF excites the
// springs, so fewer bright drips but the boing stays), CW to highs
// (splashy). The CW end is smaller than the CCW end and its extra HF gain has
// a ceiling (kTiltCeilingHz: the boost rolls off above it), "very bright,
// capped so it never turns harsh" (ADR 0017); the Loop damping and the high
// path level (Mappings.h) brighten CW further on their own.
constexpr float kTiltPivotHz   = 900.0f;
constexpr float kTiltCcwDb     = -9.0f; // TONE 0
constexpr float kTiltCwDb      = 5.0f;  // TONE 1
constexpr float kTiltCeilingHz = 7000.0f;
inline float toneTiltDb(float v)
{
    return v < 0.5f ? kTiltCcwDb * (1.0f - 2.0f * v) : kTiltCwDb * (2.0f * v - 1.0f);
}
// Bright-side low cut (owner, 29 Sep: "the brighter tone seems to boost the
// highs, but doesn't seem to high pass much. It'd be good to filter some of
// the lows"). The tilt alone only takes the lows down 2.5 dB at TONE 1, so
// the CW half also sweeps a 2nd-order high-pass (Q 0.707) in front of the
// springs: less low energy goes in, so the whole tank (tail included) thins
// out, bright and splashy like a small dub tank. Off at noon and CCW
// (kToneLowCutMinHz, below anything the DriveIn lets through), then up the
// CW half on a log scale with a slightly early curve (u^0.7), so the
// thinning builds evenly: ~3 o'clock ≈ 105 Hz, full CW kToneLowCutMaxHz.
// Pre-tank, so the Kick's thud (direct to the wet bus) keeps its weight.
// (Tone voicing 0, a Renderer reference now. The Big Knob that replaced it
// sits on the wet since 4 Oct 2026, "TONE placement" below, and so thins
// the Kick's thud too.)
constexpr float kToneLowCutMinHz = 20.0f;
constexpr float kToneLowCutMaxHz = 300.0f;
inline float toneLowCutHz(float v)
{
    return v <= 0.5f ? kToneLowCutMinHz
                     : map::expLerp(kToneLowCutMinHz, kToneLowCutMaxHz, std::exp(0.7f * std::log(2.0f * v - 1.0f)));
}
// ---- Big Knob TONE voicings (prototype, owner 1 Oct 2026; ADR 0036 Proposed) --
// King Tubby's "Big Knob": the stepped passive inductor high-pass (Altec
// 9069B) he swept on the sends to thin a sound out, telephone-like, before
// the spring. Research and numbers: docs/research/big-knob.md. TONE's right
// half becomes a smooth version of it (no steps, owner); left of noon and
// noon are today's, bit for bit, in every voicing.
//   u = 2·TONE - 1 (0 at noon, 1 fully CW).
//   Cutoff fc: kToneLowCutMinHz at noon up to kBigKnobMaxHz fully CW, on the
//     same log + early curve as today's low cut (u^0.7): TONE 0.7 ≈ 170 Hz,
//     0.85 ≈ 380 Hz, 1 = 800 Hz ("telephone"; first voiced to 1.2 kHz,
//     lowered because KICKED, 3 Springs, TENSION 0 rang at 3.1 kHz above
//     ~850 Hz: with the lows gone its LoopSat lets a Loop mode ring;
//     backlog "Big Knob TONE" round 2).
//   Slope: 3rd order, 18 dB/oct (the Altec's): today's 2nd-order section
//     (its Q raised from 0.707 to 1: a flat 3rd-order Butterworth) times a
//     1st-order section y = x - k·LP(x). k fades in from 0 at noon (the
//     section is then an exact pass-through) to 1 by u = kBigKnobOrderIn,
//     so there's no jump anywhere.
//   Bump (voicings 2, 3): a nasal, ringy peak just above fc that grows with
//     u. Why a peak at all: the Altec is a 600 ohm constant-k T-section,
//     exactly a flat 3rd-order Butterworth when driven and loaded at 600
//     ohm; fed from a low-impedance output into a higher-impedance input, as
//     on a console, it peaks at ~1.3-1.4 fc (tools/research/
//     big_knob_circuit.py). The same circuit's poles, as a 1st-order corner
//     f1 and a 2nd-order pair f2, Q: matched (f1 = f2 = fc, Q 1) at u -> 0,
//     moving in a straight line to a near-zero source into ~1 kohm with a
//     coil Q of ~10 fully CW (kBumpF1/F2 x fc, kBumpQ): +1.3 / 3.0 / 4.6 /
//     6.1 dB at u 0.25 / 0.5 / 0.75 / 1, at ~1.35-1.4 fc; -3 dB stays at
//     ~0.9 fc. Size: the "medium" loading (between the KTBK-style mild bump
//     and a bridging input's +16 dB), chosen on the measurements in
//     docs/m8-tuning-backlog.md "Big Knob TONE" (M6 grid, loudness, Sustain
//     trim).
//   Level: thinning takes energy out of the tank, and how much depends on
//     the material (at 1.2 kHz a skank lost 9 dB, a snare 2: no fixed makeup
//     keeps both within ±3 dB). So the Tank follows the power going into and
//     out of the Tilt above ~90 Hz (slow followers and the high-pass of the
//     Excitation trim: a bass pad's C2 fundamental, cut but never heard by
//     the tank, made a broadband makeup bring it back ~4.5 dB hotter) and gives
//     back kBigKnobMakeupShare of what the low cut took out (in dB), at most
//     kBigKnobMakeupMaxDb, held in silence. It trims the Springs' input
//     (never a ringing tail), like the Excitation and Sustain trims.
//     Two measured corrections on top (bigKnobTrimDb, dB x u): the bump puts
//     energy where the tank and the ear are most sensitive (~1-2 kHz), so
//     voicings 2-3 come down kBigKnobBumpTrimDb; and a driven tank squashes
//     on its lows (the LoopSat and pickups), so taking the lows away lets
//     hits come back louder (today's TONE 1 already +2.7 dB on KICKED hits
//     at DRIVE 0.8): DRIVEN (half) and KICKED come down by
//     kBigKnobSquashDb + kBigKnobSquashDriveDb x DRIVE.
//   Voicing 3 adds "ringier when driven": the inductor's core saturates on
//     loud lows. DriveIn already models a coil saturating on flux (lows); in
//     voicing 3 its lows are pushed kBigKnobPushDb · u harder into the
//     transducer (highs as before, small signals unchanged, the automatic
//     makeup keeps the level), so DRIVE up adds lows' harmonics that the
//     Big Knob then passes. No new nonlinear stage, no CPU.
// Renderer-only key "tone_voicing" (the firmware and the plugin use
// kToneDefaultVoicing).
constexpr int   kToneVoicingToday   = 0;
constexpr int   kToneVoicingSteep   = 1;
constexpr int   kToneVoicingBump    = 2;
constexpr int   kToneVoicingDriven  = 3;
// 4 = the owner's pick (1 Oct 2026, renders/proto_big_knob/): voicing 2, with
// the bump easing off over the top of the knob (v1, no bump, won fully CW on
// skank and KICKED hits): the same up to u = kBumpEaseFrom, kBumpEase less
// bump amount fully CW.
constexpr int   kToneVoicingGentle  = 4;
// 5 = the bump on hits only (owner, 1 Oct 2026, renders/proto_big_knob2/: the
// gentle bump won on drum hits, no bump won on pads, chords, clicks and
// KICKED skank). The Tilt runs voicing 1 (steep, no bump) and voicing 4
// (gentle bump) side by side and blends to 4 while a sharp hit lasts: the
// Splash's hit detector, before the SPLASH knob scales it (sudden x loud x
// short: a hit with crack in it, not a chord or a pad), x kHitBumpGain, up
// at once, back over kHitBumpReleaseS. Held sounds and chords hear voicing 1.
constexpr int   kToneVoicingHits    = 5;
constexpr float kHitBumpGain        = 2.0f;
constexpr float kHitBumpReleaseS    = 0.15f;
constexpr float kBumpEaseFrom       = 0.5f;
constexpr float kBumpEase           = 0.25f;
constexpr int   kToneDefaultVoicing = kToneVoicingHits; // owner pick, 2 Oct 2026
constexpr float kBigKnobMaxHz       = 800.0f;
constexpr float kBigKnobOrderIn     = 0.2f;
constexpr float kBumpF1             = 0.70f; // x fc, fully CW
constexpr float kBumpF2             = 1.30f; // x fc, fully CW
constexpr float kBumpQ              = 2.20f; // fully CW
constexpr float kBigKnobMakeupShare = 0.75f;
constexpr float kBigKnobMakeupMaxDb = 12.0f;
constexpr float kBigKnobBumpTrimDb  = 1.2f;
constexpr float kBigKnobSquashDb    = 0.2f;
constexpr float kBigKnobSquashDriveDb = 3.3f;
constexpr float kBigKnobPushDb      = 4.0f;
struct BigKnob {
    float hz  = kToneLowCutMinHz; // 2nd-order section's cutoff
    float q   = 0.7071f;          // and Q
    float hz1 = kToneLowCutMinHz; // 1st-order section's cutoff
    float k   = 0.0f;             // and depth (0 = pass-through)
    float pushDb   = 0.0f;        // DriveIn lows push (voicing 3)
};
inline float bigKnobHz(float u) // the step's nominal cutoff fc
{
    return map::expLerp(kToneLowCutMinHz, kBigKnobMaxHz, std::exp(0.7f * std::log(u)));
}
inline BigKnob bigKnob(int voicing, float v)
{
    BigKnob b;
    if (voicing == kToneVoicingToday) { b.hz = b.hz1 = toneLowCutHz(v); return b; }
    if (v <= 0.5f) return b; // today's, exactly
    const float u  = std::min(1.0f, 2.0f * v - 1.0f);
    const float s  = std::min(1.0f, u / kBigKnobOrderIn);
    const float fc = bigKnobHz(u);
    if (voicing == kToneVoicingHits) voicing = kToneVoicingSteep; // its plain path (the Tilt adds the bumped one)
    float w = voicing >= kToneVoicingBump ? u : 0.0f; // bump amount
    if (voicing == kToneVoicingGentle && u > kBumpEaseFrom) {
        const float e = (u - kBumpEaseFrom) / (1.0f - kBumpEaseFrom);
        w -= kBumpEase * e * e;
    }
    b.hz  = fc * (1.0f + (kBumpF2 - 1.0f) * w);
    b.hz1 = fc * (1.0f + (kBumpF1 - 1.0f) * w);
    b.q   = 0.7071f + (1.0f - 0.7071f) * s + (kBumpQ - 1.0f) * w;
    b.k   = s;
    if (voicing == kToneVoicingDriven) b.pushDb = kBigKnobPushDb * u;
    return b;
}

// The Big Knob makeup's measured corrections (dB, <= 0) for TONE v > 0.5;
// w = the ATTITUDE Morph weights (CLEAN, DRIVEN, KICKED).
inline float bigKnobTrimDb(int voicing, float v, const std::array<float, 3>& w, float drive)
{
    const float u = std::min(1.0f, 2.0f * v - 1.0f);
    const bool bumped = voicing >= kToneVoicingBump; // 2-5 (v5: its hits carry the bump; held sounds come out ~1 dB quieter at TONE 1)
    const float bump = bumped ? kBigKnobBumpTrimDb : 0.0f;
    return -u * (bump + (0.5f * w[1] + w[2]) * (kBigKnobSquashDb + kBigKnobSquashDriveDb * drive));
}

// ---- TONE placement: the Big Knob after the Springs (owner, 4 Oct 2026;
// ADR 0036 "Placement: after the springs"; docs/research/dub-lens-critique.md
// §3.2 and direction B; prototype docs/prototypes/tone-place/) ----
// "When you turn TONE right during a ringing tail, what should thin out?"
// The owner, by ear: the tail you hear. The Big Knob (TONE's right half: the
// low cut above, its slope, cutoff curve and bump on hits) runs on the
// stereo wet, after the Springs, pickups and shelf, before the limiter and
// MIX: Black Ark's low cut on the spring's return, dub techno's filter on
// the wet. Turning TONE right thins the ringing tail at once, and turning
// back gives its body back. Everything else TONE does (the tilt, the Loop
// damping, the high path, tank voicing 7's coil and pickup) stays where it
// was, and left of noon and noon are unchanged, bit for bit.
//   kTonePlacePre  (0): the Big Knob before the Springs (ADR 0036 as first
//                  shipped, SPEC <= v1.0.28): the next hit goes in thin, the
//                  tail already ringing changes only slowly. Kept renderable
//                  for reference (Renderer key "tone_place_voicing" = 0).
//   kTonePlacePost (1): THE behaviour. Firmware, plugin and Renderer default.
// (The prototype's split placement, 1st-order section before / 2nd-order on
// the wet, was dropped with the owner's pick.)
// Level (the wet, not the Springs' input, so the makeup is its own): slow
// followers of the wet's power into and out of the return filter above
// ~90 Hz (the Excitation trim's weighting, so the Kick's sub thump doesn't
// count), give back kPostMakeupShare of what it took (in dB, at most
// kPostMakeupMaxDb either way), held while the wet is silent. It reads the
// wet itself, so it follows a ringing tail too: a sweep thins the tail at
// once and the level catches up over kExcSeconds (a gentle swell back, not
// a jump; ramped per sample). The squash correction (a driven tank squashing
// on the lows the pre cut fed it) doesn't apply after the tank; the bump
// correction does, on the wet.
// The firmware (RV_FIXED_VOICINGS) builds only kTonePlaceDefault.
constexpr int   kTonePlacePre     = 0;
constexpr int   kTonePlacePost    = 1;
constexpr int   kNumTonePlaces    = 2;
constexpr int   kTonePlaceDefault = kTonePlacePost; // owner pick, 4 Oct 2026
constexpr float kPostMakeupShare  = 0.75f;
constexpr float kPostMakeupMaxDb  = 12.0f;
// The Big Knob's sections before the Springs: all of them (pre), or noon's
// (post: a 20 Hz 2nd-order guard, no 1st-order section; the Tilt then skips
// its Big Knob sections, which are pass-throughs there).
inline BigKnob bigKnobPre(int place, int voicing, float v)
{
    if (place == kTonePlacePre || v <= 0.5f) return bigKnob(voicing, v);
    BigKnob b = bigKnob(voicing, 0.5f); // noon's, exactly
    b.pushDb = bigKnob(voicing, v).pushDb;
    return b;
}
// ... and on the wet (post). depth crossfades the 2nd-order section in from
// an exact pass-through at noon (as k does the 1st-order one), so crossing
// noon never jumps.
struct PostKnob {
    BigKnob b;
    float   depth = 0.0f;
};
inline PostKnob bigKnobPost(int place, int voicing, float v)
{
    PostKnob p;
    if (place == kTonePlacePre || v <= 0.5f) return p; // pass-through
    p.b     = bigKnob(voicing, v);
    p.depth = std::min(1.0f, (2.0f * v - 1.0f) / kBigKnobOrderIn);
    p.b.pushDb = 0.0f;
    return p;
}
// The makeups' measured corrections: pre keeps ADR 0036's (bump + squash);
// post has no pre cut (none before the Springs) and moves the bump
// correction to the wet.
inline float bigKnobPreTrimDb(int place, int voicing, float v, const std::array<float, 3>& w, float drive)
{
    return place == kTonePlacePre ? bigKnobTrimDb(voicing, v, w, drive) : 0.0f;
}
inline float bigKnobPostTrimDb(int place, int voicing, float v)
{
    if (place == kTonePlacePre || v <= 0.5f) return 0.0f;
    const float u = std::min(1.0f, 2.0f * v - 1.0f);
    return voicing >= kToneVoicingBump ? -u * kBigKnobBumpTrimDb : 0.0f;
}

// Tilt level compensation, dB per dB of tilt. A spring tank's loudness sits
// mostly *below* the pivot (the Loop is dark and its low-mids ring longest),
// so tilting toward the lows makes it louder: CCW gets pulled down. Keeps
// TONE sweeps within ±3 dB (test_drive).
constexpr float kTiltCompCcw = 0.36f;
constexpr float kTiltCompCw  = 0.0f;
inline float toneTiltCompDb(float v)
{
    const float t = toneTiltDb(v);
    return t < 0.0f ? kTiltCompCcw * t : -kTiltCompCw * t;
}

// ---- Howl zone (ADR 0002, 0018, 0019) -----------------------------------------
// KICKED only, top ~10 % of DECAY. Inside the zone the Loop's small-signal
// peak gain P (g × max|H|) is raised from its normal value (< 1) toward
// kHowlPeakGain (> 1): the Loop then grows until the LoopSat's compression
// brings the gain back to 1 (saturated, rough self-oscillation). Outside the
// zone nothing changes, so CLEAN / DRIVEN and KICKED below the zone keep
// Loop gain < 1 at every frequency (ADR 0001, AntiRes layer 1).
//   h = zone position 0..1 (DECAY 0.9 -> 1) × KICKED Morph weight
//   P = P0 + (kHowlPeakGain - P0) · sqrt(h)
// sqrt: P reaches 1 about a fifth of the way into the zone (DECAY ~0.92),
// so the zone is "long bloom -> tips over -> roar", without a cliff at 0.9.
// Leaving the zone (DECAY glide, ATTITUDE Morph) brings P back below 1
// through the normal smoothing: the Howl dies away naturally (ADR 0018).
constexpr float kHowlZoneStart = 0.90f;
constexpr float kHowlPeakGain  = 1.10f;
inline float howlZone(float decay)
{
    const float z = (decay - kHowlZoneStart) / (1.0f - kHowlZoneStart);
    return z <= 0.0f ? 0.0f : (z >= 1.0f ? 1.0f : z);
}

} // namespace rv::drive
