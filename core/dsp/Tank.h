#pragma once
// The Tank: 1–3 Springs plus Tank-level stages (CONTEXT.md).
//
// Signal flow (M7):
//
//   in L,R ─ mono sum ─┬─ × G (INPUT) ─ Splash ─ Clang c, Bite b; Jolt ─► each Spring's L, a; Kick's Clatter
//                      └─ × (1+b) ─ DriveIn ─ ÷ √(1+b) ─ Tilt ─ + c·highs ─ + ─ x ─── low in  ─┐
//                                                                   ▲  └ (x + Clatter) × HF gain ─ high in ─┤  Spring A, B, C (each has a LoopSat in its Loop)
//   kick() ─ KickVoice ─ loop feed (HP 160 Hz⁴) ────────────────────┘                        │  Wobble[i] ─► Spring i's L
//                      ├ direct thump ────────────────────────────────┐                        │
//                      └ forced Splash (Hit 1, SPLASH 1)              │                        ▼
//                  × G^kInputHeard ─ SPRINGS mid/side mix ─ mid ┴ + ─┬─────────────┐   (per mode, 20 ms fade)
//                                                                     └ decorrelator ─ D
//     L = mid + side + w·D,  R = mid - side - w·D ─ DriveOut (L, R) ─ high-shelf cut ─ limiter ─ wet
//   out = dry · sqrt(1 - MIX) + wet · sqrt(MIX)   (equal power, dry stays stereo)
//   out ─ mu-law box (DRIVEN 24 kHz / 12-bit, KICKED 24 kHz / 8-bit; CLEAN untouched; ADR 0042, dsp/OutputBits.h)
//
// Drive chain (M5, SPEC §4.9, dsp/Drive.h, numbers in params/DriveVoicing.h):
// DriveIn (input transducer -> tape) and Tilt (TONE) are Tank-level: one
// copy, on the mono tank input. Each Spring has its own LoopSat. DriveOut
// (output pickup) runs on the stereo wet, one per channel. All nonlinear
// stages are oversampled (DriveVoicing.h kOversampleFactor).
//
// ATTITUDE Morph (ADR 0003): the switch sets a target; three weights
// (CLEAN, DRIVEN, KICKED) glide linearly to it over drive::kMorphSeconds on
// the control grid, and every attitude-dependent number is the weighted
// blend of the three voicings (dsp::blendVoice). So a flip mid-tail
// re-voices the live tail smoothly; nothing is ever stepped or restarted.
//
// Excitation trim (M8, DriveVoicing.h): slow followers of the raw input's
// full power and of its power in the band the Tank resonates in; the
// Springs' input (after Tilt) is trimmed by their ratio, so broadband or
// bright material comes back about as loud as in-band material. It trims
// only new input (never a ringing tail) and holds in silence.
//
// Sustain trim (M8, ADR 0035, DriveVoicing.h): while the input is held (a
// pad, a drone; never a hit), the Tank reads its own build-up gain for the
// sound (the wet's peaks, where the limiter reads them, over what went into
// the Springs) and eases the Springs' input down. Default voicing (round 3,
// "gentle"): a safety net that leaves a held sound alone until its loudest
// swell would push the limiter in by more than a fraction of a dB, then
// glides it to just under the knee (-2.5 dBFS peaks), at most 5 dB, and
// holds. Round 2 (-7 dBFS while the sound arrives, -5 once settled) stays as
// a Renderer voicing (setSustainVoicing). Same place as the Excitation trim
// (one ramp, the product of the two); lets go as soon as the sound isn't
// held.
//
// DRIVE (ADR 0014, 0022, 0033; curves in DriveVoicing.h) is the INPUT: one
// input gain G (0 -> +24 dB) that the Splash hears first, then DriveIn's
// saturators (G x the ATTITUDE's voicing offset), plus a "push" that makes
// the DriveOut pickups bite harder, so the drive is heard in the finished
// tail, not only smeared in from the input. The LoopSat is not pushed (ADR
// 0033: that shortened the tail). Gain compensation is measured, not
// modelled (SPEC §4.9): DriveIn and DriveOut follow the slow level into and
// out of their saturators and make up the difference (DriveOut's makeup is
// linked across L/R here, on the control grid), and DriveIn lets a quarter
// of G through: DRIVE changes colour, grit and squash, and the tank's level
// only by a few dB (drive::kInputHeard), whatever the material.
//
// Latency: the wet path picks up ~5 samples (0.1 ms at 48 kHz) of group
// delay from the DriveIn and DriveOut oversamplers (2.5 each at x2): like a
// tiny pre-delay, inaudible in a reverb. The dry path is untouched, so the Plugin still reports latency 0
// (MIX 0 stays a bit-identical null in CLEAN). In DRIVEN / KICKED the output's
// mu-law box (ADR 0042) delays dry and wet together by ~6 samples (0.12 ms);
// an ATTITUDE flip crossfades it against the undelayed CLEAN over 20 ms. Inside each Loop the LoopSat's
// oversampler delay is counted in the round trip (Spring.h).
//
// SPLASH / KICK / WOBBLE (M7, SPEC §4.5-4.7, docs/m7-integration.md; all
// numbers in params/SplashVoicing.h):
// - Splash (one per Tank) listens to the mono input after the INPUT gain G,
//   before any saturation and before Tilt (so neither DRIVE's colour nor
//   TONE changes SPLASH sensitivity, and DRIVE up only ever adds splash;
//   ADR 0032, 0033). A hit's splash is its own sound (ADR 0032): the Clang
//   feeds the hit's highs harder into the springs (every ATTITUDE), the Bite
//   pushes a short, cracking hit harder into DriveIn (DRIVEN, KICKED). No
//   noise is added on a hit; the Clatter is the Kick's crash only, into
//   every Spring's Loop and high path. Hit is level-adaptive (judged against
//   a slow program level, SplashVoicing.h). Its Jolt moves each Spring's L
//   per sample (Spring B the other way) and adds to each Spring's allpass a
//   on the control grid (clamped |a| <= 0.85).
// - Kick (ADR 0005, 0013, 0016): kick(offset) starts a KickVoice on its exact
//   sample. The high-passed thump + burst is added after DriveIn and Tilt
//   (post-drive: a knock on the tank bypasses the transducer and the EQ),
//   the full thump goes straight to the wet mid (the pickup hears the tank
//   body move), and the Kick forces a maximal Splash on the same sample.
//   The Kick is heard from sample N itself (the DriveOut oversampler's first
//   tap answers at once), for any block size (test_kick, plugin_host_test).
// - WOBBLE: one generator per Spring, a Loop delay offset in samples added
//   on top of the Micro-mod floor; bipolar, exactly 0 at noon (ADR 0034,
//   WobbleVoicing.h); Springs B and C follow A at low amounts. Plus
//   the transport (M8): one more generator, shared by all Springs, that
//   moves every pickup read, so the first echoes waver too.
// The M7 components run their control logic on their own 32-sample grid
// counted from reset(), which lines up with the Tank's (static_assert).
// ATTITUDE's Morph weights feed their tables too, so a flip Morphs them.
//
// Every Spring hears the same mono input, including the Kick, like the
// springs in one physical tank all hang off the same driver. Each Spring is
// detuned (own L, fC, a) and placed in the stereo field by the SPRINGS mode;
// all the numbers live in core/params/SpringModes.h.
//
// SPRINGS switching (ADR 0003): all three Springs run all the time. A Spring
// that is not heard in the current mode ("idle") still gets the input and
// keeps a live tail, at the minimum stage count (24, the TENSION floor), and
// simply has gain 0 in the output mix. A SPRINGS change is then only a
// change of output mix, faded over kSpringsFadeSeconds (20 ms) from
// wherever the gains are now, so it is click-free even when flipped mid-fade.
// Why run them rather than start them on demand: a Spring started at the
// switch would be empty, so switching 1 -> 2 on a ringing tail would leave
// the right channel almost silent until new input arrives, and no fade can
// hide an empty tank. Why it is affordable: idle Springs run at the floor
// stage count, and the stage caps are chosen so every mode, idle Springs
// included, costs no more than 3-Spring mode (SpringModes.h). 3 Springs is
// the SPEC §5 worst case anyway, so running idle Springs raises the average
// load in 1/2-Spring mode but never the peak the budget is written for.
// After a change, stage counts glide to the new mode (one stage per 8 ms, as
// a TENSION move), so an idle Spring's Chirp grows to full length over a few
// hundred ms after it becomes audible.
//
// Tank voicings (Wellspring fit round 3, params/TankVoicing.h, ADR 0038
// Proposed; Renderer key tank_voicing, default 0 = everything above, bit for
// bit): 1 puts the shared Sweep after the Clang (mono -> Sweep -> every
// Spring's Loop and high path), with fewer Loop sections and B's aligned high
// path; 2 zeroes the side (no Spring panned) and widens with D alone, its
// bass taken out; 3 adds the Loop diffusers (Spring::setDiffusion); 4 adds a
// low cut before the Clang, less Loop damping and a longer high path T60.
// Round 4: 5 = 3 + transducers (a resonant low-pass on what enters the
// Springs, after the Clang, and on the wet before DriveOut; less Loop
// damping, a longer, softer high path); 6 = 5 + wide (the Springs'
// difference back, through its own decorrelator D2: L = mid + X, R = mid -
// X); 7 = 6 + 4's low cut with a level makeup.
// The firmware compiles only the default.
// SPRINGS 3 palette (PROTOTYPE, ADR 0037 proposed; params/Springs3Voicing.h):
// a Renderer-only voicing (setSprings3Voicing) changes what position 3 does
// (long tank, Springs in series, wide, pan tank). It reshapes each Spring
// (L, fC, a, damping, T60, high path, pickup), the output mix and, in series,
// feeds Spring A's output into B and C's Loops. Positions 1 and 2 never
// change; the Springs glide into and out of the voicing over springs3::
// kGlideSeconds while the mix fades as above. Not built into the firmware
// (springs3::kPaletteBuilt).
//
// Every parameter is used from M7 on.
//
// AntiRes (M6, SPEC §4.10, ADR 0010; numbers in params/AntiRes.h): layer 1
// (even Loop gain) is the Spring's g design, layer 3 the detuning above,
// layer 5 the LoopSat. Layer 2, the Micro-mod floor, is set here on every
// control tick: each Spring gets SpringSettings::modDepth = the floor, always
// on, WOBBLE 0 included, plus (KICKED Howl zone only) a slow sine and extra
// drift so the Howl moves (ADR 0019). Layer 4 (adaptive suppressor) is not
// built: the M6 grid passes without it (docs/m6-metric-calibration.md).
//
// Real-time rules: process() never allocates, locks or does I/O. All memory
// is taken once in prepare(). Output is identical for any block size:
// parameters are smoothed and applied on a fixed 32-sample control grid
// that runs across block boundaries, and the SPRINGS fade advances per sample.
//
// Memory: the object is small (~4 kB with the M5 drive stages and M7; fits in
// DTCM as a global; exact size printed by test_tank); delay memory is one
// pool of Tank::requiredPoolFloats(fs) floats for 3 Springs + decorrelator,
// sized for the most-detuned Spring (M5 adds no pool memory). Total
// (memoryBytes(), printed by test_spring and test_tank): about 105 kB at
// 48 kHz and 207 kB at 96 kHz. It is malloc'd in prepare(); on the Daisy
// the heap lives in AXI SRAM (512 KB). prepare(fs, block, pool, n) lets the
// Firmware pass its own buffer (e.g. SDRAM via DSY_SDRAM_BSS) instead.

