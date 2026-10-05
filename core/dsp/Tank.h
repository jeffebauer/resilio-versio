#pragma once
// The Tank: 1–3 Springs plus Tank-level stages (CONTEXT.md).
//
// Signal flow (M7):
//
//   in L,R ─ mono sum ─ × send (THROW) ─┬─ × G (INPUT) ─ Splash ─ Clang c, Bite b; Jolt ─► each Spring's L, a
//                                       └─ × (1+b) ─ DriveIn ─ ÷ √(1+b) ─ Tilt ─ + c·highs ─ x ─── low in  ─┐
//                                                                                 └ x × HF gain ─ high in ─┤  Spring A, B, C (each has a LoopSat in its Loop)
//                                                                                                           │  Wobble[i] ─► Spring i's L
//                                                                                                           ▼
//                  × G^kInputHeard ─ SPRINGS mid/side mix ─ mid ─┬─────────────┐   (per mode, 20 ms fade)
//                                                                └ decorrelator ─ D
//     L = mid + side + w·D,  R = mid - side - w·D ─ DriveOut (L, R) ─ high-shelf cut ─ mu-law box ─ TONE return ─ limiter ─ wet
//       (mu-law box: DRIVEN 24 kHz / 12-bit, KICKED 24 kHz / 10-bit, CLEAN untouched; ADR 0042, dsp/OutputBits.h)
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
// (MIX 0 stays a bit-identical null in every ATTITUDE). In DRIVEN / KICKED the
// wet's mu-law box (ADR 0042) adds ~6 samples (0.12 ms) to the wet; an
// ATTITUDE flip crossfades it against the undelayed CLEAN wet over 20 ms. Inside each Loop the LoopSat's
// oversampler delay is counted in the round trip (Spring.h).
//
// SPLASH / WOBBLE (M7, SPEC §4.5, §4.7, docs/m7-integration.md; all
// numbers in params/SplashVoicing.h):
// - Splash (one per Tank) listens to the mono input after the INPUT gain G,
//   before any saturation and before Tilt (so neither DRIVE's colour nor
//   TONE changes SPLASH sensitivity, and DRIVE up only ever adds splash;
//   ADR 0032, 0033). A hit's splash is its own sound (ADR 0032): the Clang
//   feeds the hit's highs harder into the springs (every ATTITUDE), the Bite
//   pushes a short, cracking hit harder into DriveIn (DRIVEN, KICKED). No
//   noise is added on a hit. Hit is level-adaptive (judged against
//   a slow program level, SplashVoicing.h). Its Jolt moves each Spring's L
//   per sample (Spring B the other way) and adds to each Spring's allpass a
//   on the control grid (clamped |a| <= 0.85).
// - The Kick (a simulated knock on the tank, with its Clatter crash) was
//   removed in ADR 0043: the button throws and taps tempo instead.
// - WOBBLE: one generator per Spring, a Loop delay offset in samples added
//   on top of the Micro-mod floor; bipolar, exactly 0 at noon (ADR 0034,
//   WobbleVoicing.h); Springs B and C follow A at low amounts. Plus
//   the transport (M8): one more generator, shared by all Springs, that
//   moves every pickup read, so the first echoes waver too.
// The M7 components run their control logic on their own 32-sample grid
// counted from reset(), which lines up with the Tank's (static_assert).
// ATTITUDE's Morph weights feed their tables too, so a flip Morphs them.
//
// Every Spring hears the same mono input, like the
// springs in one physical tank all hang off the same driver. Each Spring is
// detuned (own L, fC, a) and placed in the stereo field by the SPRINGS mode;
// all the numbers live in core/params/SpringModes.h.
//
// SPRINGS switching (ADR 0003): every Spring that can be heard runs all the
// time. A Spring that is not heard in the current mode ("idle") still gets the input and
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
// Since echo mode (ADR 0041) Spring C is heard nowhere: positions 1 and 2
// never had it (SpringModes.h stereoMix), and position 3's echo plays A and
// B. So its audio doesn't run (no input, its output 0); every switch stays
// seamless because nothing it would have played is ever heard. Its settings
// still follow the knobs on its turn (control rate, cheap): the turn order
// then hands A and B their settings on the same ticks as before, so
// positions 1 and 2 stay bit for bit while knobs move.
// The Renderer's coupled reference (setEchoMode(false)) runs it as before.
//
// SPRINGS 3 = echo mode (ADR 0041; params/EchoVoicing.h, dsp/Echo.h,
// dsp/EchoClock.h): a tape echo in front of the 2-Spring tank. On the mono
// input before the Splash, so each repeat hits the springs as a new hit
// would: the springs hear the tape's output (the input plus its repeats),
// the feedback stays on the tape. The Springs play position 2 (its mix,
// stages and level) at a fixed tank (echo::kSpringsTension, kSpringsT60Seconds):
// DECAY is the echo's feedback (ATTITUDE-dependent: KICKED may run away at
// the top), TENSION its time (free, or a division of the clock), the gate
// its clock (clock(); the Plugin: setHostTempo()) and the button its tap
// tempo (button(), ADR 0043). Switching in and out
// glides over springs3::kGlideSeconds: the echo fades in on a fresh tape (or
// out, its repeats ringing on in the springs) while the Springs glide between
// the knobs' tank and the fixed one; positions 1 and 2 are bit for bit as
// before.
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
// SPRINGS 3 palette (ADR 0037; params/Springs3Voicing.h): since ADR 0041 a
// Renderer-only reference for position 3 with echo mode off
// (setEchoMode(false)): a voicing (setSprings3Voicing; default 13, coupled
// wire gauges, what shipped before echo mode) changes what position 3 does
// (long tank, Springs in series, wide, pan tank, coupled). It reshapes each Spring
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
// Firmware pass its own buffer (DTCM there, firmware/main.cpp) instead.
// Echo mode's tape is a second buffer, requiredTapeFloats(fs) (2 s: 96,000
// floats + a margin at 48 kHz, 375 KB): malloc'd too, or the Firmware's own
// (AXI SRAM) through prepare(fs, block, pool, n, tape, tapeN).

