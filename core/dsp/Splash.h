#pragma once
// SPLASH / ATTITUDE nonlinear model (SPEC §4.5; CONTEXT.md: Hit, Clang,
// Bite, Jolt, Splash; ADR 0032). One Splash per Tank (Tank.h).
//
//   mono in × G (INPUT, before any saturation) ─ HP 200 Hz ─┬─ HitEnvelope ─ e, short (per sample)
//                                                            │     ├─► Clang: × Voice::clang ─► the Tank feeds the hit's own highs into the springs
//                                                            │     └─► Bite:  × Voice::bite × short ─► the Tank pushes DriveIn harder
//                                                            └─ HitDetector ─ Hit (0..1, control rate)
//                                                                   │ onset → impact (seeded jitter)
//                                                                   └─► Jolt: envelope ─► Loop delay offset (fraction of L), Δa
//
// (Until ADR 0043 a Kick also struck it: a forced Hit 1 that fired the
// Clatter, a burst of band-passed knocks into every Spring. Both are gone.)
//
// Numbers: core/params/SplashVoicing.h. Everything is one sample at a time,
// no allocation, seeded (reset() restores the seed). The control-rate part
// (Hit, onsets, the Jolt's rattle) runs on its own fixed 32-sample grid counted from
// reset(), so output is identical for any block size. Sample-rate aware: all
// times are in ms and converted with the rate passed to prepare().

#include "dsp/Filters.h"
#include "dsp/Select.h"
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
        // (Select.h: branch-free choices on the firmware, same results.)
        const float a = absSel(x) + 1.0e-20f;
        fast_ += selGt(a, fast_, fastAtt_, fastRel_) * (a - fast_);
        // The slow follower tracks the fast envelope (not |x|): on sustained
        // sound it settles onto it, so d -> ~0 (only the fast one's ripple).
        slow_ += selGt(fast_, slow_, slowAtt_, slowRel_) * (fast_ - slow_);
        const float d = fast_ - slow_;
        dMax_ = selGt(d, dMax_, d, dMax_);
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
        const float r = p * splash::kLoudRel > loudRef_ ? p * splash::kLoudRel : loudRef_;
        invRef2_ = 1.0f / (r * r);
        if constexpr (splash::kVoicingsBuilt) { // today's reference, for today's Clang
            const float t = p * splash::kLoudRel > splash::kLoudRef ? p * splash::kLoudRel : splash::kLoudRef;
            invRef2Today_ = 1.0f / (t * t);
        }
    }
    // SPLASH stronger: today's Clang amounts (chord-like, drum-like), for
    // pushToday's floor (the ceiling never takes a voicing below today).
    void setToday(float clang, float clangShort)
    {
        clangToday_ = clang;
        clangShortDeltaToday_ = clangShort - clang;
    }
    // SPLASH stronger (SplashVoicing.h "SPLASH stronger"): the loud floor
    // kLoudRef × scale (voicings 2, 3: the INPUT gain re DRIVE 0.8), and the
    // Clang's hold (per-sample decay of the held e; 0 = no hold, e as is).
    void setLoudScale(float scale) { loudRef_ = splash::kLoudRef * scale; }
    void setHold(float decay) { hold_ = decay; }

    // h = the input, high-passed at splash::kDetectorHpHz. today = the Clang
    // today's voicing would give (SPLASH stronger voicings only; else 0).
    void push(float h, float& clang, float& bite, float& today)
    {
        lp_ += lpC_ * (h - lp_);
        const float hi = h - lp_; // the part above splash::kClangHz
        // (Select.h: branch-free choices on the firmware, same results.)
        const float a = absSel(h) + 1.0e-20f, ah = absSel(hi);
        fast_ += selGt(a, fast_, fa_, fr_) * (a - fast_);
        hiFast_ += selGt(ah, hiFast_, fa_, fr_) * (ah - hiFast_);
        slow_ += selGt(fast_, slow_, sa_, sr_) * (fast_ - slow_);
        const float inv    = 1.0f / fast_;
        const float sudden = selGt(fast_, slow_, (fast_ - slow_) * inv, 0.0f);
        const float lf     = fast_ * fast_ * invRef2_;
        const float e0     = splash_ * sudden * (lf < 1.0f ? lf : 1.0f);
        const float e      = e0 < 1.0f ? e0 : 1.0f;
        float sh = (hiFast_ * inv - splash::kShortLo) * (1.0f / (splash::kShortHi - splash::kShortLo));
        sh = selGt(0.0f, sh, 0.0f, selGt(sh, 1.0f, 1.0f, sh)); // sh < 0 ? 0 : (sh > 1 ? 1 : sh)
        if constexpr (splash::kVoicingsBuilt) {
            const float held = eh_ * hold_;
            eh_ = e > held ? e : held; // = e without a hold
        } else {
            eh_ = e;
        }
        e_     = e;
        short_ = sh;
        // A sharp hit before the SPLASH knob scales it (Big Knob voicing 5).
        const float hit = sudden * (lf < 1.0f ? lf : 1.0f) * sh;
        hitMax_ = selGt(hit, hitMax_, hit, hitMax_);
        clang  = (clang_ + clangShortDelta_ * sh) * eh_;
        bite   = biteGain_ * sh * e;
        if constexpr (splash::kVoicingsBuilt) {
            const float lt = fast_ * fast_ * invRef2Today_, et0 = splash_ * sudden * (lt < 1.0f ? lt : 1.0f);
            today = (clangToday_ + clangShortDeltaToday_ * sh) * (et0 < 1.0f ? et0 : 1.0f);
        } else {
            today = 0.0f;
        }
    }
    float envelope() const { return e_; }  // e of the last sample (tests, meters)
    float shortness() const { return short_; }
    // The largest sharp-hit reading since the last call (SPLASH-independent).
    float takeHitMax() { const float h = hitMax_; hitMax_ = 0.0f; return h; }

