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
//   DECAY   = the echo's feedback (kFeedback*): 0 = one repeat; long builds
//             towards the top; at the very top, in every ATTITUDE (owner,
//             5 Oct), persistent repeats at a roughly constant level held by
//             the tape's saturator (KICKED gets there a little earlier on
//             the knob), never a runaway; they die away naturally when
//             DECAY comes back down.
//   TENSION = the echo time. Unclocked: free, kFreeLongSeconds (TENSION 0)
//             to kFreeShortSeconds (TENSION 1), log. Clocked: TENSION picks
//             one of kDivisionBeats. Turning TENSION up is tighter either
//             way (a tighter tank today, shorter repeats here).
//   gate    = a clock (one pulse = one beat, a quarter note; dsp/EchoClock.h).
//             Positions 1-2 keep the gate's own job there.
//   button  = tap tempo (one tap interval = one beat, ADR 0043; Tank::button).
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
// the clock let go) swoops like tape: the tape speed follows over
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
// Unused by the Tank since v1.0.42 (ADR 0041 amendment "The gate clock
// holds"): both the gate clock and tap tempo hold their tempo when the pulses
// stop, and a lone pulse or tap (none within kClockMinBpm's interval) lets it
// go, back to free time. Kept for EchoClock::update(now) without the hold:
// lost after kClockLostBeats beats without a pulse, at most kClockLostSeconds.
constexpr float kClockLostBeats   = 2.25f;
constexpr float kClockLostSeconds = 2.5f;

// ---- The swoop -------------------------------------------------------------------------------
// The tape speed follows a new time over this long (one-pole on the delay)
// and never faster than kMaxSlew samples of delay per sample (the repeats
// bend within x0.5 .. x1.5 in pitch while it moves).
constexpr float kTimeGlideSeconds = 0.30f;
constexpr float kMaxSlew          = 0.5f;

// ---- Feedback (DECAY) ------------------------------------------------------------------------
// The base curve: kFeedbackMax x DECAY^p, kFeedbackNoon at noon (the
// prototype's page played 0.45). 0 = one repeat. The heads lose a little
// every pass (the high-pass and low-pass are under 1 everywhere, ~0.965 at
// their peak, ~700 Hz), so a pass gains only past ~1.04 (1 / 0.965).
constexpr float kFeedbackMax  = 0.95f;
constexpr float kFeedbackNoon = 0.5f;
// The top: above a start (per ATTITUDE) the feedback rises (smoothstep, no
// corner in the knob) from the base curve to kFeedbackTop at DECAY 1, the
// same top in every ATTITUDE. There a pass gains a little (x ~1.12 at the
// heads' peak) until the record head's softClip holds it: persistent
// repeats at a roughly constant level, held by the tape (the tape never
// carries more than 1 / kTapeDrive), not a runaway; backing DECAY off lets
// them die away like any repeats.
// Owner, 5 Oct 2026 (ADR 0041 amendment): "In KICKED, I like that the decay
// can reach persistent feedback levels around 91 % ... bring this same level
// of feedback to CLEAN and DRIVEN as well", and cap it "to keep it in the
// manageable constant feedback without hitting runaway chaos". kFeedbackTop
// is what KICKED's DECAY 0.92 gave before (its curve then rose to 1.25: from
// DECAY ~0.87 a pass gained more and more, into chaos); the same curve with the lower top keeps the
// knob's travel even (no dead zone at the end).
constexpr float kFeedbackTop = 1.16f;
// KICKED rises from kKickedFrom: a pass gains from DECAY ~0.89, repeats stop
// fading from ~0.90 (measured).
constexpr float kKickedFrom = 0.75f;
// CLEAN and DRIVEN rise later (below kCleanFrom bit for bit as before: the
// long build that still fades, KICKED's earlier tip kept as its own): a pass
// gains from DECAY ~0.93, repeats stop fading from ~0.94 (test_echo_mode
// "feedback" reports where each pass gains).
constexpr float kCleanFrom = 0.85f;

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