#include "dsp/Drive.h"
#include "dsp/Echo.h"
#include "dsp/EchoClock.h"
#include "dsp/Filters.h"
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
#include "params/EchoVoicing.h"
#include "params/ThrowHold.h"

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
    static constexpr int   kMaxPendingClocks = 16;
    static constexpr float kSpringsFadeSeconds = 0.020f; // SPRINGS crossfade (ADR 0003)

    Tank() = default;
    ~Tank();
    Tank(const Tank&)            = delete;
    Tank& operator=(const Tank&) = delete;

    // Allocation happens here only (owned pool and tape, grown if needed).
    void prepare(float sampleRate, int maxBlockSize);
    // Same, using a caller-supplied pool of >= requiredPoolFloats(sampleRate)
    // floats and echo mode's tape (requiredTapeFloats(sampleRate) floats; a
    // shorter one caps the longest echo time, none = the echo stays silent).
    void prepare(float sampleRate, int maxBlockSize, float* pool, size_t poolFloats, float* tape = nullptr,
                 size_t tapeFloats = 0);

    static size_t requiredPoolFloats(float sampleRate);
    static size_t requiredTapeFloats(float sampleRate) { return echo::tapeFloats(sampleRate); }
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

    // Echo mode's clock (ADR 0041): a rising edge of the gate at a sample
    // offset within the next process() block (clamped to it). One pulse = one
    // beat. Feed every edge in every position (the tempo is then known before
    // SPRINGS reaches 3); only echo mode uses it. Up to kMaxPendingClocks per block.
    void clock(int sampleOffset);
    // The Plugin's clock (ADR 0004 parity): the host's tempo in bpm (one beat
    // = its quarter note), 0 = none (gate clock, tap tempo or free time).
    // Overrides clock() and the taps.
    void setHostTempo(float bpm) { hostBpm_ = bpm > 0.0f ? bpm : 0.0f; }

    // The button (ADR 0043, params/ThrowHold.h): its state from a sample
    // offset within the next process() block (clamped to it; give changes in
    // time order). Hosts pass every change (the firmware once per block on a
    // change, the Plugin's panel button, the Renderer's "buttons" events).
    // Positions 1-2: a hand throw. The first press switches throw mode on
    // (as the gate's first rising edge); thrown, the send is open while the
    // gate is high OR the button is held. Tap, tap and hold the second press
    // kThrowExitHoldSeconds: throw mode off (as exitThrowMode(); throwExits()
    // counts it for the LEDs). Position 3 (echo mode): each press is a tap of
    // the echo's tempo (one tap interval = one beat; the gate clock's code,
    // EchoClock); of the gate clock and the taps, the last to set a tempo wins.
    // Up to kMaxPendingGates per block; extras are dropped.
    void button(bool down, int sampleOffset);
    // Times throw mode was switched off (the button's gesture or
    // exitThrowMode()) since prepare()/reset(): the firmware blinks its LEDs
    // when this moves.
    uint32_t throwExits() const { return throwExits_; }
    // The tapped tempo is the echo's clock now (tests).
    bool tapClockInUse() const { return tapClock_.locked() && (tapWins_ || !clock_.locked()); }

    // THROW (ADR 0039, params/ThrowHold.h): the gate's level from a sample
    // offset within the next process() block (clamped to it; give them in
    // time order). Hosts pass every change (the firmware once per block on a
    // change; the Plugin's THROW param and the Renderer's "gates" events
    // too). The Tank keeps the gate's role (gateRole(SPRINGS)) and the
    // latch: the throw is off, and the send open, until the first rising
    // edge after prepare()/reset(); from then on the Springs' input is open
    // only while the gate is high (opens over 2 ms, closes over 15 ms).
    // Up to kMaxPendingGates per block; extras are dropped.
    void gate(bool high, int sampleOffset);
    // Throw mode off (ADR 0039; the button's gesture does the same from
    // inside, ADR 0043; the Renderer's "throw_exits" call it): at the next
    // block's start the send glides back to open over the open ramp and the
    // latch clears, so the next rising edge or press switches the throw on
    // again. Returns true if throw mode was on; false, and nothing changes,
    // if it was off.
    bool exitThrowMode();
    // The throw has latched on (the first rising edge has come).
    bool throwOn() const { return throwOn_; }
    // The send's gain now in effect (1 = open), throw x Hold, for tests.
    float sendGain() const { return sendNow_; }

    // HOLD (ADR 0040, params/ThrowHold.h): CLEAN / DRIVEN top of DECAY.
    // The zone weight now in effect (zone x the CLEAN + DRIVEN Morph
    // weight; 0 outside it and in KICKED), and the ducking's gain on the
    // wet (1 = none), for tests and meters.
    float holdWeight() const { return holdZ_; }
    float duckGain() const { return duckTo_; }
    // Renderer / test hook (not a panel control, ADR 0040): which Hold
    // voicing (ThrowHold.h: 0 = "freeze", 1 = "layer", the default). The firmware
    // and plugin never call it (throwhold::kDefaultVoicing). Set it before
    // rendering.
    void setHoldVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        holdVoicing_ = std::clamp(v, 0, throwhold::kNumVoicings - 1);
