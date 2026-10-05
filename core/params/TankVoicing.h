#pragma once
#include <cmath>
// Tank voicings: Wellspring fit rounds 3 and 4 (docs/m8-tuning-backlog.md
// "Wellspring fit round 3" / "round 4" / "Wellspring F merge", ADR 0038,
// accepted 2 Oct 2026). The owner picked 7 ("F, plus gentler"): the plugin
// and the firmware play kDefaultVoicing, and the firmware compiles only that
// one (RV_FIXED_VOICINGS, core/dsp/Drive.h); 0-6 are Renderer-only references
// (Tank::setTankVoicing(v), the hidden "tank_voicing" key, host/common/
// ParamsJson.h). What 7 took on to ship is at the end of Tuning ("7 as
// shipped").
//
// What the owner hears (2 Oct 2026, their Wellspring next to our closest
// knob settings): the Wellspring "sounds more diffuse", "further away and
// gentler in tone" (ours has more low end / low mids, "present and
// forward"), and its "repeats diffuse faster whilst ours flicker back and
// forth from left to right". Each voicing adds one fix to the one before, so
// each fix can be heard on its own:
//
//   0 today
//   1 Sweep          proto/wellspring-fit's Sweep (version B, the owner's pick):
//                    one chain of "highs later" allpass sections in front of
//                    the Springs, shared by them, so every echo is the same
//                    smooth "pew"; the Loops keep fewer sections (the Sweep
//                    carries most of the Chirp) and their first echoes keep
//                    their time (ADR 0029).
//   2 + together     both outputs hear every Spring: no Spring panned to one
//                    side (that put alternate echoes in alternate ears, the
//                    "flicker"); the width comes from the decorrelator D only,
//                    which is mono-safe by construction (SpringModes.h), made
//                    stronger, and kept off the bass (bass centred like the
//                    Wellspring's).
//   3 + diffusion    a few short allpasses on each Loop's feedback (after the
//                    pickup): every trip round the Loop smears each echo a
//                    little more, so the repeats blur into a wash within a few
//                    hundred ms, while the first echo (never through them) and
//                    the echo spacing (L shortened by their delay) stay put.
//   4 + gentler      the tail's 150-800 Hz brought down to the Wellspring's
//                    balance (a low cut in front of the Springs), and 2-4 kHz
//                    lasting a little longer (less Loop damping, a longer
//                    high path), towards the Wellspring's T60 shape.
//
// Round 4 (docs/m8-tuning-backlog.md "Wellspring fit round 4"): the owner
// heard the Wellspring "more muted with less highs", its repeats darkening
// while ours "sound more metallic and bright", and its stereo wider. 5-7
// build on 3 (not on 4), so the transducers can be heard without 4's low cut:
//
//   5 = 3 + transducers  a real tank drives the spring through a coil and a
//                    magnet and picks it up the same way at the other end;
//                    both lose treble (the coil's inductance, the magnet's
//                    mass), so every sound and every echo is filtered twice
//                    and the highs are gentle from the first moment. Ours was
//                    nearly flat at both ends. Here: a resonant low-pass on
//                    everything going into the Springs (after the Clang;
//                    the Kick's knock bypasses it) and one on the wet coming
//                    out (before the pickups' DriveOut), and inside the tank
//                    the highs lose less per trip (less Loop damping, a
//                    longer high path) so the tail keeps its balance and the
//                    repeats darken gently instead of starting bright.
//   6 = 5 + wide again   the tail as wide as the Wellspring's without the
//                    flicker: the Springs' difference (A - B, panned in 0)
//                    comes back, but through its own decorrelator, so it
//                    widens the fine structure and never puts one Spring's
//                    echo in one ear. Mono is still exactly the mid.
//   7 = 6 + gentler      4's low cut (the tail's 150-800 Hz down) on top of
//                    6, with a level makeup (power into / out of the low cut)
//                    so low material isn't 3 dB quieter. (4's Loop damping
//                    and high path T60 are 5's business here.)
//
// Every number for the new versions lives here. Desktop builds can override
// them before a Tank is prepared (Tuning, mutableTuning(); the Renderer reads
// RV_TANKV_TUNE, used by docs/prototypes/wellspring-fit-3/ to fit them);
// the firmware never can.

// What this build can play, for the preprocessor (the voicings build on each
// other, so one number says it): the firmware (RV_FIXED_VOICINGS) builds only
// its default voicing's parts (7: everything up to 7); desktop builds hold
// all of them.
#ifdef RV_FIXED_VOICINGS
#ifdef RV_TANK_DEFAULT_VOICING
#define RV_TANKV_BUILT RV_TANK_DEFAULT_VOICING
#else
#define RV_TANKV_BUILT 8 // the default, kR5 (ADR 0038 Round 5, owner pick 5 Oct 2026)
#endif
#else
#define RV_TANKV_BUILT 10 // desktop: every voicing, round 5's (8-10) included
#endif

