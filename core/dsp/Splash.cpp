#include "dsp/Splash.h"
#include "dsp/SizeOpt.h"

#include "params/DriveVoicing.h"

#include <cmath>

namespace rv::dsp {

namespace {

// One-pole coefficient for a 1/e time of ms at rate fs, stepped every `every` samples.
float onePole(float ms, float sampleRate, float every = 1.0f)
{
    return 1.0f - std::exp(-1000.0f * every / (ms * sampleRate));
}
float decayPerStep(float ms, float sampleRate, float every = 1.0f)
{
    return std::exp(-1000.0f * every / (ms * sampleRate));
}
float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace

// ---- HitDetector -----------------------------------------------------------------

RV_SIZE_OPT void HitDetector::prepare(float sampleRate)
{
    hpLp_.setCutoff(splash::kDetectorHpHz, sampleRate);
    fastAtt_ = onePole(splash::kFastAttackMs, sampleRate);
    fastRel_ = onePole(splash::kFastReleaseMs, sampleRate);
    slowAtt_ = onePole(splash::kSlowAttackMs, sampleRate);
    slowRel_ = onePole(splash::kSlowReleaseMs, sampleRate);
    const float every = float(splash::kControlInterval);
    progAtt_ = onePole(splash::kProgAttackMs, sampleRate, every);
    progRel_ = onePole(splash::kProgReleaseMs, sampleRate, every);
    reset();
}

RV_SIZE_OPT void HitDetector::reset()
{
    hpLp_.reset();
    fast_ = slow_ = dMax_ = lastD_ = prog_ = 0.0f;
}

// ---- HitEnvelope -------------------------------------------------------------------

RV_SIZE_OPT void HitEnvelope::prepare(float sampleRate)
{
    fa_  = onePole(splash::kEnvFastAttackMs, sampleRate);
    fr_  = onePole(splash::kEnvFastReleaseMs, sampleRate);
    sa_  = onePole(splash::kEnvSlowAttackMs, sampleRate);
    sr_  = onePole(splash::kEnvSlowReleaseMs, sampleRate);
    lpC_ = 1.0f - std::exp(-2.0f * map::kPi * splash::kClangHz / sampleRate);
    reset();
}

RV_SIZE_OPT void HitEnvelope::reset()
{
    fast_ = slow_ = hiFast_ = lp_ = e_ = short_ = eh_ = hitMax_ = 0.0f;
    loudRef_ = splash::kLoudRef; // as constructed (the voicing's scale is set again on the next tick)
    invRef2_ = 1.0f / (loudRef_ * loudRef_);
}

// ---- Jolt ----------------------------------------------------------------------------

RV_SIZE_OPT void Jolt::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_ = sampleRate;
    seed_       = seed;
    attack_     = onePole(splash::kJoltAttackMs, sampleRate);
    rStep_      = splash::kRattleHz * float(splash::kControlInterval) / sampleRate;
    set(100.0f, 0.0f, 0.0f, 0.0f);
    reset();
}

RV_SIZE_OPT void Jolt::reset()
{
    target_ = j_ = 0.0f;
    rng_.seed(seed_);
    rPos_ = 0.0f;
    rA_   = 0.0f;
    rB_   = rng_.bipolar();
    rPrev_ = rCur_ = 0.0f;
}

void Jolt::set(float decayMs, float loopFrac, float allpass, float rattleDepth)
{
    decay_       = decayPerStep(decayMs, sampleRate_);
    loopFrac_    = loopFrac;
    allpass_     = allpass;
    rattleDepth_ = rattleDepth;
}

RV_SIZE_OPT void Jolt::tick(float tankLevel)
{
    rPos_ += rStep_;
    if (rPos_ >= 1.0f) {
        rPos_ -= 1.0f;
        rA_ = rB_;
        rB_ = rng_.bipolar();
    }
    float amount = j_ + splash::kRattleEnergyGain * tankLevel;
    amount = amount > 1.0f ? 1.0f : amount;
    rPrev_ = rCur_;
    rCur_  = rattleDepth_ * amount * (rA_ + (rB_ - rA_) * smoothstep(rPos_));
}

// ---- Splash ----------------------------------------------------------------------------

RV_SIZE_OPT void Splash::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_ = sampleRate;
    seed_       = mixSeed(seed);
    hpLp_.setCutoff(splash::kDetectorHpHz, sampleRate);
    detector_.prepare(sampleRate);
    envelope_.prepare(sampleRate);
    jolt_.prepare(sampleRate, mixSeed(seed_ + 2u));
    const float every = float(splash::kControlInterval);
    minStroke_     = int(splash::kMinStrokeMs * 0.001f * sampleRate);
    maxRiseTicks_  = int(splash::kMaxRiseMs * 0.001f * sampleRate / every + 0.5f);
    attW_   = {{-1.0f, -1.0f, -1.0f}};
    splash_ = driveGain_ = inputGain_ = -1.0f;
    invInputRef_ = 1.0f / drive::dbToGain(drive::inputGainDb(splash::kSplashRefDrive));
    setVoicing(voicing_);
    set({{0.0f, 1.0f, 0.0f}}, 0.3f);
    reset();
}

RV_SIZE_OPT void Splash::reset()
{
    hpLp_.reset();
    detector_.reset();
    envelope_.reset();
    jolt_.reset();
    rng_.seed(seed_);
    k_ = 0;
    hit_ = strokePeak_ = valley_ = 0.0f;
    pending_ = false;
    armed_   = true;
    countdown_ = 0;
    strength_  = 0.0f;
    impacts_   = 0;
    strokes_   = 0;
    sinceStroke_ = 1 << 30;
}

