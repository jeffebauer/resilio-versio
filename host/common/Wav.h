#pragma once
// Minimal WAV I/O for desktop Hosts (Renderer, tests). Not part of Core.
// Supports PCM 16/24-bit and IEEE float 32-bit, any channel count.
// 16/24-bit round-trips bit-identically through float.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace rv::wav {

struct Audio {
    int sampleRate    = 48000;
    int bitsPerSample = 24;
    bool isFloat      = false;
    std::vector<std::vector<float>> channels; // [channel][frame]

    size_t frames() const { return channels.empty() ? 0 : channels[0].size(); }
};

namespace detail {
inline uint32_t u32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
inline void put32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }
inline void put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
} // namespace detail

inline bool read(const std::string& path, Audio& out, std::string& error)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return false; }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
        error = "not a RIFF/WAVE file"; return false;
    }
    int format = 0, channels = 0;
    const uint8_t* data = nullptr;
    uint32_t dataSize = 0;
    for (size_t pos = 12; pos + 8 <= d.size();) {
        uint32_t size = detail::u32(&d[pos + 4]);
        const uint8_t* body = &d[pos + 8];
        if (pos + 8 + size > d.size()) size = uint32_t(d.size() - pos - 8);
        if (!std::memcmp(&d[pos], "fmt ", 4) && size >= 16) {
            format            = detail::u16(body);
            channels          = detail::u16(body + 2);
            out.sampleRate    = int(detail::u32(body + 4));
            out.bitsPerSample = detail::u16(body + 14);
            if (format == 0xFFFE && size >= 26) format = detail::u16(body + 24); // WAVE_FORMAT_EXTENSIBLE
        } else if (!std::memcmp(&d[pos], "data", 4)) {
            data = body; dataSize = size;
        }
        pos += 8 + size + (size & 1);
    }
    if (!data || channels <= 0) { error = "missing fmt or data chunk"; return false; }
    out.isFloat = (format == 3);
    const int bps = out.bitsPerSample;
    if (!((format == 1 && (bps == 16 || bps == 24)) || (format == 3 && bps == 32))) {
        error = "unsupported format (need PCM16, PCM24 or float32)"; return false;
    }
    const int bytes     = bps / 8;
    const size_t frames = dataSize / (bytes * channels);
    out.channels.assign(channels, std::vector<float>(frames));
    for (size_t i = 0; i < frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            const uint8_t* p = data + (i * channels + c) * bytes;
            float v;
            if (format == 3) { std::memcpy(&v, p, 4); }
            else if (bps == 16) { v = float(int16_t(detail::u16(p))) / 32768.0f; }
            else { int32_t s = int32_t((uint32_t(p[0]) << 8) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 24)) >> 8; v = float(s) / 8388608.0f; }
            out.channels[c][i] = v;
        }
    }
    return true;
}

inline bool write(const std::string& path, const Audio& in, std::string& error)
{
    const int channels = int(in.channels.size());
    const int bps      = in.isFloat ? 32 : in.bitsPerSample;
    const int bytes    = bps / 8;
    const size_t frames = in.frames();
    std::vector<uint8_t> b;
    const uint32_t dataSize = uint32_t(frames * channels * bytes);
    b.insert(b.end(), {'R', 'I', 'F', 'F'}); detail::put32(b, 36 + dataSize);
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '}); detail::put32(b, 16);
    detail::put16(b, in.isFloat ? 3 : 1); detail::put16(b, uint16_t(channels));
    detail::put32(b, uint32_t(in.sampleRate)); detail::put32(b, uint32_t(in.sampleRate * channels * bytes));
    detail::put16(b, uint16_t(channels * bytes)); detail::put16(b, uint16_t(bps));
    b.insert(b.end(), {'d', 'a', 't', 'a'}); detail::put32(b, dataSize);
    for (size_t i = 0; i < frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            float v = in.channels[c][i];
            if (in.isFloat) { uint8_t p[4]; std::memcpy(p, &v, 4); b.insert(b.end(), p, p + 4); continue; }
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            if (bps == 16) {
                long s = std::lround(v * 32768.0f); if (s > 32767) s = 32767;
                detail::put16(b, uint16_t(int16_t(s)));
            } else {
                long s = std::lround(v * 8388608.0f); if (s > 8388607) s = 8388607;
                uint32_t u = uint32_t(int32_t(s));
                b.push_back(uint8_t(u)); b.push_back(uint8_t(u >> 8)); b.push_back(uint8_t(u >> 16));
            }
        }
    }
    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "cannot write " + path; return false; }
    f.write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
    return bool(f);
}

} // namespace rv::wav
