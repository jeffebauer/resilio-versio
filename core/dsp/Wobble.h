#pragma once
// WOBBLE generator for one Spring (SPEC §3 K5, §4.7; ADR 0008, 0020;
// CONTEXT.md: Drift, Warble, Micro-mod floor). M7 stand-alone component, not
// yet wired into the Spring (docs/m7-integration.md: it is added to M6's
// Micro-mod floor offset on the Loop delay read).
//
//   m(t) = D · (wS · sin(2π f t + φ) + wR · r(t))      samples of Loop delay
//
//   f  = rate, 0.12 → 1.4 Hz, rising with WOBBLE (× a per-Spring ratio)
//   r  = seeded random points at 1.37 f, smoothstep-interpolated
//   wS = sine share, 0.3 (Drift: mostly random) → 0.9 (Warble: periodic wow)
//   D  = depth from a cents-per-pass curve (splash::wobbleDepthSamples):
//        Drift 0–3 cents, transition 3–10.5, Warble 10.5–35 (SplashVoicing.h
//        has the maths). Exactly 0 at WOBBLE 0.
//
// Independent per Spring: each instance has its own seed (random start
// phase φ and random line) and rate ratio (splash::kWobbleSpringRate).
// The sine and random line are evaluated on a fixed 32-sample grid counted
// from reset() (one sinf per 32 samples) and linearly interpolated per
// sample, so output is block-size independent and costs ~2 ops per sample.
// Sample-rate aware: D scales with fs, rates are in Hz.

#include "dsp/Filters.h"
#include "dsp/Seed.h"
#include "params/SplashVoicing.h"

#include <cstdint>

namespace rv::dsp {

class Wobble {
public:
    // springIndex picks the rate ratio (0..2); seed makes it independent.
    void prepare(float sampleRate, int springIndex, uint32_t seed);
    void reset();

    // Control rate: smoothed WOBBLE Normalised value (Gliding tier, ADR 0015).
    // Re-maps (a few exp) only when it changed.
    void setAmount(float wobble);

    // One sample of Loop delay modulation, in samples (signed, |m| <= D).
    float next()
    {
        if (k_ == 0) tick();
        const float y = prev_ + (cur_ - prev_) * (float(k_) * (1.0f / float(splash::kControlInterval)));
        if (++k_ == splash::kControlInterval) k_ = 0;
        return y;
    }
    void process(float* out, int n)
    {
        for (int i = 0; i < n; ++i) out[i] = next();
    }

    float depthSamples() const { return depth_; }
    float rateHz() const { return rateHz_; }

private:
    void  tick();
    float value() const;

    float    sampleRate_ = 48000.0f;
    float    rateScale_  = 1.0f;
    uint32_t seed_       = 1;
    Rng      rng_;

    float amount_ = -1.0f;
    float depth_ = 0.0f, rateHz_ = 0.0f, sineW_ = 0.0f;
    float phaseStep_ = 0.0f, randStep_ = 0.0f;

    float phase_ = 0.0f, rPos_ = 0.0f, rA_ = 0.0f, rB_ = 0.0f;
    float prev_ = 0.0f, cur_ = 0.0f;
    int   k_ = 0;
};

} // namespace rv::dsp