#include "dsp/Drive.h"
#include "dsp/Filters.h"
#include "dsp/Kick.h"
#include "dsp/OutputBits.h"
#include "dsp/Splash.h"
#include "dsp/Spring.h"
#include "dsp/Sweep.h"
#include "dsp/Wobble.h"
#include "params/ParamSpec.h"
#include "params/DriveVoicing.h"
#include "params/SpringModes.h"
#include "params/TankVoicing.h"
#include "params/Springs3Voicing.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace rv {

class Tank {
public:
    static constexpr int   kMaxSprings      = 3;
    using CoupleMatrix = std::array<float, size_t(kMaxSprings * kMaxSprings)>; // SPRINGS 3 coupled Loops (row-major)
    static constexpr int   kControlInterval = 32;      // samples between coefficient updates
    static constexpr float kWetGain         = 1.5f;     // +3.5 dB: wet ≈ dry level on noise at noon DECAY
    static constexpr float kShelfHz         = 5000.0f; // output high-shelf corner (SPEC §4.8)
    static constexpr float kShelfGain       = 0.7f;    // -3 dB above the corner
    static constexpr float kLimitThreshold  = 0.89f;   // ≈ -1 dBFS: wet peaks never reach 1.0
    static constexpr float kLimitKnee       = 0.82f;   // limiter aims peaks here; soft clip from here to the threshold
    // Gain glides down (no corner in the waveform). HighsLater Chirp
    // (Mappings.h): the lows are barely dispersed, so echo onsets of low
    // chords rise faster and a 1 ms glide lets them reach the soft clip's
    // ceiling (test_clicks "limiter pushed"); 0.5 ms keeps up, still click-free.
    static constexpr float kLimitAttackS    = map::kHighsLater ? 0.0005f : 0.001f;
    static constexpr float kLimitReleaseS   = 0.15f;
    // Hold before release (owner, 1 Oct 2026: limiting on a bass pad "sounds
    // overdriven, as if DRIVE is way up"). Without a hold the envelope sags
    // ~0.9 dB between the peaks of a 65 Hz tone, so the gain rides each low
    // cycle: intermodulation 26-32 dB under the wet, heard as drive. Held
    // longer than a 40 Hz cycle, and refreshed by any peak within
    // kLimitHoldRefresh of the envelope, the gain sits still on a steady
    // tone (-45 to -53 dB) and still releases 30 ms after the loud part ends.
    static constexpr float kLimitHoldS      = 0.030f;
    static constexpr float kLimitHoldRefresh = 0.944f; // -0.5 dB
    static constexpr int   kMaxPendingKicks = 16;
    static constexpr float kSpringsFadeSeconds = 0.020f; // SPRINGS crossfade (ADR 0003)

