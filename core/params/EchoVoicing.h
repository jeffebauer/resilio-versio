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

// ---- Diffuse repeats (PROTOTYPE, owner 4 Oct 2026) -------------------------------------------
// "Repeats become slightly diffuse as they repeat, so there's a feeling of
// sound degradation with each repeat." A short diffuser (kDiffuseStages
// Schroeder allpasses) on the tape's FEEDBACK only, so it compounds: the
// first repeat is the input itself (bit for bit as voicing 0), the second has
// passed it once, the third twice ... Allpasses keep every frequency's level
// (no energy added), they only smear each repeat in time. Their lengths drift
// slowly (kDiffuseModHz, a fraction of a ms) so the smear doesn't settle into
// a metallic comb. Renderer key echo_diffuse_voicing; the firmware builds
// only kDiffuseDefault (0 = none: no diffuser compiled in).
constexpr int kDiffuseStages = 3;
struct DiffuseVoicing {
    float g;                      // allpass coefficient (how much each pass smears)
    float ms[kDiffuseStages];     // allpass lengths
    float modMs;                  // their slow drift, +- ms
    float trim;                   // KICKED's feedback x this: a smeared repeat's lower peaks saturate less on the tape, so near the runaway it lasted longer (T60 +10-25 %)
};
constexpr int kNumDiffuseVoicings = 4;
constexpr DiffuseVoicing kDiffuse[kNumDiffuseVoicings] = {
    {0.0f, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f},        // 0 none: today's echo
    {0.35f, {2.3f, 4.1f, 6.7f}, 0.15f, 0.985f},    // 1 light: each pass a little softer-edged
    {0.5f, {3.1f, 6.9f, 10.3f}, 0.25f, 0.975f},    // 2 medium: worn by the 3rd-4th repeat
    {0.62f, {4.3f, 9.1f, 14.7f}, 0.35f, 0.965f},   // 3 heavy: blurred into a smear by the 4th-5th
};
constexpr float kDiffuseModHz[kDiffuseStages] = {0.31f, 0.47f, 0.73f};
constexpr float kDiffuseMaxMs = 16.0f; // allpass memory (each stage), at 48 kHz: 768 floats
#ifdef RV_ECHO_DIFFUSE_DEFAULT
constexpr int kDiffuseDefault = RV_ECHO_DIFFUSE_DEFAULT;
#else
constexpr int kDiffuseDefault = 0;
#endif
#if defined(RV_FIXED_VOICINGS)
constexpr bool kDiffuseBuilt = kDiffuseDefault != 0;
#else
constexpr bool kDiffuseBuilt = true;
#endif

// ---- The first repeat (owner, 4 Oct 2026: a level correction) ---------------------------------
// "The first repeat is the same amplitude as the hit, which makes it feel
// like the decay isn't linear." The input goes on the tape at the feedback's
// own gain, so every repeat is a step down from the hit, the first included:
// repeat n = hit x g^n (g = DECAY's feedback, less the heads' small loss each
// pass). Never quieter than kFirstRepeatMin (DECAY 0 = a single repeat at
// -10 dB), never louder than kFirstRepeatMax (KICKED's runaway zone climbs
// from the second repeat on).
constexpr float kFirstRepeatMin = 0.316f; // -10 dB
constexpr float kFirstRepeatMax = kFeedbackMax;

