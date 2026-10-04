#pragma once
// A chain of stretched allpass sections for one sample: the Spring's Chirp
// (Spring.cpp) and the shared Sweep (Sweep.h). Each section
//   H(z) = (a + D(z)) / (1 + a D(z)),  D(z) = z^-N · Thiran(d) ≈ z^-K,
// in Schroeder form: v = x - a·D{v}, y = a·v + D{v}, with
//   D{v} = eta·(v[n-N] - D{v}[n-1]) + v[n-N-1]   (first-order Thiran).
//
// Memory: the rings are section-interleaved, row p (ring position) holding
// every section's sample p side by side. A sample reads rows w - N (p0) and
// w - N - 1 (p1) and the Thiran state y (one float per section), and writes
// row w (pw) and y: four plain runs, walked with post-increment loads and
// stores and no per-section address arithmetic.
//
// Scheduling (the Cortex-M7 issues in order; ADR 0030, perf/run16): a
// section's D{v} needs only last sample's state, not this sample's x, so
// the next two sections' D{v} are worked out while this pair's x chain
// (x - a·D, a·v, + D: three dependent steps per section) runs, and a·D of
// both is ready when the pair starts. Two sections per pass, so no value
// has to be copied between registers. Every value is computed by the same
// expression as one section at a time (so also the same FMA contraction on
// the desktop): bit-exact with the plain loop.
//
// full whole sections, then (frac > 0, full < maxStages) section `full`
// cross-faded in by frac (a stage gliding in or out).

namespace rv::dsp {

inline float stretchedChain(float x, const float* __restrict p0, const float* __restrict p1, float* __restrict pw,
                            float* __restrict y, int full, float frac, int maxStages, float a, float eta)
{
    if (full == 1) {
        const float d = eta * (p0[0] - y[0]) + p1[0];
        y[0] = d;
        const float v = x - a * d;
        pw[0] = v;
        x = a * v + d;
    } else if (full >= 2) {
        float dA = eta * (p0[0] - y[0]) + p1[0];
        float dB = eta * (p0[1] - y[1]) + p1[1];
        int   j  = 0;
        for (; j + 3 < full; j += 2) {
            // Each D{v} two sections ahead is worked out right after the
            // last use of the one it replaces (no register copies), while
            // the next section's x chain runs.
            y[j] = dA;
            float v = x - a * dA;
            pw[j] = v;
            x = a * v + dA;
            dA = eta * (p0[j + 2] - y[j + 2]) + p1[j + 2];
            y[j + 1] = dB;
            v = x - a * dB;
            pw[j + 1] = v;
            x = a * v + dB;
            dB = eta * (p0[j + 3] - y[j + 3]) + p1[j + 3];
        }
        // the last two or three (j = full - 2 or full - 3)
        const bool three = j + 3 == full;
        const float dC = three ? eta * (p0[j + 2] - y[j + 2]) + p1[j + 2] : 0.0f;
        y[j] = dA;
        float v = x - a * dA;
        pw[j] = v;
        x = a * v + dA;
        y[j + 1] = dB;
        v = x - a * dB;
        pw[j + 1] = v;
        x = a * v + dB;
        if (three) {
            y[j + 2] = dC;
            v = x - a * dC;
            pw[j + 2] = v;
            x = a * v + dC;
        }
    }
    if (frac > 0.0f && full < maxStages) {
        const float d = eta * (p0[full] - y[full]) + p1[full];
        y[full] = d;
        const float v = x - a * d;
        pw[full] = v;
        x += frac * (a * v + d - x);
    }
    return x;
}

} // namespace rv::dsp
