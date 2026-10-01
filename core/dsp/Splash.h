#pragma once
// SPLASH / ATTITUDE nonlinear model (SPEC §4.5; CONTEXT.md: Hit, Clang,
// Bite, Clatter, Jolt, Splash; ADR 0032). One Splash per Tank (Tank.h).
//
//   mono in × G (INPUT, before any saturation) ─ HP 200 Hz ─┬─ HitEnvelope ─ e, short (per sample)
//                                                            │     ├─► Clang: × Voice::clang ─► the Tank feeds the hit's own highs into the springs
//                                                            │     └─► Bite:  × Voice::bite × short ─► the Tank pushes DriveIn harder
//                                                            └─ HitDetector ─ Hit (0..1, control rate)
//                                                                   │ onset → impact (seeded jitter)
//                                                                   └─► Jolt: envelope ─► Loop delay offset (fraction of L), Δa
//   Kick ─ strike() (forced: Hit 1, SPLASH 1) ─► Jolt + Clatter: a burst of band-passed knocks
//                                                (then KICKED rattle impacts) ─► each Spring's Loop + high path
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
    void push(float x) { pushHighpassed(x - hpLp_.process(x)); } // high-pass: x − LP(x)
    // The same, for input the caller has already high-passed at
    // splash::kDetectorHpHz (the Splash shares one high-pass with HitEnvelope).
    void pushHighpassed(float x)
    {
        // +1e-20: the followers settle at ~1e-20 in silence, never denormal.
        const float a = (x < 0.0f ? -x : x) + 1.0e-20f;
        fast_ += (a > fast_ ? fastAtt_ : fastRel_) * (a - fast_);
        // The slow follower tracks the fast envelope (not |x|): on sustained
        // sound it settles onto it, so d -> ~0 (only the fast one's ripple).
        slow_ += (fast_ > slow_ ? slowAtt_ : slowRel_) * (fast_ - slow_);
        const float d = fast_ - slow_;
        if (d > dMax_) dMax_ = d;
    }
    // Control tick: Hit (level-adaptive, SplashVoicing.h) from the largest d
    // since the last take(), judged against R = max(T, q · P); then P (the
    // program level) steps toward the fast envelope.
    float take()
    {
        const float ref = threshold_ > rel_ * prog_ ? threshold_ : rel_ * prog_;
        const float h   = splash::hitCurve(dMax_, ref);
        prog_ += (fast_ > prog_ ? progAtt_ : progRel_) * (fast_ - prog_);
        lastD_ = dMax_;
        dMax_  = 0.0f;
        return h;
    }
    float lastDifference() const { return lastD_; } // d behind the last take()
    float programLevel() const { return prog_; }    // P, for tests
    void setThresholds(float t, float rel)
    {
        threshold_ = t;
        rel_       = rel;
    }

private:
    OnePoleLowpass hpLp_;
    float fastAtt_ = 1.0f, fastRel_ = 1.0f, slowAtt_ = 1.0f, slowRel_ = 1.0f;
    float progAtt_ = 1.0f, progRel_ = 1.0f; // per control tick
    float fast_ = 0.0f, slow_ = 0.0f, dMax_ = 0.0f, lastD_ = 0.0f, prog_ = 0.0f;
    float threshold_ = 0.2f, rel_ = 1.0f;
};