#endif
    }
    int holdVoicing() const { return holdVoicing_; }
    // Renderer / test hook: the ducking's depth on the lows (ThrowHold.h
    // kDuckDepthDb: 0 = 12 dB, the default; 1 = 18 dB).
    void setDuckVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        duckVoicing_ = std::clamp(v, 0, throwhold::kNumDuckVoicings - 1);
#endif
    }
    int duckVoicing() const { return duckVoicing_; }
    // Test hook: no Hold at all (the Tank as before ADR 0040), to check the
    // Howl flip against it bit for bit.
    void setHoldEnabled(bool on) { holdOn_ = on; }

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
    // Clang, Bite and Jolt are not applied, so a test can measure the
    // Splash's share of the output by difference. Default true.
    void setSplashEnabled(bool on) { splashOn_ = joltOn_ = on; }
    // Finer: the Splash's sound (Clang and Bite), and the Jolt (L and a),
    // separately.
    void setSplashParts(bool sound, bool jolt)
    {
        splashOn_ = sound;
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
    // Renderer / test hook (not a panel control; DriveVoicing.h "TONE
    // placement", ADR 0036 amendment): where the Big Knob acts: 0 = before
    // the Springs (as first shipped, for reference), 1 = on the wet (the
    // return; the default). The firmware and plugin never call it
    // (drive::kTonePlaceDefault; RV_FIXED_VOICINGS builds only that). Set it
    // before rendering.
    void setTonePlaceVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        tilt_.setPlace(std::clamp(v, 0, drive::kNumTonePlaces - 1));
#endif
    }
    int  tonePlaceVoicing() const { return tilt_.place(); }
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
    // Renderer / test hook (not a panel control, ADR 0041): false = SPRINGS 3
    // is the coupled Springs reference (setSprings3Voicing) instead of echo
    // mode. The firmware and plugin never call it (the firmware always plays
    // echo mode and builds no palette). Set it before rendering.
    void setEchoMode([[maybe_unused]] bool on)
    {
#ifndef RV_FIXED_VOICINGS
        if (on == echoMode_) return;
        echoMode_ = on;
        keyMode_  = -1;
#endif
    }
    bool echoMode() const { return echoMode_; }
    // Renderer / test hook (PROTOTYPE, owner 4 Oct): diffuse repeats, 0 none
    // (default) ... 3 heavy (EchoVoicing.h kDiffuse). Set it before rendering.
    void setEchoDiffuseVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        echo_.setDiffuseVoicing(v);
