#pragma once
// Tank voicings: Wellspring fit round 3 (docs/m8-tuning-backlog.md "Wellspring
// fit round 3", ADR 0038 Proposed). Renderer-only until the owner picks:
// Tank::setTankVoicing(v), the hidden "tank_voicing" key (host/common/
// ParamsJson.h). The firmware and the plugin keep kDefaultVoicing, and the
// firmware compiles only that one (RV_FIXED_VOICINGS, core/dsp/Drive.h).
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
// its default voicing's parts, so with the default 0 its code, its Tank
// object and its pool are today's; desktop builds hold all of them.
#ifdef RV_FIXED_VOICINGS
#ifdef RV_TANK_DEFAULT_VOICING
#define RV_TANKV_BUILT RV_TANK_DEFAULT_VOICING
#else
#define RV_TANKV_BUILT 0
#endif
#else
#define RV_TANKV_BUILT 7
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
constexpr int kNumVoicings     = 8;
// Until the owner picks. RV_TANK_DEFAULT_VOICING (a scratch build's
// CMAKE_CXX_FLAGS) makes another voicing the default, so the whole test suite
// can be run as if it shipped (docs/prototypes/wellspring-fit-3/).
#ifdef RV_TANK_DEFAULT_VOICING
constexpr int kDefaultVoicing = RV_TANK_DEFAULT_VOICING;
#else
constexpr int kDefaultVoicing  = kToday;
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
};

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
