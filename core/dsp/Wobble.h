#pragma once
// WOBBLE generator for one Spring (SPEC §3 P6, §4.7; ADR 0034 bipolar
// WOBBLE, ADR 0020; CONTEXT.md: Drift, Warble, wow, flutter, Micro-mod
// floor). One per Spring, owned by the Tank; its output is added on top of
// M6's Micro-mod floor on the Loop delay read (Spring::process()'s
// lSamples, docs/m7-integration.md).
//
// Bipolar knob (params/WobbleVoicing.h has every number and the maths):
//   left of noon   m = Dw·wow(t) + Df·flutter(t)   smooth random, never repeats
//   noon           m = 0 (dead zone; only the Micro-mod floor remains)
//   right of noon  m = Ds·sin(2π ∫f dt + φ)        sine LFO, rate drifting slightly
// wow / flutter are seeded random lines (Catmull-Rom through random points,
// each segment with its own random length). Only the active side's lines
// advance; the other side's depth is exactly 0.
//
// Independent per Spring (own seed, start phase and rate ratio), except at
// low amounts, where Springs B and C share Spring A's movement (next(leader),
// WobbleVoicing.h "Shared Drift"). The lines and the sine are evaluated on
// a fixed 32-sample grid counted from reset() (one sinf per 32 samples) and
// linearly interpolated per sample, so output is block-size independent
// and costs ~2 ops per sample plus ~3 per sample for the grid work.
// Sample-rate aware: depths scale with fs, rates are in Hz.
//
// Transport role (M8): the same generator with the first-echo depths
// (WobbleVoicing.h k*EarlyCents) and its own rate ratio. The Tank runs one,
// shared by all Springs, as an offset on every pickup read, so the first
// echoes waver too.

#include "dsp/Filters.h"
#include "dsp/Seed.h"
#include "params/SplashVoicing.h"
#include "params/WobbleVoicing.h"

#include <cstdint>

namespace rv::dsp {

class Wobble {
public:
    enum class Role : uint8_t { Loop, Transport };

    // springIndex picks the rate ratio and Loop-length ratio (0..2; ignored
    // for the Transport); seed makes it independent.
    void prepare(float sampleRate, int springIndex, uint32_t seed, Role role = Role::Loop);
    void reset();

    // Control rate: smoothed WOBBLE Normalised value (Gliding tier, ADR 0015),
    // bipolar (noon = still). Re-maps (a few exp) only when it changed.
    void setAmount(float wobble);

    // One sample of Loop delay modulation, in samples (signed).
    float next()
    {
        if (k_ == 0) tick();
        const float y = prev_ + (cur_ - prev_) * (float(k_) * (1.0f / float(splash::kControlInterval)));
        if (++k_ == splash::kControlInterval) k_ = 0;
        return y;
    }
    // Springs B and C: blend with Spring A's value for the same sample
    // (leader), scaled by this Spring's Loop ratio to A's. Shared at low
    // amounts, independent from WobbleVoicing.h kShareTo up.
    float next(float leader)
    {
        const float lead = loopRatio_ * leader;
        return lead + indep_ * (next() - lead);
    }
    void process(float* out, int n)
    {
        for (int i = 0; i < n; ++i) out[i] = next();
    }

    float depthSamples() const { return depths_.lfo + depths_.wow + depths_.flutter; }
    const wobble::Depths& depths() const { return depths_; }
    float independence() const { return indep_; }

private:
    // Seeded random line through points p0..p3 (Catmull-Rom between p1 and
    // p2); each segment runs at the base rate × its own random factor.
    struct RandomLine {
        float p0 = 0, p1 = 0, p2 = 0, p3 = 0, pos = 0, factor = 1;
        void start(Rng& rng, float lo, float hi);
        void advance(Rng& rng, float step, float lo, float hi)
        {
            pos += step * factor;
            if (pos >= 1.0f) {
                pos -= 1.0f;
                p0 = p1;
                p1 = p2;
                p2 = p3;
                p3 = rng.bipolar();
                factor = lo + (hi - lo) * 0.5f * (rng.bipolar() + 1.0f);
            }
        }
        float value() const
        {
            const float t = pos;
            return p1 + 0.5f * t * ((p2 - p0) + t * ((2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) + t * (3.0f * (p1 - p2) + p3 - p0)));
        }
    };

    void  tick();
    float value() const;

    float    sampleRate_ = 48000.0f;
    float    rateScale_  = 1.0f;
    float    loopRatio_  = 1.0f;
    Role     role_       = Role::Loop;
    uint32_t seed_       = 1;
    Rng      rng_;

    float          amount_ = -1.0f;
    wobble::Depths depths_{};
    float          indep_ = 1.0f;
    float          lfoStep_ = 0.0f, wanderStep_ = 0.0f, wowStep_ = 0.0f, flutterStep_ = 0.0f;

    float      phase_ = 0.0f;
    RandomLine wander_, wow_, flutter_;
    float      prev_ = 0.0f, cur_ = 0.0f;
    int        k_ = 0;
};

} // namespace rv::dsp