#endif
    }
    int echoDiffuseVoicing() const { return echo_.diffuseVoicing(); }
    // Renderer / test hook (PROTOTYPE, owner 4 Oct): the repeats break up,
    // 0 none (default) ... 4 crushed (EchoVoicing.h "Wear"). Set it before rendering.
    void setEchoWearVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        echo_.setWearVoicing(v);
#endif
    }
    int echoWearVoicing() const { return echo_.wearVoicing(); }
    // Renderer / test hook (PROTOTYPE, owner 4 Oct): the BBD's strength,
    // 0 A today ... 3 D clock follows the echo time (EchoVoicing.h kBbd).
    void setBbdVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        echo_.setBbdVoicing(v);
#endif
    }
    int   bbdVoicing() const { return echo_.bbdVoicing(); }
    // Renderer / test hook (PROTOTYPE, owner 4 Oct): the repeats' bit depth,
    // 0 none ... 3 8-bit mu-law (EchoVoicing.h "Bits"), on top of the wear.
    void setEchoBitsVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        echo_.setBitsVoicing(v);
#endif
    }
    int echoBitsVoicing() const { return echo_.bitsVoicing(); }
    float bbdClockHz() const { return echo_.bbdClockHz(); }
    float echoFirstRepeatGain() const { return ginTo_; }
    // Echo mode, read-only (tests, meters): how far the echo is in (0..1,
    // glides over springs3::kGlideSeconds), the time it aims for (seconds),
    // the feedback in use, the clock division (-1 = free time), the clock.
    float echoBlend() const { return echoW_; }
    float echoSeconds() const { return echoSecs_; }
    float echoFeedback() const { return fbTo_; }
    int   echoDivision() const { return division_; }
    bool  clockLocked() const { return clock_.locked(); }
    float clockBeatSeconds() const { return clock_.beatSamples() / sampleRate_; }
    const dsp::TapeEcho& tapeEcho() const { return echo_; }
    // Output safety limiter's gain now in effect (linear, stereo-linked):
    // 1 = not limiting, below 1 = pulling the wet down (e.g. a loud Howl).
    // Read-only, for meters (the release firmware's output LEDs, ADR 0031).
    float limiterGain() const { return limitGain_; }
    // Renderer / test hook (ADR 0042, OutputVoicing.h): the output's bit
    // depth. 1 (the default) = DRIVEN 24 kHz / 12-bit mu-law, KICKED 24 kHz /
    // 10-bit mu-law on the wet before TONE's return filter (CLEAN untouched;
    // ADR 0042 amendment, 5 Oct 2026); 0 = without
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
    void processCoupled(const float* mono, const float* jolt, const float* tapSamples, float* wobA,
                        float (*wet)[kControlInterval], int tick, int n);
    void releaseOwnedPool();
    void echoTick(float decayKnob, float tensionKnob, bool fresh, bool snap); // echo mode's control tick (after the Morph)
    void feedClocks(); // queued gate edges and taps up to now -> the clocks (before each control tick)
    void buttonExitTick(); // the exit gesture's hold, on the control grid (ADR 0043)
    void startThrowExit(); // the send glides back open, then the latch clears

    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> smoothed_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> tickCoeff_{};

    std::array<Spring, kMaxSprings> springs_{};

    // SPRINGS mode and its output-matrix fade (see "SPRINGS switching").
    int              mode_     = 1;
    int              springsPos_ = 1; // SPRINGS as on the panel (mode_ is 1 in echo mode)
    modes::StereoMix mixFrom_{}, mixTo_{}, mixCur_{};
    float            trimFrom_ = 1.0f, trimTo_ = 1.0f, trimCur_ = 1.0f;
    float            mixScale_ = 1.0f; // trim / sqrt(mixPower(mixCur_))
    float            fadePos_  = 1.0f; // 0 -> 1 over kSpringsFadeSeconds; 1 = settled
    float            fadeStep_ = 0.0f;

    float* pool_       = nullptr;
    float* ownedPool_  = nullptr;
    size_t poolFloats_ = 0, ownedFloats_ = 0;
    float* ownedTape_  = nullptr; // echo mode's tape, when the Tank allocates it
    size_t ownedTapeFloats_ = 0;
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
    dsp::OutputBits                     outBits_; // the wet's mu-law box (ADR 0042, before TONE's return)
    float                               mixAt_ = -1.0f; // MIX value mixGains_ holds
    map::MixGains                       mixGains_{1.0f, 0.0f};
    float hitBlend_ = 0.0f, hitRelease_ = 0.0f; // Big Knob voicing 5
    float limitEnv_ = 0.0f, limitGain_ = 1.0f, limitAttack_ = 1.0f, limitRelease_ = 0.0f;
    int   limitHold_ = 0, limitHoldSamples_ = 0;

    // THROW (ADR 0039): pending gate changes, the gate's level as the Tank
    // has it, the latch, and the send's ramp (position 0..1, smoothstep'd).
    static constexpr int kMaxPendingGates = 16;
    struct GateEvent {
        int  at;
        bool high;
    };
    std::array<GateEvent, kMaxPendingGates> pendingGates_{};
    int   numPendingGates_ = 0;
    bool  gateHigh_ = false, throwOn_ = false, throwParamHigh_ = false;
    bool  thrReleasing_ = false, releaseThrow_ = false; // exitThrowMode(): gliding back / asked
    float thrPos_ = 1.0f, thrOpenStep_ = 0.0f, thrCloseStep_ = 0.0f, thrRelPos_ = 0.0f;
    void  latchThrow(float holdSend);
    // The button (ADR 0043): pending changes and its state; the exit gesture
    // (ThrowHold.h): the last press and release (absolute samples), whether
    // the last press threw (positions 1-2), was a short tap and stood alone,
    // the armed exit and when it fires; and the exits so far.
    std::array<GateEvent, kMaxPendingGates> pendingButtons_{};
    int      numPendingButtons_ = 0;
    bool     buttonDown_ = false, btnPressThrew_ = false, btnHaveRelease_ = false;
    bool     btnTapShort_ = false, btnTapAlone_ = false, exitArmed_ = false;
    uint32_t btnPressAt_ = 0, btnReleaseAt_ = 0, exitAt_ = 0, throwExits_ = 0;
    uint32_t exitGap_ = 0, exitTapMax_ = 0, exitHold_ = 0; // ThrowHold.h, in samples
    void     buttonEvent(bool down, uint32_t at, bool throwRole, float holdSend);
    float sendNow_ = 1.0f; // last sample's send gain (tests)
    // HOLD (ADR 0040): zone weight, bed weight (freeze / duck / layer), the
    // Hold's send gain over the tick, the ducking follower and its gain over
    // the tick (from -> to, ramped per sample).
    float holdZ_ = 0.0f, holdBed_ = 0.0f, holdSendFrom_ = 1.0f, holdSendTo_ = 1.0f;
    // Ducking (round 3, ThrowHold.h): the key (input low-passed), its peak
    // follower, the dip in dB with its hold, and the gain over the tick.
    dsp::Biquad duckKey_[2];
    float duckEnv_ = 0.0f, duckAtt_ = 1.0f, duckRel_ = 1.0f, duckFrom_ = 1.0f, duckTo_ = 1.0f;
    float duckDb_ = 0.0f, duckDbAtt_ = 1.0f, duckDbRel_ = 1.0f;
    int   duckHoldTicks_ = 0, duckHoldLeft_ = 0;
    // The Hold arms when DECAY enters its zone outside KICKED; leaving KICKED
    // inside the zone keeps it disarmed (the Howl calms into the plain long
    // tail, ADR 0018) until DECAY leaves the zone and comes back.
    bool holdArmed_ = true;
    bool holdOn_    = true; // test hook (setHoldEnabled)
    // CLEAN + DRIVEN Morph weight while the Hold is armed (else 0).
    float holdNotKicked() const { return holdOn_ && holdArmed_ ? attW_[0] + attW_[1] : 0.0f; }
