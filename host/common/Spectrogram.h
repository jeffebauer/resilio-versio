#pragma once
// Sidecar spectrogram (docs/m1-contracts.md Stream B contract with
// Stream C's review page): log-frequency rows, uint8 magnitude map,
// row 0 = top = f_max.

#include <cstdint>
#include <vector>

namespace rv::spectrogram {

struct Spectrogram {
    int width  = 600;
    int height = 160;
    double t0S = 0.0, t1S = 0.0;
    double fMinHz = 40.0, fMaxHz = 16000.0;
    double dbMin = -100.0, dbMax = 0.0;
    std::vector<uint8_t> data; // width*height, row-major, row 0 = top = f_max
};

// STFT: 2048-point FFT, 256-sample hop (~8x overlap, ~5.3 ms at 48 kHz) --
// fine enough to resolve the M1 click render's individual descending
// chirps. Each output cell takes the MAX magnitude of every STFT
// frame/bin landing in its time/log-frequency box (not the average), so
// brief chirps survive the width/height downsampling instead of being
// smeared into the noise floor.
Spectrogram compute(const std::vector<float>& mono, float sampleRate, int width = 600, int height = 160);

} // namespace rv::spectrogram