namespace rv::tankv {

constexpr int kToday    = 0;
constexpr int kSweep    = 1;
constexpr int kTogether = 2;
constexpr int kDiffuse  = 3;
constexpr int kGentle   = 4;
constexpr int kTransducers = 5;
constexpr int kWide        = 6;
constexpr int kGentleWide  = 7;
// Wellspring fit round 5 (ADR 0038 "Round 5", proposed; docs/m8-tuning-
// backlog.md "Wellspring round 5"), all on top of 7 as shipped (F round 2's
// low cut step included): 8 = "B" the front, the resonance and the stereo
// image; 9 = "C" = 8 + held sounds that settle flat; 10 = "D" halfway from 7
// to 9 (Tuning "Round 5").
constexpr int kR5         = 8;
constexpr int kR5Flat     = 9;
constexpr int kR5Half     = 10;
constexpr int kNumVoicings     = 11;
// The owner's pick (2 Oct 2026, Wellspring fit round 4, "F, plus gentler";
// ADR 0038 Decision): 7. The plugin and the firmware play it; 0-6 stay as
// Renderer-only references (tank_voicing). RV_TANK_DEFAULT_VOICING (a scratch
// build's CMAKE_CXX_FLAGS) makes another voicing the default, so the whole
// test suite can be run as if it shipped (docs/prototypes/wellspring-fit-3/).
#ifdef RV_TANK_DEFAULT_VOICING
constexpr int kDefaultVoicing = RV_TANK_DEFAULT_VOICING;
#else
constexpr int kDefaultVoicing  = kR5; // round 5's B (owner, 5 Oct 2026: "B" in every row; ADR 0038 Round 5)
#endif

constexpr bool hasSweep(int v) { return v >= kSweep; }
constexpr bool hasTogether(int v) { return v >= kTogether; }
constexpr bool hasDiffusion(int v) { return v >= kDiffuse; }
// 5-7 build on 3: 4's low cut comes back in 7 (its Loop damping and high
// path T60 don't: 5 has its own).
constexpr bool hasGentle(int v) { return v == kGentle; }
constexpr bool hasLowCut(int v) { return v == kGentle || v >= kGentleWide; }
constexpr bool hasTransducers(int v) { return v >= kTransducers; }
constexpr bool hasWide(int v) { return v >= kWide; }
constexpr bool hasGentleMakeup(int v) { return v >= kGentleWide; }
// What 7 took on to ship (ADR 0038 Decision): TONE's re-map, the coil's
// square term high-passed, the low cut's makeup read on the input too, and
// the wet trim after the pickups. 5 and 6 stay as round 4 played them.
constexpr bool hasShipFixes(int v) { return v >= kGentleWide; }
// Round 5 (8-10): how far each of its changes goes, 0 = 7 as shipped, 1 =
// the full change (D, 10, goes half way). flat: the held-sound part (C, 9,
// and D's half of it).
constexpr bool  hasR5(int v) { return v >= kR5; }
constexpr float r5Amount(int v) { return v < kR5 ? 0.0f : v == kR5Half ? 0.5f : 1.0f; }
constexpr float r5FlatAmount(int v) { return v == kR5Flat ? 1.0f : v == kR5Half ? 0.5f : 0.0f; }
// The stereo image (r5Wide*, r5DiffScale) goes all the way in D
// too: half of it put D(mid) back at half weight, and with it the tones'
// left / right lean (500 Hz +3.2 dB) and out-of-phase fronts.
constexpr float r5StereoAmount(int v) { return v >= kR5 ? 1.0f : 0.0f; }

// Diffusers per Loop (voicing 3).
constexpr int kNumDiffusers = 3;

struct Tuning {
    // ---- 1 Sweep (shared, in front of every Spring's Loop and high path) ----
    // Sections at TENSION noon; x sweepTightScale at the tight end, x
    // sweepLooseScale at the loose end (linear in between, as the fit
    // branch). Its top (fC) = the Loop's fC (Mappings.h tensionTransitionHz)
    // x sweepFcRatio. Coefficient a > 0: highs later.
    // Re-fitted on today's tank (docs/prototypes/wellspring-fit-3/
    // fit_sweep.py, fit_sweep.json: the Wellspring's first arc and arrival
    // times at the closest settings). It lands on B's own numbers: 40
    // sections at noon (B 41), a 0.35 (B 0.37), top 2.8 kHz at noon and
    // 3.6 kHz at TENSION 0.875 (B 3.6 kHz at noon, on its higher Loop fC).
    float sweepStagesNoon = 40.0f;
    float sweepTightScale = 0.6f;
    float sweepLooseScale = 1.25f;
    float sweepFcRatio    = 0.85f;
    float sweepCoeff      = 0.35f;
    // The Loops' share of (cap - floor) at noon and at the loose end (today
    // 0.40 / 1.0, Mappings.h kTensionStageFracMid; the tight end stays at
    // the 24-section floor, ADR 0007). B's: 0.261 / 0.385, so the worst case
    // (3 Springs, loosest: Sweep 50 + 3 x 35 sections) costs about today's
    // (3 x 52).
    float loopFracNoon  = 0.261f;
    float loopFracLoose = 0.385f;
    // The high path (B's): high-pass at hiXoverRatio x the Loop's fC (today
    // 0.8; here 1.2 kHz at noon, 1.5 kHz at TENSION 0.875, B 1.5 kHz), its
    // pickup aligned on the Loop's first echo at that frequency plus
    // hiAlignMs (B 3.7 ms). Without it the high path's first echo came a few
    // ms ahead of the Loop's, an undispersed copy on top of the arc (round 1
    // of the fit could not draw the arc past ~3.5 kHz).
    float hiXoverRatio = 0.36f;
    float hiAlignMs    = 3.7f;
    // The pickups move back by the Sweep's delay at this frequency, less
    // what the Loops' fewer sections no longer delay there, so the first
    // echo's body keeps today's time (ADR 0029). (At the Loops' own
    // alignment frequency, modes::kPickupAlignHz 800 Hz, near the loose
    // Sweep's top, the body came ~2.6 ms early.)
    float sweepAlignHz = 400.0f;