// Hit envelope (ADR 0032, SplashVoicing.h "Hit envelope e"): per sample, on
// the high-passed input after the INPUT gain. e = SPLASH × sudden × loud
// (0..1, a hit's first ~10-25 ms), and short = how much of the hit is crack
// (highs) rather than notes. clang = (Voice::clang + (Voice::clangShort −
// Voice::clang) × short) × e, bite = kBiteGain × Voice::bite × short × e. The
// Tank applies them.
class HitEnvelope {
public:
    void prepare(float sampleRate);
    void reset();
    // Control rate: SPLASH, the blended Voice's clang amount and bite weight,
    // and DRIVE's gain on both (splash::splashDriveGain).
    void set(float splash, float clang, float clangShort, float biteWeight, float driveGain = 1.0f)
    {
        splash_   = splash;
        clang_    = clang * driveGain;
        clangShortDelta_ = (clangShort - clang) * driveGain;
        biteGain_ = splash::kBiteGain * driveGain * biteWeight;
    }
    // Control rate: the program level P (HitDetector::programLevel): the loud
    // reference R = max(kLoudRef, kLoudRel · P).
    void setProgramLevel(float p)
    {
        const float r = p * splash::kLoudRel > splash::kLoudRef ? p * splash::kLoudRel : splash::kLoudRef;
        invRef2_ = 1.0f / (r * r);
    }
    // h = the input, high-passed at splash::kDetectorHpHz.
    void push(float h, float& clang, float& bite)
    {
        lp_ += lpC_ * (h - lp_);
        const float hi = h - lp_; // the part above splash::kClangHz
        const float a = (h < 0.0f ? -h : h) + 1.0e-20f, ah = hi < 0.0f ? -hi : hi;
        fast_ += (a > fast_ ? fa_ : fr_) * (a - fast_);
        hiFast_ += (ah > hiFast_ ? fa_ : fr_) * (ah - hiFast_);
        slow_ += (fast_ > slow_ ? sa_ : sr_) * (fast_ - slow_);
        const float inv    = 1.0f / fast_;
        const float sudden = fast_ > slow_ ? (fast_ - slow_) * inv : 0.0f;
        const float lf     = fast_ * fast_ * invRef2_;
        const float e0     = splash_ * sudden * (lf < 1.0f ? lf : 1.0f);
        const float e      = e0 < 1.0f ? e0 : 1.0f;
        float sh = (hiFast_ * inv - splash::kShortLo) * (1.0f / (splash::kShortHi - splash::kShortLo));
        sh = sh < 0.0f ? 0.0f : (sh > 1.0f ? 1.0f : sh);
        e_     = e;
        short_ = sh;
        // A sharp hit before the SPLASH knob scales it (Big Knob voicing 5).
        const float hit = sudden * (lf < 1.0f ? lf : 1.0f) * sh;
        if (hit > hitMax_) hitMax_ = hit;
        clang  = (clang_ + clangShortDelta_ * sh) * e;
        bite   = biteGain_ * sh * e;
    }
    float envelope() const { return e_; }  // e of the last sample (tests, meters)
    float shortness() const { return short_; }
    // The largest sharp-hit reading since the last call (SPLASH-independent).
    float takeHitMax() { const float h = hitMax_; hitMax_ = 0.0f; return h; }

private:
    float invRef2_ = 1.0f / (splash::kLoudRef * splash::kLoudRef);
    float fa_ = 1.0f, fr_ = 1.0f, sa_ = 1.0f, sr_ = 1.0f, lpC_ = 1.0f;
    float fast_ = 0.0f, slow_ = 0.0f, hiFast_ = 0.0f, lp_ = 0.0f;
    float splash_ = 0.0f, clang_ = 0.0f, clangShortDelta_ = 0.0f, biteGain_ = 0.0f, e_ = 0.0f, short_ = 0.0f;
    float hitMax_ = 0.0f;
};

