// M3 sanity checks, profile build only (SPEC §7 M3): the clock and cache
// state, and a multiply-add's latency and throughput (run 5 found GCC's
// accumulate-form vfma serialising everything; run 6's -ffp-contract=off
// fixed it: this line shows it stays fixed). Timed with the DWT cycle counter.
//
// Runs 5-8 also timed the Spring's Chirp section in five loop shapes (fused,
// split, split3, pipe, fused3); `pipe` won (Spring::processLow). Those
// shapes were dropped in run 12 to make flash room (git history has them).

#include "m3_bench.h"

#include "stm32h7xx.h"

namespace m3bench {
namespace {

volatile float gSink;

__attribute__((noinline)) float ChainFma(float x, float a, float b, int n)
{
    for (int i = 0; i < n; ++i) x = x * a + b;
    return x;
}

__attribute__((noinline)) float IndepFma(float a, float b, int n)
{
    float x0 = 0.1f, x1 = 0.2f, x2 = 0.3f, x3 = 0.4f;
    for (int i = 0; i < n; ++i) {
        x0 = x0 * a + b;
        x1 = x1 * a + b;
        x2 = x2 * a + b;
        x3 = x3 * a + b;
    }
    return x0 + x1 + x2 + x3;
}

template <class F>
uint32_t Time(F&& f)
{
    f(); // warm the caches
    const uint32_t t0 = DWT->CYCCNT;
    f();
    return DWT->CYCCNT - t0;
}

int Tenths(uint32_t cycles, uint32_t count) { return int((uint64_t(cycles) * 10u + count / 2u) / count); }

} // namespace

Results Run()
{
    Results r{};
    r.clockHz = SystemCoreClock;
    r.icache  = (SCB->CCR & SCB_CCR_IC_Msk) != 0;
    r.dcache  = (SCB->CCR & SCB_CCR_DC_Msk) != 0;

    volatile float a = 0.999f, b = 1.0e-3f;
    constexpr int  kOps = 4096;
    r.fmaLatency    = Tenths(Time([&] { gSink = ChainFma(0.5f, a, b, kOps); }), kOps);
    r.fmaThroughput = Tenths(Time([&] { gSink = IndepFma(a, b, kOps); }), 4 * kOps);
    return r;
}

} // namespace m3bench
