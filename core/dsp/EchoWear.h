#pragma once
// Wear: the echo's repeats break up (PROTOTYPE, owner 4 Oct 2026; numbers and
// the musical story in params/EchoVoicing.h "Wear"). One process per voicing,
// run on the tape's feedback (TapeEcho::wear), so it compounds pass by pass.
// Deterministic: every random choice comes from a seeded Rng, re-seeded by
// reset(). Small-signal gain <= 1 everywhere (no added energy).
//
// The worn tape's modulated delay adds latency to the feedback; delayInput()
// gives the input the same fixed delay and TapeEcho shortens the tape by it,
// so the repeats keep the echo time (clocked divisions stay on the beat).

#include "dsp/Drive.h"
#include "dsp/Filters.h"
#include "params/EchoVoicing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rv::dsp {

// ---- Tape wear (voicings 5-7; numbers in params/EchoVoicing.h "Tape wear") ----------------
// Tape saturation + roll-off, and the crinkle on top. TapeSat<false> is an
// empty no-op, so a firmware that doesn't ship them carries no code or state.
template <bool kOn>
class TapeSat {
public:
    void prepare(float) {}
    void reset() {}
    template <bool kCrinkleOn>
    void process(float*, int, int, Rng&) {}
};

template <>
class TapeSat<true> {
public:
    // The 1-pole high shelf e = x + K (x - LP(x)) (pre-emphasis) and its exact
    // inverse (de-emphasis), solved per sample from the same 1-pole:
    //   y = (s + K (1 - c) lb) / (1 + K (1 - c)),  lb += c (y - lb)
    // so with the saturator quiet (s = e) the pair gives back x exactly.
    RV_SIZE_OPT void prepare(float fs) // set-up
    {
        using namespace echo;
        fs_ = fs;
        const float c = 1.0f - std::exp(-2.0f * map::kPi * kTapeSatEmphHz / fs_);
        emphC_ = c;
        emphK_ = std::exp(kTapeSatEmphDb * (2.302585093f / 20.0f)) - 1.0f;
        deB_   = emphK_ * (1.0f - c);
        deInv_ = 1.0f / (1.0f + deB_);
        satK_ = kTapeSatDriveK, satInv_ = 1.0f / kTapeSatDriveK;
        roll_.setCutoff(std::min(kTapeSatRollHz, 0.45f * fs_), fs_);
        split_.setCutoff(kCrinkleSplitHz, fs_);
        {   // The head bump: an RBJ peaking filter (its skirts back to 0 dB, unlike a shelf's tail).
            const float A = std::exp(kTapeSatBumpDb * (2.302585093f / 40.0f));
            const float w = 2.0f * map::kPi * kTapeSatBumpHz / fs_, cw = std::cos(w), al = std::sin(w) / (2.0f * kTapeSatBumpQ);
            const float a0 = 1.0f + al / A;
            bump_.b0 = (1.0f + al * A) / a0, bump_.b1 = -2.0f * cw / a0, bump_.b2 = (1.0f - al * A) / a0;
            bump_.a1 = -2.0f * cw / a0, bump_.a2 = (1.0f - al / A) / a0;
        }
        // Make-up: the mids' loudest (kTapeSatMakeupFromHz up) at exactly 1.
        float mids = 0.0f;
        for (float f = kTapeSatMakeupFromHz; f < 0.45f * fs_; f *= 1.02f) {
            const float cw = std::cos(2.0f * map::kPi * f / fs_);
            mids = std::max(mids, roll_.magnitudeSquared(cw) * bump_.magnitudeSquared(cw));
        }
        makeup_ = 1.0f / std::sqrt(mids);
    }
    void reset()
    {
        emphLp_ = deLp_ = 0.0f;
        roll_.reset(), bump_.reset(), split_.reset();
        patchWait_ = -1, patchLeft_ = 0, flickWait_ = 0, flickLeft_ = 0, flickLen_ = 1;
        patchDepth_ = flickDepth_ = 0.0f;
    }
    // The feedback, worn (in place). crinkle: which kCrinkle (with kCrinkleOn).
    template <bool kCrinkleOn>
    void process(float* x, int n, int crinkle, Rng& rngIn)
    {
        const echo::CrinkleVoicing& cv = echo::kCrinkle[crinkle];
        // The state in locals for the block (the output can't alias it: kept in registers).
        Rng   rng = rngIn;
        float eLp = emphLp_, dLp = deLp_, sLp = split_.y;
        int   pWait = patchWait_, pLeft = patchLeft_, fWait = flickWait_, fLeft = flickLeft_, fLen = flickLen_;
        float pDepth = patchDepth_, fDepth = flickDepth_;
        for (int i = 0; i < n; ++i) {
            // Record: pre-emphasis, the tape saturates (the highs first); playback: de-emphasis.
            const float u = x[i];
            eLp += emphC_ * (u - eLp);
            const float e = u + emphK_ * (u - eLp);
            const float s = softClip(satK_ * e) * satInv_;
            float y = (s + deB_ * dLp) * deInv_;
            dLp += emphC_ * (y - dLp);
            // A little more treble gone each pass; the head bump; the make-up.
            y = makeup_ * bump_.process(roll_.process(y));
            if constexpr (kCrinkleOn) {
                // Crinkled patches, and inside them, flickers of lost contact.
                sLp += split_.c * (y - sLp);
                if (pLeft > 0) {
                    --pLeft;
                    if (fLeft == 0 && --fWait <= 0) {
                        fLen = fLeft = msLen(rng, cv.flickMinMs, cv.flickMaxMs);
                        fDepth = pDepth * (0.4f + 0.6f * uni(rng));
                        fWait  = wait(rng, cv.flicksPerSecond);
                    }
                } else {
                    if (pWait < 0) pWait = wait(rng, cv.patchesPerSecond); // the first, after a reset
                    if (--pWait <= 0) {
                        pLeft  = msLen(rng, cv.patchMinMs, cv.patchMaxMs);
                        pDepth = cv.depthMin + uni(rng) * (cv.depthMax - cv.depthMin);
                        fWait  = 1 + int(uni(rng) * 0.002f * fs_);
                        pWait  = wait(rng, cv.patchesPerSecond);
                    }
                }
                if (fLeft > 0) {
                    // A smooth dip (16 t^2 (1-t)^2: no corner at either end).
                    const float t = float(fLen - fLeft) / float(fLen), w = t * (1.0f - t);
                    const float g = fDepth * 16.0f * w * w;
                    --fLeft;
                    // The highs lose all of the gap, the whole level kCrinkleLevelDip of it.
                    y = (1.0f - echo::kCrinkleLevelDip * g) * (y - g * (y - sLp));
                }
            }
            x[i] = y;
        }
        rngIn = rng;
        emphLp_ = eLp, deLp_ = dLp, split_.y = sLp;
        patchWait_ = pWait, patchLeft_ = pLeft, flickWait_ = fWait, flickLeft_ = fLeft, flickLen_ = fLen;
        patchDepth_ = pDepth, flickDepth_ = fDepth;
    }

private:
    // Uniform in [0, 1); an exponential wait (samples) at `perSecond` on average; a length in ms.
    static float uni(Rng& r) { return 0.5f * (r.bipolar() + 1.0f); }
    int wait(Rng& r, float perSecond) const { return 1 + int(-std::log(std::max(uni(r), 1.0e-6f)) * fs_ / perSecond); }
    int msLen(Rng& r, float minMs, float maxMs) const { return std::max(2, int(0.001f * fs_ * (minMs + uni(r) * (maxMs - minMs)))); }

