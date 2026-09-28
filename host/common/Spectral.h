#pragma once
// Small shared helpers for spectral analysis (Metrics + Spectrogram).
// docs/m1-contracts.md Stream B.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace rv::spectral {

constexpr float kMinLinear = 1e-9f; // floor before log, ~ -180 dB

inline float toDb(float linear) { return 20.0f * std::log10(std::max(linear, kMinLinear)); }
inline float fromDb(float db) { return std::pow(10.0f, db / 20.0f); }

// 1/3-octave-smoothed median magnitude (dB) per bin: for bin i, take the
// median of magDb over bins whose frequency falls within one 1/3-octave
// band centred on freqHz[i] (SPEC §4.10 / §6.2 resonance metric).
//
// freqHz is assumed non-decreasing (true for any linear-frequency FFT bin
// axis), so each bin's band [lo,hi) is found with a two-pointer sweep in
// O(n) rather than the O(n^2) all-pairs scan an inner loop would need --
// this runs once per 0.5 s frame in the steady_tone check, so it matters
// for a full-length render (SPEC §6.2 performance budget).
//
// minHalfBins (default 0 = pure 1/3 octave, the SPEC resonance metric):
// widen each band to at least +-minHalfBins bins. At low frequencies a
// 1/3-octave band holds only a handful of FFT bins, so a single strong
// peak would be half of its own "median"; the M6 Ringing metric uses a
// floor of a few bins so the reference really is the neighbourhood.
inline std::vector<float> thirdOctaveSmoothedMedian(const std::vector<float>& magDb, const std::vector<float>& freqHz,
                                                    size_t minHalfBins = 0)
{
    const size_t n = magDb.size();
    std::vector<float> smoothed(n, 0.0f);
    if (n == 0) return smoothed;
    const double factor = std::pow(2.0, 1.0 / 6.0); // half of a 1/3-octave band each side

    std::vector<size_t> loIdx(n), hiIdx(n); // band is [loIdx[i], hiIdx[i]] inclusive
    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < n; ++i) {
        const double bandLo = double(freqHz[i]) / factor, bandHi = double(freqHz[i]) * factor;
        while (lo < n && double(freqHz[lo]) < bandLo) ++lo;
        if (hi < lo) hi = lo;
        while (hi < n && double(freqHz[hi]) <= bandHi) ++hi;
        loIdx[i] = lo;
        hiIdx[i] = hi > lo ? hi - 1 : lo;
        if (minHalfBins) {
            loIdx[i] = std::min(loIdx[i], i >= minHalfBins ? i - minHalfBins : 0);
            hiIdx[i] = std::max(hiIdx[i], std::min(n - 1, i + minHalfBins));
        }
    }

    std::vector<float> scratch;
    for (size_t i = 0; i < n; ++i) {
        const size_t bLo = loIdx[i], bHi = hiIdx[i];
        const size_t count = bHi - bLo + 1;
        scratch.assign(magDb.begin() + long(bLo), magDb.begin() + long(bHi) + 1);
        const size_t mid = count / 2;
        std::nth_element(scratch.begin(), scratch.begin() + long(mid), scratch.end());
        if (count % 2) {
            smoothed[i] = scratch[mid];
        } else {
            const float upper = scratch[mid];
            std::nth_element(scratch.begin(), scratch.begin() + long(mid) - 1, scratch.begin() + long(mid));
            smoothed[i] = 0.5f * (scratch[mid - 1] + upper);
        }
    }
    return smoothed;
}

// Cheaper cousin of thirdOctaveSmoothedMedian: the mean instead of the
// median, via prefix sums, so the whole spectrum is O(n) instead of
// O(n * bandwidth). Used where the smoothed curve is only a persistence
// reference re-computed every analysis frame (steady_tone), not the
// SPEC-defined resonance metric itself, which keeps the median.
inline std::vector<float> thirdOctaveSmoothedMean(const std::vector<float>& magDb, const std::vector<float>& freqHz)
{
    const size_t n = magDb.size();
    std::vector<float> smoothed(n, 0.0f);
    if (n == 0) return smoothed;
    const double factor = std::pow(2.0, 1.0 / 6.0);

    std::vector<double> prefix(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) prefix[i + 1] = prefix[i] + double(magDb[i]);

    size_t lo = 0, hi = 0;
    for (size_t i = 0; i < n; ++i) {
        const double bandLo = double(freqHz[i]) / factor, bandHi = double(freqHz[i]) * factor;
        while (lo < n && double(freqHz[lo]) < bandLo) ++lo;
        if (hi < lo) hi = lo;
        while (hi < n && double(freqHz[hi]) <= bandHi) ++hi;
        const size_t bHi = hi > lo ? hi : lo + 1;
        smoothed[i] = float((prefix[bHi] - prefix[lo]) / double(bHi - lo));
    }
    return smoothed;
}

} // namespace rv::spectral
