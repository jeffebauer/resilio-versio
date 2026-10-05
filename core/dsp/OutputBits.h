#pragma once
// The output's bit depth (ADR 0042, owner 4 Oct 2026; numbers in
// params/OutputVoicing.h, method and measurements in
// docs/prototypes/output-mulaw/README.md). The echo branch's 8-bit mu-law
// "box" (feat/echo-mode 2792b14, dsp/EchoBits.h) moved to the very end of
// the Tank: the stereo output after MIX, so dry and wet both go through it.
// CLEAN: bypassed, bit for bit. DRIVEN: 24 kHz / 12-bit mu-law. KICKED:
// 24 kHz / 10-bit mu-law (8-bit until 5 Oct 2026, ADR 0042 amendment).
//
// Where: after the wet's limiter and after MIX (the dry has to be in it).
// The limiter keeps the wet under -1 dBFS, so the wet alone never reaches
// the box's full scale; dry + wet at mid MIX can, and is then clipped at
// full scale (Tank units, the Plugin's 0 dBFS) like a real converter. The
// firmware's kOutputTrim (-1.17 dB and the polarity flip) comes after.
//
// The rate: 48 -> 24 -> 48 kHz through the Drive oversamplers' polyphase IIR
// half-band (dsp/Oversampler.h Halfband: flat to 10.6 kHz, >= 85 dB down
// from 13.4 kHz), down and up, so nothing folds back and no new pitch
// appears. IIR rather than the echo's 63-tap FIR: the same stop band for a
// fifth of the work, and ~5 samples (0.1 ms) of delay instead of 62
// (1.3 ms), so an ATTITUDE flip can crossfade the box against the
// undelayed CLEAN output without a time jump (CLEAN itself never delayed).
//
// The bits: mu-law (mu 255) against FULL SCALE, quantised in the compressed
// domain (signed, so 0 is a code): fine steps when quiet, coarse when loud.
// Dithered (TPDF, +-1 step) and rounded to nearest down to the last step, so
// the error is a soft hiss that follows the signal, never a pitched
// granulation; the dither fades out between 1 and 0.5 steps of signal
// (kDitherFullLsb, kDitherOffLsb, on a 1 ms envelope), and anything under
// half a step rounds to 0: a fading tail becomes soft grain, then exact
// silence, never a stuck buzz. Silence in, silence out: no dither, no hiss.
// (The echo's version rounds toward zero below 4 steps, undithered: in its
// feedback loop that guarantees the repeats die. Out here there is no loop,
// and those undithered last steps left a pitched fizz at the end of tails:
// M6 ringing_db 7.7 / 10.3 dB vs today's 0.4 / 1.8; README.)
//
// ATTITUDE (ADR 0003): weights (CLEAN, DRIVEN, KICKED) glide linearly to the
// switch over kFadeSeconds: out = wC * x + up(wD * mu12(y) + wK * mu10(y)),
// y = down(x). CLEAN <-> DRIVEN fades the box in and out; DRIVEN <-> KICKED
// crossfades the two depths.

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "params/OutputVoicing.h"

#include <cmath>
#include <cstdint>

namespace rv::dsp {

class OutputBits {
public:
#ifdef RV_FIXED_VOICINGS
    static constexpr bool kBuilt = outbits::kOutputBitsDefault != 0;
#else
    static constexpr bool kBuilt = true;
#endif

    void prepare(float sampleRate, uint32_t seed)
    {
        seed_ = seed;
        if (!kBuilt) return;
        fadeStep_ = 1.0f / (outbits::kFadeSeconds * sampleRate);
        envRel_   = 1.0f - std::exp(-1000.0f / (outbits::kEnvReleaseMs * 0.5f * sampleRate)); // runs at the half rate
        for (int d = 1; d < 3; ++d) q_[d] = std::exp2(outbits::kDepth[d].bits - 1.0f);
        reset();
    }

    void setVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        v_ = v > 0 && v < outbits::kNumVoicings ? v : 0;
        reset();
#endif
    }
    int  voicing() const { return v_; }
    bool active() const { return kBuilt && v_ != 0; }

    // ATTITUDE switch position (0 CLEAN, 1 DRIVEN, 2 KICKED); snap jumps there.
    void setTarget(int att, bool snap)
    {
        target_ = att < 0 ? 0 : (att > 2 ? 2 : att);
        if (snap) {
            for (int a = 0; a < 3; ++a) w_[a] = a == target_ ? 1.0f : 0.0f;
            if (target_ == 0 && kBuilt) { // straight to bypass: nothing left in the up filter
                drain_ = 0;
                for (auto& ch : ch_) {
                    ch.up.reset();
                    ch.pend = 0.0f;
                }
            }
        }
    }
    const float* weights() const { return w_; }

    void reset()
    {
        if (!kBuilt) return;
        for (int c = 0; c < 2; ++c) {
            Chan& ch = ch_[c];
            ch.down.reset();
            ch.up.reset();
            ch.held = ch.pend = ch.env = 0.0f;
            ch.zeros = 0;
            ch.rng.seed(seed_ + 0x9E3779B9u * uint32_t(c + 1));
        }
        phase_ = 0;
        drain_ = w_[0] < 1.0f ? kDrain : 0;
    }