    Tank() = default;
    ~Tank();
    Tank(const Tank&)            = delete;
    Tank& operator=(const Tank&) = delete;

    // Allocation happens here only (owned pool, grown if needed).
    void prepare(float sampleRate, int maxBlockSize);
    // Same, using a caller-supplied pool of >= requiredPoolFloats(sampleRate) floats.
    void prepare(float sampleRate, int maxBlockSize, float* pool, size_t poolFloats);

    static size_t requiredPoolFloats(float sampleRate);
    // What the pool would need if Tank voicing v (params/TankVoicing.h) were
    // the only one compiled in (the firmware's RV_FIXED_VOICINGS layout):
    // its Sweep, its Loop diffusers, and Loop rings only for the sections it
    // uses. Desktop builds hold every voicing at once (requiredPoolFloats).
    static size_t poolFloatsForVoicing(float sampleRate, int v);

    void setParam(ParamId id, float normalised)
    {
        if (normalised < 0.0f) normalised = 0.0f;
        if (normalised > 1.0f) normalised = 1.0f;
        values_[static_cast<size_t>(id)] = normalised;
    }

    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Kick at a sample offset within the next process() block (clamped to it).
    // Up to kMaxPendingKicks per block; extras are dropped.
    void kick(int sampleOffset);

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    // Clear all state (tails, filters, noise seed) without re-preparing.
    // Smoothed parameters jump to their current values on the next process().
    void reset();

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }
    const Spring& spring(int i) const { return springs_[static_cast<size_t>(i)]; }
    // SPRINGS mode now in effect: 0, 1, 2 = 1, 2, 3 Springs.
    int springsMode() const { return mode_; }
    // ATTITUDE Morph weights now in effect (CLEAN, DRIVEN, KICKED), sum 1.
    const std::array<float, 3>& attitudeWeights() const { return attW_; }
    size_t memoryBytes() const { return sizeof(Tank) + poolFloats_ * sizeof(float); }
    // Test hook (not a panel control): false = the Splash still runs, but its
    // Clang, Bite, Clatter and Jolt are not applied, so a test can
    // measure the Splash's share of the output by difference. Default true.
    void setSplashEnabled(bool on) { splashOn_ = joltOn_ = on; }
    // Finer: the Splash's sound (Clang, Bite and the Kick's Clatter), and the
    // Jolt (L and a), separately.
    void setSplashParts(bool clatter, bool jolt)
    {
        splashOn_ = clatter;
        joltOn_   = jolt;
    }
    // Renderer / test hook (not a panel control, ADR 0034 round 2): which
    // WOBBLE voicing (WobbleVoicing.h: 0 = A round 1, 1 = B, 2 = C, 3 = D). The
    // firmware and plugin never call it (wobble::kDefaultVoicing).
    void setWobbleVoicing(int v)
    {
        for (auto& w : wobble_) w.setVoicing(v);
        transport_.setVoicing(v);
    }
    int wobbleVoicing() const { return transport_.voicing(); }
    // Renderer / test hook (not a panel control, ADR 0037 proposed): what
    // SPRINGS position 3 does (Springs3Voicing.h: 0 = today, 1 = long tank,
    // 2 = in series, 3 = wide, 4 = pan tank). Positions 1 and 2 never
    // change. The firmware and plugin never call it (springs3::
    // kDefaultVoicing; the firmware doesn't even build it: springs3::
    // kPaletteBuilt). Set it before rendering.
    void setSprings3Voicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        s3Voicing_ = springs3::kPaletteBuilt ? std::clamp(v, 0, springs3::kNumVoicings - 1) : springs3::kToday;
