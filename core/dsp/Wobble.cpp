#include "dsp/Wobble.h"

#include <cmath>

namespace rv::dsp {

void Wobble::prepare(float sampleRate, int springIndex, uint32_t seed, Role role)
{
    sampleRate_ = sampleRate;
    role_       = role;
    const int s = springIndex < 0 ? 0 : (springIndex > 2 ? 2 : springIndex);
    rateScale_  = role == Role::Transport ? splash::kWobbleTransportRate : splash::kWobbleSpringRate[size_t(s)];
    seed_       = mixSeed(seed);
    amount_     = -1.0f;
    setAmount(0.0f);
    reset();
}

void Wobble::reset()
{
    rng_.seed(seed_);
    phase_ = 0.5f * (rng_.bipolar() + 1.0f); // seeded start phase: Springs never in step
    rPos_  = 0.0f;
    rA_    = rng_.bipolar();
    rB_    = rng_.bipolar();
    k_     = 0;
    // Start from the generator's own value (no jump from 0 when WOBBLE is up).
    cur_  = value();
    prev_ = cur_;
}

void Wobble::setAmount(float wobble)
{
    if (wobble == amount_) return;
    amount_ = wobble;
    depth_  = role_ == Role::Transport ? splash::wobbleEarlyDepthSamples(wobble, sampleRate_, rateScale_)
                                       : splash::wobbleDepthSamples(wobble, sampleRate_, rateScale_);
    rateHz_ = splash::wobbleRateHz(wobble) * rateScale_;
    sineW_  = splash::wobbleSineWeight(wobble);
    const float every = float(splash::kControlInterval) / sampleRate_;
    phaseStep_ = rateHz_ * every;
    randStep_  = rateHz_ * splash::kWobbleRandomRateRatio * every;
}

void Wobble::tick()
{
    phase_ += phaseStep_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    rPos_ += randStep_;
    if (rPos_ >= 1.0f) {
        rPos_ -= 1.0f;
        rA_ = rB_;
        rB_ = rng_.bipolar();
    }
    prev_ = cur_;
    cur_  = value();
}

float Wobble::value() const
{
    const float t = rPos_ * rPos_ * (3.0f - 2.0f * rPos_);
    const float r = rA_ + (rB_ - rA_) * t;
    const float s = std::sin(2.0f * map::kPi * phase_);
    return depth_ * (sineW_ * s + (1.0f - sineW_) * r);
}

} // namespace rv::dsp
