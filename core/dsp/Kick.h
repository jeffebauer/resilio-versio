#pragma once
// KickVoice: the simulated strike on the Tank (SPEC §4.6; ADRs 0005, 0013,
// 0016; CONTEXT.md "Kick"). M7 stand-alone component, not yet wired into the
// Tank (docs/m7-integration.md).
//
//   trigger(offset) ─► thump: decaying sine, pitch glides f0 → f1 (40–80 Hz), 20–40 ms
//                   ─► burst: ~10 ms broadband noise (high-passed at 150 Hz)
//                   ─► joltOffset(): "forces maximal SPLASH jolt" → Splash::strike()
//
//   loopOut   = HP120⁴(thump + burst) → Tank input, post-DriveIn/Tilt (SPEC §4.6 "post-drive")
//   directOut = thump                 → wet bus before DriveOut (the pickup hears the body move)
//
// Why two outputs (ADR 0016 "a Kick-path high-pass on the part fed into the
// Loop"): the thud must be tight. Fed whole into a 9 s Loop, the thump's
// 45–80 Hz would recirculate; high-passed, only its upper part and the burst
// excite the Springs, and the low thud is heard once, directly.
//
// Where it goes in the Tank: see docs/m7-integration.md. It moves the Kick
// from the M2 placeholder's "added to the input before DriveIn" to after the
// drive, so test_kick's "Kick at N == input impulse at N" becomes "Kick onset
// sample-accurate at N for every block size".
//
// Fixed strength (ADR 0005), scaled by ATTITUDE only (setAttitude()). Rising
// edges only (ADR 0013); edges closer than kKickMergeMs merge into one Kick
// (bounce guard). Two voices round-robin so a fast gate train (12/s) never
// cuts a ringing thump off mid-cycle. Real-time safe, seeded, block-size
// independent (onsets are sample-accurate, voices run per sample).

#include "dsp/Filters.h"
#include "dsp/Seed.h"
#include "params/SplashVoicing.h"

#include <array>
#include <cstdint>

namespace rv::dsp {

class KickVoice {
public:
    static constexpr int kMaxPending = 16;
    static constexpr int kVoices     = 2;

    void prepare(float sampleRate, uint32_t seed);
    void reset();

    // Control rate: ATTITUDE Morph weights (CLEAN, DRIVEN, KICKED). A Kick
    // takes its levels when it starts; a Morph does not re-voice a ringing one.
    void setAttitude(const std::array<float, 3>& weights);

    // Kick at a sample offset within the next process() call (clamped to
    // its last sample, as Tank::kick). Up to kMaxPending per call.
    void trigger(int sampleOffset);

    // n samples of both outputs (added to nothing: they are overwritten).
    void process(float* loopOut, float* directOut, int n);

    // Sample offset (in the last process() call) of the first Kick that
    // started there, or -1: the Tank passes it to Splash::strike().
    int joltOffset() const { return joltAt_; }
    int kicksStarted() const { return started_; } // since reset (tests)

private:
    struct Voice {
        float phase = 0.0f, freq = 0.0f, glide = 0.0f, fEnd = 0.0f, glideCoeff = 0.0f;
        float thumpEnv = 0.0f, thumpDecay = 0.0f;
        float burstEnv = 0.0f, burstDecay = 0.0f;
        bool  active = false;
    };
    void start(Voice& v);

    float sampleRate_ = 48000.0f;
    splash::KickParams params_{};
    std::array<Voice, kVoices> voices_{};
    int    nextVoice_ = 0;
    std::array<Biquad, 2> loopHp_{}; // 4th order: ~-30 dB at 50 Hz
    OnePoleLowpass burstLp_; // burst high-pass = x − LP(x)
    Rng    rng_;
    uint32_t seed_ = 1;

    std::array<int, kMaxPending> pending_{};
    int  numPending_ = 0;
    int  sinceLast_  = 1 << 30; // samples since the last Kick started (merge guard)
    int  mergeSamples_ = 240;
    int  joltAt_ = -1, started_ = 0;
    int  quiet_ = 0, idleSamples_ = 2400; // samples since the last voice ended; idle at idleSamples_
};

} // namespace rv::dsp