#endif
        keyMode_   = -1; // re-derive the Springs' settings and the mix on the next tick
    }
    int springs3Voicing() const { return s3Voicing_; }
    // How far the Springs are into position 3's voicing (0..1, glides over
    // springs3::kGlideSeconds), for tests.
    float springs3Blend() const { return s3W_; }
    // Renderer / test hook (not a panel control, ADR 0032 "SPLASH stronger",
    // Proposed): which SPLASH voicing (SplashVoicing.h: 0 = today, 1 = stronger
    // top, 2 = + DRIVE-free, 3 = bolder). The firmware and plugin never call it
    // (splash::kDefaultVoicing). Set it after prepare(), before rendering.
    void setSplashVoicing(int v)
    {
        splash_.setVoicing(v);
        clangLp_.setCutoff(splash::strong(splash_.voicing()).clangHz, sampleRate_);
    }
    int splashVoicing() const { return splash_.voicing(); }
    // M7 components, read-only (tests, meters).
    const dsp::Splash&    splash() const { return splash_; }
    const dsp::KickVoice& kickVoice() const { return kick_; }
    const dsp::Wobble&    wobble(int i) const { return wobble_[static_cast<size_t>(i)]; }
    const dsp::Wobble&    transport() const { return transport_; }
    // M8 excitation trim now in effect (linear, DriveVoicing.h), for tests.
    float excitationTrim() const { return excTrimTo_; }
    // M8 Sustain trim now in effect (linear, 1 = none; DriveVoicing.h), for tests.
    float sustainTrim() const { return susGain_; }
    // Test hook (not a panel control): false = no Sustain trim (it stays at
    // 1), so a test can measure something else on held sounds at the level
    // they had before it (e.g. WOBBLE's pitch). Default true.
    void setSustainTrimEnabled(bool on) { susOn_ = on; }
    // Renderer / test hook (not a panel control, ADR 0035 round 3): which
    // Sustain trim voicing (DriveVoicing.h: 0 = off, the limiter hold only;
    // 1 = round 2; 2 = gentle). The firmware and plugin never call it
    // (drive::kSusDefaultVoicing). Set it before rendering (it doesn't reset
    // a trim already in effect).
    void setSustainVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        susVoicing_ = std::clamp(v, 0, 2);
