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
    const float every = float(splash::kControlInterval);
    progAtt_ = onePole(splash::kProgAttackMs, sampleRate, every);
    progRel_ = onePole(splash::kProgReleaseMs, sampleRate, every);
    reset();
}

void HitDetector::reset()
{
    hpLp_.reset();
    fast_ = slow_ = dMax_ = lastD_ = prog_ = 0.0f;
}

// ---- Clatter -----------------------------------------------------------------------

void Clatter::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_ = sampleRate;
    seed_       = seed;
    for (auto& f : hp_) f.setHighpass(splash::kClatterHpHz, 0.707f, sampleRate);
    for (auto& f : lp_) f.setLowpass(splash::kClatterLpHz, 0.707f, sampleRate);
    clickGain_ = std::sqrt(float(kCell) / 3.0f);
    reset();
}

void Clatter::reset()
{
    for (auto& f : hp_) f.reset();
    for (auto& f : lp_) f.reset();
    // Stream 0 keeps the Clatter's own seed; the others are scrambled from it.
    for (size_t s = 0; s < rng_.size(); ++s) {
        rng_[s].seed(s == 0 ? seed_ : mixSeed(seed_ + uint32_t(s)));
        if (kCell > 1) newCell(s);
    }
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
    minStroke_     = int(splash::kMinStrokeMs * 0.001f * sampleRate);
    maxRiseTicks_  = int(splash::kMaxRiseMs * 0.001f * sampleRate / every + 0.5f);
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
    hit_ = strokePeak_ = valley_ = 0.0f;
    pending_ = pendingPrimary_ = forced_ = false;
    armed_   = true;
    countdown_ = secondaries_ = 0;
    strength_  = 0.0f;
    level_     = 1.0f;
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
    detector_.setThresholds(splash::hitThreshold(splash), splash::relThreshold(splash));
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
    // The crash follows the stroke's size (λ, SplashVoicing.h); a Kick has its own.
    const float lvl = forced_ ? splash::kKickClatterLevel : level_;
    if (clat > 0.0f) clatter_.impact(splash::kClatterGain * s * clat * lvl, decayMs);
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
            jitter_         = countdown_;
            riseTicks_      = 0;
            strength_       = h;
            level_          = 0.0f;
        }
    } else if (!armed_) {
        if (h > strokePeak_) strokePeak_ = h;
        if (h < splash::kRearmRatio * strokePeak_ && sinceStroke_ >= minStroke_) {
            armed_  = true;
            valley_ = h;
        }
    }
    // While the primary waits out its jitter it takes the stroke's peak Hit
    // and level λ (largest d so far, for the crash size). The jitter runs
    // from the peak (M8): while the stroke is still growing (for at most
    // splash::kMaxRiseMs) the countdown restarts, so a short jitter can no
    // longer fire a weak burst on a hit's first millisecond.
    if (pending_ && pendingPrimary_ && !forced_) {
        const float lv = std::fmin(detector_.lastDifference() * (1.0f / splash::kClatterLevelRef), splash::kClatterLevelMax);
        const bool rising = h > strength_ || lv > level_;
        if (h > strength_) strength_ = h;
        if (lv > level_) level_ = lv;
        if (rising && ++riseTicks_ <= maxRiseTicks_) countdown_ = jitter_ + splash::kControlInterval; // re-checked next tick
    }
    jolt_.tick(tankLevel_);
}

void Splash::process(const float* driven, float* clatterOut, float* clatterB, float* clatterC, float* joltLoopOut, int n)
{
    const int streams = clatterB && clatterC ? Clatter::kStreams : 1;
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
        float cy[Clatter::kStreams];
        clatter_.process(cy, streams);
        clatterOut[i] = cy[0];
        if (streams > 1) {
            clatterB[i] = cy[1];
            clatterC[i] = cy[2];
        }
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
