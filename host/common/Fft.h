#pragma once
// Radix-2 iterative in-place FFT for metrics + spectrogram analysis
// (docs/m1-contracts.md Stream B). No dependencies. Size must be a
// power of two; callers zero-pad real input to the next power of two.

#include <cstddef>
#include <vector>

namespace rv::fft {

constexpr bool isPow2(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

// In-place complex FFT (Cooley-Tukey, decimation in time). `inverse`
// computes the unnormalised inverse transform (caller divides by N).
// re.size() == im.size() must be a power of two.
void transform(std::vector<float>& re, std::vector<float>& im, bool inverse);

// Convenience: magnitude spectrum of a real, Hann-windowed frame.
// `frame` is padded/truncated to `fftSize` (must be a power of two).
// Returns fftSize/2 + 1 magnitudes (bins 0..Nyquist).
std::vector<float> magnitudeSpectrum(const float* frame, size_t frameLen, size_t fftSize);

} // namespace rv::fft