private:
    float invRef2_ = 1.0f / (splash::kLoudRef * splash::kLoudRef), loudRef_ = splash::kLoudRef, hold_ = 0.0f, eh_ = 0.0f;
    float invRef2Today_ = 1.0f / (splash::kLoudRef * splash::kLoudRef), clangToday_ = 0.0f, clangShortDeltaToday_ = 0.0f;
    float fa_ = 1.0f, fr_ = 1.0f, sa_ = 1.0f, sr_ = 1.0f, lpC_ = 1.0f;
    float fast_ = 0.0f, slow_ = 0.0f, hiFast_ = 0.0f, lp_ = 0.0f;
    float splash_ = 0.0f, clang_ = 0.0f, clangShortDelta_ = 0.0f, biteGain_ = 0.0f, e_ = 0.0f, short_ = 0.0f;
    float hitMax_ = 0.0f;
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
// + impact sequencer + Jolt. One instance; the Tank applies the Clang and
// the Bite, and the Jolt to every Spring's L and a.
class Splash {
public:
    void prepare(float sampleRate, uint32_t seed);
    void reset();

    // Control rate: ATTITUDE Morph weights (CLEAN, DRIVEN, KICKED; sum 1),
    // the smoothed SPLASH Normalised value and DRIVE's gain on the Clang and
    // the Bite (splash::splashDriveGain; 1 = as picked at DRIVE 0.8).
    // inputGain = the INPUT gain G (linear; SPLASH stronger voicings only).
    void set(const std::array<float, 3>& attitudeWeights, float splash, float driveGain = 1.0f, float inputGain = 1.0f);
    // SPLASH stronger (SplashVoicing.h): 0 = today .. 3; Renderer / tests only.
    void setVoicing(int v);
    int  voicing() const { return splash::kVoicingsBuilt ? voicing_ : splash::kDefaultVoicing; }
    // The Clang's ceiling (SplashVoicing.h "SPLASH stronger"; the Tank applies
    // it on the springs' input, never below today's Clang, process()'s
    // clangTodayOut): 0 = none.
    float clangCeiling() const { return splash::kVoicingsBuilt ? clangCeil_ : 0.0f; }
    // Wet level 0..1 (e.g. a smoothed RMS), for KICKED's energy-dependent
    // rattle. Optional: 0 leaves the rattle Hit-driven only.
    void setTankLevel(float level) { tankLevel_ = level; }

    // n samples. in = the mono input after the INPUT gain, before any
    // saturation (the detector input, ADR 0032 / 0033). clangOut / biteOut =
    // the Clang and Bite amounts per sample (HitEnvelope; may be null).
    // joltLoopOut = Loop delay offset as a fraction of L (Spring A scale; may
    // be null). clangTodayOut (may be null): the Clang today's voicing would
    // give (SPLASH stronger voicings only, else 0).
    void process(const float* in, float* joltLoopOut, int n) { process(in, nullptr, nullptr, joltLoopOut, n); }
    void process(const float* in, float* clangOut, float* biteOut, float* joltLoopOut, int n,
                 float* clangTodayOut = nullptr);

    // Control-rate outputs (valid after process()).
    float hit() const { return hit_; }
    float allpassDelta() const { return jolt_.allpassDelta(); }
    float joltEnvelope() const { return jolt_.envelope(); }
    float hitEnvelope() const { return envelope_.envelope(); }     // e (ADR 0032), last sample (tests)
    float takeHitMax() { return envelope_.takeHitMax(); }          // sharp hits, SPLASH-independent
    int   impactCount() const { return impacts_; } // impacts fired since reset (tests)
    int   strokeCount() const { return strokes_; } // strokes (one impact each)
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
    Jolt        jolt_;
    Rng         rng_;
    uint32_t    seed_ = 1;

    splash::Voice voice_{};
    std::array<float, 3> attW_{{-1.0f, -1.0f, -1.0f}};
    float splash_ = -1.0f, tankLevel_ = 0.0f, driveGain_ = -1.0f, inputGain_ = -1.0f;
    float invInputRef_ = 1.0f; // 1 / G at splash::kSplashRefDrive
    float clangCeil_ = 0.0f;
    int   voicing_ = splash::kDefaultVoicing;

    int   k_ = 0; // position in the control grid
    float hit_ = 0.0f;
    int   sinceStroke_ = 1 << 30, minStroke_ = 0; // samples

    // Impact sequencer: one pending impact (a stroke's).
    bool  pending_ = false;
    bool  armed_ = true;      // stroke detector ready for the next stroke
    float strokePeak_ = 0.0f; // largest Hit since the stroke began
    float valley_ = 0.0f;     // lowest Hit since re-arming
    int   countdown_ = 0;
    int   jitter_ = 0, riseTicks_ = 0, maxRiseTicks_ = 8; // primary jitter runs from the stroke's peak
    float strength_ = 0.0f;
    int   impacts_ = 0, strokes_ = 0;
};

} // namespace rv::dsp
