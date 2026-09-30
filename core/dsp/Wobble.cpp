#include "dsp/Wobble.h"

#include "params/SpringModes.h"

#include <cmath>

namespace rv::dsp {

void Wobble::RandomLine::start(Rng& rng, float lo, float hi)
{
    p0     = rng.bipolar();
    p1     = rng.bipolar();
    p2     = rng.bipolar();
    p3     = rng.bipolar();
    pos    = 0.5f * (rng.bipolar() + 1.0f); // seeded start point along the segment
    factor = lo + (hi - lo) * 0.5f * (rng.bipolar() + 1.0f);
}

void Wobble::prepare(float sampleRate, int springIndex, uint32_t seed, Role role)
{
    sampleRate_ = sampleRate;
    role_       = role;
    const int s = springIndex < 0 ? 0 : (springIndex > 2 ? 2 : springIndex);
    rateScale_  = role == Role::Transport ? wobble::kTransportRate : wobble::kSpringRate[size_t(s)];
    loopRatio_  = modes::kDetune[size_t(s)].loopDelay / modes::kDetune[0].loopDelay;
    seed_       = mixSeed(seed);
    amount_     = -1.0f;
    setAmount(wobble::kNoon);
    reset();
}

void Wobble::reset()
{
    rng_.seed(seed_);
    phase_ = 0.5f * (rng_.bipolar() + 1.0f); // seeded start phase: Springs never in step
    wander_.start(rng_, 0.7f, 1.4f);
    wow_.start(rng_, wobble::kWowSpreadLo, wobble::kWowSpreadHi);
    flutter_.start(rng_, wobble::kFlutterSpreadLo, wobble::kFlutterSpreadHi);
    k_ = 0;
    // Start from the generator's own value (no jump from 0 when WOBBLE is up).
    cur_  = value();
    prev_ = cur_;
}

void Wobble::setAmount(float wobble, float depthScale)
{
    if (wobble == amount_ && depthScale == depthScale_) return;
    amount_     = wobble;
    depthScale_ = depthScale;
    depths_ = wobble::depths(wobble, role_ == Role::Transport, rateScale_, sampleRate_);
    depths_.lfo *= depthScale;
    depths_.wow *= depthScale;
    depths_.flutter *= depthScale;
    indep_  = depths_.independence;
    const float every = float(splash::kControlInterval) / sampleRate_;
    lfoStep_     = depths_.lfoHz * every;
    wanderStep_  = wobble::kLfoWanderRateHz * every;
    wowStep_     = depths_.wowHz * every;
    flutterStep_ = depths_.flutterHz * every;
}

void Wobble::tick()
{
    // Only the active side moves (the other's depth is exactly 0); a frozen
    // side restarts from where it stopped, from depth 0, so nothing jumps.
    if (depths_.lfo > 0.0f) {
        wander_.advance(rng_, wanderStep_, 0.7f, 1.4f);
        phase_ += lfoStep_ * (1.0f + wobble::kLfoRateWander * wander_.value());
        if (phase_ >= 1.0f) phase_ -= 1.0f;
    }
    if (depths_.wow > 0.0f) {
        wow_.advance(rng_, wowStep_, wobble::kWowSpreadLo, wobble::kWowSpreadHi);
        flutter_.advance(rng_, flutterStep_, wobble::kFlutterSpreadLo, wobble::kFlutterSpreadHi);
    }
    prev_ = cur_;
    cur_  = value();
}

float Wobble::value() const
{
    return depths_.lfo * std::sin(2.0f * map::kPi * phase_) + depths_.wow * wow_.value()
         + depths_.flutter * flutter_.value();
}

} // namespace rv::dsp