    float fs_ = 48000.0f;
    float emphC_ = 0.0f, emphK_ = 0.0f, deB_ = 0.0f, deInv_ = 1.0f, emphLp_ = 0.0f, deLp_ = 0.0f;
    float satK_ = 1.0f, satInv_ = 1.0f, makeup_ = 1.0f;
    OnePoleLowpass roll_{}, split_{};
    Biquad         bump_{};
    int   patchWait_ = -1, patchLeft_ = 0, flickWait_ = 0, flickLeft_ = 0, flickLen_ = 1;
    float patchDepth_ = 0.0f, flickDepth_ = 0.0f;
};

class TapeWear {
public:
#ifdef RV_FIXED_VOICINGS
    static constexpr bool kBuilt = echo::kWearDefault != echo::kWearNone;
#else
    static constexpr bool kBuilt = true;
#endif
    static constexpr int kLineSize = kBuilt ? 256 : 1; // the worn tape's wow line (>= 2 x its depth at 96 kHz)
    // The tape wear round (5-7) and its crinkle (6-7): only compiled into the
    // firmware if one of them is the default.
#ifdef RV_FIXED_VOICINGS
    static constexpr bool kTapeSatBuilt = echo::kWearDefault >= echo::kWearTapeSat;
#else
    static constexpr bool kTapeSatBuilt = true;
#endif