// Band-passed seeded sparse knocks (or noise) with an exponential burst
// envelope (SPEC §4.5 step 2; band and density in SplashVoicing.h). impact() raises the envelope; process()
// per sample. kStreams independent noise streams share the one envelope:
// one per Spring, so each Spring clangs on its own (a common burst through
// the Springs, whose first echoes are lined up, combs the mono sum).
class Clatter {
public:
    static constexpr int kStreams = 3;
    void prepare(float sampleRate, uint32_t seed);
    void reset();
    // Peak amplitude (before the band-pass) and 1/e decay time.
    void impact(float amplitude, float decayMs);
    // Stream 0 only.
    float process()
    {
        float y[1];
        process(y, 1);
        return y[0];
    }
    // Streams 0..count-1 into y (count <= kStreams). Streams above 0 only
    // run when asked for, so count must stay the same from call to call.
    void process(float* y, int count)
    {
        if (env_ == 0.0f && idle_) {
            for (int s = 0; s < count; ++s) y[s] = 0.0f;
            return;
        }
        float mx = 0.0f;
        for (int s = 0; s < count; ++s) {
            const float x = env_ * excite(size_t(s));
            const float v = lp_[size_t(s)].process(hp_[size_t(s)].process(x));
            y[s] = v;
            mx = mx > (v < 0.0f ? -v : v) ? mx : (v < 0.0f ? -v : v);
        }
        env_ *= decay_;
        if (env_ < 1.0e-7f) env_ = 0.0f;
        idle_ = env_ == 0.0f && mx < 1.0e-9f;
    }
    float envelope() const { return env_; }

private:
    // Excitation sample of stream s: white noise (splash::kClatterSparse 0),
    // or sparse clicks, "velvet noise": time is cut into cells of
    // kClatterCell samples, and each cell holds exactly one click of fixed
    // size at a random position with a random sign. Sharp little knocks that
    // each chirp through the Springs, a clang rather than a hiss. One click
    // per cell (not a coin toss per sample) keeps the number of knocks in a
    // burst, and so its energy, steady from hit to hit: a quiet hit can't
    // draw a lucky handful of clicks and crash like a loud one.
    float excite(size_t s)
    {
        if constexpr (splash::kClatterSparse <= 0.0f) {
            return rng_[s].bipolar();
        } else {
            float x = 0.0f;
            if (cellPos_[s] == clickAt_[s]) x = clickSign_[s] * clickGain_;
            if (++cellPos_[s] == kCell) newCell(s);
            return x;
        }
    }
    void newCell(size_t s)
    {
        cellPos_[s] = 0;
        const float u = rng_[s].bipolar();                          // position, [-1, 1)
        clickAt_[s]   = int(0.5f * (u + 1.0f) * float(kCell)) % kCell;
        clickSign_[s] = rng_[s].bipolar() < 0.0f ? -1.0f : 1.0f;
    }
    // Cell length (samples) and click size: same power as the dense noise
    // (E[u²] = 1/3): a² / kCell = 1/3.
    static constexpr int   kCell      = splash::kClatterSparse > 0.0f ? int(1.0f / splash::kClatterSparse + 0.5f) : 1;
    float clickGain_ = 1.0f; // sqrt(kCell / 3), prepare()
    std::array<int, kStreams>   cellPos_{}, clickAt_{};
    std::array<float, kStreams> clickSign_{};

    float  sampleRate_ = 48000.0f;
    std::array<Biquad, kStreams> hp_{}, lp_{};
    std::array<Rng, kStreams>    rng_{};
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
    // Add to a: pushes |a| up (more dispersion) whichever sign the Chirp uses
    // (map::kChirpSign; for LowsLater this is exactly -j·allpass, as before).
    float allpassDelta() const { return (map::kChirpSign * j_) * allpass_; }

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

// The whole Splash for one Tank: Hit detector + hit envelope (Clang, Bite)
// + impact sequencer + Jolt + the Kick's Clatter. One instance; the Tank
// applies the Clang and the Bite, feeds the Clatter to every Spring's Loop
// and high path and the Jolt to every Spring's L and a.
class Splash {
public:
    void prepare(float sampleRate, uint32_t seed);
    void reset();

    // Control rate: ATTITUDE Morph weights (CLEAN, DRIVEN, KICKED; sum 1),
    // the smoothed SPLASH Normalised value and DRIVE's gain on the Clang and
    // the Bite (splash::splashDriveGain; 1 = as picked at DRIVE 0.8).
    void set(const std::array<float, 3>& attitudeWeights, float splash, float driveGain = 1.0f);
    // Wet level 0..1 (e.g. a smoothed RMS), for KICKED's energy-dependent
    // rattle. Optional: 0 leaves the rattle Hit-driven only.
    void setTankLevel(float level) { tankLevel_ = level; }