// ---- Wear: the repeats break up (PROTOTYPE, owner 4 Oct 2026) --------------------------------
// "More degradation in the repeats ... make it sound like it's breaking up:
// aliasing, bitcrushing, or something more tape-centric." Each voicing is a
// process INSIDE the tape's feedback (dsp/EchoWear.h), so it compounds: the
// first repeat is the input itself, the second has been through it once, the
// sixth five times. None adds energy (small-signal gain <= 1). Renderer key
// echo_wear_voicing; the firmware builds only kWearDefault (0 = none).
constexpr int kNumWearVoicings = 5;
constexpr int kWearNone = 0, kWearTape = 1, kWearRadio = 2, kWearBbd = 3, kWearCrushed = 4;
// 1 WORN TAPE (Space Echo, Black Ark): each pass adds its own wow and
// flutter (a short modulated delay: two slow sines + a flutter, out of step
// with the echo, so it accumulates like a worn transport), random oxide
// dropouts (a brief dip, more of them on older repeats because they've passed
// more tape), and a saturation that bites harder as repeats build.
constexpr float kTapeWowMs = 0.6f, kTapeWowHz1 = 0.55f, kTapeWowHz2 = 1.3f;
constexpr float kTapeFlutterMs = 0.03f, kTapeFlutterHz = 7.5f;
constexpr float kTapeDropoutsPerSecond = 2.0f;
constexpr float kTapeDropoutMinMs = 3.0f, kTapeDropoutMaxMs = 40.0f;
constexpr float kTapeDropoutMinDb = 4.0f, kTapeDropoutMaxDb = 14.0f;
constexpr float kTapeSatDrive = 2.5f; // softClip(k x) / k: unity when quiet, bites as a build grows
// 2 RADIO BAND (dub techno's band-pass in the delay feedback): each pass
// through a 2-pole high-pass and low-pass around ~450 Hz, so the repeats
// narrow to a telephone / radio band.
constexpr float kRadioHpHz = 250.0f, kRadioLpHz = 900.0f, kRadioQ = 0.6f;
// 3 BBD GRIT (Memory Man, the Wellspring's delay): the feedback through a
// bucket brigade at a low clock (kBbdClockHz, sample-and-hold, no exact
// relation to 48 kHz) with a gentle anti-alias filter (so each pass folds a
// little more back down), a 2:1 compander whose expander tracks a little
// differently from the compressor (pumping, breathing), and a faint clock
// whine riding on the signal, which builds as the repeats pass again.
// BBD strength (owner picked BBD grit on 4 Oct and asked for more audible
// aliasing; Renderer key bbd_voicing). Why the first version (and crushed)
// sounded subtle: the feedback reaching the bucket brigade has already been
// through the playback head (2-pole low-pass 3.5 kHz), so it holds almost
// nothing above half a 9.7 kHz clock to fold back, and the images it makes
// (clock - f, 6-9.7 kHz) land above the head, which erases them on the next
// pass. A lower clock puts half the clock inside the head's passband: the
// content between it and 3.5 kHz folds back down to 0.5-3 kHz, and the
// images (clock - f) fall at 1-5 kHz, under the head, where they're heard
// and compound pass by pass. Weaker filters (cutoff a larger share of the
// clock) let more of both through.
//   clockHz      the bucket brigade's clock (sample-and-hold rate)
//   filterRatio  anti-alias and reconstruction 1-pole cutoff / clock
//   tracksTime   true: the clock follows the echo time like a real BBD
//                (Memory Man): clockHz at kBbdTrackRefSeconds, x (ref / time)^kBbdTrackExp
struct BbdVoicing {
    float clockHz, filterRatio;
    bool  tracksTime;
};
constexpr int kNumBbdVoicings = 4;
constexpr BbdVoicing kBbd[kNumBbdVoicings] = {
    {9700.0f, 0.464f, false}, // A today's BBD grit (filters 4.5 kHz): the reference
    {4800.0f, 0.6f, false},   // B stronger: aliasing heard from the 2nd-3rd repeat
    {3800.0f, 0.68f, false},  // C strongest: gritty, "broken" by the 3rd-4th
    {4800.0f, 0.6f, true},    // D follows the echo time: B at 0.4 s, beyond C at 2 s, clean when short
};
constexpr float kBbdTrackRefSeconds = 0.4f, kBbdTrackExp = 0.35f;
constexpr float kBbdClockMinHz = 2500.0f, kBbdClockMaxHz = 12000.0f;
// The level each pass loses in the filters and the hold's droop is made up at
// kBbdMakeupHz (where the heads pass the most), so a repeat keeps its level.
constexpr float kBbdMakeupHz = 500.0f;
#ifdef RV_BBD_DEFAULT
constexpr int kBbdDefault = RV_BBD_DEFAULT;
#else
constexpr int kBbdDefault = 0; // owner to pick from renders/feat_echo_bbd
#endif
constexpr float kBbdCompAttackMs = 2.0f, kBbdCompReleaseMs = 40.0f;
constexpr float kBbdExpAttackMs = 2.5f, kBbdExpReleaseMs = 36.0f;
constexpr float kBbdWhineDb = -55.0f; // the clock's whine (at the clock), re the signal
// 4 CRUSHED (SDE-3000 digital dub, samplers, Pole's crackle): each pass
// re-sampled at kCrushRateHz without an anti-alias filter and re-quantised
// to kCrushBits (relative to the signal's own level, as a gain-ranging
// sampler: the crunch stays as the repeats fade, never a stuck tone), plus
// sparse crackle riding on the signal.
constexpr float kCrushRateHz = 11300.0f, kCrushBits = 7.0f;
constexpr float kCrushReleaseMs = 120.0f;
constexpr float kCrackleRate = 5.0f, kCrackleLevel = 0.35f; // per second; re the signal's level
#ifdef RV_ECHO_WEAR_DEFAULT
constexpr int kWearDefault = RV_ECHO_WEAR_DEFAULT;
#else
constexpr int kWearDefault = kWearBbd; // owner's pick, 4 Oct 2026 (renders/feat_echo_wear D)

