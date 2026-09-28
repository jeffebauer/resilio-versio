#pragma once
// SPLASH / ATTITUDE nonlinear model (SPEC §4.5; CONTEXT.md: Hit, Clatter,
// Jolt, Splash). One Splash per Tank (Tank.h; hooks in docs/m7-integration.md).
//
//   driven mono (post-DriveIn) ─ HitDetector ─ Hit (0..1, control rate)
//                                                  │ onset → impact (seeded jitter, then KICKED rattle impacts)
//                                                  ├─► Clatter: band-passed noise bursts ─► each Spring's high path
//                                                  └─► Jolt: envelope ─► Loop delay offset (fraction of L), Δa
//   Kick ─ strike() ────────────────────────────────┘ (forced: Hit 1, SPLASH 1)
//
// Numbers: core/params/SplashVoicing.h. Everything is one sample at a time,
// no allocation, seeded (reset() restores the seed). The control-rate part
// (Hit, onsets, rattle) runs on its own fixed 32-sample grid counted from
// reset(), so output is identical for any block size. Sample-rate aware: all
// times are in ms and converted with the rate passed to prepare().

#include "dsp/Filters.h"
#include "dsp/Seed.h"
#include "params/SplashVoicing.h"

#include <array>
#include <cstdint>

namespace rv::dsp {

// Fast − slow envelope followers (SPEC §4.5 step 1): fast = peak follower
// on |x|, slow = slow follower of the fast envelope. push() per sample;
// take() once per control tick returns Hit from the largest d = fast − slow
// seen since the previous take().
class HitDetector {
public:
    void prepare(float sampleRate);
    void reset();
    void setThreshold(float t) { threshold_ = t; }
    void push(float x)
    {
        x -= hpLp_.process(x); // high-pass: x − LP(x)
        // +1e-20: the followers settle at ~1e-20 in silence, never denormal.
        const float a = (x < 0.0f ? -x : x) + 1.0e-20f;
        fast_ += (a > fast_ ? fastAtt_ : fastRel_) * (a - fast_);
        // The slow follower tracks the fast envelope (not |x|): on sustained
        // sound it settles onto it, so d -> ~0 (only the fast one's ripple).
        slow_ += (fast_ > slow_ ? slowAtt_ : slowRel_) * (fast_ - slow_);
        const float d = fast_ - slow_;
        if (d > dMax_) dMax_ = d;
    }
    float take()
    {
        const float h = splash::hitCurve(dMax_, threshold_);
        lastD_ = dMax_;
        dMax_  = 0.0f;
        return h;
    }
    float lastDifference() const { return lastD_; } // d behind the last take(), for tests

private:
    OnePoleLowpass hpLp_;
    float fastAtt_ = 1.0f, fastRel_ = 1.0f, slowAtt_ = 1.0f, slowRel_ = 1.0f;
    float fast_ = 0.0f, slow_ = 0.0f, dMax_ = 0.0f, lastD_ = 0.0f;
    float threshold_ = 0.2f;
};

// Band-passed (1–6 kHz) seeded noise with an exponential burst envelope
// (SPEC §4.5 step 2). impact() raises the envelope; process() per sample.
class Clatter {
public:
    void prepare(float sampleRate, uint32_t seed);
    void reset();
    // Peak amplitude (before the band-pass) and 1/e decay time.
    void impact(float amplitude, float decayMs);
    float process()
    {
        if (env_ == 0.0f && idle_) return 0.0f;
        const float x = env_ * rng_.bipolar();
        env_ *= decay_;
        if (env_ < 1.0e-7f) env_ = 0.0f;
        const float y = lp_.process(hp_.process(x));
        idle_ = env_ == 0.0f && (y < 0.0f ? -y : y) < 1.0e-9f;
        return y;
    }
    float envelope() const { return env_; }

private:
    float  sampleRate_ = 48000.0f;
    Biquad hp_, lp_;
    Rng    rng_;
    uint32_t seed_ = 1;
    float  env_ = 0.0f, decay_ = 0.0f;
    bool   idle_ = true;
};

// Jolt envelope (SPEC §4.5 step 3): impact() raises the target, which decays
// over joltDecayMs; j follows it with a kJoltAttackMs rise. process() per
// sample returns the Loop delay offset as a fraction of L (Spring A; scale
// per Spring by splash::kJoltSpringScale). Includes KICKED's rattle.
class Jolt {
public:
    void prepare(float sampleRate, uint32_t seed);
    void reset();
    void set(float decayMs, float loopFrac, float allpass, float rattleDepth);
    void impact(float amount) { if (amount > target_) target_ = amount; }
    // Control tick: advance the rattle's random line. level = tank energy 0..1.
    void tick(float tankLevel);
    // Per sample. k = sample index inside the control tick (0..31).
    float process(int k)
    {
        target_ *= decay_;
        j_ += attack_ * (target_ - j_);
        if (j_ < 1.0e-7f && target_ < 1.0e-7f) j_ = target_ = 0.0f;
        const float r = rPrev_ + (rCur_ - rPrev_) * (float(k) * (1.0f / float(splash::kControlInterval)));
        return j_ * loopFrac_ + r;
    }
    float envelope() const { return j_; }
    float allpassDelta() const { return -j_ * allpass_; } // add to a (a < 0: more dispersion)

private:
    float sampleRate_ = 48000.0f;
    float decay_ = 0.0f, attack_ = 1.0f, loopFrac_ = 0.0f, allpass_ = 0.0f, rattleDepth_ = 0.0f;
    float target_ = 0.0f, j_ = 0.0f;
    // Rattle: smoothstep between seeded random points at kRattleHz.
    Rng      rng_;
    uint32_t seed_ = 1;
    float    rPos_ = 0.0f, rStep_ = 0.0f, rA_ = 0.0f, rB_ = 0.0f;
    float    rPrev_ = 0.0f, rCur_ = 0.0f;
};

// The whole Splash for one Tank: Hit detector + impact sequencer + Clatter
// + Jolt. One instance; the Tank feeds its Clatter to every Spring's high
// path and its Jolt to every Spring's L and a (docs/m7-integration.md).
class Splash {
public:
    void prepare(float sampleRate, uint32_t seed);
    void reset();

