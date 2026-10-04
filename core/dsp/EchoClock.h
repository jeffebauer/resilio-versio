#pragma once
// The echo's clock (SPRINGS 3 echo mode, ADR 0041; numbers in
// params/EchoVoicing.h). The gate's rising edges in, a beat length out: one
// pulse = one beat (a quarter note).
//
// Tempo is the interval between rising edges, read robustly: the median of
// the last three intervals (two: their mean; one: itself), and a new reading
// within echo::kClockJitter of the tempo in use is ignored, so a gate read
// once per audio block (1 ms) or a slightly wobbly clock doesn't move the
// echo. An interval shorter than kClockMaxBpm's is a bounce (ignored); one
// longer than kClockMinBpm's starts the count again. The clock is lost after
// kClockLostBeats beats with no pulse (at most kClockLostSeconds): the Tank
// then glides back to free time.
//
// Sample positions are a running uint32 count (wraps after ~24 h at 48 kHz;
// differences stay right across the wrap).

#include "params/EchoVoicing.h"

#include <cstdint>

namespace rv::dsp {

class EchoClock {
public:
    void prepare(float sampleRate)
    {
        minIv_ = sampleRate * 60.0f / (echo::kClockMaxBpm * (1.0f + echo::kClockJitter));
        maxIv_ = sampleRate * 60.0f / (echo::kClockMinBpm * (1.0f - echo::kClockJitter));
        lostMax_ = echo::kClockLostSeconds * sampleRate;
        reset();
    }
    void reset()
    {
        haveLast_ = locked_ = false;
        n_    = 0;
        beat_ = 0.0f;
    }

    // A rising edge at absolute sample `at`.
    void edge(uint32_t at)
    {
        if (!haveLast_) {
            haveLast_ = true;
            last_     = at;
            return;
        }
        const float d = float(at - last_);
        if (d < minIv_) return; // a bounce (or faster than kClockMaxBpm): ignored, the beat goes on from the last edge
        last_ = at;
        if (d > maxIv_) { // slower than kClockMinBpm: count again from here
            n_ = 0;
            return;
        }
        iv_[0] = iv_[1];
        iv_[1] = iv_[2];
        iv_[2] = d;
        if (n_ < 3) ++n_;
        float m;
        if (n_ == 1) m = d;
        else if (n_ == 2) m = 0.5f * (iv_[1] + iv_[2]);
        else {
            const float a = iv_[0], b = iv_[1], c = iv_[2];
            m = a > b ? (b > c ? b : (a > c ? c : a)) : (a > c ? a : (b > c ? c : b)); // median
        }
        const float dev = m > beat_ ? m - beat_ : beat_ - m;
        if (!locked_ || dev > echo::kClockJitter * beat_) beat_ = m;
        locked_ = true;
    }

    // Lost? Call with the current absolute sample (control rate).
    void update(uint32_t now)
    {
        if (!haveLast_) return;
        const float since = float(int32_t(now - last_)); // an edge later in this block: negative, not lost
        float lost = locked_ ? echo::kClockLostBeats * beat_ : maxIv_;
        lost = lost < lostMax_ ? lost : lostMax_;
        if (since > lost) reset();
    }

    bool  locked() const { return locked_; }
    float beatSamples() const { return locked_ ? beat_ : 0.0f; } // 0 = no clock

private:
    float    iv_[3] = {0.0f, 0.0f, 0.0f};
    float    beat_ = 0.0f, minIv_ = 0.0f, maxIv_ = 0.0f, lostMax_ = 0.0f;
    uint32_t last_ = 0;
    int      n_ = 0;
    bool     haveLast_ = false, locked_ = false;
};

} // namespace rv::dsp