    // ---- 2 together ----
    // Decorrelator weight w per mode (L = mid + w D, R = mid - w D; today
    // 0.75 / 0.65 / 0.65), with the side (Spring A left, B right) at 0. The
    // L/R correlation of the fine structure is about (1 - w^2) / (1 + w^2):
    // 0.16 at 0.85 (Wellspring tail -0.04; today's 2 Springs 0.02, from the
    // side). w stays below 1: each ear hears mid + w D, which dips by
    // 1 - w where D turns against mid (-16 dB at 0.85, a full notch at 1).
    float togetherW1 = 0.85f, togetherW2 = 0.85f, togetherW3 = 0.85f;
    // D is high-passed here (one-pole) so the bass stays in the middle.
    float togetherBassHz = 150.0f;

    // ---- 3 diffusion (Schroeder allpasses on each Loop's feedback) ----
    // Delays (ms) for Spring A; B and C scale them (diffSpringScale) so no
    // two Loops share a comb. Coefficient c. Sized by docs/prototypes/
    // wellspring-fit-3/fit_diffusion.py (fit_diffusion.json): echo density
    // 0.84 / 0.97 at 100-200 / 300-500 ms (Wellspring 0.77 / 0.97; today
    // 0.64 / 0.66); c 0.3 or longer delays overshoot to a wash by 100 ms
    // (0.94-1.0). 667 floats of pool for the three Springs.
    float diffMs[kNumDiffusers] = {0.85f, 1.45f, 2.15f};
    float diffSpringScale[3]    = {1.0f, 1.13f, 0.89f};
    float diffCoeff             = 0.2f;

    // ---- 4 gentler ----
    // Sized by docs/prototypes/wellspring-fit-3/fit_gentle.py (fit_gentle.json),
    // DECAY re-fitted per cell to the Wellspring's broadband T60: tail
    // low-mid balance -5.9 dB (Wellspring -6.0; today -1.7), per-octave T60
    // within 14 % on average (today 15 %; 250 Hz-4 kHz 3.7 / 3.5 / 3.3 /
    // 2.6 / 1.2 s, Wellspring 4.7 / 3.8 / 3.3 / 2.7 / 1.9). Tried and dropped: lows that ring
    // longer than DECAY asks (a low shelf inside each Loop, the Loop gain
    // design allowing 1.5 x T60 below 250 Hz; 250 Hz T60 4.2 s, Wellspring
    // 4.7): it breaks AntiRes layer 1 (no band longer than designed) and
    // stretches DECAY 1 to 12 s (ADR 0001: 8-10).
    // A low cut in front of the Springs (after TONE's Tilt): 2nd-order
    // high-pass at gentleHpHz, Q gentleHpQ (real tanks roll off below
    // ~160-250 Hz: "Sonic signature vs real springs"), plus an RBJ low shelf
    // at gentleShelfHz of gentleShelfDb (the 150-800 Hz body).
    float gentleHpHz    = 260.0f;
    float gentleHpQ     = 0.6f;
    float gentleShelfHz = 600.0f;
    float gentleShelfDb = -3.0f;
    // The high path's T60 re DECAY's (today Spring::kHighT60Ratio 0.45).
    float gentleHighT60Ratio = 0.7f;
    // The Loop's damping cutoff x this (TONE's, Mappings.h toneDampingHz;
    // clamped at 0.45 fs): 2-4 kHz lose less per trip and last longer
    // (2 kHz T60 2.6 s, Wellspring 2.7; today 2.2). The top octave stays
    // short (1.2 s, Wellspring 1.9): the Loop's fC low-pass (4.4 kHz at
    // TENSION 0.875) takes it on every trip, and moving fC is the Chirp's
    // business, not this voicing's.
    float gentleDampingScale = 1.5f;