    // Control rate: ATTITUDE Morph weights (CLEAN, DRIVEN, KICKED; sum 1)
    // and the smoothed SPLASH Normalised value.
    void set(const std::array<float, 3>& attitudeWeights, float splash);
    // Wet level 0..1 (e.g. a smoothed RMS), for KICKED's energy-dependent
    // rattle. Optional: 0 leaves the rattle Hit-driven only.
    void setTankLevel(float level) { tankLevel_ = level; }

    // Forced impact (a Kick, SPEC §4.6: "forces maximal SPLASH jolt") at a
    // sample offset within the next process() call (clamped into it).
    void strike(float strength, int sampleOffset);

    // n samples. driven = post-DriveIn mono (the detector input).
    // clatterOut = Clatter (feed the Springs' high path). joltLoopOut =
    // Loop delay offset as a fraction of L (Spring A scale; may be null).
    void process(const float* driven, float* clatterOut, float* joltLoopOut, int n);

    // Control-rate outputs (valid after process()).
    float hit() const { return hit_; }
    float allpassDelta() const { return jolt_.allpassDelta(); }
    float highPathGain() const { return 1.0f + hfEnv_ * splash_ * voice_.hfEmphasis; } // CLEAN HF emphasis
    float joltEnvelope() const { return jolt_.envelope(); }
    int   impactCount() const { return impacts_; } // impacts fired since reset, rattle included (tests)
    int   strokeCount() const { return strokes_; } // primary impacts (one per stroke / strike)
    const splash::Voice& voice() const { return voice_; }

private:
    void controlTick();
    void fire();

    float sampleRate_ = 48000.0f;
    HitDetector detector_;
    Clatter     clatter_;
    Jolt        jolt_;
    Rng         rng_;
    uint32_t    seed_ = 1;

    splash::Voice voice_{};
    std::array<float, 3> attW_{{-1.0f, -1.0f, -1.0f}};
    float splash_ = -1.0f, tankLevel_ = 0.0f;

    int   k_ = 0; // position in the control grid
    float hit_ = 0.0f;
    float hfEnv_ = 0.0f, hfCoeff_ = 0.0f;
    int   sinceStroke_ = 1 << 30, minStroke_ = 0; // samples

    // Impact sequencer: one pending impact (primary or rattle).
    bool  pending_ = false, pendingPrimary_ = false, forced_ = false;
    bool  armed_ = true;      // stroke detector ready for the next stroke
    float strokePeak_ = 0.0f; // largest Hit since the stroke began
    float valley_ = 0.0f;     // lowest Hit since re-arming
    int   countdown_ = 0, secondaries_ = 0;
    float strength_ = 0.0f;
    int   impacts_ = 0, strokes_ = 0;
    // Strikes queued for the next process() call.
    static constexpr int kMaxStrikes = 4;
    std::array<int, kMaxStrikes>   strikeAt_{};
    std::array<float, kMaxStrikes> strikeStrength_{};
    int numStrikes_ = 0;
};

} // namespace rv::dsp
