#pragma once
// Minimal base64 codec for embedding the spectrogram byte-map in sidecar
// JSON (docs/m1-contracts.md Stream B). No dependencies.

#include <cstdint>
#include <string>
#include <vector>

namespace rv::base64 {

std::string encode(const uint8_t* data, size_t size);
inline std::string encode(const std::vector<uint8_t>& data) { return encode(data.data(), data.size()); }

// Returns false on malformed input.
bool decode(const std::string& text, std::vector<uint8_t>& out);

} // namespace rv::base64
