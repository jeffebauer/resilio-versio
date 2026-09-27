#include "Spectrogram.h"
#include "Fft.h"
#include "Spectral.h"

#include <algorithm>
#include <cmath>

namespace rv::spectrogram {

namespace {
constexpr size_t kFftSize = 2048;
constexpr size_t kHop     = 256;
}

Spectrogram compute(const std::vector<float>& mono, float sampleRate, int width, int height)
{
    Spectrogram out;
    out.width = width;
    out.height = height;
    out.t0S = 0.0;
    out.t1S = double(mono.size()) / double(sampleRate);
    out.fMinHz = 40.0;
    out.fMaxHz = std::min(16000.0, double(sampleRate) * 0.5 * 0.999);
    out.dbMin = -100.0;
    out.dbMax = 0.0;
    out.data.assign(size_t(width) * size_t(height), 0);
    if (mono.empty() || width <= 0 || height <= 0) return out;

    struct Frame { double tStart; std::vector<float> mag; };
    std::vector<Frame> frames;
    frames.reserve(mono.size() / kHop + 1);
    for (size_t pos = 0; pos < mono.size(); pos += kHop) {
        const size_t n = std::min(kFftSize, mono.size() - pos);
        Frame fr;
        fr.tStart = double(pos) / double(sampleRate);
        fr.mag = fft::magnitudeSpectrum(mono.data() + pos, n, kFftSize);
        frames.push_back(std::move(fr));
    }
    if (frames.empty()) return out;

    // Row band edges, log-spaced, row 0 = top = f_max.
    std::vector<double> edges(size_t(height) + 1);
    for (int i = 0; i <= height; ++i) {
        const double frac = double(i) / double(height);
        edges[size_t(i)] = out.fMaxHz * std::pow(out.fMinHz / out.fMaxHz, frac);
    }

    const double colWidthS = (out.t1S - out.t0S) / double(width);
    size_t searchFrom = 0;
    for (int c = 0; c < width; ++c) {
        const double tLo = out.t0S + double(c) * colWidthS;
        const double tHi = out.t0S + double(c + 1) * colWidthS;
        size_t startIdx = searchFrom;
        while (startIdx < frames.size() && frames[startIdx].tStart < tLo) ++startIdx;
        size_t endIdx = startIdx;
        while (endIdx < frames.size() && frames[endIdx].tStart < tHi) ++endIdx;
        if (startIdx == endIdx) {
            const size_t nearest = std::min(startIdx, frames.size() - 1);
            startIdx = nearest;
            endIdx = nearest + 1;
        }
        searchFrom = startIdx;

        for (int r = 0; r < height; ++r) {
            const double loF = edges[size_t(r) + 1], hiF = edges[size_t(r)];
            float maxMag = 0.0f;
            for (size_t fi = startIdx; fi < endIdx; ++fi) {
                const auto& mag = frames[fi].mag;
                const size_t maxBin = mag.empty() ? 0 : mag.size() - 1;
                size_t kLo = size_t(std::max(0.0, std::floor(loF * double(kFftSize) / double(sampleRate))));
                size_t kHiBin = size_t(std::max(0.0, std::ceil(hiF * double(kFftSize) / double(sampleRate))));
                kLo = std::min(kLo, maxBin);
                kHiBin = std::min(kHiBin, maxBin);
                if (kHiBin < kLo) kHiBin = kLo;
                for (size_t k = kLo; k <= kHiBin; ++k) maxMag = std::max(maxMag, mag[k]);
            }
            float db = spectral::toDb(maxMag);
            db = std::clamp(db, float(out.dbMin), float(out.dbMax));
            const int val = int(std::lround((db - out.dbMin) / (out.dbMax - out.dbMin) * 255.0));
            out.data[size_t(r) * size_t(width) + size_t(c)] = uint8_t(std::clamp(val, 0, 255));
        }
    }
    return out;
}

} // namespace rv::spectrogram
