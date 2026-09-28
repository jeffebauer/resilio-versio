#pragma once
// Oversampling for the nonlinear stages (SPEC §4.9, §5 mitigation 4).
//
// Why: a saturator makes harmonics. Any harmonic above half the sample rate
// can't exist in the digital signal, so it "folds back" to an unrelated,
// inharmonic frequency (aliasing: a gritty, digital fizz). Running the
// saturator at 2x the rate doubles the room above the audio band; a sharp
// low-pass then removes everything above the audio band before coming
// back down, so only a tiny fraction folds back.
//
// Filter: polyphase IIR halfband (Valenzuela & Constantinides 1983; the
// structure in Laurent de Soras' HIIR). Two branches of first-order allpass
// sections:
//     H(z) = ½ [ A0(z²) + z⁻¹ A1(z²) ],   A0 = coefs 0, 2, 4;  A1 = coefs 1, 3, 5
// Each section only ever runs at the LOW rate, so the whole up + down pair
// costs 12 first-order allpasses per base-rate sample. Cheap and low-latency
// (IIR, no long FIR); the price is a non-linear phase, which is fine inside
// a reverb.
//
// Neat property used for the Loop design: with nothing (or anything linear
// with gain 1) between them, up-then-down is exactly A0(z)·A1(z) at the base
// rate: a pure allpass. Magnitude exactly 1 at every frequency, so it can
// never push Loop gain up; it only adds a small group delay (the latency,
// 2.48 samples at low frequencies for x2), which the Spring counts in its
// round trip (Spring::roundTripSamples) so g and the repeat time stay right.
//
// Design: 6 coefficients, transition bandwidth 0.06 (HIIR's designer):
// passband flat to 0.22·fs_high (21 kHz at 48 kHz base rate), stopband
// ≥ 85 dB down from 0.28·fs_high (26.9 kHz). Computed offline, fixed.

#include "params/DriveVoicing.h"
#include "params/Mappings.h"

#include <array>
#include <cmath>

namespace rv::dsp {

inline constexpr std::array<float, 6> kHalfbandCoefs{{0.0542175258f, 0.1967979698f, 0.3830873273f, 0.5731364111f,
                                                      0.7487209444f, 0.9142937097f}};

// One halfband filter (fs <-> 2 fs). Holds its own state; use one object per
// direction per stage.
struct Halfband {
    static constexpr int kPerBranch = int(kHalfbandCoefs.size()) / 2;
    // First-order allpass per section, at the low rate: y = c(x - y1) + x1.
    std::array<float, kPerBranch> x0{}, y0{}, x1{}, y1{};

    void reset() { x0.fill(0.0f); y0.fill(0.0f); x1.fill(0.0f); y1.fill(0.0f); }

    static float branch(float x, int first, std::array<float, kPerBranch>& xs, std::array<float, kPerBranch>& ys)
    {
        for (int i = 0; i < kPerBranch; ++i) {
            const float c = kHalfbandCoefs[size_t(first + 2 * i)];
            const float y = c * (x - ys[size_t(i)]) + xs[size_t(i)];
            xs[size_t(i)] = x;
            ys[size_t(i)] = y;
            x = y;
        }
        return x;
    }
    // Up: one low-rate sample -> two high-rate samples (first, second).
    // Zero-stuffing + H (with gain 2) reduces to: first = A0·x, second = A1·x.
    void up(float x, float& first, float& second)
    {
        first  = branch(x, 0, x0, y0);
        second = branch(x, 1, x1, y1);
    }
    // Down: two high-rate samples (first, second) -> one low-rate sample.
    // H then keep every other sample: ½ (A0·second + A1·first).
    float down(float first, float second) { return 0.5f * (branch(second, 0, x0, y0) + branch(first, 1, x1, y1)); }

    // Group delay (low-rate samples) of A0·A1 at w (radians per low-rate sample).
    static float groupDelay(float w)
    {
        float d = 0.0f;
        const float cw = std::cos(w);
        for (float c : kHalfbandCoefs) d += (1.0f - c * c) / (1.0f + c * c + 2.0f * c * cw);
        return d;
    }
};

// Runs f(x) at drive::kOversampleFactor x the rate: up, f on every
// high-rate sample, down. f may hold state (e.g. the tape's emphasis
// filters run at the high rate): it is always called in time order.
class Oversampler {
public:
    static constexpr int kFactor = drive::kOversampleFactor;

    void reset()
    {
        for (auto& h : up_) h.reset();
        for (auto& h : down_) h.reset();
    }

    template <class F>
    float process(float x, F&& f)
    {
        if constexpr (kFactor == 1) {
            return f(x);
        } else if constexpr (kFactor == 2) {
            float a, b;
            up_[0].up(x, a, b);
            const float fa = f(a); // explicit order: argument evaluation order is unspecified
            const float fb = f(b);
            return down_[0].down(fa, fb);
        } else {
            float a, b, a0, a1, b0, b1;
            up_[0].up(x, a, b);
            up_[1].up(a, a0, a1);
            up_[1].up(b, b0, b1);
            const float fa0 = f(a0);
            const float fa1 = f(a1);
            const float fb0 = f(b0);
            const float fb1 = f(b1);
            const float da  = down_[1].down(fa0, fa1);
            const float db  = down_[1].down(fb0, fb1);
            return down_[0].down(da, db);
        }
    }

    // Latency (base-rate samples) of the linear part at freqHz: the group
    // delay a signal picks up going through up + down. Exact for x2 (the
    // pair is the allpass A0·A1); for x4 the inner pair runs at 2 fs, so its
    // delay counts half.
    static float latencySamples(float freqHz, float sampleRate)
    {
        if constexpr (kFactor == 1) return 0.0f;
        const float w = 2.0f * map::kPi * freqHz / sampleRate;
        float d = Halfband::groupDelay(w);
        if constexpr (kFactor == 4) d += 0.5f * Halfband::groupDelay(0.5f * w);
        return d;
    }

private:
    static constexpr int kStages = kFactor == 4 ? 2 : 1;
    std::array<Halfband, kStages> up_{}, down_{};
};

} // namespace rv::dsp
