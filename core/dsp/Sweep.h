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
// move per control tick. Memory: requiredFloats() floats from the caller's
// pool (the firmware's is in DTCM): the rings, section-interleaved, and each
// section's Thiran state (perf/run16, below).

#include "params/Mappings.h"
#include "dsp/SizeOpt.h"
#include "dsp/StretchedChain.h"

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
    // The rings plus one row of Thiran state (y1), maxStages floats each row.
    static size_t requiredFloats(int maxStages, float minFcHz, float sampleRate)
    {
        return maxStages > 0 ? size_t(std::min(maxStages, kMaxStages)) * size_t(ringSize(minFcHz, sampleRate) + 1) : 0;
    }

    RV_SIZE_OPT void prepare(float sampleRate, float* pool, int maxStages, float minFcHz)
    {
        sampleRate_ = sampleRate;
        rows_       = pool;
        maxStages_  = std::min(maxStages, kMaxStages);
        ringMask_   = maxStages_ > 0 ? ringSize(minFcHz, sampleRate) - 1 : 0;
        y1_         = rows_ ? rows_ + size_t(ringMask_ + 1) * size_t(maxStages_) : nullptr;
        rate_       = 1.0f / (0.008f * sampleRate); // one stage per 8 ms, as the Loop's
        reset();
    }
    void reset()
    {
        if (rows_ && maxStages_ > 0) std::fill(rows_, rows_ + size_t(maxStages_) * size_t(ringMask_ + 2), 0.0f);
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
    // One sample (gliding or not).
    float process(float x)
    {
        glide();
        const int   full = int(pos_);
        const float frac = pos_ - float(full);
        const int   w    = w_;
        w_ = (w + 1) & ringMask_;
        return step(x, w, full, frac);
    }
    // A run of samples in place: the Tank's call. While the stage count
    // rests (always, except for ~8 ms per section after a TENSION move),
    // the stage count and coefficients are fixed for the run.
    void process(float* buf, int n)
    {
        if (pos_ != float(target_)) {
            for (int i = 0; i < n; ++i) buf[i] = process(buf[i]);
            return;
        }
        const int full = int(pos_);
        int       w    = w_;
        for (int i = 0; i < n; ++i) {
            buf[i] = step(buf[i], w, full, 0.0f);
            w      = (w + 1) & ringMask_;
        }
        w_ = w;
    }

private:
    static constexpr int kMaxStages = 64;
    void glide()
    {
        if (pos_ != float(target_)) {
            pos_ = pos_ < float(target_) ? std::min(float(target_), pos_ + rate_) : std::max(float(target_), pos_ - rate_);
            const int act = int(std::ceil(pos_));
            for (int j = act; j < active_; ++j) clear(j);
            active_ = act;
        }
    }
    // The chain for one sample written at ring row w (dsp/StretchedChain.h:
    // rows section-interleaved, row p, section j at rows_[p * maxStages_ +
    // j], so a sample walks four plain runs; pipelined for the M7, as the
    // Spring's Chirp).
    float step(float x, int w, int full, float frac)
    {
        const size_t S = size_t(maxStages_);
        return stretchedChain(x, rows_ + size_t((w - n_) & ringMask_) * S, rows_ + size_t((w - n_ - 1) & ringMask_) * S,
                              rows_ + size_t(w) * S, y1_, full, frac, maxStages_, a_, eta_);
    }
    void clear(int j)
    {
        const size_t S = size_t(maxStages_);
        for (size_t p = 0; p <= size_t(ringMask_); ++p) rows_[p * S + size_t(j)] = 0.0f;
        y1_[j] = 0.0f;
    }
    float  sampleRate_ = 48000.0f;
    float* rows_ = nullptr; // [ringMask_ + 1][maxStages_] rings, then y1_
    float* y1_   = nullptr; // [maxStages_] each section's Thiran output, last sample
    int    maxStages_ = 0, ringMask_ = 0, w_ = 0, n_ = 1, target_ = 0, active_ = 0;
    float  k_ = 4.0f, eta_ = 0.0f, a_ = 0.5f, pos_ = 0.0f, rate_ = 0.0f;
};

} // namespace rv::dsp
