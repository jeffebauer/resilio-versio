#pragma once
// The Tank: 1–3 Springs plus Tank-level stages (CONTEXT.md).
//
// M4 signal flow:
//
//   in L,R ─ mono sum (+ Kick) ─┬─ Spring A ─┐
//                               ├─ Spring B ─┼─ SPRINGS mid/side mix ─ mid ─┬──────────────┐
//                               └─ Spring C ─┘  (per mode, 20 ms fade)      └ decorrelator ─ D
//                                               side ─────────────────────────────────────┤
//     L = mid + side + w·D,  R = mid - side - w·D ─ high-shelf cut ─ limiter ─ wet
//   out = dry · sqrt(1 - MIX) + wet · sqrt(MIX)   (equal power, dry stays stereo)
//
// Every Spring hears the same mono input, including the Kick, like the
// springs in one physical tank all hang off the same driver. Each Spring is
// detuned (own L, fC, a) and placed in the stereo field by the SPRINGS mode;
// all the numbers live in core/params/SpringModes.h.
//
// SPRINGS switching (ADR 0003): all three Springs run all the time. A Spring
// that is not heard in the current mode ("idle") still gets the input and
// keeps a live tail, at the minimum stage count (24, the BOING floor), and
// simply has gain 0 in the output mix. A SPRINGS change is then only a
// change of output mix, faded over kSpringsFadeSeconds (20 ms) from
// wherever the gains are now, so it is click-free even when flipped mid-fade.
// Why run them rather than start them on demand: a Spring started at the
// switch would be empty, so switching 1 -> 2 on a ringing tail would leave
// the right channel almost silent until new input arrives, and no fade can
// hide an empty tank. Why it is affordable: idle Springs run at the floor
// stage count, and the stage caps are chosen so every mode, idle Springs
// included, costs no more than 3-Spring mode (SpringModes.h). 3 Springs is
// the SPEC §5 worst case anyway, so running idle Springs raises the average
// load in 1/2-Spring mode but never the peak the budget is written for.
// After a change, stage counts glide to the new mode (one stage per 8 ms, as
// a BOING move), so an idle Spring's Chirp grows to full length over a few
// hundred ms after it becomes audible.
//
// Used at M4: DECAY, BOING, TONE, MIX, SPRINGS. Stored but ignored until
// their milestone: ATTITUDE (M5/M7; always CLEAN), SPLASH, WOBBLE (M7),
// DRIVE (M5). Until M7, kick() injects a placeholder impulse into the Tank
// input at the exact sample, so Kick timing is testable (M2); the real
// thud + crash (ADR 0016) replaces it at M7.
//
// Real-time rules: process() never allocates, locks or does I/O. All memory
// is taken once in prepare(). Output is identical for any block size:
// parameters are smoothed and applied on a fixed 32-sample control grid
// that runs across block boundaries, and the SPRINGS fade advances per sample.
//
// Memory: the object is small (fits in DTCM as a global; exact size printed
// by test_tank); delay memory is one pool of Tank::requiredPoolFloats(fs)
// floats for 3 Springs + decorrelator, sized for the most-detuned Spring.
// Total (memoryBytes(), printed by test_spring and test_tank): about 104 kB
// at 48 kHz and 206 kB at 96 kHz. It is malloc'd in prepare(); on the Daisy
// the heap lives in AXI SRAM (512 KB). prepare(fs, block, pool, n) lets the
// Firmware pass its own buffer (e.g. SDRAM via DSY_SDRAM_BSS) instead.

#include "dsp/Filters.h"
#include "dsp/Spring.h"
#include "params/ParamSpec.h"
#include "params/SpringModes.h"

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
    static constexpr int   kMaxPendingKicks = 16;
    static constexpr float kKickPlaceholder = 0.5f;    // impulse height (M2 placeholder)
    static constexpr float kSpringsFadeSeconds = 0.020f; // SPRINGS crossfade (ADR 0003)

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

    // Kick at a sample offset within the next process() block (clamped to it).
    // Up to kMaxPendingKicks per block; extras are dropped.
    void kick(int sampleOffset);

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);

    // Clear all state (tails, filters, noise seed) without re-preparing.
    // Smoothed parameters jump to their current values on the next process().
    void reset();

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }
    const Spring& spring(int i) const { return springs_[static_cast<size_t>(i)]; }
    // SPRINGS mode now in effect: 0, 1, 2 = 1, 2, 3 Springs.
    int springsMode() const { return mode_; }
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

    // SPRINGS mode and its output-matrix fade (see "SPRINGS switching").
    int              mode_     = 1;
    modes::StereoMix mixFrom_{}, mixTo_{}, mixCur_{};
    float            trimFrom_ = 1.0f, trimTo_ = 1.0f, trimCur_ = 1.0f;
    float            mixScale_ = 1.0f; // trim / sqrt(mixPower(mixCur_))
    float            fadePos_  = 1.0f; // 0 -> 1 over kSpringsFadeSeconds; 1 = settled
    float            fadeStep_ = 0.0f;

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

    std::array<int, kMaxPendingKicks> pendingKicks_{};
    int numPendingKicks_ = 0;
};

} // namespace rv