// ---- Bits: the repeats' bit depth (PROTOTYPE, owner 4 Oct 2026) ------------------------------
// The owner kept BBD A (B-D's lower clocks left pitched images in the band:
// "a higher pitched chirp ... after the 2nd repeat") and asked for bit depth
// instead, after Mutable Instruments Beads' "Sunny Tape" (24 kHz / 12-bit)
// and "Scorched Cassette" (24 kHz / 8-bit). dsp/EchoBits.h, on the feedback
// after the BBD: 24 kHz through proper low-pass filters both ways (no new
// pitches), quantised against full scale (each quieter repeat has fewer bits,
// so the grain grows as the echoes fade), rounded toward zero (a fading
// repeat always reaches silence, never a stuck buzz), dithered while the
// signal is well above the bottom bit (soft hiss, not a pitched granulation).
// Renderer key echo_bits_voicing; the firmware builds only kBitsDefault.
struct BitsVoicing {
    float bits;
    bool  muLaw; // 8-bit mu-law (mu 255): steps fine when quiet, coarse when loud
};
constexpr int kNumBitsVoicings = 4;
constexpr BitsVoicing kBits[kNumBitsVoicings] = {
    {24.0f, false}, // A none (BBD A alone)
    {12.0f, false}, // B "Sunny Tape": 24 kHz / 12-bit
    {8.0f, false},  // C "Scorched Cassette": 24 kHz / 8-bit
    {8.0f, true},   // D 24 kHz / 8-bit mu-law: C's grit on the loud repeats, without C's hiss bed
                    //   cutting the quiet ones off (linear 8 bits run out at -42 dBFS)
};
constexpr int   kBitsTaps          = 63;    // the 24 kHz filters (Blackman-windowed sinc)
constexpr float kBitsDitherFadeLsb = 4.0f;  // dither fades out below this many steps of signal
constexpr float kBitsEnvReleaseMs  = 40.0f; // the signal's level for the dither
#ifdef RV_ECHO_BITS_DEFAULT
constexpr int kBitsDefault = RV_ECHO_BITS_DEFAULT;
#else
constexpr int kBitsDefault = 0;
#endif
#endif

// ---- Level -----------------------------------------------------------------------------------
// The wet's trim in echo mode (x position 2's). Since the first-repeat level
// fix the repeats add little at DECAY noon (-6 dB each), so the trim is near
// unity: position 3 within +-2 dB of 2 on hits and skank (measured -0.1 ...
// +0.3 dB K-weighted, test_echo_mode "level"); was 0.84 before the fix.
constexpr float kTrim = 0.98f;

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
