#pragma once
// One simulated Spring (CONTEXT.md): low-chirp path + high path, after
// Välimäki, Parker & Abel, "Parametric Spring Reverberation Effect", JAES 2010
// (SPEC §4.1–4.2).
//
//   LOW-CHIRP PATH (the Loop)
//   in ─ + ─ DC block ─ M × stretched allpass ─ LPF(fC) ─ damping LPF ─ delay L ─┬─ tap ~L/2 ─► out
//        ▲                                                                     │
//        └──────────────── g ◄── LoopSat (x2 oversampled) ◄──────────────────────┘
//
//   HIGH PATH (faster wideband echoes)
//   in ─ + ─ 6 × allpass ─ HPF ─ ceiling LPF ─ delay L_hf ─┬─► × highPathLevel ─► out
//        ▲                                                │
//        └────────────────── g_hf ◄───────────────────────┘
//
// Plain-language version: a click goes round a loop. Each trip it passes a
// long chain of "stretched" allpass filters. Allpass filters don't change
// loudness, only *when* each frequency comes out: here lows come out later
// than highs, so every echo is smeared into a falling "peeew" (the Chirp).
// The loop repeats it every round trip, losing a little each time (g < 1).
//
// Detuning (M4) is not done here: the Tank gives each Spring its own detuned
// SpringSettings (core/params/SpringModes.h); the delay memory is sized for
// the most-detuned Spring. Not yet here (later milestones): Clatter/Jolt
// (M7), WOBBLE (M7; it rides on the modulation hook below).
//
// Micro-mod floor (M6, AntiRes layer 2, params/AntiRes.h): the Loop's delay
// reads (feedback and pickup tap) use L·(1 + m(t)), where m is a slow
// seeded smoothed random of depth SpringSettings::modDepth plus a slow sine
// of depth lfoDepth (Howl zone only at M6). lCur_ itself (the glided DECAY
// length, and what the Loop gain design sees) is untouched: at 0.05 % the
// modulation is far below anything the design or the T60 would notice. The
// high path is left unmodulated: its read is at 8-9 kHz territory, where the
// linear interpolator's loss depends on the fractional delay (0 to -1.2 dB
// per trip at 8 kHz), so a drifting fraction would make the HF decay rate
// wobble; the Loop (the path that could Ring, and the one the Howl lives in)
// is dark above fC, where that loss is under 0.3 dB. Cost:
// two one-poles, a two-multiply sine oscillator and one multiply per sample;
// the reads were already fractional (linear interpolation).
//
// LoopSat (M5, SPEC §4.9): a saturator on the feedback, before g. Off in
// CLEAN, gentle symmetric in DRIVEN, hard asymmetric in KICKED (amounts and
// hardness come from the Tank's ATTITUDE Morph via SpringSettings). Its
// curve never has a slope above 1, so it can only lower the Loop gain: it
// bounds loud tails and thickens them, and can never cause a runaway. It
// runs oversampled (dsp/Oversampler.h); the oversampler's small group delay
// (2.5 samples at x2) is part of the Loop, so roundTripSamples() counts it and g
// is designed with it in. The oversampler runs in every ATTITUDE, so the
// Loop's delay never changes when the Morph fades the saturation in or out.
//
// Howl (ADR 0002, 0019): SpringSettings::howl (0..1, the Tank sets it only in
// KICKED's top DECAY zone) raises the Loop's small-signal peak gain above 1
// (DriveVoicing.h "Howl zone"). The LoopSat then limits the growth: a
// saturated, self-sustaining roar. howl = 0 leaves g exactly as designed.
//
// Multirate (SPEC §5 mitigation 2, Parker 2011), not done at M1: the chain,
// LPF(fC) and damping only carry content below fC (< 4.2 kHz), so they could
// run at fs/2 or fs/4 between a halfband decimator (after the DC blocker) and
// a halfband interpolator (before the delay write). K halves with the rate
// (K = rate/(2 fC)), so stretchK() and the ring sizes already follow from the
// rate passed to prepare(); the decimated Loop would simply be prepared at
// fs/2. Cost of the chain drops by the same factor.
//
// Memory: all delay memory comes from a caller-supplied pool (requiredFloats()),
// so the Firmware can place it in SRAM/SDRAM. The object itself is small.

#include "dsp/Drive.h"
#include "dsp/Filters.h"
#include "params/Mappings.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rv {

// Internal-unit settings for one Spring, produced from Normalised values by
// core/params/Mappings.h. Applied at control rate.
struct SpringSettings {
    float loopDelaySeconds = 0.05f; // L
    float t60Seconds       = 2.0f;
    float transitionHz     = 3400.0f; // fC -> stretch K
    float allpassCoeff     = -0.6f;   // a
    int   stages           = 44;      // M target
    float dampingHz        = 3800.0f;
    float highPathLevel    = 0.2f;
    // Where along the delay line the pickup sits, as a fraction of L. The
    // first echo arrives after about tapRatio·L (+ the chain's delay).
    // Fixed per Spring (not a knob), so it never needs to glide.
    float tapRatio         = 0.5f;
    // LoopSat (ATTITUDE Morph): amount 0 = linear, hardness per half.
    float loopSatAmount    = 0.0f;
    float loopSatKPos      = 1.0f;
    float loopSatKNeg      = 1.0f;
    // Howl zone position × KICKED weight, 0..1 (0 everywhere else).
    float howl             = 0.0f;
    // L modulation (AntiRes Micro-mod floor, params/AntiRes.h), as fractions
    // of L (peak): modDepth = smoothed random (floor, + WOBBLE at M7),
    // lfoDepth / lfoHz = slow sine (Howl movement now, WOBBLE at M7).
    float modDepth         = 0.0f;
    float lfoDepth         = 0.0f;
    float lfoHz            = 0.35f;
};

