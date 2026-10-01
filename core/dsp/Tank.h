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
// (MIX 0 stays a bit-identical null). Inside each Loop the LoopSat's
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
#include "dsp/Splash.h"
#include "dsp/Spring.h"
#include "dsp/Wobble.h"
#include "params/ParamSpec.h"
#include "params/SpringModes.h"

#include <array>
#include <cstddef>

namespace rv {

class Tank {
public:
    static constexpr int   kMaxSprings      = 3;
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
    // M7 components, read-only (tests, meters).
    const dsp::Splash&    splash() const { return splash_; }
    const dsp::KickVoice& kickVoice() const { return kick_; }
    const dsp::Wobble&    wobble(int i) const { return wobble_[static_cast<size_t>(i)]; }
    const dsp::Wobble&    transport() const { return transport_; }
    // M8 excitation trim now in effect (linear, DriveVoicing.h), for tests.
    float excitationTrim() const { return excTrimTo_; }
    // Output safety limiter's gain now in effect (linear, stereo-linked):
    // 1 = not limiting, below 1 = pulling the wet down (e.g. a loud Howl).
    // Read-only, for meters (the release firmware's output LEDs, ADR 0031).
    float limiterGain() const { return limitGain_; }

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
    void controlTick(bool snap);
    void updateBaseSettings(float decay, float tension, float tone, const drive::Voice& voice);
    void updateSpringSettings(size_t i);
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
    float                               mixAt_ = -1.0f; // MIX value mixGains_ holds
    map::MixGains                       mixGains_{1.0f, 0.0f};
    float limitEnv_ = 0.0f, limitGain_ = 1.0f, limitAttack_ = 1.0f, limitRelease_ = 0.0f;

    std::array<int, kMaxPendingKicks> pendingKicks_{};
    int numPendingKicks_ = 0;

    // M7: Splash (Hit, Clatter, Jolt), the Kick voice and one Wobble per Spring.
    dsp::Splash                        splash_;
    dsp::OnePoleLowpass                clangLp_{}; // the Clang's split at splash::kClangHz (ADR 0032)
    float splashDrive_ = 1.0f;                        // DRIVE's gain on the Clang / Bite (splash::splashDriveGain)
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
};

} // namespace rv