#endif
    }
    int  sustainVoicing() const { return susVoicing_; }
    // Renderer / test hook (not a panel control, ADR 0036 Proposed): which
    // Big Knob TONE voicing (DriveVoicing.h: 0 = today, 1 = steep, 2 = steep
    // + bump, 3 = + ringier when driven). The firmware and plugin never call
    // it (drive::kToneDefaultVoicing). Set it before rendering.
    void setToneVoicing(int v)
    {
        tilt_.setVoicing(std::clamp(v, 0, drive::kToneVoicingHits));
        compDrive_ = -1.0f; // DriveIn settings again on the next tick (voicing 3)
    }
    int  toneVoicing() const { return tilt_.voicing(); }
    // Renderer / test hook (not a panel control, ADR 0038 Proposed): which
    // Tank voicing (params/TankVoicing.h: 0 = today, 1 = Sweep, 2 = + stereo
    // together, 3 = + diffusion, 4 = + gentler). The firmware and plugin
    // never call it (tankv::kDefaultVoicing). Set it after prepare() and
    // before rendering: it clears the tails.
    void setTankVoicing(int v);
    int  tankVoicing() const { return tankVoicing_; }
    // Renderer / test hook (not a panel control, ADR 0038 "Round F2",
    // proposed): which step of 7's low cut (TankVoicing.h kFLowCutSteps: 0 =
    // F's own, 1-3 gentler). Only tank voicing 7 hears it. The firmware and
    // plugin never call it. Set it after prepare(), before rendering: it
    // clears the tails.
    void setFLowCutVoicing(int v);
    int  fLowCutVoicing() const { return fLowCut_; }
    // Output safety limiter's gain now in effect (linear, stereo-linked):
    // 1 = not limiting, below 1 = pulling the wet down (e.g. a loud Howl).
    // Read-only, for meters (the release firmware's output LEDs, ADR 0031).
    float limiterGain() const { return limitGain_; }
    // Renderer / test hook (ADR 0042, OutputVoicing.h): the output's bit
    // depth. 1 (the default) = DRIVEN 24 kHz / 12-bit mu-law, KICKED 24 kHz /
    // 8-bit mu-law on the whole output after MIX (CLEAN untouched); 0 = before
    // the box (the pre-ADR 0042 reference; tests read the Tank there). The
    // firmware and plugin never call it (outbits::kOutputBitsDefault; the
    // firmware compiles it out). Set it after prepare(), before rendering.
    void setOutputBitsVoicing(int v) { outBits_.setVoicing(v); }
    int  outputBitsVoicing() const { return outBits_.voicing(); }
    const dsp::OutputBits& outputBits() const { return outBits_; }

