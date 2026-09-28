#include "dsp/Splash.h"

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

void HitDetector::prepare(float sampleRate)
{
    hpLp_.setCutoff(splash::kDetectorHpHz, sampleRate);
    fastAtt_ = onePole(splash::kFastAttackMs, sampleRate);
    fastRel_ = onePole(splash::kFastReleaseMs, sampleRate);
    slowAtt_ = onePole(splash::kSlowAttackMs, sampleRate);
    slowRel_ = onePole(splash::kSlowReleaseMs, sampleRate);
    reset();
}

void HitDetector::reset()
{
    hpLp_.reset();
    fast_ = slow_ = dMax_ = lastD_ = 0.0f;
}

// ---- Clatter -----------------------------------------------------------------------

void Clatter::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_ = sampleRate;
    seed_       = seed;
    hp_.setHighpass(splash::kClatterHpHz, 0.707f, sampleRate);
    lp_.setLowpass(splash::kClatterLpHz, 0.707f, sampleRate);
    reset();
}

void Clatter::reset()
{
    hp_.reset();
    lp_.reset();
    rng_.seed(seed_);
    env_ = decay_ = 0.0f;
    idle_ = true;
}

void Clatter::impact(float amplitude, float decayMs)
{
    // The louder burst wins, with its own decay (a weak rattle impact
    // during a big burst does not shorten it).
    if (amplitude <= env_) return;
    env_   = amplitude;
    decay_ = decayPerStep(decayMs, sampleRate_);
    idle_  = false;
}

// ---- Jolt ----------------------------------------------------------------------------

void Jolt::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_ = sampleRate;
    seed_       = seed;
    attack_     = onePole(splash::kJoltAttackMs, sampleRate);
    rStep_      = splash::kRattleHz * float(splash::kControlInterval) / sampleRate;
    set(100.0f, 0.0f, 0.0f, 0.0f);
    reset();
}

void Jolt::reset()
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

void Jolt::tick(float tankLevel)
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

void Splash::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_ = sampleRate;
    seed_       = mixSeed(seed);
    detector_.prepare(sampleRate);
    clatter_.prepare(sampleRate, mixSeed(seed_ + 1u));
    jolt_.prepare(sampleRate, mixSeed(seed_ + 2u));
    const float every = float(splash::kControlInterval);
    hfCoeff_       = decayPerStep(splash::kHfEmphasisReleaseMs, sampleRate, every);
    minStroke_     = int(splash::kMinStrokeMs * 0.001f * sampleRate);
    attW_   = {{-1.0f, -1.0f, -1.0f}};
    splash_ = -1.0f;
    set({{0.0f, 1.0f, 0.0f}}, 0.3f);
    reset();
}

void Splash::reset()
{
    detector_.reset();
    clatter_.reset();
    jolt_.reset();
    rng_.seed(seed_);
    k_ = 0;
    hit_ = hfEnv_ = strokePeak_ = valley_ = 0.0f;
    pending_ = pendingPrimary_ = forced_ = false;
    armed_   = true;
    countdown_ = secondaries_ = 0;
    strength_  = 0.0f;
    impacts_   = 0;
    strokes_   = 0;
    sinceStroke_ = 1 << 30;
    numStrikes_ = 0;
}

void Splash::set(const std::array<float, 3>& attitudeWeights, float splash)
{
    if (attitudeWeights == attW_ && splash == splash_) return; // blend + exp only on change
    attW_   = attitudeWeights;
    splash_ = splash;
    voice_  = splash::blendVoice(attitudeWeights);
    detector_.setThreshold(splash::hitThreshold(splash));
    jolt_.set(voice_.joltDecayMs, voice_.joltLoopFrac, voice_.joltAllpass, voice_.rattleDepth);
}