    RV_SIZE_OPT void prepare(float sampleRate, uint32_t seed) // set-up
    {
        fs_   = sampleRate;
        seed_ = seed;
        if (!kBuilt) return;
        using namespace echo;
        wowDepth_  = std::min(0.001f * kTapeWowMs * fs_, 0.4f * float(kLineSize));
        flutDepth_ = 0.001f * kTapeFlutterMs * fs_;
        base_      = float(int(wowDepth_ + flutDepth_ + 3.0f)); // whole samples: the input's delay matches it exactly
        wowStep1_  = kTapeWowHz1 / fs_;
        wowStep2_  = kTapeWowHz2 / fs_;
        flutStep_  = kTapeFlutterHz / fs_;
        dropP_     = kTapeDropoutsPerSecond / fs_;
        radioHp1_.setHighpass(kRadioHpHz, kRadioQ, fs_);
        radioLp1_.setLowpass(std::min(kRadioLpHz, 0.45f * fs_), kRadioQ, fs_);
        {   // Peak gain 1 (at the band's centre): the band keeps its level, the rest thins away.
            const float cw = std::cos(2.0f * map::kPi * std::sqrt(kRadioHpHz * kRadioLpHz) / fs_);
            radioGain_ = 1.0f / std::sqrt(radioHp1_.magnitudeSquared(cw) * radioLp1_.magnitudeSquared(cw));
        }
        setBbdClock(kBbd[bbdV_].clockHz);
        compA_     = coeff(kBbdCompAttackMs), compR_ = coeff(kBbdCompReleaseMs);
        expA_      = coeff(kBbdExpAttackMs), expR_ = coeff(kBbdExpReleaseMs);
        whineGain_ = std::exp(kBbdWhineDb * (2.302585093f / 20.0f));
        crushStep_ = kCrushRateHz / fs_;
        crushRel_  = coeff(kCrushReleaseMs);
        crushQ_    = std::exp2(kCrushBits - 1.0f);
        crackP_    = kCrackleRate / fs_;
        sat_.prepare(fs_);
        reset();
    }

    void setVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        v_ = v > 0 && v < echo::kNumWearVoicings ? v : 0;
        reset();
#endif
    }
    int voicing() const { return v_; }
    // BBD strength (EchoVoicing.h kBbd): 0 A ... 3 D.
    void setBbdVoicing([[maybe_unused]] int v)
    {
#ifndef RV_FIXED_VOICINGS
        bbdV_ = v > 0 && v < echo::kNumBbdVoicings ? v : 0;
        setBbdClock(echo::kBbd[bbdV_].clockHz);
#endif
    }
    int bbdVoicing() const { return bbdV_; }
    float bbdClockHz() const { return bbdClock_; }
    // The echo time now (control rate): a time-tracking BBD's clock follows it.
    void setDelaySeconds(float s)
    {
        if (!kBuilt || v_ != echo::kWearBbd || !echo::kBbd[bbdV_].tracksTime || s <= 0.0f) return;
        const float hz = echo::kBbd[bbdV_].clockHz
                       * std::exp(echo::kBbdTrackExp * std::log(echo::kBbdTrackRefSeconds / s));
        const float c = std::clamp(hz, echo::kBbdClockMinHz, echo::kBbdClockMaxHz);
        if (std::fabs(c - bbdClock_) > 0.002f * bbdClock_) setBbdClock(c);
    }
    bool active() const { return kBuilt && v_ != echo::kWearNone; }
    // The fixed delay this voicing adds to the feedback (samples; the worn tape's).
    float latencySamples() const { return kBuilt && v_ == echo::kWearTape ? base_ : 0.0f; }

    RV_SIZE_OPT void reset()
    {
        if (!kBuilt) return;
        std::fill(line_, line_ + kLineSize, 0.0f);
        std::fill(inLine_, inLine_ + kLineSize, 0.0f);
        w_ = 0;
        ph1_ = 0.0f, ph2_ = 0.37f, phF_ = 0.71f;
        dropLeft_ = dropLen_ = 0;
        dropDepth_ = 0.0f;
        rng_.seed(seed_);
        radioHp1_.reset(), radioLp1_.reset();
        bbdAa_.reset(), bbdRec_.reset();
        bbdPh_ = 0.0f, bbdHeld_ = 0.0f, envC_ = envE_ = 0.0f, whinePh_ = 0.0f;
        crushPh_ = 0.0f, crushHeld_ = 0.0f, crushEnv_ = 0.0f;
        sat_.reset();
    }

    // The input, delayed by latencySamples() (worn tape only; others: untouched).
    void delayInput(float* x, int n)
    {
        if (!kBuilt || v_ != echo::kWearTape) return;
        const int d = int(base_);
        for (int i = 0; i < n; ++i) {
            int r = inW_ - d;
            if (r < 0) r += kLineSize;
            inLine_[inW_] = x[i];
            x[i]          = inLine_[r];
            if (++inW_ == kLineSize) inW_ = 0;
        }
    }

    // The feedback, worn (in place).
    void process(float* x, int n)
    {
        if (!kBuilt) return;
        switch (v_) {
        case echo::kWearTape: tape(x, n); break;
        case echo::kWearRadio: radio(x, n); break;
        case echo::kWearBbd: bbd(x, n); break;
        case echo::kWearCrushed: crushed(x, n); break;
        case echo::kWearTapeSat: sat_.template process<false>(x, n, 0, rng_); break;
        case echo::kWearCrinkle: sat_.template process<true>(x, n, 0, rng_); break;
        case echo::kWearCrinkleHeavy: sat_.template process<true>(x, n, 1, rng_); break;
        default: break;
        }
    }

