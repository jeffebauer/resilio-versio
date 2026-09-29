#pragma once
// M3 micro-benchmarks (profile build only): how fast this chip really runs
// the Spring's Chirp section in a few loop shapes, so one flash can choose
// between them. See m3_bench.cpp.

#include <cstdint>

namespace m3bench {

struct Results {
    uint32_t clockHz;
    bool     icache, dcache;
    // cycles x10 (tenths)
    int fmaLatency;    // one dependent multiply-add
    int fmaThroughput; // four independent chains, per multiply-add
    int fused;         // Chirp section as in Spring.cpp today, per section-sample
    int split;         // same maths: all D{v} first, then the x chain
    int split3;        // split, with the three Springs' x chains interleaved
    int pipe;          // fused, next section's D{v} computed during this one's x chain
    int fused3;        // fused, the three Springs' sections side by side
};

// Runs everything once (a few ms); needs the DWT cycle counter running.
Results Run();

} // namespace m3bench
