#pragma once
// The Tank: 1–3 Springs plus Tank-level stages (CONTEXT.md).
//
// M1 signal flow (one Spring, CLEAN):
//
//   in L,R ─ mono sum ─ Spring ─┬──────────────── L ─┐
//                               └─ decorrelator ─ R ─┴─ high-shelf cut ─ limiter ─ wet
//   out = dry · sqrt(1 - MIX) + wet · sqrt(MIX)   (equal power, dry stays stereo)
//
// Used at M1: DECAY, BOING, TONE, MIX. Stored but ignored until their
// milestone: SPRINGS (M4; always one Spring), ATTITUDE (M5/M7; always CLEAN),
// SPLASH, WOBBLE (M7), DRIVE (M5). kick() is a no-op until M7.
//
// Real-time rules: process() never allocates, locks or does I/O. All memory
// is taken once in prepare(). Output is identical for any block size:
// parameters are smoothed and applied on a fixed 32-sample control grid
// that runs across block boundaries.
//
// Memory: the object is 1,880 bytes (fits in DTCM as a global); delay memory
// is one pool of Tank::requiredPoolFloats(fs) floats for 3 Springs +
// decorrelator. Total (memoryBytes(), printed by test_spring): 99,548 bytes
// at 48 kHz, 197,000 bytes at 96 kHz. It is malloc'd in prepare(); on
// the Daisy the heap lives in AXI SRAM (512 KB). prepare(fs, block, pool, n)
// lets the Firmware pass its own buffer (e.g. SDRAM via DSY_SDRAM_BSS) instead.

#include "dsp/Filters.h"
#include "dsp/Spring.h"
#include "params/ParamSpec.h"

#include <array>
#include <cstddef>

namespace rv {

class Tank {
public:
    static constexpr int   kMaxSprings      = 3;
    static constexpr int   kControlInterval = 32;      // samples between coefficient updates
    static constexpr float kWetGain         = 1.5f;     // +3.5 dB: wet ≈ dry level on noise at noon DECAY
    static constexpr float kShelfHz         = 5000.0f; // output high-shelf corner (SPEC §4.8)
    static constexpr float kShelfGain       = 0.7f;    // -3 dB above the corner
    static constexpr float kLimitThreshold  = 0.89f;   // ≈ -1 dBFS: wet peaks never reach 1.0
    static constexpr float kLimitReleaseS   = 0.15f;

    Tank() = default;
    ~Tank();
    Tank(const Tank&)            = delete;
    Tank& operator=(const Tank&) = delete;

    // Allocation happens here only (owned pool, grown if needed).
    void prepare(float sampleRate, int maxBlockSize);
    // Same, using a caller-supplied pool of >= requiredPoolFloats(sampleRate) floats.
    void prepare(float sampleRate, int maxBlockSize, float* pool, size_t poolFloats);

    static size_t requiredPoolFloats(float sampleRate);

    void setParam(ParamId id, float normalised)
    {
        if (normalised < 0.0f) normalised = 0.0f;
        if (normalised > 1.0f) normalised = 1.0f;
        values_[static_cast<size_t>(id)] = normalised;
    }

    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Kick event at sample offset within the next process() block. No-op until M7.
    void kick(int /*sampleOffset*/) {}

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    // Clear all state (tails, filters, noise seed) without re-preparing.
    // Smoothed parameters jump to their current values on the next process().
    void reset();

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }
    const Spring& spring(int i) const { return springs_[static_cast<size_t>(i)]; }
    size_t memoryBytes() const { return sizeof(Tank) + poolFloats_ * sizeof(float); }

private:
    // Schroeder allpass (c + z^-D)/(1 + c z^-D): smears phase, keeps level.
    struct Diffuser {
        float* buf = nullptr;
        int    size = 0, w = 0;
        float  c = 0.5f;
        float process(float x)
        {
            const float d = buf[w];
            const float v = x - c * d;
            buf[w] = v;
            if (++w == size) w = 0;
            return c * v + d;
        }
    };

    void bindPool(float* pool);
    void controlTick(bool snap);
    void releaseOwnedPool();

    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> smoothed_{};
    std::array<float, static_cast<size_t>(ParamId::Count)> tickCoeff_{};

    std::array<Spring, kMaxSprings> springs_{};
    int activeSprings_ = 1;

    float* pool_       = nullptr;
    float* ownedPool_  = nullptr;
    size_t poolFloats_ = 0, ownedFloats_ = 0;
    bool   ok_         = false;
    bool   primed_     = false;
    int    tick_       = 0;

    // Tank-level stages.
    std::array<Diffuser, 2>             decorrelator_{};
    std::array<dsp::OnePoleLowpass, 2>  shelfSplit_{};
    dsp::Smoother                       mix_;
    float limitEnv_ = 0.0f, limitRelease_ = 0.0f;
};

} // namespace rv
