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
#include "dsp/Select.h"
#include "dsp/SizeOpt.h"
#include "params/OutputVoicing.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rv::dsp {

class OutputBits {
public:
#ifdef RV_FIXED_VOICINGS
    static constexpr bool kBuilt = outbits::kOutputBitsDefault != 0;
#else
    static constexpr bool kBuilt = true;
#endif

    RV_SIZE_OPT void prepare(float sampleRate, uint32_t seed) // set-up
    {
        seed_ = seed;
        if (!kBuilt) return;
        fadeStep_ = 1.0f / (outbits::kFadeSeconds * sampleRate);
        envRel_   = 1.0f - std::exp(-1000.0f / (outbits::kEnvReleaseMs * 0.5f * sampleRate)); // runs at the half rate
        for (int d = 1; d < 3; ++d) q_[d] = std::exp2(outbits::kDepth[d].bits - 1.0f);
        // The expansion of every code |v| = 0 .. q - 1, worked out here with
        // the very expression quantise() used per sample (same exp, same
        // float steps), so a table read gives the same bits (M3 run 17: an
        // exp and a divide per half-rate sample and channel were most of the
        // box's cost on the chip).
        for (int d = 1; d < 3; ++d) {
            float* const t = expand_ + kTabOffset[d];
            const float  q = q_[d];
            for (int k = 0; k < kTabSize[d]; ++k) t[k] = (std::exp(float(k) * (kLnMu1 / q)) - 1.0f) * (1.0f / kMu);
        }
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

    RV_SIZE_OPT void reset()
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
        // Steady (no ATTITUDE fade under way: setTarget only lands between
        // calls, so the weights hold for the whole call): whole half-rate
        // steps (sample pairs), both channels side by side, without the
        // per-sample glide and drain bookkeeping. In CLEAN only the down
        // filters run (kept warm). The same steps, in the same order per
        // channel, as processSamples: bit for bit (M3 run 17: the box was
        // ~8-9 points; this is most of what it can lose without changing it).
        if (steady() && (w_[0] < 1.0f || drain_ == 0)) {
            int i = 0;
            if (phase_ != 0 && n > 0) processSamples(l, r, i = 1); // finish a step begun in the last call
            const int pairs = (n - i) >> 1;
            if (pairs > 0) {
                if (w_[0] < 1.0f) boxPairs(l + i, r + i, pairs);
                else bypassPairs(l + i, r + i, pairs);
                i += 2 * pairs;
            }
            if (i < n) processSamples(l + i, r + i, n - i); // a step the next call finishes
            return;
        }
        processSamples(l, r, n);
    }

private:
    static constexpr float kMu = 255.0f, kLnMu1 = 5.5451774445f; // ln(1 + mu)
    // The expansion tables (prepare): q = 2^(bits - 1) codes per depth. Whole
    // bits only, so every code is a whole number and indexes its entry.
    static constexpr int kCodes1 = kBuilt ? 1 << int(outbits::kDepth[1].bits - 1.0f) : 1;
    static constexpr int kCodes2 = kBuilt ? 1 << int(outbits::kDepth[2].bits - 1.0f) : 1;
    static_assert(outbits::kDepth[1].bits == float(int(outbits::kDepth[1].bits))
                      && outbits::kDepth[2].bits == float(int(outbits::kDepth[2].bits))
                      && outbits::kDepth[1].bits >= 2.0f && outbits::kDepth[1].bits <= 16.0f
                      && outbits::kDepth[2].bits >= 2.0f && outbits::kDepth[2].bits <= 16.0f,
                  "OutputBits: the expansion tables need whole bit depths (2-16)");
    static constexpr int kTabSize[3]   = {0, kCodes1, kCodes2};
    static constexpr int kTabOffset[3] = {0, 0, kCodes1};
    static constexpr int kDrain   = 96;  // samples the up filter rings on after a fade back to CLEAN (2 ms)
    static constexpr int kZeroRun = 64;  // half-rate steps of exact zeros before the up filter is cleared (2.7 ms)
    struct Chan {
        Halfband down, up;
        float    held = 0.0f, pend = 0.0f, env = 0.0f;
        int      zeros = 0;
        Rng      rng;
    };

    bool steady() const
    {
        for (int a = 0; a < 3; ++a)
            if (w_[a] != (a == target_ ? 1.0f : 0.0f)) return false;
        return true;
    }

    // Halfband::down / up (dsp/Oversampler.h): the same expressions, always
    // inlined (on local copies), so in the pair loops the two channels'
    // chains overlap.
    using Branch = std::array<float, Halfband::kPerBranch>;
    static RV_INLINE float hbBranch(float x, int first, Branch& xs, Branch& ys)
    {
        for (int i = 0; i < Halfband::kPerBranch; ++i) {
            const float c = kHalfbandCoefs[size_t(first + 2 * i)];
            const float y = c * (x - ys[size_t(i)]) + xs[size_t(i)];
            xs[size_t(i)] = x;
            ys[size_t(i)] = y;
            x = y;
        }
        return x;
    }
    static RV_INLINE float hbDown(Halfband& h, float first, float second)
    {
        return 0.5f * (hbBranch(second, 0, h.x0, h.y0) + hbBranch(first, 1, h.x1, h.y1));
    }
    static RV_INLINE void hbUp(Halfband& h, float x, float& first, float& second)
    {
        first  = hbBranch(x, 0, h.x0, h.y0);
        second = hbBranch(x, 1, h.x1, h.y1);
    }