#ifdef RV_FIXED_VOICINGS
    static constexpr int duckVoicing_ = 0;
#else
    int duckVoicing_ = 0; // setDuckVoicing
#endif
#ifdef RV_FIXED_VOICINGS
    static constexpr int holdVoicing_ = throwhold::kDefaultVoicing; // firmware: Drive.h RV_FIXED_VOICINGS
#else
    int holdVoicing_ = throwhold::kDefaultVoicing; // setHoldVoicing
#endif

    // M7: Splash (Hit, Clang, Bite, Jolt) and one Wobble per Spring.
    dsp::Splash                        splash_;
    dsp::OnePoleLowpass                clangLp_{}; // the Clang's split at splash::kClangHz (ADR 0032)
    float splashDrive_ = 1.0f;                        // DRIVE's gain on the Clang / Bite (splash::splashDriveGain)
    float clangEnv_ = 0.0f, clangAtt_ = 1.0f, clangRel_ = 1.0f; // the Clang's ceiling: peak follower of the springs' input highs
    float clangCeilPush_ = 1.0f; // its credit for the pickups' push (splash::kCeilPushShare)
    float splashInput_ = 1.0f;                        // the INPUT gain G, for the Splash (SPLASH stronger voicings)
    float dcNoon_ = 0.4f, dcRef_ = 0.75f;             // driveCurve at noon and at splash::kSplashRefDrive
    std::array<dsp::Wobble, kMaxSprings> wobble_{};
    dsp::Wobble                        transport_; // WOBBLE on the first echoes: every pickup, shared
    bool  splashOn_ = true, joltOn_ = true; // test hooks (setSplashParts): Clang + Bite, Jolt
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
    // The Big Knob on the wet (DriveVoicing.h "TONE placement", ADR 0036
    // amendment), and its own makeup: the wet's power (L + R) into and out
    // of it above ~90 Hz, slow followers, gain ramped per sample.
    dsp::ToneReturn toneReturn_;
    std::array<dsp::OnePoleLowpass, 4> trHp_{}; // 2 x one-pole HP into, 2 out of the return filter
    float trAccIn_ = 0.0f, trAccOut_ = 0.0f, trIn_ = 0.0f, trOut_ = 0.0f, trGain_ = 1.0f;
    dsp::Ramp trMakeup_;
    bool trIdle_ = false; // the stage is an exact pass-through: skipped (controlTick)
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
    float splashLift_ = 1.0f;         // the Clang at low DRIVE (tdSplashLiftDb)
    std::array<float, kMaxSprings> hiT60Set_{{-1.0f, -1.0f, -1.0f}}; // high path T60 ratio sent to each Spring
