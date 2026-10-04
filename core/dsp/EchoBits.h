#pragma once
// Bit depth on the echo's repeats (PROTOTYPE, owner 4 Oct 2026; numbers and
// the musical story in params/EchoVoicing.h "Bits"). Runs on the tape's
// feedback after the wear (TapeEcho::wear), so it compounds pass by pass.
//
// The rate: 48 kHz -> 24 kHz -> 48 kHz through a linear-phase low-pass
// (windowed sinc, kBitsTaps taps, cut at a quarter of 48 kHz) on the way down
// and again on the way up, so nothing folds back and no new pitch appears
// (the BBD round's pitched artefacts came from images left in the band).
//
// The bits: fixed point against FULL SCALE (as real 12- or 8-bit audio), so
// each quieter repeat has fewer bits left and the grain grows as the echoes
// fade. Rounded toward zero (magnitude truncation): in a loop whose gain is
// under 1 this can never hold a value up, so a fading repeat always reaches
// silence; never a stuck buzz (a limit cycle). Dithered (TPDF) while the
// signal is well above the bottom bit, so the grain is a soft hiss instead
// of tonal "granulation" (a pitched buzz on a decaying tone); the dither
// fades out with the signal (kBitsDitherFadeLsb), so the hiss ends in
// silence and never feeds itself round the loop.
//
// Its fixed delay (the two filters') is taken off the tape (TapeEcho::tick)
// and given to the input (delayInput), so the echo time holds.

#include "dsp/Filters.h"
#include "params/EchoVoicing.h"

#include <cmath>
#include <cstdint>

namespace rv::dsp {

class TapeBits {
public:
#ifdef RV_FIXED_VOICINGS
    static constexpr bool kBuilt = echo::kBitsDefault != 0;
#else
    static constexpr bool kBuilt = true;
#endif
    static constexpr int kTaps = kBuilt ? echo::kBitsTaps : 1;
    static constexpr int kRing = kBuilt ? 128 : 1; // >= kTaps, and the input's delay (kTaps - 1)

    void prepare(float sampleRate, uint32_t seed)
    {
        seed_ = seed;
        if (!kBuilt) return;
        // Windowed sinc (Blackman), cut at a quarter of the sample rate
        // (12 kHz at 48 kHz: the 24 kHz rate's Nyquist), unity gain at DC.
        const int   m = kTaps - 1;
        float       sum = 0.0f;
        for (int k = 0; k < kTaps; ++k) {
            const float t = float(k) - 0.5f * float(m);
            const float x = map::kPi * 0.5f * t;
            // A half-band: every second tap away from the centre is exactly 0.
            const int   ti   = k - m / 2;
            const float sinc = ti == 0 ? 1.0f : (ti % 2 == 0 ? 0.0f : std::sin(x) / x);
            const float w = 0.42f - 0.5f * std::cos(2.0f * map::kPi * float(k) / float(m)) + 0.08f * std::cos(4.0f * map::kPi * float(k) / float(m));
            h_[k] = 0.5f * sinc * w;
            sum += h_[k];
        }
        for (int k = 0; k < kTaps; ++k) h_[k] /= sum;
        nz_ = 0;
        for (int k = 0; k < kTaps; ++k)
            if (h_[k] != 0.0f) nzIdx_[nz_++] = k;
        envRel_ = 1.0f - std::exp(-1000.0f / (echo::kBitsEnvReleaseMs * sampleRate));
        reset();
    }

    void setVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        v_ = v > 0 && v < echo::kNumBitsVoicings ? v : 0;
        reset();
#endif
    }
    int  voicing() const { return v_; }
    bool active() const { return kBuilt && v_ != 0; }
    // The two filters' delay: (kTaps - 1) / 2 each.
    float latencySamples() const { return active() ? float(kTaps - 1) : 0.0f; }

    void reset()
    {
        if (!kBuilt) return;
        for (int i = 0; i < kRing; ++i) down_[i] = up_[i] = in_[i] = 0.0f;
        w_ = inW_ = 0;
        phase_ = 0;
        env_ = 0.0f;
        rng_.seed(seed_);
    }

    void delayInput(float* x, int n)
    {
        if (!active()) return;
        const int d = kTaps - 1;
        for (int i = 0; i < n; ++i) {
            int r = inW_ - d;
            if (r < 0) r += kRing;
            in_[inW_] = x[i];
            x[i]      = in_[r];
            if (++inW_ == kRing) inW_ = 0;
        }
    }

    void process(float* x, int n)
    {
        if (!active()) return;
        const echo::BitsVoicing& b = echo::kBits[v_];
        const float q = std::exp2(b.bits - 1.0f); // steps per full scale (one side)
        for (int i = 0; i < n; ++i) {
            down_[w_] = x[i];
            float u = 0.0f;
            if (phase_ == 0) { // a 24 kHz sample: low-pass (its non-zero taps), then the bits
                float y = 0.0f;
                for (int j = 0; j < nz_; ++j) y += h_[nzIdx_[j]] * down_[idx(w_ - nzIdx_[j])];
                u = 2.0f * quantise(y, q, b.muLaw); // zero-stuffed back up: x2 keeps the level
            }
            up_[w_] = u;
            // Back up to 48 kHz: only taps landing on a 24 kHz sample (the
            // stuffed zeros between them add nothing): every second one.
            float z = 0.0f;
            for (int k = phase_; k < kTaps; k += 2) z += h_[k] * up_[idx(w_ - k)];
            x[i]  = z;
            phase_ ^= 1;
            if (++w_ == kRing) w_ = 0;
        }
    }

private:
    static int idx(int i) { return i < 0 ? i + kRing : i; }

    float quantise(float x, float q, bool muLaw)
    {
        constexpr float kMu = 255.0f;
        const float invLnMu = 1.0f / std::log1p(kMu);
        const float s = x < 0.0f ? -1.0f : 1.0f;
        float a = std::fabs(x);
        a = a > 1.0f ? 1.0f : a;
        if (muLaw) a = std::log1p(kMu * a) * invLnMu; // compress, quantise, expand
        // Dither while the signal is well above the bottom bit (env in steps).
        env_ = a > env_ ? a : env_ + envRel_ * (a - env_);
        const float lsbs = env_ * q;
        const float amt  = lsbs <= echo::kBitsDitherFadeLsb ? lsbs / echo::kBitsDitherFadeLsb : 1.0f;
        const float tpdf = 0.5f * (rng_.bipolar() + rng_.bipolar()) * amt; // +-1 step, triangular
        // Nearest while the signal is well above the bottom bit (no level
        // lost); toward zero as it fades into it, so it always reaches silence.
        const float sc = a * q + tpdf;
        float v = amt >= 1.0f ? std::nearbyint(sc) : std::trunc(sc);
        v = v < 0.0f ? 0.0f : (v > q - 1.0f ? q - 1.0f : v);
        float y = v / q;
        if (muLaw) y = std::expm1(y * std::log1p(kMu)) / kMu;
        return s * y;
    }

    uint32_t seed_ = 1;
#ifdef RV_FIXED_VOICINGS
    static constexpr int v_ = echo::kBitsDefault;
#else
    int v_ = echo::kBitsDefault;
#endif
    float h_[kTaps] = {};
    int   nzIdx_[kTaps] = {};
    int   nz_ = 0;
    float down_[kRing] = {}, up_[kRing] = {}, in_[kRing] = {};
    int   w_ = 0, inW_ = 0, phase_ = 0;
    float env_ = 0.0f, envRel_ = 0.0f;
    Rng   rng_;
};

} // namespace rv::dsp
