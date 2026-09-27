#include "Base64.h"

namespace rv::base64 {

namespace {
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int8_t decodeChar(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return int8_t(c - 'A');
    if (c >= 'a' && c <= 'z') return int8_t(c - 'a' + 26);
    if (c >= '0' && c <= '9') return int8_t(c - '0' + 52);
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
} // namespace

std::string encode(const uint8_t* data, size_t size)
{
    std::string out;
    out.reserve(((size + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 3 <= size; i += 3) {
        const uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
        out += kAlphabet[(n >> 18) & 0x3F];
        out += kAlphabet[(n >> 12) & 0x3F];
        out += kAlphabet[(n >> 6) & 0x3F];
        out += kAlphabet[n & 0x3F];
    }
    const size_t rem = size - i;
    if (rem == 1) {
        const uint32_t n = uint32_t(data[i]) << 16;
        out += kAlphabet[(n >> 18) & 0x3F];
        out += kAlphabet[(n >> 12) & 0x3F];
        out += "==";
    } else if (rem == 2) {
        const uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
        out += kAlphabet[(n >> 18) & 0x3F];
        out += kAlphabet[(n >> 12) & 0x3F];
        out += kAlphabet[(n >> 6) & 0x3F];
        out += '=';
    }
    return out;
}

bool decode(const std::string& text, std::vector<uint8_t>& out)
{
    out.clear();
    uint32_t buf = 0;
    int bits = 0;
    for (char ch : text) {
        if (ch == '=' || ch == '\n' || ch == '\r' || ch == ' ') continue;
        const int8_t v = decodeChar(uint8_t(ch));
        if (v < 0) return false;
        buf = (buf << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t((buf >> bits) & 0xFF));
        }
    }
    return true;
}

} // namespace rv::base64
