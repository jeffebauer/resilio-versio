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
// Every number for the new versions lives here. Desktop builds can override
// them before a Tank is prepared (Tuning, mutableTuning(); the Renderer reads
// RV_TANKV_TUNE, used by docs/prototypes/wellspring-fit-3/ to fit them);
// the firmware never can.

namespace rv::tankv {

constexpr int kToday    = 0;
constexpr int kSweep    = 1;
constexpr int kTogether = 2;
constexpr int kDiffuse  = 3;
constexpr int kGentle   = 4;
constexpr int kNumVoicings     = 5;
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
constexpr bool hasGentle(int v) { return v >= kGentle; }

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
    // (0.94-1.0). 663 floats of pool for the three Springs.
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