    // One half-rate step's up filter (and the silence clear-out) for one
    // channel; returns the first of its two high-rate samples.
    RV_NOINLINE float upStep(Chan& ch, float u)
    {
        // Silence: once the quantiser has given exact zeros for kZeroRun
        // steps, what's left ringing in the up filter is under -150 dBFS;
        // clear it, so the output is exactly 0 a few ms after the signal
        // goes (not ~40 ms of decaying denormals later).
        ch.zeros = u == 0.0f ? ch.zeros + 1 : 0;
        if (ch.zeros >= kZeroRun) {
            if (ch.zeros == kZeroRun) ch.up.reset();
            ch.zeros = kZeroRun + 1;
            ch.pend  = 0.0f;
            return 0.0f;
        }
        float first, second;
        ch.up.up(u, first, second);
        ch.pend = second;
        return first;
    }

    // Steady DRIVEN / KICKED (or a blend held still): `pairs` whole steps.
    void boxPairs(float* l, float* r, int pairs)
    {
        drain_         = kDrain;
        const float w0 = w_[0];
        Chan&       L  = ch_[0];
        Chan&       R  = ch_[1];
        for (int k = 0; k < pairs; ++k, l += 2, r += 2) {
            const float xl0 = l[0], xl1 = l[1], xr0 = r[0], xr1 = r[1];
            const float bl0 = L.pend, br0 = R.pend; // phase 0: the up filter's second sample from the step before
            // The filters' state read whole into locals, worked, and written
            // back: GCC can't tell the state arrays apart, and would run each
            // section after the last one's stores (no overlap at all).
            Halfband    dl = L.down, dr = R.down;
            const float yl = hbDown(dl, xl0, xl1);
            const float yr = hbDown(dr, xr0, xr1);
            L.down = dl;
            R.down = dr;
            const float ul  = quantise(L, yl);
            const float ur  = quantise(R, yr);
            float bl1, br1;
            if (ul != 0.0f && ur != 0.0f) { // both sounding: the two up filters side by side
                L.zeros = R.zeros = 0;
                float    sl, sr;
                Halfband ql = L.up, qr = R.up;
                hbUp(ql, ul, bl1, sl);
                hbUp(qr, ur, br1, sr);
                L.up   = ql;
                R.up   = qr;
                L.pend = sl;
                R.pend = sr;
            } else {
                bl1 = upStep(L, ul);
                br1 = upStep(R, ur);
            }
            l[0] = w0 * xl0 + bl0;
            l[1] = w0 * xl1 + bl1;
            r[0] = w0 * xr0 + br0;
            r[1] = w0 * xr1 + br1;
            L.held = xl0;
            R.held = xr0;
        }
    }

    // Steady CLEAN, the up filter rung out: the output untouched; only the
    // down filters run, so a flip to DRIVEN or KICKED starts from them warm.
    void bypassPairs(const float* l, const float* r, int pairs)
    {
        for (auto& ch : ch_) {
            ch.up.reset();
            ch.pend = ch.env = 0.0f;
        }
        Chan& L = ch_[0];
        Chan& R = ch_[1];
        Halfband dl = L.down, dr = R.down; // in locals for the whole run (boxPairs)
        for (int k = 0; k < pairs; ++k, l += 2, r += 2) {
            hbDown(dl, l[0], l[1]);
            hbDown(dr, r[0], r[1]);
        }
        L.down = dl;
        R.down = dr;
        L.held = l[-2];
        R.held = r[-2];
    }

    // Sample by sample: an ATTITUDE fade (20 ms), the 2 ms drain after one
    // back to CLEAN, and a half step at either end of a call. Built for size
    // in the firmware (the steady pair loops above carry the time).
    RV_SIZE_OPT void processSamples(float* l, float* r, int n)
    {
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
                        boxed = upStep(ch, quantise(ch, y));
                    }
                }
                if (!bypass) io[c][i] = w_[0] * x + boxed;
            }
            phase_ ^= 1;
        }
    }

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
    RV_NOINLINE float quantise(Chan& ch, float y)
    {
        float a = std::fabs(y);
        a = a > 1.0f ? 1.0f : a;
        const float cpr = std::log(1.0f + kMu * a) * (1.0f / kLnMu1); // compressed, 0..1 (log, exp: already in the firmware; log1p, expm1 cost 4.8 KB of flash)
        ch.env = selGt(cpr, ch.env, cpr, ch.env + envRel_ * (cpr - ch.env)); // attack at once, release on envRel_ (VSEL on the chip: the signal's coin flips are no branches)
        if (ch.env <= 0.0f) return 0.0f;                         // silence in, silence out (and no dither drawn)
        const float sc   = selGt(0.0f, y, -cpr, cpr);            // signed (y < 0: -cpr): 0 is a code, the dither symmetric round it
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
            const float m = expand_[kTabOffset[d] + int(std::fabs(v))]; // expand: (exp(|v| ln(1 + mu) / q) - 1) / mu, the table (prepare)
            out += w_[d] * selGt(0.0f, v, -m, m); // v < 0: -m
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
    float expand_[kCodes1 + kCodes2] = {};
};

} // namespace rv::dsp