private:
    float coeff(float ms) const { return 1.0f - std::exp(-1000.0f / (ms * fs_)); }
    void setBbdClock(float hz)
    {
        if (!kBuilt) return;
        bbdClock_ = std::min(hz, 0.95f * fs_);
        const float fc = std::min(echo::kBbd[bbdV_].filterRatio * bbdClock_, 0.45f * fs_);
        bbdAa_.setCutoff(fc, fs_);
        bbdRec_.setCutoff(fc, fs_);
        bbdStep_   = bbdClock_ / fs_;
        whineStep_ = std::min(bbdClock_, 0.45f * fs_) / fs_;
        // Make-up at kBbdMakeupHz: the two filters and the hold's sinc droop.
        const float f  = echo::kBbdMakeupHz;
        const float cw = std::cos(2.0f * map::kPi * f / fs_);
        const float x  = map::kPi * f / bbdClock_;
        const float sinc = std::sin(x) / x;
        bbdMakeup_ = 1.0f / std::sqrt(bbdAa_.magnitudeSquared(cw) * bbdRec_.magnitudeSquared(cw) * sinc * sinc);
    }
    static float wrap(float p) { return p >= 1.0f ? p - 1.0f : p; }

    void tape(float* x, int n)
    {
        using namespace echo;
        constexpr float k2pi = 2.0f * map::kPi;
        for (int i = 0; i < n; ++i) {
            // Oxide dropout: a raised-cosine dip, started at random.
            float g = 1.0f;
            if (dropLeft_ > 0) {
                const float u = float(dropLen_ - dropLeft_) / float(dropLen_);
                g = 1.0f - dropDepth_ * 0.5f * (1.0f - std::cos(k2pi * u));
                --dropLeft_;
            } else if (0.5f * (rng_.bipolar() + 1.0f) < dropP_) {
                const float a = 0.5f * (rng_.bipolar() + 1.0f), b = 0.5f * (rng_.bipolar() + 1.0f);
                dropLen_ = dropLeft_ = std::max(2, int(0.001f * fs_ * (kTapeDropoutMinMs + a * (kTapeDropoutMaxMs - kTapeDropoutMinMs))));
                const float db = kTapeDropoutMinDb + b * (kTapeDropoutMaxDb - kTapeDropoutMinDb);
                dropDepth_ = 1.0f - std::exp(-db * (2.302585093f / 20.0f));
            }
            // Saturation that bites as a build grows (unity when quiet).
            const float s = softClip(kTapeSatDrive * g * x[i]) * (1.0f / kTapeSatDrive);
            // Wow and flutter: this pass's own, out of step with the echo.
            ph1_ = wrap(ph1_ + wowStep1_), ph2_ = wrap(ph2_ + wowStep2_), phF_ = wrap(phF_ + flutStep_);
            const float d = base_ + wowDepth_ * (0.6f * std::sin(k2pi * ph1_) + 0.4f * std::sin(k2pi * ph2_ + 1.0f))
                          + flutDepth_ * std::sin(k2pi * phF_);
            line_[w_]      = s;
            const int   di = int(d);
            const float fr = d - float(di);
            int r0 = w_ - di;
            if (r0 < 0) r0 += kLineSize;
            const int r1 = r0 == 0 ? kLineSize - 1 : r0 - 1;
            x[i]         = line_[r0] + fr * (line_[r1] - line_[r0]);
            if (++w_ == kLineSize) w_ = 0;
        }
    }

    void radio(float* x, int n)
    {
        for (int i = 0; i < n; ++i) x[i] = radioGain_ * radioLp1_.process(radioHp1_.process(x[i]));
    }

    void bbd(float* x, int n)
    {
        constexpr float kRef = 0.01f, kFloor = 1.0e-7f, kMax = 8.0f;
        for (int i = 0; i < n; ++i) {
            // Compressor 2:1 (power envelope; gain = (ref / P)^(1/4)).
            const float p = x[i] * x[i];
            envC_ += (p > envC_ ? compA_ : compR_) * (p - envC_);
            const float gc = std::min(kMax, std::sqrt(std::sqrt(kRef / std::max(envC_, kFloor))));
            // The bucket brigade: imperfect anti-alias, sample-and-hold at the clock, reconstruction.
            const float a = bbdAa_.process(x[i] * gc);
            bbdPh_ += bbdStep_;
            if (bbdPh_ >= 1.0f) {
                bbdPh_ -= 1.0f;
                bbdHeld_ = a;
            }
            const float y = bbdMakeup_ * bbdRec_.process(bbdHeld_);
            // Expander 1:2, tracking with its own (different) times: the pumping.
            const float py = y * y;
            envE_ += (py > envE_ ? expA_ : expR_) * (py - envE_);
            // Never more than the compressor took (gc x ge <= 1): it breathes
            // down, never up, so a repeat can't gain on the way round.
            const float ge = std::min(std::min(kMax, std::sqrt(std::max(envE_, kFloor) / kRef)), 1.0f / gc)
                           * (envC_ > kFloor ? 1.0f : 0.0f);
            // The clock's whine, riding on the signal's level.
            whinePh_ = wrap(whinePh_ + whineStep_);
            x[i] = ge * y + whineGain_ * std::sqrt(envE_) * ge * std::sin(2.0f * map::kPi * whinePh_);
        }
    }

    void crushed(float* x, int n)
    {
        for (int i = 0; i < n; ++i) {
            const float ax = std::fabs(x[i]);
            crushEnv_ = ax > crushEnv_ ? ax : crushEnv_ + crushRel_ * (ax - crushEnv_);
            // Re-sampled without an anti-alias filter ...
            crushPh_ += crushStep_;
            if (crushPh_ >= 1.0f) {
                crushPh_ -= 1.0f;
                // ... re-quantised relative to the signal's level (gain-ranging) ...
                const float step = std::max(crushEnv_, 1.0e-9f) / crushQ_;
                crushHeld_ = step * std::nearbyint(x[i] / step);
            }
            float y = crushHeld_;
            // ... and sparse crackle riding on it.
            if (0.5f * (rng_.bipolar() + 1.0f) < crackP_) y += echo::kCrackleLevel * crushEnv_ * rng_.bipolar();
            x[i] = y;
        }
    }

    float    fs_ = 48000.0f;
    uint32_t seed_ = 1;
