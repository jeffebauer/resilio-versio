#pragma once
// Seed scrambling for the M7 components (Splash, KickVoice, Wobble).
// dsp::Rng is a plain LCG: seeds that differ only in their low bits give
// nearly the same first outputs (1, 2, 3 ... all start near 1013904223), so
// "independent" instances would jitter and start in step. A murmur3
// finaliser spreads every seed bit over the whole state first.

#include <cstdint>

namespace rv::dsp {

inline uint32_t mixSeed(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h ? h : 0x6D2B79F5u;
}

} // namespace rv::dsp
