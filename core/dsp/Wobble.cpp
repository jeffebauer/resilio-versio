#include "dsp/Wobble.h"
#include "dsp/SizeOpt.h"

#include "params/SpringModes.h"

#include <cmath>

namespace rv::dsp {

RV_SIZE_OPT void Wobble::RandomLine::start(Rng& rng, float lo, float hi)
{
    p0     = rng.bipolar();
    p1     = rng.bipolar();
    p2     = rng.bipolar();
    p3     = rng.bipolar();
    pos    = 0.5f * (rng.bipolar() + 1.0f); // seeded start point along the segment
    factor = lo + (hi - lo) * 0.5f * (rng.bipolar() + 1.0f);
}

RV_SIZE_OPT void Wobble::prepare(float sampleRate, int springIndex, uint32_t seed, Role role)
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

RV_SIZE_OPT void Wobble::reset()
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
    gCur_ = gPrev_ = tremoloGain();
}

RV_SIZE_OPT void Wobble::setAmount(float wobble, float depthScale)
{
    if (wobble == amount_ && depthScale == depthScale_) return;
    amount_     = wobble;
    depthScale_ = depthScale;
    depths_ = wobble::depths(wobble, role_ == Role::Transport, rateScale_, sampleRate_, voicing_);
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

void Wobble::setVoicing([[maybe_unused]] int voicing)
{
#ifdef RV_FIXED_VOICINGS
    return; // firmware: only the default exists
#else
    if (voicing == voicing_) return;
    voicing_ = voicing;
#endif
    const float w = amount_ < 0.0f ? wobble::kNoon : amount_, sc = depthScale_ < 0.0f ? 1.0f : depthScale_;
    amount_ = -1.0f; // force a re-map
    setAmount(w, sc);
}

RV_SIZE_OPT void Wobble::tick()
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
        // The flutter's speed follows the wow (B / C): a tape running fast
        // or slow flutters faster or slower, so it never sits on one rate.
        const float follow = 1.0f + depths_.flutterFollow * wow_.value();
        flutter_.advance(rng_, flutterStep_ * follow, wobble::kFlutterSpreadLo, wobble::kFlutterSpreadHi);
    }
    prev_  = cur_;
    cur_   = value();
    gPrev_ = gCur_;
    gCur_  = tremoloGain();
}

// Flutter tremolo (Transport, left side, B / C): 10^(dB·flutter/20), one
// exp per 32-sample tick; exactly 1 when off, so noon stays bit-identical.
RV_SIZE_OPT float Wobble::tremoloGain() const
{
    if (depths_.tremoloDb <= 0.0f) return 1.0f;
    return std::exp((0.115129255f * depths_.tremoloDb) * flutter_.value()); // ln(10)/20
}

RV_SIZE_OPT float Wobble::value() const
{
    return depths_.lfo * std::sin(2.0f * map::kPi * phase_) + depths_.wow * wow_.value()
         + depths_.flutter * flutter_.value();
}

} // namespace rv::dsp
