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
constexpr float kMorphSeconds = 0.030f;

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
    // DRIVE -> pre-gain into the chain, dB at DRIVE 0 and DRIVE 1 (driveCurve
    // in between). Automatic gain compensation (Drive.h) undoes the level
    // change, so these set colour, not loudness.
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
    // DRIVE also pushes the stages *after* the input (ADR 0022). Without
    // this, the springs smear the input's distortion into a dark tail and
    // most of it is lost: the colour has to be made where you hear it too.
    //   loopDriveDb: how much harder the LoopSat bites at DRIVE 1 (its
    //     hardness k is raised by this many dB along pushCurve). The
    //     curve's slope never goes above 1 whatever k is, so this thickens
    //     and compresses the tail but can never make the Loop run away.
    //   outDriveDb: the same for the output pickup (DriveOut), which sits
    //     after the springs: its grit is heard directly, not smeared.
    float loopDriveDb;
    float outDriveDb;
    // Output pickup (DriveOut)
    float outK;        // soft-clip hardness at DRIVE 0 (light: only bends near full scale)
    float outAsym;     // negative-half hardness = outK * (1 + outAsym)
    float outLpHz;     // pickup band-limit low-pass
    // Wet makeup at DRIVE 1, dB (along pushCurve²): gives back the level the
    // pushed LoopSat squashes out of a tail, which no level follower can
    // see (it happens inside the Loop), so DRIVE stays colour, not volume
    // (±2 dB, SPEC §7 M5). Calibrated on 02_hits and steady noise.
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
// CLEAN +3 dB; the new push columns (lDrv, oDrv) and wMk; KICKED's range
// shifted down 2 dB (-11 dB, 9 o'clock stays clean-ish with the earlier
// curve) and its top kept at +20 dB (aliasing at 10 Vpp, see above).
inline constexpr std::array<Voice, 3> kVoice{{
    //  hp      lp       tK+    tK-    flux   dB0     dB1    tape  tapeK  emph   smear    loop  lK+    lK-    lDrv   oDrv   oK     oAs    oLp      wMk   trim
    {  45.0f, 11000.f, 0.30f, 0.38f,  6.0f,  -6.0f, 12.0f, 0.0f, 0.60f, 5.0f,  9000.f, 0.0f, 0.60f, 0.60f,  0.0f, 14.0f, 0.35f, 0.15f, 15000.f,  0.0f, 0.0f}, // CLEAN
    {  85.0f,  6500.f, 0.45f, 0.60f,  9.0f,  -6.0f, 16.0f, 1.0f, 0.85f, 5.0f,  6000.f, 1.0f, 0.70f, 0.70f, 24.0f, 24.0f, 0.55f, 0.20f, 11000.f,  0.4f, 0.0f}, // DRIVEN
    { 130.0f,  5000.f, 0.80f, 1.40f, 15.0f, -11.0f, 20.0f, 1.0f, 1.00f, 4.0f,  4500.f, 1.0f, 1.60f, 2.60f, 22.0f, 26.0f, 0.60f, 0.50f,  8500.f,  1.3f, 0.0f}, // KICKED
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
constexpr float kPreEmphHz = 3000.0f;
// HF smear low-pass at DRIVE 0 (effectively open).
constexpr float kSmearOpenHz = 16000.0f;
// Output pickup flux shelf (DriveOut): highs cut by kOutFluxDb above
// kOutFluxHz into its saturator and restored after (same idea as kFluxHz).
constexpr float kOutFluxHz = 800.0f;
constexpr float kOutFluxDb = 18.0f;
// Automatic makeup (DriveIn and DriveOut, see "Automatic gain compensation"
// below): averaging time of the level followers (slow enough not to pump
// on single hits), and the most DriveOut may add (a squashed Howl or a
// very hot tail gets no more, so the Howl stays clear of the limiter).
constexpr float kAutoMakeupSeconds = 0.3f;
constexpr float kAutoMakeupMax     = 2.0f; // linear, +6 dB
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
inline float drivePreGainDb(const Voice& vc, float drive)
{
    return vc.driveDbMin + (vc.driveDbMax - vc.driveDbMin) * driveCurve(drive);
}
// HF smear cutoff: open at DRIVE 0, closes toward smearHzMax as DRIVE rises
// (tape loses top end when pushed). Exponential so it sweeps evenly by ear.
inline float smearHz(const Voice& vc, float drive)
{
    return map::expLerp(kSmearOpenHz, vc.smearHzMax, driveCurve(drive));
}
//
// 2. "Push" on the stages after the input, LoopSat and output pickup
//    (ADR 0022: most of what DRIVE does to the input is smeared into the
//    dark tail and lost; these make it heard). An S-curve (logistic, scaled
//    to run 0 -> 1): 20 % of the push at 9 o'clock, 68 % at noon, 95 % at
//    3 o'clock. Why an S: saturation is heard late (a curve twice as hard
//    sounds much more than twice as driven), so the push has to arrive
//    early and level off, or noon would be subtle and the top a cliff.
constexpr float kPushSlope  = 8.0f;
constexpr float kPushCentre = 0.40f;
inline float pushCurve(float v)
{
    auto l = [](float x) { return 1.0f / (1.0f + std::exp(-kPushSlope * (x - kPushCentre))); };
    const float l0 = l(0.0f), l1 = l(1.0f);
    return (l(v) - l0) / (l1 - l0);
}
struct Push {
    float loopDb = 0.0f; // LoopSat hardness raise, dB (the Tank scales it down in the Howl zone)
    float out    = 1.0f; // output pickup hardness factor, linear
    float wet    = 1.0f; // static wet makeup, linear
};
// Computed only when DRIVE or the ATTITUDE Morph moved (a few exp calls).
inline Push push(const Voice& vc, float drive)
{
    const float c = pushCurve(drive);
    Push p;
    p.loopDb = vc.loopDriveDb * c;
    p.out    = dbToGain(vc.outDriveDb * c);
    // The LoopSat's squash of the tail (the part the automatic makeups
    // cannot see, it happens inside the Loop) lags the push: c².
    p.wet    = dbToGain(vc.wetMakeupDb * c * c);
    return p;
}

// Automatic gain compensation (SPEC §4.9, ADR 0022): measured, not
// modelled. DriveIn and DriveOut each follow the slow mean-square level
// (kAutoMakeupSeconds) going into and coming out of their saturators and
// make up the difference (Drive.h). So DRIVE changes the shape (rounded,
// squashed, gritty) but the average level stays where it was at DRIVE 0,
// for loud hits and quiet pads alike (test_drive: within ~1 dB on snare
// hits, 02_hits and steady noise). Before ADR 0022 this was a static model
// calibrated on -6 dBFS snares, which made quieter material up to 5 dB
// louder once the curves were pushed harder. The one part neither follower
// can see is the LoopSat's squash inside the Loop: Voice::wetMakeupDb.

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
