#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rv::fft {

void transform(std::vector<float>& re, std::vector<float>& im, bool inverse)
{
    const size_t n = re.size();
    if (n < 2 || !isPow2(n)) return;

    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }

    const double sign = inverse ? 1.0 : -1.0;
    for (size_t len = 2; len <= n; len <<= 1) {
        const double angStep = sign * 2.0 * M_PI / double(len);
        const double wRe = std::cos(angStep), wIm = std::sin(angStep);
        for (size_t start = 0; start < n; start += len) {
            double curRe = 1.0, curIm = 0.0;
            const size_t half = len >> 1;
            for (size_t k = 0; k < half; ++k) {
                const size_t a = start + k, b = start + k + half;
                const double uRe = re[a], uIm = im[a];
                const double vRe = re[b] * curRe - im[b] * curIm;
                const double vIm = re[b] * curIm + im[b] * curRe;
                re[a] = float(uRe + vRe); im[a] = float(uIm + vIm);
                re[b] = float(uRe - vRe); im[b] = float(uIm - vIm);
                const double nextRe = curRe * wRe - curIm * wIm;
                const double nextIm = curRe * wIm + curIm * wRe;
                curRe = nextRe; curIm = nextIm;
            }
        }
    }
}

std::vector<float> magnitudeSpectrum(const float* frame, size_t frameLen, size_t fftSize)
{
    std::vector<float> re(fftSize, 0.0f), im(fftSize, 0.0f);
    const size_t n = std::min(frameLen, fftSize);
    // Hann window (periodic form), reduces spectral leakage for the
    // resonance / steady-tone / spectrogram analyses.
    for (size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(n > 1 ? n : 1));
        re[i] = frame[i] * float(w);
    }
    transform(re, im, false);
    std::vector<float> mag(fftSize / 2 + 1);
    for (size_t k = 0; k < mag.size(); ++k)
        mag[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
    return mag;
}

} // namespace rv::fft
