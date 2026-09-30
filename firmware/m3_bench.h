#pragma once
// M3 sanity checks (profile build only): the clock, the cache state and one
// multiply-add's cost, printed as a BENCH line with the corners. See
// m3_bench.cpp. (Runs 5-8 also timed the Chirp section in five loop shapes;
// `pipe` won and is in Spring.cpp. Dropped in run 12 to free flash.)

#include <cstdint>

namespace m3bench {

struct Results {
    uint32_t clockHz;
    bool     icache, dcache;
    // cycles x10 (tenths)
    int fmaLatency;    // one dependent multiply-add
    int fmaThroughput; // four independent chains, per multiply-add
};

// Runs everything once (well under a ms); needs the DWT cycle counter running.
Results Run();

} // namespace m3bench