#ifdef RV_FIXED_VOICINGS
    static constexpr int v_ = echo::kWearDefault;
#else
    int v_ = echo::kWearDefault;
#endif
    Rng rng_;
    // Worn tape.
    float line_[kLineSize] = {}, inLine_[kLineSize] = {};
    int   w_ = 0, inW_ = 0;
    float base_ = 0.0f, wowDepth_ = 0.0f, flutDepth_ = 0.0f, wowStep1_ = 0.0f, wowStep2_ = 0.0f, flutStep_ = 0.0f;
    float ph1_ = 0.0f, ph2_ = 0.0f, phF_ = 0.0f, dropP_ = 0.0f, dropDepth_ = 0.0f;
    int   dropLeft_ = 0, dropLen_ = 0;
    // Radio band.
    Biquad radioHp1_{}, radioLp1_{};
    float  radioGain_ = 1.0f;
    // BBD.
    OnePoleLowpass bbdAa_{}, bbdRec_{};
    float bbdStep_ = 0.0f, bbdPh_ = 0.0f, bbdHeld_ = 0.0f, envC_ = 0.0f, envE_ = 0.0f, bbdClock_ = 9700.0f, bbdMakeup_ = 1.0f;
#ifdef RV_FIXED_VOICINGS
    static constexpr int bbdV_ = echo::kBbdDefault;
#else
    int bbdV_ = echo::kBbdDefault;
#endif
    float compA_ = 0.0f, compR_ = 0.0f, expA_ = 0.0f, expR_ = 0.0f, whineStep_ = 0.0f, whineGain_ = 0.0f, whinePh_ = 0.0f;
    // Crushed.
    float crushStep_ = 0.0f, crushPh_ = 0.0f, crushHeld_ = 0.0f, crushEnv_ = 0.0f, crushRel_ = 0.0f, crushQ_ = 64.0f;
    float crackP_ = 0.0f;
    // Tape wear (5-7): an empty no-op unless built (the firmware, unless it is the default).
    TapeSat<kTapeSatBuilt> sat_;
};

} // namespace rv::dsp
