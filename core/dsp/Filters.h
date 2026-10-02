#pragma once
// Small DSP building blocks shared by Spring and Tank. Header-only, no
// allocation, one sample at a time. Coefficient setters may use libm; the
// per-sample process() calls never do.

#include "params/Mappings.h"
#include "dsp/SizeOpt.h"

#include <cmath>
#include <cstdint>

namespace rv::dsp {

// One-pole low-pass: y += c (x - y). Cheap, gentle 6 dB/oct slope.
struct OnePoleLowpass {
    float c = 1.0f, y = 0.0f;

    void setCutoff(float hz, float sampleRate) { c = 1.0f - std::exp(-2.0f * map::kPi * hz / sampleRate); }
    float process(float x) { y += c * (x - y); return y; }
    void reset() { y = 0.0f; }

    // |H|² at angular frequency w (radians/sample), for Loop gain design.
    float magnitudeSquared(float cosW) const
    {
        const float p = 1.0f - c;
        return c * c / (1.0f + p * p - 2.0f * p * cosW);
    }
    // Group delay (samples) at w.
    float groupDelay(float cosW) const
    {
        const float p = 1.0f - c;
        return (p * cosW - p * p) / (1.0f - 2.0f * p * cosW + p * p);
    }
};

// DC blocker (first-order high-pass, ~40 Hz in the Loop as in Välimäki 2010).
// Stops any DC offset from recirculating. The (1+R)/2 factor keeps its gain
// <= 1 at every frequency, so it can never push Loop gain above 1.
struct DcBlocker {
    float r = 0.995f, b = 0.9975f, x1 = 0.0f, y1 = 0.0f;

    void setCutoff(float hz, float sampleRate)
    {
        r = 1.0f - 2.0f * map::kPi * hz / sampleRate;
        b = 0.5f * (1.0f + r);
    }
    float process(float x)
    {
        const float y = b * (x - x1) + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0f; }
    float magnitudeSquared(float cosW) const { return b * b * (2.0f - 2.0f * cosW) / (1.0f + r * r - 2.0f * r * cosW); }
};

// Biquad (RBJ cookbook), transposed direct form II.
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;

    void setLowpass(float hz, float q, float sampleRate) { design(hz, q, sampleRate, true); }
    void setHighpass(float hz, float q, float sampleRate) { design(hz, q, sampleRate, false); }
    // RBJ low shelf, shelf slope 1 (Tank voicing 4, params/TankVoicing.h).
    RV_SIZE_OPT void setLowShelf(float hz, float gainDb, float sampleRate) // set-up only
    {
        const float A = std::exp(gainDb * (2.302585093f / 40.0f)); // 10^(dB/40); exp, not pow, for the Firmware's flash
        const float w = 2.0f * map::kPi * hz / sampleRate;
        const float cw = std::cos(w), alpha = std::sin(w) / 2.0f * std::sqrt(2.0f);
        const float sA = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1.0f) + (A - 1.0f) * cw + sA;
        b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + sA) / a0;
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw) / a0;
        b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - sA) / a0;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw) / a0;
        a2 = ((A + 1.0f) + (A - 1.0f) * cw - sA) / a0;
    }

    float process(float x)
    {
        const float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { s1 = s2 = 0.0f; }

    float magnitudeSquared(float cosW) const
    {
        const float sinW = std::sqrt(std::fmax(0.0f, 1.0f - cosW * cosW));
        const float cos2 = 2.0f * cosW * cosW - 1.0f, sin2 = 2.0f * sinW * cosW;
        const float nr = b0 + b1 * cosW + b2 * cos2, ni = b1 * sinW + b2 * sin2;
        const float dr = 1.0f + a1 * cosW + a2 * cos2, di = a1 * sinW + a2 * sin2;
        return (nr * nr + ni * ni) / (dr * dr + di * di);
    }

private:
    void design(float hz, float q, float sampleRate, bool lowpass)
    {
        const float w = 2.0f * map::kPi * hz / sampleRate;
        const float cw = std::cos(w), alpha = std::sin(w) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        const float k  = lowpass ? (1.0f - cw) : (1.0f + cw);
        b0 = 0.5f * k / a0;
        b1 = (lowpass ? k : -k) / a0;
        b2 = b0;
        a1 = -2.0f * cw / a0;
        a2 = (1.0f - alpha) / a0;
    }
};

// Parameter smoother: one-pole glide toward the target with time constant
// smoothingMs (ADR 0015). Snaps exactly onto the target once within 1e-6 so
// a resting control is exactly its value (MIX 0 -> bit-identical dry).
struct Smoother {
    float value = 0.0f, coeff = 1.0f;

    void setTime(float ms, float sampleRate) { coeff = 1.0f - std::exp(-1000.0f / (ms * sampleRate)); }
    float process(float target)
    {
        const float d = target - value;
        value = std::fabs(d) < 1.0e-6f ? target : value + coeff * d;
        return value;
    }
};

// Deterministic white noise (32-bit LCG). Seeded, and re-seeded by reset().
struct Rng {
    uint32_t state = 0x12345678u;

    void seed(uint32_t s) { state = s; }
    float bipolar() // uniform in [-1, 1)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(static_cast<int32_t>(state)) * (1.0f / 2147483648.0f);
    }
};

} // namespace rv::dsp