    // ---- 5 transducers ----
    // Input coil: a 2nd-order low-pass (corner tdInHz, resonance tdInQ) on
    // what enters the Springs (Loop and high path, after the Clang); output
    // pickup: the same shape (tdOutHz, tdOutQ) on the wet L and R before
    // DriveOut. Sized by docs/prototypes/wellspring-fit-4/fit_transducers.py
    // (the Wellspring's onset and tail spectra; see fit_transducers.json).
    float tdInHz  = 2350.0f;
    float tdInQ   = 1.0f;
    float tdOutHz = 4500.0f;
    float tdOutQ  = 0.67f;
    // The coil's even-order colour (magnetic, transformer-like: the
    // Wellspring's sweep take shows 2nd harmonic -25 dB, 3rd -36): u + g x
    // (u^2 - m), m = the slow average of u^2 (20 Hz), g = tdEven / (1 +
    // tdEvenEase x m): a plain square (only doubled frequencies, nothing
    // aliases), no thump, easing off on loud input (at DRIVE 1 its
    // intermodulation otherwise reached -47 dB in test_drive's aliasing
    // check). On what enters the Springs, before the coil's low-pass. 0 = none.
    float tdEven     = 1.1f;
    float tdEvenEase = 32.0f;
    // Inside the tank: the Loop's damping cutoff x tdDampingScale and the
    // high path's T60 at tdHighT60Ratio x DECAY's (today 1 / 0.45), so the
    // highs the transducers let through last (the repeats darken slowly).
    float tdDampingScale  = 3.6f;
    float tdHighT60Ratio  = 1.5f;
    // The high path's ceiling (today Spring::kHighCeilingHz, 9 kHz): its
    // echoes start with the arc, not a click.
    float tdHighCeilHz    = 9000.0f;
    // The wet's level (dB): less Loop damping keeps more energy in the tail
    // (noise bursts at DECAY 1, tightest TENSION came back ~3.5 dB louder
    // than voicing 3 and tripped the M6 grid's steady_tone check, a mode
    // above -30 dBFS for > 2 s); -2.5 dB clears it in 5-7.
    float tdTrimDb        = -2.5f;
    // The high path's level x this (TONE's, Mappings.h toneHighPathLevel).
    float tdHighLevel     = 0.9f;

    // ---- 6 wide again ----
    // L = mid + X, R = mid - X, X = bass-cut(wideW x D(mid) + wideSide x D2(A - B)):
    // the Springs' difference through its own decorrelator D2 (Schroeder
    // allpasses, wideDecorrMs, coefficient wideDecorrCoeff), so its echoes
    // are smeared across both ears instead of panned. wideSide is the side
    // gain k (today's 2-Spring k 0.36; 3 Springs x wideSide3 / wideSide).
    float wideW     = 0.7f;
    float wideSide  = 0.38f;
    float wideSide3 = 0.45f;
    float wideDecorrMs[3] = {1.7f, 2.9f, 4.3f};
    float wideDecorrCoeff = 0.5f;

    // ---- 7 gentler, with makeup ----
    // 4's low cut (same shape: high-pass + low shelf, after TONE's Tilt),
    // re-sized on top of 6 (lc*), whose transducers already moved the
    // low-mid balance most of the way. Power into and out of it (slow
    // followers, drive::kExcSeconds) is made up, up to gentleMakeupMaxDb.
    float lcHpHz    = 220.0f;
    float lcHpQ     = 0.6f;
    float lcShelfHz = 300.0f;
    float lcShelfDb = -3.0f;
    float gentleMakeupMaxDb = 6.0f;
    // The share of the measured loss given back (dB x this): the Springs
    // ring their lowest notes louder than the rest (the response's 100-160 Hz
    // bump), so the power going in under-reads what the low cut takes out
    // of the tail.
    float gentleMakeupShare = 1.0f;