// ---- Springs blend (owner's pick 6 Oct 2026: C, 25 % springs, wide) --------------------------
// "The springs add diffusion which makes the individual repeats less
// distinct." The owner heard A (today: the whole wet through the springs),
// 50 / 25 / 0 % through the springs with the rest heard directly, wide or
// ping-pong (renders/echo_springs_blend, docs/prototypes/echo-springs-blend),
// and picked C-wide on every row and ATTITUDE: a quarter of the wet through
// the springs, three quarters the tape's repeats heard directly, wide.
//
// The wet is s x (the springs' output) + (1 - s) x (the direct repeats),
// then x makeup (per ATTITUDE, blended with the Morph). The direct repeats
// are the tape's playback only (the dry hit already reaches the output
// through MIX), joined at the springs' output just before the output
// pickups (DriveOut), so they go through everything after the tank as the
// springs' output does: DriveOut, the shelf, the mu-law box, TONE's return,
// the limiter, the Hold's ducking, MIX. They follow DRIVE's heard gain and
// kTrim like the springs' output. The springs keep hearing the input plus
// the tape's (mono) repeats, as before.
//
// makeup: the owner judged the versions level-matched, so the shipped one
// is as loud as A was (test_echo_mode "level", "blendlevel"): with only a
// quarter of the springs the wet loses their splash of each hit and their
// ring, ~10-12 dB. It goes on the whole blended wet, after the output
// pickups (DriveOut), so the springs : direct balance and the pickups'
// colour stay as the owner heard them; only the wet's level moves.
//
// Wide (dsp/EchoDirect.h): two playback heads, the image centred. Above
// kWideSplitHz L plays the repeats a touch darker (kWideDarkHz) and R plays
// them kWideMs later, at full brightness x kWideRTrim (L and R equally loud
// on average); below it L and R are the same. In mono the 8 ms between them
// is a fine comb on the repeats' highs (notches every 125 Hz): the price of
// the width. Ping-pong (Renderer only): two tapes cross-fed inside the
// feedback, repeat 1 left, 2 right ...
// Renderer key echo_springs_voicing; the firmware builds only the default.
enum class DirectStyle { None, Wide, PingPong };
struct SpringsBlendVoicing {
    float       springs;   // s: the springs' share of the wet
    DirectStyle style;
    float       makeup[3];  // the wet x this at DECAY noon, CLEAN / DRIVEN / KICKED (linear)
    float       makeupFbDb; // ... changed by this many dB per unit of feedback above noon's (kFeedbackNoon)
};
constexpr int kNumSpringsBlendVoicings = 7;
constexpr SpringsBlendVoicing kSpringsBlend[kNumSpringsBlendVoicings] = {
    {1.0f, DirectStyle::None, {1.0f, 1.0f, 1.0f}, 0.0f},           // 0 A: the whole wet through the springs (until 6 Oct 2026)
    {0.5f, DirectStyle::Wide, {1.0f, 1.0f, 1.0f}, 0.0f},           // 1 B-wide: half through the springs, half direct (prototype)
    {0.25f, DirectStyle::Wide, {3.33f, 3.39f, 3.30f}, -8.0f},// 2 C-wide: a quarter through the springs (the owner's pick, the default)
    {0.0f, DirectStyle::Wide, {1.0f, 1.0f, 1.0f}, 0.0f},           // 3 D-wide: the tape echo alone, no springs (prototype)
    {0.5f, DirectStyle::PingPong, {1.0f, 1.0f, 1.0f}, 0.0f},       // 4 B-pp (prototype; Renderer only)
    {0.25f, DirectStyle::PingPong, {1.0f, 1.0f, 1.0f}, 0.0f},      // 5 C-pp
    {0.0f, DirectStyle::PingPong, {1.0f, 1.0f, 1.0f}, 0.0f},       // 6 D-pp
};
#ifdef RV_ECHO_SPRINGS_DEFAULT
constexpr int kSpringsBlendDefault = RV_ECHO_SPRINGS_DEFAULT;
#else
constexpr int kSpringsBlendDefault = 2; // owner's pick, 6 Oct 2026 (renders/echo_springs_blend C, every row and ATTITUDE)
#endif
#if defined(RV_FIXED_VOICINGS)
static_assert(kSpringsBlend[kSpringsBlendDefault].style != DirectStyle::PingPong, "the firmware has no second tape");
#endif
constexpr float kWideMs      = 8.0f;    // R's head behind L's (Haas range: one wide repeat, not two)
constexpr float kWideDarkHz  = 6000.0f; // L's head a touch darker
constexpr float kWideSplitHz = 250.0f;  // below this L and R are the same (one-pole split)
constexpr float kWideRTrim   = 0.93f; // R's highs: L and R equally loud on average

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
constexpr int kNumWearVoicings = 8;
constexpr int kWearNone = 0, kWearTape = 1, kWearRadio = 2, kWearBbd = 3, kWearCrushed = 4;
// Tape wear round (PROTOTYPE, owner 5 Oct 2026; see "Tape wear" below):
// 5 tape saturation + roll-off, 6 the same + crinkle (subtle), 7 + crinkle (obvious).
constexpr int kWearTapeSat = 5, kWearCrinkle = 6, kWearCrinkleHeavy = 7;
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
// ---- Tape wear (PROTOTYPE, owner 5 Oct 2026) ----
// "Given our modulation knob already provides wow and flutter, adding more
// doesn't feel like the right fit. However maybe a crinkle effect like the
// magneto, or the tape-style saturation and roll off." The BBD's aliasing
// adds new, inharmonic pitches no tape machine makes; these wear like tape
// instead. No wow or flutter (WOBBLE already moves the tape), no new pitches
// (nothing folds back: the feedback has already been through the heads'
// low-passes, and the saturator is gentle), no added energy.
// 5 TAPE SATURATION + ROLL-OFF (Space Echo, Black Ark): the tape's own
// record EQ. The highs are boosted on the way onto the tape (pre-emphasis,
// a 1-pole shelf, +kTapeSatEmphDb above ~kTapeSatEmphHz), saturated (softClip,
// unity when quiet), and cut back by the exact inverse on playback
// (de-emphasis). A quiet repeat comes back unchanged; a loud, bright one has
// had its highs squashed first (high-frequency compression): thicker and
// duller. Then a little more treble lost every pass (1-pole, kTapeSatRollHz) and the
// playback head's bump (a gentle lift around 50-100 Hz, kTapeSatBumpDb at
// its peak; the heads' 140 Hz high-pass still thins the lows each pass, so it
// slows that loss, never a boom). Level: gain 1 at the mids' loudest (above
// kTapeSatMakeupFromHz), never over it there, as the heads' peak sets the held
// top.
constexpr float kTapeSatEmphHz   = 1600.0f; // pre-emphasis corner (a Space Echo's slow speed is ~1.5-2 kHz)
constexpr float kTapeSatEmphDb   = 8.0f;   // the shelf's boost of the highs going onto the tape (cut back after)
constexpr float kTapeSatDriveK   = 1.25f;    // softClip(k e) / k on the emphasised signal: unity when quiet
constexpr float kTapeSatRollHz   = 6500.0f; // 1-pole low-pass per pass: a little more treble lost every pass
constexpr float kTapeSatBumpHz   = 70.0f, kTapeSatBumpQ = 1.0f; // the head bump (a peaking filter: back to 0 dB by ~300 Hz)
constexpr float kTapeSatBumpDb   = 1.5f;    // the bump's lift at its peak, re the mids
constexpr float kTapeSatMakeupFromHz = 300.0f; // gain <= 1 from here up, = 1 at its loudest
// 6, 7 CRINKLE: 5 plus wrinkled tape briefly losing contact with the head.
// Crinkled patches (kCrinklePatch*: a few per second, tens of ms long, the
// gaps between them random) and inside each, fast irregular flickers
// (kCrinkleFlick*: ~1-4 ms each, at random) where the level AND the highs
// dip together (spacing loss: the highs lose far more than the lows; the
// split at kCrinkleSplitHz): a papery, broken-up texture. Pitch untouched.
// Faster and rougher than the worn tape's dropouts (3-40 ms, 2 per second).
// Each pass gets its own, so older repeats (more passes, more tape) crinkle
// more. Seeded: renders repeat exactly.
struct CrinkleVoicing {
    float patchesPerSecond;           // crinkled patches (on average)
    float patchMinMs, patchMaxMs;     // each patch's length
    float flicksPerSecond;            // flickers inside a patch (on average)
    float flickMinMs, flickMaxMs;     // each flicker's length
    float depthMin, depthMax;         // a patch's depth, 0..1 (1 = the highs gone, the level kCrinkleLevelDip down)
};
constexpr CrinkleVoicing kCrinkle[2] = {
    {2.5f, 20.0f, 80.0f, 150.0f, 0.5f, 2.5f, 0.3f, 0.7f},   // 6 C1 subtle: ~1/8 of the tape crinkled, a short papery flutter now and then
    {3.5f, 30.0f, 150.0f, 220.0f, 0.7f, 4.0f, 0.5f, 0.95f}, // 7 C2 obvious: ~1/3 of the tape crinkled, older repeats clearly broken up
};
constexpr float kCrinkleSplitHz  = 1200.0f; // below: the lows (lose kCrinkleLevelDip at depth 1); above: the highs (lose all of it)
constexpr float kCrinkleLevelDip = 0.5f;    // the whole level's dip at depth 1 (-6 dB)
// The held top with tape wear (owner, 6 Oct 2026: "lock in like today"). At
// the top of DECAY a pass gains a little (the heads' peak x kFeedbackTop ~
// 1.12) and the tape's saturation holds the repeats' peaks; but each held
// repeat's quieter edges gain too, so on a clean tape the repeats slowly
// spread in time until they run into one another (the held level crept ~1 dB
// a minute and never settled). The BBD held them because of its compander,
// so tape wear takes the same compander there, and only there: a 2:1
// compressor and a 1:2 expander following the level at slightly different
// speeds (unity on a steady level; a little less on each repeat's rise and
// fall), blended in by holdWeight: 0 where a pass can't gain (below it, bit
// for bit as before), 1 at kFeedbackTop. No sample-and-hold, no filters: it
// adds no pitch, it only shapes each repeat's level.
constexpr float kTapeHoldCompAttackMs = 2.0f, kTapeHoldCompReleaseMs = 40.0f; // the BBD's (kBbdComp*)
constexpr float kTapeHoldExpAttackMs  = 2.5f, kTapeHoldExpReleaseMs  = 36.0f; // the BBD's (kBbdExp*)
// And dense material (a skank, chords) still crept up slowly (~1.3 dB over
// 30-120 s): above kTapeHoldRms (the feedback's RMS over ~kTapeHoldRmsMs),
// each pass gives back a little (gain 1 / (1 + q (P / P0 - 1)), q =
// kTapeHoldRmsQ), so the held top sits at about that level whatever is held.
constexpr float kTapeHoldRms   = 0.07f;  // -23 dBFS on the tape
constexpr float kTapeHoldRmsMs = 300.0f; // slow: no pumping on single repeats
constexpr float kTapeHoldRmsQ  = 0.1f;
constexpr float kHeadsPeakGain         = 0.965f; // the heads' gain at their peak (~700 Hz; "Feedback" above)
// 0 where a pass can't gain (feedback x the heads' peak <= 1), 1 at kFeedbackTop.
inline float holdWeight(float fb)
{
    const float w = (fb * kHeadsPeakGain - 1.0f) * (1.0f / (kFeedbackTop * kHeadsPeakGain - 1.0f));
    return w <= 0.0f ? 0.0f : (w < 1.0f ? w : 1.0f);
}
#ifdef RV_ECHO_WEAR_DEFAULT
constexpr int kWearDefault = RV_ECHO_WEAR_DEFAULT;
#else
constexpr int kWearDefault = kWearTapeSat; // owner's pick, 6 Oct 2026 (renders/echo_tape_wear B, every row and ATTITUDE); was kWearBbd (4 Oct)

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
inline float feedbackBase(float decay)
{
    if (decay <= 0.0f) return 0.0f;
    const float p = std::log(kFeedbackNoon / kFeedbackMax) / std::log(0.5f); // noon -> kFeedbackNoon
    return kFeedbackMax * std::exp(p * std::log(decay < 1.0f ? decay : 1.0f));
}
// The base curve (c = feedbackBase(decay)) up to `from`, then the smoothstep
// rise to kFeedbackTop. (The Tank reads the base once for both ATTITUDE curves.)
inline float feedbackRise(float c, float decay, float from)
{
    float u = (decay - from) * (1.0f / (1.0f - from));
    if (u <= 0.0f) return c;
    u = u < 1.0f ? u : 1.0f;
    return c + u * u * (3.0f - 2.0f * u) * (kFeedbackTop - c);
}
inline float feedbackClean(float decay) { return feedbackRise(feedbackBase(decay), decay, kCleanFrom); }   // CLEAN and DRIVEN
inline float feedbackKicked(float decay) { return feedbackRise(feedbackBase(decay), decay, kKickedFrom); } // KICKED

} // namespace rv::echo