private:
    // Schroeder allpass (c + z^-D)/(1 + c z^-D): smears phase, keeps level.
    struct Diffuser {
        float* buf = nullptr;
        int    size = 0, w = 0;
        float  c = 0.5f;
        float process(float x)
        {
            const float d = buf[w];
            const float v = x - c * d;
            buf[w] = v;
            if (++w == size) w = 0;
            return c * v + d;
        }
    };

    void bindPool(float* pool);
    void applyTankVoicing(); // per-Spring parts of the voicing (diffusers, high path T60)
    modes::StereoMix stereoMixFor(int mode) const;
    float loopDampingScale() const; // the tank voicing's Loop damping x (1 today)
    void controlTick(bool snap);
    void updateBaseSettings(float decay, float tension, float tone, const drive::Voice& voice);
    void updateSpringSettings(size_t i);
    // SPRINGS position 3 voicing (Springs3Voicing.h): the output mix and trim
    // a mode plays, and each Spring's shape at the blend s3W_.
    modes::StereoMix modeMix(int mode) const;
    float            modeTrim(int mode) const;
    void             updateShapes();
    // Round 2 (Springs3Voicing.h): today's repeat timing for Spring i
    // (keepTiming), and the coupled Loops (couplingAngle / couplingKind).
    void keepTodaysTiming(size_t i, SpringSettings& s) const;
    static CoupleMatrix coupleMatrix(float angle, int kind);
    void processCoupled(const float* mono, float* const* clat, const float* jolt, const float* tapSamples, float* wobA,
                        float (*wet)[kControlInterval], int tick, int n);
    void releaseOwnedPool();

    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> smoothed_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> tickCoeff_{};

    std::array<Spring, kMaxSprings> springs_{};

    // SPRINGS mode and its output-matrix fade (see "SPRINGS switching").
    int              mode_     = 1;
    modes::StereoMix mixFrom_{}, mixTo_{}, mixCur_{};
    float            trimFrom_ = 1.0f, trimTo_ = 1.0f, trimCur_ = 1.0f;
    float            mixScale_ = 1.0f; // trim / sqrt(mixPower(mixCur_))
    float            fadePos_  = 1.0f; // 0 -> 1 over kSpringsFadeSeconds; 1 = settled
    float            fadeStep_ = 0.0f;

    float* pool_       = nullptr;
    float* ownedPool_  = nullptr;
    size_t poolFloats_ = 0, ownedFloats_ = 0;
    bool   ok_         = false;
    bool   primed_     = false;
    int    tick_       = 0;
    int    springTurn_ = 0; // which Spring takes new settings this control tick (controlTick)
    int    gridTick_   = 0; // control ticks since reset, mod 3 (controlTick: heavy redesign steps)

    // ATTITUDE Morph (see "ATTITUDE Morph").
    std::array<float, 3> attW_{{0.0f, 1.0f, 0.0f}};
    float                morphStep_ = 0.0f; // weight change per control tick
    // Last inputs of the gain-compensation model (recomputed only on change).
    float                compDrive_ = -1.0f;
    float                compTone_  = -1.0f; // Big Knob voicing 3's DriveIn push
    std::array<float, 3> compW_{{-1.0f, -1.0f, -1.0f}};
    dsp::DriveInSettings driveInSettings_{};
    dsp::Ramp            heardGain_{};  // the level DRIVE adds (ADR 0033), on the Springs' output
    dsp::Ramp            inputGain_{};  // the INPUT gain G (ADR 0033): what the Splash hears
    drive::Push          push_{};

    // Tank-level stages.
    dsp::DriveIn                        driveIn_;
    dsp::Tilt                           tilt_;
    std::array<dsp::DriveOut, 2>        driveOut_{};
    std::array<Diffuser, modes::kDecorrSeconds.size()> decorrelator_{};
    std::array<dsp::OnePoleLowpass, 2>  shelfSplit_{};
    dsp::Smoother                       mix_;
    dsp::OutputBits                     outBits_; // the output's mu-law box (ADR 0042, after MIX)
    float                               mixAt_ = -1.0f; // MIX value mixGains_ holds
    map::MixGains                       mixGains_{1.0f, 0.0f};
    float hitBlend_ = 0.0f, hitRelease_ = 0.0f; // Big Knob voicing 5
    float limitEnv_ = 0.0f, limitGain_ = 1.0f, limitAttack_ = 1.0f, limitRelease_ = 0.0f;
    int   limitHold_ = 0, limitHoldSamples_ = 0;

    std::array<int, kMaxPendingKicks> pendingKicks_{};
    int numPendingKicks_ = 0;

    // M7: Splash (Hit, Clatter, Jolt), the Kick voice and one Wobble per Spring.
    dsp::Splash                        splash_;
    dsp::OnePoleLowpass                clangLp_{}; // the Clang's split at splash::kClangHz (ADR 0032)
    float splashDrive_ = 1.0f;                        // DRIVE's gain on the Clang / Bite (splash::splashDriveGain)
    float clangEnv_ = 0.0f, clangAtt_ = 1.0f, clangRel_ = 1.0f; // the Clang's ceiling: peak follower of the springs' input highs
    float clangCeilPush_ = 1.0f; // its credit for the pickups' push (splash::kCeilPushShare)
    float splashInput_ = 1.0f;                        // the INPUT gain G, for the Splash (SPLASH stronger voicings)
    float dcNoon_ = 0.4f, dcRef_ = 0.75f;             // driveCurve at noon and at splash::kSplashRefDrive
    dsp::KickVoice                     kick_;
    std::array<dsp::Wobble, kMaxSprings> wobble_{};
    dsp::Wobble                        transport_; // WOBBLE on the first echoes: every pickup, shared
    bool  splashOn_ = true, joltOn_ = true; // test hooks (setSplashParts): Clang + Bite + Clatter, Jolt
    float levelAcc_ = 0.0f, levelMs_ = 0.0f, levelCoeff_ = 0.0f; // wet mid power -> Splash tank level, LoopSat fade
    float satFloorMs_ = 0.0f, satInvSpanMs_ = 0.0f; // LoopSat quiet-tail fade (AntiRes.h), mean-square units
    // M8 excitation trim (DriveVoicing.h "Excitation trim"): band-weighted and
    // full power of the driven input, slow followers, trim ramped per tick.
    std::array<dsp::OnePoleLowpass, 2> excHp_{}, excLp_{}; // 2 x one-pole HP, 2 x one-pole LP
    float excAccBroad_ = 0.0f, excAccBand_ = 0.0f, excBroad_ = 0.0f, excBand_ = 0.0f, excCoeff_ = 0.0f;
    float excTrimFrom_ = 1.0f, excTrimTo_ = 1.0f, excGate_ = 1.0e-12f;
    // Big Knob makeup (DriveVoicing.h, Renderer voicings 1-3): power into
    // and out of the Tilt above ~90 Hz, slow followers (kExcSeconds), gain
    // (1 = none).
    std::array<dsp::OnePoleLowpass, 4> bkHp_{}; // 2 x one-pole HP into, 2 out of the Tilt
    float bkAccIn_ = 0.0f, bkAccOut_ = 0.0f, bkIn_ = 0.0f, bkOut_ = 0.0f, bkGain_ = 1.0f;
    // M8 Sustain trim (DriveVoicing.h "Sustain trim"): held detector, the
    // followers behind the tank's build-up gain K, the trim (ln gain, <= 0);
    // the Springs' input trim ramped per tick is Excitation x Sustain
    // (inTrimFrom_ -> inTrimTo_).
    float susFast_ = 0.0f, susFill_ = 0.0f, susFillIn_ = 0.0f, susFed_ = 0.0f, susHeld_ = 0.0f, susLn_ = 0.0f, susAim_ = 0.0f, susGain_ = 1.0f;
    float susPeak_ = 0.0f, susPeakEnv_ = 0.0f, susPeakRelease_ = 0.0f; // the wet's peak (limiter input): tick, envelope
    float susFastCoeff_ = 0.0f, susFillCoeff_ = 0.0f, susDownCoeff_ = 0.0f, susUpCoeff_ = 0.0f, susLetGoCoeff_ = 0.0f;
    float susHeldRatio_ = 0.25f, susTarget_ = 1.0f;
    // Round 2: the steady-onset path (the input's fast level since it began:
    // its peak, how long it has stayed near it), the held latch, and the
    // high-water K (ln) with its hold time, and the time since it was held
    // (arriving vs settled).
    static constexpr float kSusNoK = -1.0e30f; // susKHw_ before the first read
    float susFastPk_ = 0.0f, susStill_ = 0.0f, susKHw_ = kSusNoK, susKHold_ = 0.0f;
    float susStillRatio_ = 0.5f, susKRelease_ = 0.0f, susOnsetDownCoeff_ = 0.0f, susSince_ = 0.0f;
    bool  susEngaged_ = false;
    bool  susOn_ = true; // test hook (setSustainTrimEnabled)
