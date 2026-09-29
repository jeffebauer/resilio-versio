// M3 micro-benchmarks, profile build only (SPEC §7 M3). The Chirp chain of
// three Springs, 48 sections each, 8-float rings in DTCM like the real pool,
// timed with the DWT cycle counter in three loop shapes that do the same
// arithmetic (Spring.cpp processLow):
//   fused  : per section, D{v} then v and y, as today;
//   split  : per sample, every section's D{v} first (it only needs last
//            sample's state, not this sample's x), then the x chain;
//   split3 : split, the three Springs' x chains interleaved.
// Plus a multiply-add's latency and throughput, the clock and cache state.

#include "m3_bench.h"

#include "stm32h7xx.h"

namespace m3bench {
namespace {

constexpr int kSprings = 3, kStages = 48, kRing = 8, kMask = kRing - 1, kN = 5, kSamples = 256;

__attribute__((section(".dtcmram_bss"))) float gRings[kSprings][kStages][kRing];
__attribute__((section(".dtcmram_bss"))) float gY1[kSprings][kStages];
float          gD[kSprings][kStages];
volatile float gSink;

const float kA[kSprings]   = {-0.60f, -0.62f, -0.58f};
const float kEta[kSprings] = {0.30f, 0.28f, 0.33f};

void Clear()
{
    for (auto& s : gRings)
        for (auto& st : s)
            for (auto& v : st) v = 0.0f;
    for (auto& s : gY1)
        for (auto& v : s) v = 0.0f;
}

inline float Input(int t) { return (t & 63) == 0 ? 1.0f : 0.0f; }

__attribute__((noinline)) float RunFused()
{
    float acc = 0.0f;
    for (int t = 0; t < kSamples; ++t) {
        const int iw = t & kMask, ir0 = (iw - kN) & kMask, ir1 = (iw - kN - 1) & kMask;
        for (int s = 0; s < kSprings; ++s) {
            const float a = kA[s], eta = kEta[s];
            float       x = Input(t);
            for (int j = 0; j < kStages; ++j) {
                float*      ring = gRings[s][j];
                const float dOut = eta * (ring[ir0] - gY1[s][j]) + ring[ir1];
                gY1[s][j]        = dOut;
                const float v    = x - a * dOut;
                ring[iw]         = v;
                x                = a * v + dOut;
            }
            acc += x;
        }
    }
    return acc;
}

__attribute__((noinline)) float RunSplit()
{
    float acc = 0.0f;
    for (int t = 0; t < kSamples; ++t) {
        const int iw = t & kMask, ir0 = (iw - kN) & kMask, ir1 = (iw - kN - 1) & kMask;
        for (int s = 0; s < kSprings; ++s) {
            const float a = kA[s], eta = kEta[s];
            for (int j = 0; j < kStages; ++j) {
                const float* ring = gRings[s][j];
                const float  d    = eta * (ring[ir0] - gY1[s][j]) + ring[ir1];
                gY1[s][j]         = d;
                gD[s][j]          = d;
            }
            float x = Input(t);
            for (int j = 0; j < kStages; ++j) {
                const float d = gD[s][j];
                const float v = x - a * d;
                gRings[s][j][iw] = v;
                x                = a * v + d;
            }
            acc += x;
        }
    }
    return acc;
}

__attribute__((noinline)) float RunSplit3()
{
    float acc = 0.0f;
    for (int t = 0; t < kSamples; ++t) {
        const int iw = t & kMask, ir0 = (iw - kN) & kMask, ir1 = (iw - kN - 1) & kMask;
        for (int s = 0; s < kSprings; ++s) {
            const float eta = kEta[s];
            for (int j = 0; j < kStages; ++j) {
                const float* ring = gRings[s][j];
                const float  d    = eta * (ring[ir0] - gY1[s][j]) + ring[ir1];
                gY1[s][j]         = d;
                gD[s][j]          = d;
            }
        }
        const float a0 = kA[0], a1 = kA[1], a2 = kA[2];
        float       x0 = Input(t), x1 = x0, x2 = x0;
        for (int j = 0; j < kStages; ++j) {
            const float d0 = gD[0][j], d1 = gD[1][j], d2 = gD[2][j];
            const float v0 = x0 - a0 * d0, v1 = x1 - a1 * d1, v2 = x2 - a2 * d2;
            gRings[0][j][iw] = v0;
            gRings[1][j][iw] = v1;
            gRings[2][j][iw] = v2;
            x0 = a0 * v0 + d0;
            x1 = a1 * v1 + d1;
            x2 = a2 * v2 + d2;
        }
        acc += x0 + x1 + x2;
    }
    return acc;
}

// fused, with the next section's D{v} (independent of x) computed while this
// section's x chain waits: the in-order M7 then has other work to issue.
__attribute__((noinline)) float RunPipe()
{
    float acc = 0.0f;
    for (int t = 0; t < kSamples; ++t) {
        const int iw = t & kMask, ir0 = (iw - kN) & kMask, ir1 = (iw - kN - 1) & kMask;
        for (int s = 0; s < kSprings; ++s) {
            const float a = kA[s], eta = kEta[s];
            float       x = Input(t);
            float       d = eta * (gRings[s][0][ir0] - gY1[s][0]) + gRings[s][0][ir1];
            for (int j = 0; j < kStages - 1; ++j) {
                const float* next = gRings[s][j + 1];
                const float  dn   = eta * (next[ir0] - gY1[s][j + 1]) + next[ir1];
                gY1[s][j]         = d;
                const float v     = x - a * d;
                gRings[s][j][iw]  = v;
                x                 = a * v + d;
                d                 = dn;
            }
            gY1[s][kStages - 1]          = d;
            const float v                = x - a * d;
            gRings[s][kStages - 1][iw]   = v;
            x                            = a * v + d;
            acc += x;
        }
    }
    return acc;
}

// fused, the three Springs' sections side by side: three independent chains.
__attribute__((noinline)) float RunFused3()
{
    float acc = 0.0f;
    for (int t = 0; t < kSamples; ++t) {
        const int   iw = t & kMask, ir0 = (iw - kN) & kMask, ir1 = (iw - kN - 1) & kMask;
        const float a0 = kA[0], a1 = kA[1], a2 = kA[2], e0 = kEta[0], e1 = kEta[1], e2 = kEta[2];
        float       x0 = Input(t), x1 = x0, x2 = x0;
        for (int j = 0; j < kStages; ++j) {
            float *r0 = gRings[0][j], *r1 = gRings[1][j], *r2 = gRings[2][j];
            const float d0 = e0 * (r0[ir0] - gY1[0][j]) + r0[ir1];
            const float d1 = e1 * (r1[ir0] - gY1[1][j]) + r1[ir1];
            const float d2 = e2 * (r2[ir0] - gY1[2][j]) + r2[ir1];
            gY1[0][j] = d0;
            gY1[1][j] = d1;
            gY1[2][j] = d2;
            const float v0 = x0 - a0 * d0, v1 = x1 - a1 * d1, v2 = x2 - a2 * d2;
            r0[iw] = v0;
            r1[iw] = v1;
            r2[iw] = v2;
            x0 = a0 * v0 + d0;
            x1 = a1 * v1 + d1;
            x2 = a2 * v2 + d2;
        }
        acc += x0 + x1 + x2;
    }
    return acc;
}

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

    constexpr uint32_t kSections = uint32_t(kSamples) * kSprings * kStages;
    Clear();
    r.fused = Tenths(Time([] { gSink = RunFused(); }), kSections);
    Clear();
    r.split = Tenths(Time([] { gSink = RunSplit(); }), kSections);
    Clear();
    r.split3 = Tenths(Time([] { gSink = RunSplit3(); }), kSections);
    Clear();
    r.pipe = Tenths(Time([] { gSink = RunPipe(); }), kSections);
    Clear();
    r.fused3 = Tenths(Time([] { gSink = RunFused3(); }), kSections);
    return r;
}

} // namespace m3bench