void Splash::strike(float strength, int sampleOffset)
{
    if (numStrikes_ >= kMaxStrikes) return;
    strikeAt_[size_t(numStrikes_)]       = sampleOffset < 0 ? 0 : sampleOffset;
    strikeStrength_[size_t(numStrikes_)] = strength;
    ++numStrikes_;
}

void Splash::fire()
{
    const float s = strength_;
    const float clat = forced_ ? voice_.clatterMax : splash::clatterAmount(voice_, splash_);
    const float jolt = forced_ ? voice_.joltMax : splash::joltAmount(voice_, splash_);
    const float decayMs = voice_.clatterDecayMinMs + (voice_.clatterDecayMaxMs - voice_.clatterDecayMinMs) * s;
    if (clat > 0.0f) clatter_.impact(splash::kClatterGain * s * clat, decayMs);
    jolt_.impact(s * jolt);
    ++impacts_;

    if (pendingPrimary_) {
        ++strokes_;
        sinceStroke_ = 0;
        secondaries_ = int(voice_.rattleImpacts * s + 0.5f);
    }
    pendingPrimary_ = false;
    if (secondaries_ > 0 && clat > 0.0f) {
        // Next rattle impact: weaker, after a seeded interval.
        --secondaries_;
        strength_ = s * splash::kRattleStrengthRatio;
        const float u = 0.5f * (rng_.bipolar() + 1.0f);
        const float ms = splash::kRattleIntervalMinMs + (splash::kRattleIntervalMaxMs - splash::kRattleIntervalMinMs) * u;
        countdown_ = int(ms * 0.001f * sampleRate_);
        pending_   = true;
    } else {
        secondaries_ = 0;
        pending_     = false;
        forced_      = false;
    }
}

void Splash::controlTick()
{
    const float h = detector_.take();
    hit_ = h;
    if (armed_ && h < valley_) valley_ = h;
    if (armed_ && h > splash::kOnsetHit + splash::kRetriggerRatio * valley_) {
        armed_      = false;
        strokePeak_ = h;
        if (!(pending_ && pendingPrimary_)) {
            // New stroke: a primary impact after a seeded timing jitter
            // (replaces any rattle still queued from the previous one).
            const float u  = 0.5f * (rng_.bipolar() + 1.0f);
            const float ms = splash::kJitterMinMs + (splash::kJitterMaxMs - splash::kJitterMinMs) * u;
            pending_        = true;
            pendingPrimary_ = true;
            forced_         = false;
            secondaries_    = 0;
            countdown_      = int(ms * 0.001f * sampleRate_);
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
    if (pending_ && pendingPrimary_ && !forced_ && h > strength_) strength_ = h;
    hfEnv_ = h > hfEnv_ * hfCoeff_ ? h : hfEnv_ * hfCoeff_;
    jolt_.tick(tankLevel_);
}

void Splash::process(const float* driven, float* clatterOut, float* joltLoopOut, int n)
{
    for (int i = 0; i < n; ++i) {
        // Kick strikes land on their exact sample, no jitter, at full force.
        for (int s = 0; s < numStrikes_; ++s) {
            const int at = strikeAt_[size_t(s)] < n ? strikeAt_[size_t(s)] : n - 1;
            if (at == i) {
                const float st = strikeStrength_[size_t(s)];
                if (!(pending_ && forced_ && countdown_ == 0 && strength_ >= st)) {
                    pending_ = pendingPrimary_ = forced_ = true;
                    countdown_ = 0;
                    strength_  = st;
                }
            }
        }
        if (pending_ && countdown_-- <= 0) fire();

        if (sinceStroke_ < (1 << 30)) ++sinceStroke_;
        detector_.push(driven[i]);
        clatterOut[i] = clatter_.process();
        const float jl = jolt_.process(k_);
        if (joltLoopOut) joltLoopOut[i] = jl;
        if (++k_ == splash::kControlInterval) {
            k_ = 0;
            controlTick();
        }
    }
    numStrikes_ = 0;
}

} // namespace rv::dsp