#ifdef RV_FIXED_VOICINGS
    static constexpr int susVoicing_ = drive::kSusDefaultVoicing; // firmware: Drive.h RV_FIXED_VOICINGS
#else
    int   susVoicing_ = drive::kSusDefaultVoicing; // setSustainVoicing
#endif
    // Round 3, the gentle voicing: its speeds and target; susGNeed_ is the
    // trim (ln) the loudest swell met needs (a low-water mark) and its hold.
    static constexpr float kSusNoNeed = 1.0e30f;
    float susGDownCoeff_ = 0.0f, susGUpCoeff_ = 0.0f, susGLetGoCoeff_ = 0.0f, susGTarget_ = 1.0f, susGNeedRelease_ = 0.0f;
    float susGNeed_ = kSusNoNeed, susGNeedHold_ = 0.0f;
    float inTrimFrom_ = 1.0f, inTrimTo_ = 1.0f;
    // Tank voicings (params/TankVoicing.h; ADR 0038 Proposed).
#ifdef RV_FIXED_VOICINGS
    static constexpr int tankVoicing_ = tankv::kDefaultVoicing; // firmware: Drive.h RV_FIXED_VOICINGS
    static constexpr int fLowCut_ = tankv::kDefaultFLowCut; // F round 2's low cut steps: Renderer-only
#else
    int tankVoicing_ = tankv::kDefaultVoicing; // setTankVoicing
    int fLowCut_ = tankv::kDefaultFLowCut;     // setFLowCutVoicing (TankVoicing.h kFLowCutSteps)
#endif
    // Each part only where this build can play it (RV_TANKV_BUILT): the
    // firmware with the default 0 carries none of them.
#if RV_TANKV_BUILT >= 1
    dsp::Sweep          sweep_;          // voicing 1+: shared, in front of every Spring
    float               sweepAlign_ = 0.0f; // its pickup alignment (samples, updateBaseSettings)
    bool                snapNow_ = false; // controlTick(snap) in progress (the Sweep's stage jump)
#endif
#if RV_TANKV_BUILT >= 2
    dsp::OnePoleLowpass dBass_{};        // voicing 2+: D's bass, taken out (bass centred)
#endif
#if RV_TANKV_BUILT >= 3
    std::array<std::array<float*, tankv::kNumDiffusers>, kMaxSprings> diffBuf_{}; // voicing 3: Loop diffusers
    std::array<std::array<int, tankv::kNumDiffusers>, kMaxSprings>    diffSize_{};
#endif
#if RV_TANKV_BUILT >= 4
    dsp::Biquad         gentleHp_{};     // voicing 4: the low cut in front of the Springs
    dsp::Biquad         gentleShelf_{};  // ... and its low-mid shelf
#endif
#if RV_TANKV_BUILT >= 5
    dsp::Biquad                tdIn_{};  // voicing 5+: the input coil's treble loss
    std::array<dsp::Biquad, 2> tdOut_{}; // ... and the output pickup's, L and R
    dsp::OnePoleLowpass        tdEvenAvg_{}; // ... the slow average of its even-order term
    float                      tdTrim_ = 1.0f; // ... and the wet's trim (tdTrimDb)
#endif
#if RV_TANKV_BUILT >= 6
    std::array<Diffuser, 3> wideDecorr_{}; // voicing 6+: D2, the Springs' difference decorrelated
#endif
#if RV_TANKV_BUILT >= 7
    // Voicing 7: the low cut's level makeup (power into / out of it above
    // ~90 Hz, slow followers; gain, 1 = none).
    std::array<dsp::OnePoleLowpass, 4> gmHp_{};
    float gmAccIn_ = 0.0f, gmAccOut_ = 0.0f, gmIn_ = 0.0f, gmOut_ = 0.0f, gmGain_ = 1.0f;
    // ... read a second time on the raw input above ~90 Hz (the Excitation
    // trim's high-passes), through a copy of the low cut: the material before
    // DRIVE colours it; the makeup is the smaller.
    dsp::Biquad lcShHp_{}, lcShShelf_{};
    float gmShAccIn_ = 0.0f, gmShAccOut_ = 0.0f, gmShIn_ = 0.0f, gmShOut_ = 0.0f;
    dsp::Biquad tdEvenHp_{};          // the coil's square term, high-passed (tdEvenHpHz)
    float tdTone_   = -1.0f;          // TONE the coil and pickup corners were set for
    float tdDrive_  = -1.0f;          // ... and KICKED x DRIVE^3 (the coil's corner opens with it)
    float toneTrim_ = 1.0f;           // TONE re-map's level right of noon, on the Springs' input
    dsp::OnePoleLowpass tdDarkLp_{};  // ... left of noon: the input's highs (toneDarkLpHz) ...
    float tdAccAll_ = 0.0f, tdAccLp_ = 0.0f, tdAll_ = 0.0f, tdLp_ = 0.0f, tdDarkDb_ = 0.0f, tdWd_ = 0.0f; // ... and its makeup
    float splashLift_ = 1.0f;         // the Clang and Clatter at low DRIVE (tdSplashLiftDb)
    std::array<float, kMaxSprings> hiT60Set_{{-1.0f, -1.0f, -1.0f}}; // high path T60 ratio sent to each Spring