#endif

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
    // SPRINGS 3 echo mode (ADR 0041, EchoVoicing.h). echoW_ glides 0 -> 1
    // while position 3 is selected (0 = no echo, positions 1 and 2 bit for
    // bit); the Springs then glide to the fixed tank (springsDecay_, the
    // DECAY that gives echo::kSpringsT60Seconds).
#ifdef RV_FIXED_VOICINGS
    static constexpr bool echoMode_ = true; // firmware: position 3 is always echo mode
#else
    bool echoMode_ = true; // setEchoMode
#endif
    dsp::TapeEcho  echo_;
    dsp::EchoClock clock_;
    dsp::EchoClock tapClock_; // the button's taps (ADR 0043)
    bool     tapWins_ = false; // both clocks locked: the taps set the tempo last
    std::array<int, kMaxPendingClocks> pendingClocks_{};
    int      numPendingClocks_ = 0;
    std::array<uint32_t, kMaxPendingClocks> clockQ_{}; // edges (absolute samples, sorted) not yet at a control tick
    int      numClockQ_ = 0;
    std::array<uint32_t, kMaxPendingClocks> tapQ_{}; // taps (absolute samples, in order) not yet at a control tick
    int      numTapQ_ = 0;
    uint32_t sampleClock_ = 0; // samples since reset (the clock's time line)
    float    hostBpm_ = 0.0f;
    float    echoW_ = 0.0f, echoWFrom_ = 0.0f; // the echo's glide in, at this tick and the last
    float    fbFrom_ = 0.0f, fbTo_ = 0.0f;     // feedback, ramped per sample over a tick
    float    ginFrom_ = 0.0f, ginTo_ = 0.0f;   // the input's gain onto the tape (the first repeat's step down)
    float    echoSecs_ = 0.0f;                 // the echo time aimed for
    int      division_ = -1;                   // clocked: TENSION's zone (EchoVoicing.h kDivisionBeats)
    float    springsDecay_ = 0.5f;             // DECAY (Normalised) giving echo::kSpringsT60Seconds
#ifndef RV_FIXED_VOICINGS // F round 2's SPRINGS 3 voicings: Renderer-only
    float s3SwellAmt_ = 0.0f, s3SwellFrom_ = 0.0f, s3SwellTo_ = 0.0f; // "coupled swell" input split
    float s3SwellTurn_ = 1.0f;                                         // ... its turn per trip re noon TENSION's
    float swFast_ = 0.0f, swSlow_ = 0.0f, swHold_ = 0.0f;              // ... its hit detector
    float swFastAtt_ = 1.0f, swSlowC_ = 1.0f, swHoldStep_ = 1.0f;
#endif
};

} // namespace rv