    // Forced impact (a Kick, SPEC §4.6: "forces maximal SPLASH jolt") at a
    // sample offset within the next process() call (clamped into it).
    void strike(float strength, int sampleOffset);

    // n samples. in = the mono input after the INPUT gain, before any
    // saturation (the detector input, ADR 0032 / 0033). clangOut / biteOut =
    // the Clang and Bite amounts per sample (HitEnvelope; may be null).
    // clatterOut = the Kick's Clatter (stream 0; the Tank feeds each Spring's
    // Loop and high path, splash::kClatterLoop / kClatterHigh). joltLoopOut =
    // Loop delay offset as a fraction of L (Spring A scale; may be null).
    void process(const float* in, float* clatterOut, float* joltLoopOut, int n)
    {
        process(in, nullptr, nullptr, clatterOut, nullptr, nullptr, joltLoopOut, n);
    }
    // Everything: the Clang and Bite, and the Clatter's other two noise
    // streams (same envelope, independent noise: one per Spring; pass both or
    // neither).
    void process(const float* in, float* clangOut, float* biteOut, float* clatterOut, float* clatterB, float* clatterC,
                 float* joltLoopOut, int n);

    // Control-rate outputs (valid after process()).
    float hit() const { return hit_; }
    float allpassDelta() const { return jolt_.allpassDelta(); }
    float joltEnvelope() const { return jolt_.envelope(); }
    float clatterEnvelope() const { return clatter_.envelope(); } // burst envelope (tests)
    float hitEnvelope() const { return envelope_.envelope(); }     // e (ADR 0032), last sample (tests)
    float takeHitMax() { return envelope_.takeHitMax(); }          // sharp hits, SPLASH-independent
    int   impactCount() const { return impacts_; } // impacts fired since reset, rattle included (tests)
    int   strokeCount() const { return strokes_; } // primary impacts (one per stroke / strike)
    const splash::Voice& voice() const { return voice_; }
    const HitDetector&   detector() const { return detector_; } // tests, meters
    const HitEnvelope&   envelope() const { return envelope_; } // tests

private:
    void controlTick();
    void fire();

    float sampleRate_ = 48000.0f;
    OnePoleLowpass hpLp_;  // the shared 200 Hz high-pass (x − LP(x))
    HitDetector detector_;
    HitEnvelope envelope_;
    Clatter     clatter_;
    Jolt        jolt_;
    Rng         rng_;
    uint32_t    seed_ = 1;

    splash::Voice voice_{};
    std::array<float, 3> attW_{{-1.0f, -1.0f, -1.0f}};
    float splash_ = -1.0f, tankLevel_ = 0.0f, driveGain_ = -1.0f;

    int   k_ = 0; // position in the control grid
    float hit_ = 0.0f;
    int   sinceStroke_ = 1 << 30, minStroke_ = 0; // samples

    // Impact sequencer: one pending impact (primary or rattle).
    bool  pending_ = false, pendingPrimary_ = false, forced_ = false;
    bool  armed_ = true;      // stroke detector ready for the next stroke
    float strokePeak_ = 0.0f; // largest Hit since the stroke began
    float valley_ = 0.0f;     // lowest Hit since re-arming
    int   countdown_ = 0, secondaries_ = 0;
    int   jitter_ = 0, riseTicks_ = 0, maxRiseTicks_ = 8; // primary jitter runs from the stroke's peak
    float strength_ = 0.0f;
    int   impacts_ = 0, strokes_ = 0;
    // Strikes queued for the next process() call.
    static constexpr int kMaxStrikes = 4;
    std::array<int, kMaxStrikes>   strikeAt_{};
    std::array<float, kMaxStrikes> strikeStrength_{};
    int numStrikes_ = 0;
};

} // namespace rv::dsp