    void process(float* l, float* r, int n)
    {
        if (!active()) return;
        float* io[2] = {l, r};
        for (int i = 0; i < n; ++i) {
            glide();
            // CLEAN: untouched once the up filter has rung out (the down
            // filter stays warm, the up filter empty).
            if (w_[0] < 1.0f) drain_ = kDrain;
            else if (drain_ > 0) --drain_;
            const bool bypass = drain_ == 0;
            for (int c = 0; c < 2; ++c) {
                Chan& ch = ch_[c];
                const float x = io[c][i];
                float       boxed;
                if (phase_ == 0) {
                    ch.held = x;
                    boxed   = ch.pend;
                } else {
                    const float y = ch.down.down(ch.held, x);
                    if (bypass) {
                        ch.up.reset();
                        ch.pend = boxed = 0.0f;
                        ch.env  = 0.0f;
                    } else {
                        const float u = quantise(ch, y);
                        // Silence: once the quantiser has given exact zeros
                        // for kZeroRun steps, what's left ringing in the up
                        // filter is under -150 dBFS; clear it, so the output is
                        // exactly 0 a few ms after the signal goes (not ~40 ms of
                        // decaying denormals later).
                        ch.zeros = u == 0.0f ? ch.zeros + 1 : 0;
                        if (ch.zeros >= kZeroRun) {
                            if (ch.zeros == kZeroRun) ch.up.reset();
                            ch.zeros = kZeroRun + 1;
                            boxed = ch.pend = 0.0f;
                        } else {
                            float s;
                            ch.up.up(u, boxed, s);
                            ch.pend = s;
                        }
                    }
                }
                if (!bypass) io[c][i] = w_[0] * x + boxed;
            }
            phase_ ^= 1;
        }
    }

private:
    static constexpr int kDrain   = 96;  // samples the up filter rings on after a fade back to CLEAN (2 ms)
    static constexpr int kZeroRun = 64;  // half-rate steps of exact zeros before the up filter is cleared (2.7 ms)
    struct Chan {
        Halfband down, up;
        float    held = 0.0f, pend = 0.0f, env = 0.0f;
        int      zeros = 0;
        Rng      rng;
    };

    void glide()
    {
        float dist = 0.0f;
        for (int a = 0; a < 3; ++a) {
            const float t = a == target_ ? 1.0f : 0.0f;
            const float d = std::fabs(t - w_[a]);
            dist = d > dist ? d : dist;
        }
        if (dist <= 0.0f) return;
        const float f = fadeStep_ >= dist ? 1.0f : fadeStep_ / dist;
        for (int a = 0; a < 3; ++a) {
            const float t = a == target_ ? 1.0f : 0.0f;
            w_[a] = f >= 1.0f ? t : w_[a] + f * (t - w_[a]);
        }
    }

    // wD * mu12(y) + wK * mu10(y): one compression, one dither draw, each
    // depth quantised and expanded only while it has weight.
    float quantise(Chan& ch, float y)
    {
        constexpr float kMu = 255.0f, kLnMu1 = 5.5451774445f; // ln(1 + mu)
        float a = std::fabs(y);
        a = a > 1.0f ? 1.0f : a;
        const float cpr = std::log(1.0f + kMu * a) * (1.0f / kLnMu1); // compressed, 0..1 (log, exp: already in the firmware; log1p, expm1 cost 4.8 KB of flash)
        ch.env = cpr > ch.env ? cpr : ch.env + envRel_ * (cpr - ch.env);
        if (ch.env <= 0.0f) return 0.0f;                         // silence in, silence out (and no dither drawn)
        const float sc   = y < 0.0f ? -cpr : cpr;                // signed: 0 is a code, the dither symmetric round it
        const float tpdf = 0.5f * (ch.rng.bipolar() + ch.rng.bipolar()); // +-1 step, triangular
        float       out  = 0.0f;
        for (int d = 1; d < 3; ++d) {
            if (w_[d] <= 0.0f) continue;
            const float q    = q_[d];
            const float lsbs = ch.env * q;
            const float amt  = lsbs >= outbits::kDitherFullLsb ? 1.0f
                             : (lsbs <= outbits::kDitherOffLsb ? 0.0f
                                                               : (lsbs - outbits::kDitherOffLsb) * (1.0f / (outbits::kDitherFullLsb - outbits::kDitherOffLsb)));
            float v = std::nearbyint(sc * q + tpdf * amt);
            v = v < 1.0f - q ? 1.0f - q : (v > q - 1.0f ? q - 1.0f : v);
            const float m = (std::exp(std::fabs(v) * (kLnMu1 / q)) - 1.0f) * (1.0f / kMu); // expand
            out += w_[d] * (v < 0.0f ? -m : m);
        }
        return out;
    }

    uint32_t seed_ = 1;
#ifdef RV_FIXED_VOICINGS
    static constexpr int v_ = outbits::kOutputBitsDefault;
#else
    int v_ = outbits::kOutputBitsDefault;
#endif
    Chan  ch_[2];
    int   phase_ = 0, target_ = 0, drain_ = 0;
    float w_[3] = {1.0f, 0.0f, 0.0f};
    float q_[3] = {1.0f, 1.0f, 1.0f};
    float fadeStep_ = 1.0f, envRel_ = 0.0f;
};

} // namespace rv::dsp
