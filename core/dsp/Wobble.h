#pragma once
// WOBBLE generator for one Spring (SPEC §3 K5, §4.7; ADR 0008, 0020;
// CONTEXT.md: Drift, Warble, Micro-mod floor). One per Spring, owned by the
// Tank; its output is added on top of M6's Micro-mod floor on the Loop delay
// read (Spring::process()'s lSamples, docs/m7-integration.md).
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
//
// Transport role (M8): the same generator with the early depth curve
// (splash::wobbleEarlyDepthSamples) and its own rate ratio. The Tank runs
// one, shared by all Springs, as an offset on every pickup read, so the
// first echoes waver too (SplashVoicing.h "WOBBLE on the first echoes").

#include "dsp/Filters.h"
#include "dsp/Seed.h"
#include "params/SplashVoicing.h"

#include <cstdint>

namespace rv::dsp {

class Wobble {
public:
    enum class Role : uint8_t { Loop, Transport };

    // springIndex picks the rate ratio (0..2; ignored for the Transport);
    // seed makes it independent.
    void prepare(float sampleRate, int springIndex, uint32_t seed, Role role = Role::Loop);
    void reset();

    // Control rate: smoothed WOBBLE Normalised value (Gliding tier, ADR 0015).
    // Re-maps (a few exp) only when it changed.
    // depthScale: the Loop depth's DECAY trim (splash::wobbleDecayScale; 1
    // for the transport). Worked out again only when either value moved.
    void setAmount(float wobble, float depthScale = 1.0f);

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
    Role     role_       = Role::Loop;
    uint32_t seed_       = 1;
    Rng      rng_;

    float amount_ = -1.0f, depthScale_ = -1.0f;
    float depth_ = 0.0f, rateHz_ = 0.0f, sineW_ = 0.0f;
    float phaseStep_ = 0.0f, randStep_ = 0.0f;

    float phase_ = 0.0f, rPos_ = 0.0f, rA_ = 0.0f, rB_ = 0.0f;
    float prev_ = 0.0f, cur_ = 0.0f;
    int   k_ = 0;
};

} // namespace rv::dsp