    // ---- 7 as shipped (ADR 0038 Decision) ----
    // The coil's square term through a high-pass at tdEvenHpHz (noon and
    // left), rising to toneBrightEvenHpHz at TONE fully right (weight
    // toneBrightWeight): it keeps the doubled frequencies (the 2nd harmonic of
    // anything the low cut lets through) and drops the difference tones
    // between them, which landed in the Springs' loudest low notes. With the
    // Big Knob thinning the input, they were most of what was left below
    // 150 Hz (TONE fully right: lows -5.5 dB re noon, test_drive's bar -8).
    // Higher at noon it moves the picked sound (250 Hz: the click take's
    // low-mid balance +1.4 dB; 60 Hz: +0.4), so it sits at 25 Hz (only the
    // sub-bass difference tones) and rises with the Big Knob. 0 = none.
    float tdEvenHpHz         = 25.0f;
    float toneBrightEvenHpHz = 250.0f;
    // TONE re-map. Round 4's numbers are noon's and everything right of it;
    // left of noon they ease, by toneDarkWeight (1 at TONE 0, 0 from noon,
    // shape toneDarkCurve), toward TONE fully left's: today's Loop damping
    // (x toneDarkDampingScale), today's short high path (toneDarkHighT60Ratio
    // x DECAY), a darker coil (toneDarkInHz), so TONE fully left is about as
    // dark as today's. Its level: the darker tank takes the highs, so it gives
    // back toneDarkShare x the dB a one-pole low-pass at toneDarkLpHz takes
    // out of the input (slow followers, as the Big Knob's makeup), up to
    // toneDarkDb, x the weight: a snare comes back about as loud as at noon, a
    // held pad (nothing to lose up there) isn't pushed at the limiter. A fixed
    // +5 dB did both. Right of noon (weight
    // toneBrightWeight, shape toneBrightCurve) the output pickup opens toward
    // toneBrightOutHz, so the Big Knob's top comes through as today's does,
    // and toneBrightDb keeps the level. Both levels on the Springs' input.
    float toneDarkDampingScale = 1.0f;
    // At the top of DECAY the Loop damping scale eases (smoothstep from
    // tdDampingDecayFrom) to tdDampingDecayMaxScale at DECAY 1: x3.6 let
    // 1-2 kHz ring as long as a DECAY-max tail (8-10 s), so a mode held on
    // (SPRINGS 3 coupled: a steady 2.1 kHz tone, test_springs3 "Ringing")
    // and a held tone's tail beat so deeply that WOBBLE's steps read out of
    // order (test_m7_tank). Real springs' highs always fade first. DECAY up
    // to 0.7 (the picked 0.665 too) is round 4's.
    float tdDampingDecayFrom     = 0.7f;
    float tdDampingDecayMaxScale = 1.5f;
    // WOBBLE's left side (random wow, in the Loops) at the top of DECAY
    // (same easing): x this at DECAY 1. In 7's denser DECAY-max tail the wow
    // builds up over more round trips than the right side's vibrato: fully
    // left read 1.6x fully right there (test_m7_tank, limit 1.6; today 1.47).
    float tdWobbleLeftDecayMax   = 0.9f;
    float toneDarkHighT60Ratio = 0.45f;
    float toneDarkInHz         = 1800.0f;
    float toneDarkDb           = 9.0f;  // the makeup's cap at TONE 0
    float toneDarkShare        = 1.6f;  // dB back per dB the low-pass takes from the input
    float toneDarkLpHz         = 500.0f;
    float toneDarkCurve        = 0.5f;
    float toneBrightOutHz      = 9000.0f;
    float toneBrightDb         = 0.0f;
    float toneBrightCurve      = 4.0f;  // steep: TONE 0.7 (the owner's F) moves < 0.1 dB
    // KICKED drives the coil hard enough to saturate it: its core loses
    // inductance, so it loses less treble. In KICKED (ATTITUDE weight) the
    // coil's corner rises by up to tdDriveOpenOct octaves with DRIVE (weight
    // DRIVE^3: DRIVE 0.25 moves it 1 %), so DRIVE's grit still comes through
    // (test_drive "DRIVE audibility" KICKED: the coil had made DRIVE 1 sound
    // like a louder DRIVE 0), and it gives up tdDriveOpenDb of level at
    // DRIVE 1, so the tail grows by DRIVE's +6 dB and no more (ADR 0033).
    // CLEAN and DRIVEN keep the coil as it is (CLEAN stays mild, ADR 0022).
    float tdDriveOpenOct       = 0.6f;
    float tdDriveOpenDb        = 1.5f;
    // SPLASH at low DRIVE: the transducers darken the crash against the
    // tail around it, most with DRIVE fully down (test_m7_tank "SPLASH
    // stronger", KICKED DRIVE 0: -2.3 dB re today). The Clang and the
    // Clatter are lifted by this at DRIVE 0, easing to none by DRIVE 0.8
    // ((1 - DRIVE / 0.8)^2), the Clang past its ceiling (a lift under the
    // ceiling did nothing: the ceiling took it back).
    // 4 dB: KICKED at DRIVE 0 splashes as today's (+10.3 vs +10.4 dB on the
    // -6 dBFS rim, SPLASH 0.75), CLEAN 2 dB more; DRIVE 0 and 0.8 within
    // 3 dB of each other for SPLASH voicings 2 and 3 (they were 3.1-4.3).
    float tdSplashLiftDb       = 4.0f;
    float tdSplashLiftFrom     = 0.5f;
    float tdSplashLiftTo       = 0.8f;
    float tdSplashLiftKickedDb = 3.0f;

#if RV_TANKV_BUILT >= 8 // (the firmware with 7 as its default carries none of it)
    // ---- Round 5 (8-10; ADR 0038 "Round 5", proposed) ----
    // Fitted to the owner's Wellspring takes of session 2 (J tone bursts, K
    // pink noise, L held tones, M pad) and session 1 (A clicks) at the
    // session-2 settings (2 Springs, CLEAN, MIX 1, DECAY 0.70, TONE / TENSION
    // noon, the rest at the panel defaults) by docs/prototypes/wellspring-fit-5/
    // fit5.py (coordinate search, score in its header; r5.py / r5an.cpp
    // measure). 7's value is what each one is at r5Amount 0; D (10) goes half
    // way. Numbers before -> after: docs/m8-tuning-backlog.md "Wellspring
    // round 5".
    //
    // The front (target 1). The Loop diffusers move in front of the pickup,
    // so the first echo is smeared too and the next echoes fill in sooner:
    // on the clicks, the first 100 ms stand 7.0-8.3 dB over the next 400 ms
    // at 125-500 Hz in 7, 4.0-4.5 here (the Wellspring 4.0-4.1). Same three
    // allpasses per Spring (no new stage); longer (8.0 ms in all: what the
    // firmware's pool holds, Spring C getting none where it never runs) and
    // stronger. The pickup stays (r5DiffAlign 0, fitted): the smear's direct
    // part keeps the first echo's time, its body comes a few ms later.
    float r5DiffMs[kNumDiffusers] = {1.9f, 2.6f, 3.5f};
    float r5DiffCoeff = 0.5f;
    // Per Spring (A, B, C): in front of the pickup, unlike diffusers smear
    // the Springs' first echoes differently, so the front comes out wide;
    // alike, the front is centred and the Springs part as their Loops drift
    // apart (the Wellspring's front: 125-500 Hz centred).
    float r5DiffScale[3] = {1.0f, 1.0f, 0.89f};
    float r5DiffAlign = 0.0f;
    // ... and in the Hold (its zone weight) they ease to this coefficient
    // (Tank.cpp updateSpringSettings: the Hold's ducking, ADR 0040).
    float r5HoldDiffCoeff = 0.15f;
    // WOBBLE's left side at DECAY 1 (7's tdWobbleLeftDecayMax 0.9, same
    // easing): fully left read 1.63x fully right at DECAY 1 (test_m7_tank,
    // bar 0.6..1.6; 7 1.53).
    float r5WobbleLeftDecayMax = 0.8f; // hump: +0.49 dB at 7's 0.2, -0.46 here, 7 itself -0.54 (bar +0.5); 0 and 0.3 read +3.1 / +0.2: the one-drop bass is phase luck
    // The resonance (target 2). A biquad in each Loop after 7's damping: a
    // peaking cut of r5EqDb per trip at r5EqHz (width r5EqQ), so the octave
    // around 1 kHz loses a little more each trip than 500 Hz and stops
    // ringing longest (the Wellspring rings longest at 500 Hz, 7 at 1 kHz).
    // Set by hand on the fit: the search alone traded it for the steady
    // colour (a shorter 1 kHz is a quieter 1 kHz; the coil gives it back).
    // Left of noon it eases out (7's tank, today's at TONE 0); right of noon
    // it eases to r5EqDbBright (TONE fully right; -0.3 let Spring B's ~1 kHz
    // sing at TENSION 1, DECAY 0.75: M6 15.9 dB, limit 15). None of it in
    // the Hold or the Howl (Tank.cpp updateSpringSettings).
    float r5EqHz       = 1200.0f;
    float r5EqDb       = -0.8f;
    float r5EqQ        = 1.4f;
    float r5EqDbBright = -0.6f;
    // (The Loop's DC blocker stays at Spring::kDcBlockHz 40 Hz: the fit's
    // 35 Hz rang 125 Hz a little longer for little and let the Kick's low
    // end ring 3 dB too long, test_kick.)
    // The coil (5's input transducer): its resonance lower and sharper, the
    // presence peak up from 1 kHz to 1.25-1.6 kHz (the Wellspring 1.25-1.6).
    float r5TdInHz  = 1960.0f;
    float r5TdInQ   = 1.57f;
    float r5TdOutHz = 4500.0f;
    float r5TdOutQ  = 0.67f;
    // The high path: 7's length (1.5 x DECAY's T60) through a higher
    // ceiling, so 4 and 8 kHz ring longer (J: 1.22 / 0.50 s in 7, 1.78 / 0.75
    // here; the Wellspring 1.81 / ~1). The fit had found 3 x DECAY's T60 at
    // 7's 9 kHz ceiling (4 kHz 1.30 s): that let the high path's ~1.2 kHz end
    // outlast the tail (test_output_bits: a new 1.25 kHz peak in the mu-law
    // box's last second, 6.5 dB, bar 6) and DECAY 0 ring 0.55 s.
    float r5HighCeilHz   = 14000.0f;
    float r5HighLevel    = 0.9f;
    // The low cut in front of the Springs (F round 2's step 2 at r5Amount 0):
    // a little more bass in, a little more 150-400 Hz out.
    float r5LcHpHz    = 138.0f;
    float r5LcShelfDb = -3.0f;
    // The stereo image (target 3). D(mid) put each pure tone left- or
    // right-heavy (500 Hz-1 kHz 6-11 dB: its phase there); the width comes
    // from the Springs' own difference (D2, 6's), which starts centred and
    // widens as the Springs drift apart, like the Wellspring's two tanks.
    // Below r5BassHz the difference is taken out (r5BassOrder2: a second pole;
    // the fit kept one). D still widens 1 Spring.
    float r5WideW    = 0.0f;
    // ... except 3 Springs (SPRINGS 3 with echo mode off, the Renderer's
    // coupled reference): its third Spring sits in the middle, so D stays at
    // this weight there (test_tank_voicing's width, fine-structure L/R
    // correlation <= 0.1, read 0.21 without it). Echo mode plays 2 Springs:
    // r5WideW, as wide as 7's echo mode above 1 kHz, its lows centred.
    float r5WideW3 = 0.5f;
    // ... and at short DECAYs D's weight r5WideWShort at DECAY 0, easing out
    // (smoothstep) by DECAY r5WideShortTo (Tank.cpp stereoMixFor).
    float r5WideWShort  = 0.6f;
    float r5WideShortTo = 0.45f;
    float r5WideSide = 0.55f;
    float r5WideSide3 = 0.55f;
    float r5BassHz   = 150.0f;
    float r5BassOrder2 = 0.0f;
    // (Tried and dropped: Spring B's pickup 0.12 ms later puts the two
    // Springs' 2-4 kHz first echoes in phase (a 2 kHz burst's front -0.85 L/R
    // -> +0.45; the Wellspring -0.12) but combs the mono sum of a short tail:
    // DECAY 0 notch -6.3 dB at 1.4 kHz, test_tank's bar -6, margin -4.5.)
    // Held sounds (target 4, C): WOBBLE's random wow / flutter inside the
    // Loops x r5FlatLoopWobble, the pickups' share (the transport) x
    // r5FlatTransportWobble, left of noon only (Tank.cpp). All of it on the
    // pickups: a held 1 kHz tone at WOBBLE 0.45 moves 2.2 dB instead of 7.8
    // (the Wellspring 1.2), and x 1.6 there keeps its pitch movement (WOBBLE
    // 0.25: p95 21.8 cents, 7 21.0; docs/prototypes/wellspring-fit-5/pitch5.py).
    // The wet's level from noon right (dB; easing to none at TONE fully left,
    // as the tank eases back to 7's; after the pickups, as 7's wet trim).
    // Round 5 came back louder than 7 at noon (02_hits +2.1 dB, clicks +0.6,
    // tone bursts +1.1; skank and pad level), and TONE's loudness spread in
    // KICKED read 3.6 dB (test_drive, bar 3; 7 2.3): TONE fully left was
    // already 7's level. One dB: hits +1.1 over 7, skank -1.2.
    float r5NoonTrimDb = -1.0f;
    // DRIVEN's output pickups pushed this much harder at DRIVE 1 (dB of
    // hardness along drive::pushCurve, x the DRIVEN Morph weight): the
    // denser, less decorrelated tail hid DRIVEN's grit ~1 dB more than 7's
    // (test_drive DRIVE audibility, level-matched DRIVE 0 vs 0.5: 7 -19.1,
    // 8 -20.1, bar -20; docs/prototypes/wellspring-fit-5/nullprobe.cpp).
    float r5DrivenPushDb = 2.0f;
    // KICKED's Clang and Clatter lifted this much more at DRIVE 0, easing out
    // by DRIVE 0.8 ((1 - DRIVE / 0.8)^2; on top of tdSplashLiftKickedDb):
    // round 5's coil gives the splash's highs back as KICKED's DRIVE relaxes
    // its resonance (Tank.cpp), and SPLASH voicing C stays DRIVE-free.
    float r5KickedLiftDb = 2.0f;
    // The level the coil's resonance gave, given back as KICKED's DRIVE
    // relaxes it (dB at full relax, on the Springs' input; Tank.cpp): without
    // it KICKED grew only +3.2 dB from DRIVE 0 to 1 (ADR 0033: +6, test_drive
    // limit 2 dB off).
    float r5CoilRelaxDb = 1.8f;
    // The Sustain trim's glide down on held sounds x this (on top of F round
    // 2's kFSusGlideScale; Tank.cpp updateBaseSettings).
    float r5SusGlideScale = 0.6f;
    float r5FlatLoopWobble = 0.0f;
    float r5FlatTransportWobble = 1.6f;
#endif
};

// ---- F round 2: the low cut at TONE noon (proto/wellspring-f2; ADR 0038
// "Round F2, proposed"; docs/m8-tuning-backlog.md "F round 2") ----
// Owner, 3 Oct 2026, candidate F in Ableton: "with tone at 50%, it's just a
// tad too thin/high passed". A hidden, Renderer-only key ("f_lowcut_voicing",
// Tank::setFLowCutVoicing) eases 7's low cut in three small steps; 0 is F
// exactly (lc* above, bit for bit). The makeup is unchanged (it gives back
// what each step still takes). The firmware never builds the steps
// (RV_FIXED_VOICINGS: Tank::fLowCut_ is a constant 0). Measured at TONE noon
// (sweep 63 / 100 / 126 / 159 / 200 Hz re 0.5-1 kHz, dB): F -18.0 / -10.7 /
// -7.8 / -3.8 / -2.7; the Wellspring (round 4) -21.2 / -11.8 / -6.6 / -5.6 /
// -3.3; today's tank +4.4 / +3.7 / +4.1 / +5.0 / +2.2.
struct LowCutStep {
    float hpHz, hpQ, shelfHz, shelfDb;
    float susGlideScale; // held sounds only: the Sustain trim's glide down x this (DriveVoicing.h kSusGentleDownSeconds)
};
constexpr int kNumFLowCuts = 4; // 0 = F, 1-3 the gentler steps
// RV_F_LOWCUT_DEFAULT (a scratch build's CMAKE_CXX_FLAGS) makes a step the
// default, so the suite can run as if it shipped.
#ifdef RV_F_LOWCUT_DEFAULT
constexpr int kDefaultFLowCut = RV_F_LOWCUT_DEFAULT;
#else
constexpr int kDefaultFLowCut = 2; // the owner's pick, 3 Oct 2026: "F, a little more" on every row (ADR 0038 Round F2)
#endif
// More bass into the Springs gives a held pad's loudest moment more to meet
// the limiter with: it comes as the pad swells in, while the Sustain trim is
// still gliding down (test_sustain_trim, bar 3.0 dB: F 2.74, step 2 3.11).
// With the gentler steps the trim glides down this much faster on held
// sounds; hits, stabs and clicks are never held, so they never move.
constexpr float kFSusGlideScale = 0.6f;
inline constexpr LowCutStep kFLowCutSteps[kNumFLowCuts - 1] = {
    {185.0f, 0.6f, 300.0f, -2.5f, kFSusGlideScale}, // 1 a touch more body: about the Wellspring's 100-160 Hz
    {155.0f, 0.6f, 300.0f, -2.0f, kFSusGlideScale}, // 2 a little more
    {130.0f, 0.6f, 300.0f, -1.5f, kFSusGlideScale}, // 3 the most: 126 Hz flat, still well under today's bump
};
// The low cut a Tank playing 7 uses at step `s` (0 = F's own, lc* above).
inline LowCutStep fLowCut(const Tuning& t, int s)
{
    return s <= 0 ? LowCutStep{t.lcHpHz, t.lcHpQ, t.lcShelfHz, t.lcShelfDb, 1.0f} : kFLowCutSteps[s - 1];
}

// Round 5: a number at r5Amount k between 7's (a) and round 5's (b),
// geometric (frequencies, gains).
inline float r5Mix(float a, float b, float k) { return k <= 0.0f ? a : k >= 1.0f ? b : a + k * (b - a); }
inline float r5MixHz(float a, float b, float k) { return k <= 0.0f ? a : k >= 1.0f ? b : a * std::exp(k * std::log(b / a)); }
#if RV_TANKV_BUILT >= 8
// The low cut a Tank playing voicing v uses (7: F round 2's step s).
inline LowCutStep lowCutFor(const Tuning& t, int v, int s)
{
    LowCutStep lc = fLowCut(t, s);
    const float k = r5Amount(v);
    if (k > 0.0f) {
        lc.hpHz    = r5MixHz(lc.hpHz, t.r5LcHpHz, k);
        lc.shelfDb = r5Mix(lc.shelfDb, t.r5LcShelfDb, k);
    }
    return lc;
}
#endif

// TONE re-map weights (7): left of noon 1 -> 0, right of noon 0 -> 1.
inline float toneDarkWeight(float tone, float curve)
{
    const float u = 1.0f - 2.0f * tone;
    return u <= 0.0f ? 0.0f : u >= 1.0f ? 1.0f : std::exp(curve * std::log(u)); // u^curve (exp, not pow: flash)
}
inline float toneBrightWeight(float tone, float curve)
{
    const float u = 2.0f * tone - 1.0f;
    return u <= 0.0f ? 0.0f : u >= 1.0f ? 1.0f : std::exp(curve * std::log(u));
}

#ifdef RV_FIXED_VOICINGS
inline constexpr Tuning kTuning{};
inline const Tuning& tuning() { return kTuning; }
#else
inline Tuning& mutableTuning()
{
    static Tuning t{};
    return t;
}
inline const Tuning& tuning() { return mutableTuning(); }
#endif

} // namespace rv::tankv