class Spring {
public:
    static constexpr int   kMaxStages          = map::kMaxStages;
    static constexpr int   kHighStages         = 6;
    static constexpr float kHighAllpassCoeff   = 0.5f;   // mild diffusion, highs slightly later
    static constexpr float kHighDelayRatio     = 0.43f;  // L_hf = 0.43 L: faster echoes than the Loop
    static constexpr float kHighT60Ratio       = 0.45f;  // HF tail dies faster: dark dub tail
    static constexpr float kHighPassRatio      = 0.8f;   // high path HPF at 0.8 fC
    static constexpr float kHighCeilingHz      = 9000.0f; // TONE ceiling (ADR 0017)
    static constexpr float kDcBlockHz          = 40.0f;
    static constexpr float kMaxGain            = 0.995f; // Loop gain always < 1 (ADR 0001)
    // The design makes the *slowest* band ring for T60. A broadband Schroeder
    // T60 (the Renderer metric) also sees the faster-dying highs early on and
    // reads ~12 % shorter, so design for a slightly longer slowest band.
    // Calibrated at 48 kHz against the -5..-35 dB fit (test_spring).
    static constexpr float kT60DesignScale     = 1.13f;
    static constexpr float kLoopSlewPerSample  = 0.08f;  // max |dL/dn|: pitch bend <= 8 % (ADR 0012)
    static constexpr float kStageRampSeconds   = 0.008f; // time to fade one stage in or out
    static constexpr float kDenormalNoise      = 1.0e-10f; // -200 dB seeded noise keeps state off denormals

    // Pool floats needed at this sample rate (worst-case settings).
    static size_t requiredFloats(float sampleRate);

    void prepare(float sampleRate, float* pool, uint32_t noiseSeed);
    void reset();

    // Control rate. snap = jump straight to the settings (first block, reset).
    void setSettings(const SpringSettings& s, bool snap);

    // n samples of mono in -> mono Spring out. Real-time safe.
    void process(const float* in, float* out, int n);

    // ---- Analysis at the current coefficients (Loop gain design + tests) ----
    float sampleRate() const { return sampleRate_; }
    float loopDelaySamples() const { return lCur_; }
    float stretchK() const { return k_; }
    float allpassCoeff() const { return a_; }
    float activeStages() const { return mPos_; }
    float feedbackGain() const { return g_; }
    float highFeedbackGain() const { return gHigh_; }
    // Group delay of the allpass chain alone, samples.
    float chainGroupDelaySamples(float freqHz) const;
    // Full Loop round trip at freqHz: L + chain + filters + LoopSat
    // oversampler latency, samples.
    float roundTripSamples(float freqHz) const;
    // Loop magnitude per trip at freqHz, excluding g (small signal: the
    // LoopSat's slope is 1 at rest and never above 1; its oversampler is an
    // allpass, magnitude 1).
    float loopMagnitude(float freqHz) const;
    // T60 (s) the Loop gives at freqHz with the current g.
    float t60AtSeconds(float freqHz) const;
    // L modulation factor 1 + m(t) applied to the last sample's delay reads
    // (Micro-mod floor + Howl movement; test_antires).
    float lengthModulation() const { return modNow_; }

private:
    void  advanceGlides();
    float processLow(float in, float lMod);
    float processHigh(float in, float lhMod);
    void  updateCoefficients();
    void  clearStage(int j);
    float readLow(float delay) const;
    float advanceModulation();

    float sampleRate_ = 48000.0f;

    // Pool-backed buffers.
    float* lowBuf_    = nullptr; int lowSize_ = 0;  int lowW_ = 0;
    float* highBuf_   = nullptr; int highSize_ = 0; int highW_ = 0;
    float* rings_     = nullptr; int ringMask_ = 0; int ringW_ = 0;

    SpringSettings settings_{};

    // Low-chirp path state.
    dsp::DcBlocker      dc_;
    dsp::Biquad         chirpLowpass_; // at fC: removes the mirrored chirp band above fC
    dsp::OnePoleLowpass damping_;      // TONE
    std::array<float, kMaxStages> thiranY1_{};
    float lTarget_ = 0.0f, lCur_ = 0.0f;
    float k_ = 6.0f, a_ = -0.6f, eta_ = 0.0f;
    int   n_ = 5; // integer part of the embedded delay
    float mPos_ = 0.0f, mRate_ = 0.0f;
    int   mTarget_ = 0, mActive_ = 0;
    float g_ = 0.0f;
    dsp::LoopSat loopSat_;

    // High path state.
    std::array<float, kHighStages> hapX1_{}, hapY1_{};
    dsp::Biquad         highpass_;
    dsp::OnePoleLowpass highCeiling_;
    float lhCur_ = 0.0f, gHigh_ = 0.0f, highPathLevel_ = 0.0f;
    float tapRatio_ = 0.5f;

    // L modulation state (see "Micro-mod floor").
    dsp::Rng modRng_;
    float    modTarget_ = 0.0f, modY1_ = 0.0f, modY2_ = 0.0f, modC_ = 0.0f;
    int      modHold_ = 1, modCount_ = 0;
    float    lfoS_ = 0.0f, lfoC_ = 1.0f, lfoE_ = 0.0f;
    float    modDepth_ = 0.0f, lfoDepth_ = 0.0f, modNow_ = 1.0f;

    dsp::Rng rng_;
    uint32_t seed_ = 1;
};

} // namespace rv
