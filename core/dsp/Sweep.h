#pragma once
// Tank voicing 1 (params/TankVoicing.h; ported from proto/wellspring-fit,
// its version B): the Sweep, a chain of
// stretched allpass sections run once in front of the Springs.
//
// Plain-language version: the same kind of filter as a Spring's Chirp
// chain (Spring.h): it changes no loudness, only when each frequency comes
// out. With a > 0 each section holds the highs back a little more than the
// lows, most just under its top frequency fC, so a click comes out as one
// rising "pew". Outside any Loop it runs once: every echo gets the same
// arc (the Wellspring's echoes keep their shape), and one chain serves all
// three Springs.
//
// Each section: H(z) = (a + D(z)) / (1 + a D(z)), D(z) = z^-N x Thiran(d),
// K = N + d = fs / (2 fC) (Mappings.h stretchK). The stage count glides one
// stage at a time (a stage crossfades in or out, as in Spring.h), K and a
// move per control tick. Memory: rings of ringSize() floats per section
// from the caller's pool.

#include "params/Mappings.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace rv::dsp {

class Sweep {
public:
    // Ring length (power of two) for the largest K (lowest fC) it will see.
    static int ringSize(float minFcHz, float sampleRate)
    {
        const int need = int(std::ceil(map::stretchK(minFcHz, sampleRate))) + 2;
        int p = 1;
        while (p < need) p <<= 1;
        return p;
    }
    static size_t requiredFloats(int maxStages, float minFcHz, float sampleRate)
    {
        return maxStages > 0 ? size_t(maxStages) * size_t(ringSize(minFcHz, sampleRate)) : 0;
    }

    void prepare(float sampleRate, float* pool, int maxStages, float minFcHz)
    {
        sampleRate_ = sampleRate;
        rings_      = pool;
        maxStages_  = std::min(maxStages, kMaxStages);
        ringMask_   = maxStages_ > 0 ? ringSize(minFcHz, sampleRate) - 1 : 0;
        rate_       = 1.0f / (0.008f * sampleRate); // one stage per 8 ms, as the Loop's
        reset();
    }
    void reset()
    {
        if (rings_ && maxStages_ > 0) std::fill(rings_, rings_ + size_t(maxStages_) * size_t(ringMask_ + 1), 0.0f);
        std::fill(y1_, y1_ + kMaxStages, 0.0f);
        w_ = 0;
    }
    // Control rate. snap: jump to the stage count at once.
    void set(float fcHz, float a, int stages, bool snap)
    {
        k_   = std::clamp(map::stretchK(fcHz, sampleRate_), 1.6f, float(ringMask_) - 0.1f);
        n_   = int(k_ - 0.5f);
        const float d = k_ - float(n_);
        eta_ = (1.0f - d) / (1.0f + d);
        a_   = a;
        target_ = std::clamp(stages, 0, maxStages_);
        if (snap) {
            pos_ = float(target_);
            for (int j = target_; j < maxStages_; ++j) clear(j);
            active_ = target_;
        }
    }
    // Group delay (samples) of the chain at freqHz, at the target stage count.
    float groupDelaySamples(float freqHz) const
    {
        return float(target_) * map::stretchedAllpassGroupDelaySamples(a_, k_, freqHz, sampleRate_);
    }
    float process(float x)
    {
        if (pos_ != float(target_)) {
            pos_ = pos_ < float(target_) ? std::min(float(target_), pos_ + rate_) : std::max(float(target_), pos_ - rate_);
            const int act = int(std::ceil(pos_));
            for (int j = act; j < active_; ++j) clear(j);
            active_ = act;
        }
        const int    full = int(pos_);
        const float  frac = pos_ - float(full);
        const int    r0 = (w_ - n_) & ringMask_, r1 = (w_ - n_ - 1) & ringMask_;
        const size_t stride = size_t(ringMask_ + 1);
        float* ring = rings_;
        for (int j = 0; j < full; ++j, ring += stride) {
            const float d = eta_ * (ring[r0] - y1_[j]) + ring[r1];
            y1_[j] = d;
            const float v = x - a_ * d;
            ring[w_] = v;
            x = a_ * v + d;
        }
        if (frac > 0.0f && full < maxStages_) {
            const float d = eta_ * (ring[r0] - y1_[full]) + ring[r1];
            y1_[full] = d;
            const float v = x - a_ * d;
            ring[w_] = v;
            x += frac * (a_ * v + d - x);
        }
        w_ = (w_ + 1) & ringMask_;
        return x;
    }

private:
    static constexpr int kMaxStages = 64;
    void clear(int j)
    {
        std::fill(rings_ + size_t(j) * size_t(ringMask_ + 1), rings_ + size_t(j + 1) * size_t(ringMask_ + 1), 0.0f);
        y1_[j] = 0.0f;
    }
    float  sampleRate_ = 48000.0f;
    float* rings_ = nullptr;
    int    maxStages_ = 0, ringMask_ = 0, w_ = 0, n_ = 1, target_ = 0, active_ = 0;
    float  k_ = 4.0f, eta_ = 0.0f, a_ = 0.5f, pos_ = 0.0f, rate_ = 0.0f;
    float  y1_[kMaxStages] = {};
};

} // namespace rv::dsp