void Splash::setVoicing(int v)
{
    voicing_ = splash::kVoicingsBuilt ? (v < 0 ? 0 : (v > 3 ? 3 : v)) : splash::kDefaultVoicing;
    splash_ = -1.0f; // set() recomputes
}

RV_SIZE_OPT void Splash::set(const std::array<float, 3>& attitudeWeights, float splash, float driveGain, float inputGain)
{
    if (attitudeWeights == attW_ && splash == splash_ && driveGain == driveGain_ && inputGain == inputGain_)
        return; // blend + exp only on change
    attW_       = attitudeWeights;
    splash_     = splash;
    driveGain_  = driveGain;
    inputGain_  = inputGain;
    voice_      = splash::blendVoice(attitudeWeights);
    // SPLASH stronger (SplashVoicing.h): a bigger, longer top quarter;
    // DRIVE-free voicings keep DRIVE 0.8's gain and judge levels as DRIVE
    // 0.8 would.
    float dg = driveGain, level = 1.0f, cb = 1.0f, bb = 1.0f;
    if constexpr (splash::kVoicingsBuilt) {
        const splash::Strong& sv = splash::strong(voicing_);
        const bool free = sv.driveFree > 0.0f;
        if (free) { // eased in over SPLASH 0 .. kFreeRampSplash: SPLASH 0 (the Jolt floor) stays today's
            dg    = 1.0f;
            const float full = inputGain * invInputRef_, r = splash * (1.0f / splash::kFreeRampSplash);
            level = r >= 1.0f ? full : map::expLerp(1.0f, full, r);
        }
        cb = splash::topBoost(splash, sv.topClang);
        bb = splash::topBoost(splash, sv.topBite);
        const float holdMs = sv.holdMs + sv.topHoldMs * (splash::topBoost(splash, 1.0f) - 1.0f);
        envelope_.setHold(holdMs > 0.0f ? decayPerStep(holdMs, sampleRate_) : 0.0f);
        envelope_.setLoudScale(level);
        clangCeil_ = sv.clangCeil;
        envelope_.setToday(voice_.clang * driveGain, voice_.clangShort * driveGain);
    }
    envelope_.set(splash, voice_.clang * cb, voice_.clangShort * cb, voice_.bite * bb, dg);
    detector_.setThresholds(splash::hitThreshold(splash) * level, splash::relThreshold(splash));
    jolt_.set(voice_.joltDecayMs, voice_.joltLoopFrac, voice_.joltAllpass, voice_.rattleDepth);
}

RV_SIZE_OPT void Splash::fire()
{
    // A hit's splash is its own sound (Clang, Bite, applied by the Tank,
    // ADR 0032); the impact fires the Jolt.
    jolt_.impact(strength_ * splash::joltAmount(voice_, splash_));
    ++impacts_;
    ++strokes_;
    sinceStroke_ = 0;
    pending_     = false;
}

RV_SIZE_OPT void Splash::controlTick()
{
    const float h = detector_.take();
    hit_ = h;
    envelope_.setProgramLevel(detector_.programLevel());
    if (armed_ && h < valley_) valley_ = h;
    if (armed_ && h > splash::kOnsetHit + splash::kRetriggerRatio * valley_) {
        armed_      = false;
        strokePeak_ = h;
        if (!pending_) {
            // New stroke: an impact after a seeded timing jitter.
            const float u  = 0.5f * (rng_.bipolar() + 1.0f);
            const float ms = splash::kJitterMinMs + (splash::kJitterMaxMs - splash::kJitterMinMs) * u;
            pending_        = true;
            countdown_      = int(ms * 0.001f * sampleRate_);
            jitter_         = countdown_;
            riseTicks_      = 0;
            strength_       = h;
        }
    } else if (!armed_) {
        if (h > strokePeak_) strokePeak_ = h;
        if (h < splash::kRearmRatio * strokePeak_ && sinceStroke_ >= minStroke_) {
            armed_  = true;
            valley_ = h;
        }
    }
    // While the primary waits out its jitter it takes the stroke's peak Hit.
    // The jitter runs from the peak (M8): while the stroke is still growing
    // (for at most splash::kMaxRiseMs) the countdown restarts, so a short
    // jitter can no longer fire a weak Jolt on a hit's first millisecond.
    if (pending_) {
        const bool rising = h > strength_;
        if (rising) strength_ = h;
        if (rising && ++riseTicks_ <= maxRiseTicks_) countdown_ = jitter_ + splash::kControlInterval; // re-checked next tick
    }
    jolt_.tick(tankLevel_);
}

void Splash::process(const float* in, float* clangOut, float* biteOut, float* joltLoopOut, int n, float* clangTodayOut)
{
    for (int i = 0; i < n; ++i) {
        if (pending_ && countdown_-- <= 0) fire();

        if (sinceStroke_ < (1 << 30)) ++sinceStroke_;
        const float h = in[i] - hpLp_.process(in[i]); // one high-pass for both detectors
        detector_.pushHighpassed(h);
        float clang, bite, today;
        envelope_.push(h, clang, bite, today);
        if (clangTodayOut) clangTodayOut[i] = today;
        if (clangOut) clangOut[i] = clang;
        if (biteOut) biteOut[i] = bite;
        const float jl = jolt_.process(k_);
        if (joltLoopOut) joltLoopOut[i] = jl;
        if (++k_ == splash::kControlInterval) {
            k_ = 0;
            controlTick();
        }
    }
}

} // namespace rv::dsp