#endif

    // M8 direct Clatter share: the side's delayed copy (splash::kClatterSideMs).
    static constexpr size_t kClatterSideMax = 160; // samples: 1.3 ms up to 96 kHz (125)
    std::array<float, kClatterSideMax> clatBuf_{};
    int clatPos_ = 0, clatDelay_ = 62;

    // ---- Control-rate caches (M3 run 12; after the per-sample state) ----
    drive::Voice         voice_{};                        // blendVoice(attW_), on Morph moves only
    std::array<float, 3> voiceW_{{-1.0f, -1.0f, -1.0f}};

    // The Springs' settings without the Jolt: the shared part (updateBaseSettings,
    // redone only when one of the key values below moved) and each Spring's
    // (updateSpringSettings, on its turn after the shared part changed).
    SpringSettings baseSet_{};
    int            activeStages_ = map::kMinStages;
    float          alignA_       = 0.0f;
    float          baseAllpass_  = 0.0f;  // TENSION's allpass coefficient (before detune and Jolt), every tick
    bool           baseDirty_    = true;  // baseSet_ is behind the key values below
    uint32_t       baseGen_      = 0;     // bumped when a key value moves
    std::array<SpringSettings, kMaxSprings> springSet_{};
    std::array<uint32_t, kMaxSprings>       springGen_{};
    std::array<float, kMaxSprings>          springTension_{}, springTone_{}; // key values springSet_ was worked out from
    float keyDecay_ = -1.0f, keyTension_ = -1.0f, keyTone_ = -1.0f;
    std::array<float, 3> keyW_{{-1.0f, -1.0f, -1.0f}};
    int   keyMode_ = -1;

    // SPRINGS 3 palette (Springs3Voicing.h, prototype). s3W_ glides 0 -> 1
    // while position 3 with a voicing is selected (0 = today's settings,
    // bit for bit); shape_ is each Spring's shape at that blend; the series
    // feed (voicing 2) ramps from s3SeriesFrom_ to s3SeriesTo_ over a tick.
#ifdef RV_FIXED_VOICINGS
    static constexpr int s3Voicing_ = springs3::kDefaultVoicing; // firmware: Drive.h RV_FIXED_VOICINGS
#else
    int   s3Voicing_ = springs3::kDefaultVoicing;
#endif
    float s3W_ = 0.0f, s3Step_ = 0.0f, keyS3W_ = -1.0f;
    float s3SeriesFrom_ = 0.0f, s3SeriesTo_ = 0.0f, s3SeriesSend_ = 1.0f;
    float s3WFrom_ = 0.0f;                    // s3W_ at the last tick (per-sample ramps)
    float s3SusMaxDb_ = drive::kSusGentleMaxDb; // the Sustain trim's ceiling at the blend
    dsp::OnePoleLowpass s3InLp_{}, s3SendLp_{}; // the voicing's low cuts (x - LP(x))
    std::array<springs3::Shape, kMaxSprings> shape_ = springs3::detuned();
    std::array<float, kMaxSprings>           springS3W_{}; // s3W_ springSet_ was worked out from
    // Round 2 (keepTiming): today's position 3 for the same knobs, the
    // reference each Spring's round trip and first echo are held to: A's
    // Chirp-chain delay (today's alignA_) and the stage count.
    float alignToday_ = 0.0f;
    int   todayStages_ = map::kMinStages;
    // Coupled Loops (voicings 8, 10): the rotation that mixes the Loops'
    // returns, at the last tick and this one (ramped per sample between).
    CoupleMatrix s3CoupleFrom_{}, s3CoupleTo_{};
    bool s3Coupled_ = false; // either end of the ramp is coupled (s3W_ > 0 in a coupled voicing)
#ifndef RV_FIXED_VOICINGS // F round 2's SPRINGS 3 voicings: Renderer-only
    float s3SwellAmt_ = 0.0f, s3SwellFrom_ = 0.0f, s3SwellTo_ = 0.0f; // "coupled swell" input split
    float s3SwellTurn_ = 1.0f;                                         // ... its turn per trip re noon TENSION's
    float swFast_ = 0.0f, swSlow_ = 0.0f, swHold_ = 0.0f;              // ... its hit detector
    float swFastAtt_ = 1.0f, swSlowC_ = 1.0f, swHoldStep_ = 1.0f;
#endif
};

} // namespace rv
