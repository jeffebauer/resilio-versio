#include "dsp/Spring.h"

#include "params/AntiRes.h"
#include "params/SplashVoicing.h"
#include "params/SpringModes.h"

#include <algorithm>
#include <cmath>

namespace rv {

namespace {

constexpr float kLn10 = 2.302585093f;

// Frequencies where the Loop gain design checks T60. The slowest-decaying
// band sets the tail length the ear (and the Schroeder T60 metric) hears. It
// sits in the low mids: below ~100 Hz the DC blocker drains energy, above
// ~1 kHz the damping LPF does and the round trip is shorter.
constexpr std::array<float, 8> kDesignHz{{70.0f, 110.0f, 170.0f, 250.0f, 370.0f, 550.0f, 800.0f, 1200.0f}};

// AntiRes layer 1 (M6): the same check just under the Chirp's top edge, as
// fractions of fC. The allpass chain's round trip peaks at one end of the
// Chirp band: at the lows when a < 0 (highs first, inside kDesignHz already)
// and at fC when a > 0 (highs later, as real tanks do: docs/ir-dispersion-study.md).
// Without these points a > 0 would let the band under fC ring up to ~1.2x
// longer than DECAY asks. With a < 0 they never set g (the round trip is
// shortest there), so the tail is unchanged.
constexpr std::array<float, 5> kDesignFcRatios{{0.6f, 0.75f, 0.85f, 0.92f, 1.0f}};

int nextPow2(int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

// Sized for the longest L any Spring can have: DECAY max times the largest
// detune factor (core/params/SpringModes.h), plus a little margin (2 %: the
// Micro-mod floor and the Howl movement, ~1.1 % at most), plus the M7
// modulation on top: the Jolt (Loop fraction + KICKED rattle, at most
// kJoltMaxLoopFrac of L) and WOBBLE (at most wobbleMaxDepthSamples()).
constexpr float kLongestLoopSeconds = map::kLoopDelayMaxSeconds * modes::kMaxLoopDelayDetune;
constexpr float kJoltMaxLoopFrac    = 0.0125f;
int lowDelaySize(float sampleRate)
{
    return int(std::ceil(kLongestLoopSeconds * sampleRate * (1.02f + kJoltMaxLoopFrac)))
         + int(std::ceil(splash::wobbleMaxDepthSamples(sampleRate))) + 8;
}
int highDelaySize(float sampleRate)
{
    return int(std::ceil(Spring::kHighDelayRatio * kLongestLoopSeconds * sampleRate * 1.02f)) + 8;
}
// Each stretched section's ring holds K+1 samples; K is largest at the lowest
// (detuned) fC.
int ringSize(float sampleRate)
{
    const float kMax = map::stretchK(map::kTransitionMinHz * modes::kMinTransitionDetune, sampleRate);
    return nextPow2(int(std::ceil(kMax * 1.1f)) + 3);
}

} // namespace

size_t Spring::requiredFloats(float sampleRate)
{
    return size_t(lowDelaySize(sampleRate)) + size_t(highDelaySize(sampleRate))
         + size_t(kMaxStages) * size_t(ringSize(sampleRate));
}

void Spring::prepare(float sampleRate, float* pool, uint32_t noiseSeed)
{
    sampleRate_ = sampleRate;
    lowSize_    = lowDelaySize(sampleRate);
    highSize_   = highDelaySize(sampleRate);
    ringMask_   = ringSize(sampleRate) - 1;
    lowBuf_     = pool;
    highBuf_    = lowBuf_ + lowSize_;
    rings_      = highBuf_ + highSize_;
    seed_       = noiseSeed;

    dc_.setCutoff(kDcBlockHz, sampleRate);
    highCeiling_.setCutoff(std::min(kHighCeilingHz, 0.45f * sampleRate), sampleRate);
    mRate_ = 1.0f / (kStageRampSeconds * sampleRate);
    loopSat_.prepare(sampleRate);
    // The Loop gain design evaluates the Loop at kDesignHz on every
    // coefficient change (each tick while a knob or the Jolt moves). cos(w)
    // and the LoopSat latency there depend only on the sample rate: work
    // them out once (M3: the redesign of three Springs was a 17 %-of-a-block
    // burst).
    static_assert(kDesignHz.size() == size_t(kNumDesignHz), "Spring.h caches one value per design frequency");
    for (size_t k = 0; k < kDesignHz.size(); ++k) {
        designCos_[k]     = std::cos(2.0f * map::kPi * kDesignHz[k] / sampleRate_);
        designLatency_[k] = dsp::LoopSat::latencySamples(kDesignHz[k], sampleRate_);
    }
    modHold_ = std::max(1, int(antires::kMicroModHoldSeconds * sampleRate));
    modC_    = 1.0f - std::exp(-1.0f / (antires::kMicroModHoldSeconds * sampleRate));

    reset();
    setSettings(settings_, true);
}

void Spring::reset()
{
    std::fill(lowBuf_, lowBuf_ + lowSize_, 0.0f);
    std::fill(highBuf_, highBuf_ + highSize_, 0.0f);
    std::fill(rings_, rings_ + size_t(kMaxStages) * size_t(ringMask_ + 1), 0.0f);
    lowW_ = highW_ = ringW_ = 0;
    thiranY1_.fill(0.0f);
    hapX1_.fill(0.0f);
    hapY1_.fill(0.0f);
    dc_.reset();
    chirpLowpass_.reset();
    damping_.reset();
    highpass_.reset();
    highCeiling_.reset();
    loopSat_.reset();
    rng_.seed(seed_);
    // The modulation starts from rest at a seeded point: deterministic.
    modRng_.seed(seed_ ^ 0x5DEECE66u);
    modTarget_ = modY1_ = modY2_ = 0.0f;
    modCount_  = 0;
    lfoS_ = 0.0f;
    lfoC_ = 1.0f;
    modNow_ = 1.0f;
    tapOffset_ = tapOffsetTarget_;
}

void Spring::setSettings(const SpringSettings& s, bool snap)
{
    // At rest (knobs still, glides finished) the coefficients can't change:
    // skip the redesign and save its transcendental maths.
    const bool same = s.loopDelaySeconds == settings_.loopDelaySeconds && s.t60Seconds == settings_.t60Seconds
                   && s.transitionHz == settings_.transitionHz && s.allpassCoeff == settings_.allpassCoeff
                   && s.stages == settings_.stages && s.dampingHz == settings_.dampingHz
                   && s.highPathLevel == settings_.highPathLevel && s.tapRatio == settings_.tapRatio
                   && s.tapOffsetSeconds == settings_.tapOffsetSeconds
                   && s.loopSatAmount == settings_.loopSatAmount && s.loopSatKPos == settings_.loopSatKPos
                   && s.loopSatKNeg == settings_.loopSatKNeg && s.howl == settings_.howl
                   && s.modDepth == settings_.modDepth && s.lfoDepth == settings_.lfoDepth && s.lfoHz == settings_.lfoHz;
    if (!snap && same && lCur_ == lTarget_ && mPos_ == float(mTarget_)) return;

    settings_ = s;
    lTarget_  = std::clamp(s.loopDelaySeconds * sampleRate_, 4.0f, float(lowSize_ - 4));
    mTarget_  = std::clamp(s.stages, 1, kMaxStages);
    if (snap) {
        lCur_ = lTarget_;
        mPos_ = float(mTarget_);
        for (int j = mTarget_; j < kMaxStages; ++j) clearStage(j);
        mActive_ = mTarget_;
    }
    updateCoefficients();
    if (snap) tapOffset_ = tapOffsetTarget_;
}

void Spring::updateCoefficients()
{
    const SpringSettings& s = settings_;

    // Stretch K = N + d: N whole samples in the ring plus a first-order Thiran
    // allpass for the fraction d in [0.5, 1.5). Thiran is itself an allpass, so
    // the section stays exactly allpass for any real K and K can glide.
    k_   = std::clamp(map::stretchK(s.transitionHz, sampleRate_), 1.6f, float(ringMask_ - 2));
    n_   = int(k_ - 0.5f);
    const float d = k_ - float(n_);
    eta_ = (1.0f - d) / (1.0f + d);
    a_   = s.allpassCoeff;

    chirpLowpass_.setLowpass(s.transitionHz, 0.7071f, sampleRate_);
    damping_.setCutoff(std::min(s.dampingHz, 0.45f * sampleRate_), sampleRate_);
    highpass_.setHighpass(kHighPassRatio * s.transitionHz, 0.7071f, sampleRate_);
    highPathLevel_ = s.highPathLevel;
    tapRatio_      = std::clamp(s.tapRatio, 0.05f, 0.95f);
    tapOffsetTarget_ = s.tapOffsetSeconds * sampleRate_; // glides in advanceGlides()

    // Loop gain g from the target T60 and the *actual* round trip. A tail
    // loses 60 dB in T60 seconds; one trip takes RT seconds, so each trip may
    // lose 60 * RT / T60 dB in total. The filters already take |H(f)| of that,
    // g supplies the rest. Take the smallest g over the design band so the
    // slowest band hits T60 and no band rings longer.
    const float t60 = kT60DesignScale * s.t60Seconds;
    float g = kMaxGain, maxMag = 0.0f;
    for (size_t k = 0; k < kDesignHz.size(); ++k) {
        const float rt = roundTripAt(kDesignHz[k], designCos_[k], designLatency_[k]);
        const float m  = loopMagnitudeAt(designCos_[k]);
        const float gf = std::exp(-3.0f * kLn10 * rt / (t60 * sampleRate_)) / m;
        g = std::min(g, gf);
        maxMag = std::max(maxMag, m);
    }
    for (float r : kDesignFcRatios) {
        const float hz = r * s.transitionHz;
        const float cw = std::cos(2.0f * map::kPi * hz / sampleRate_);
        g = std::min(g, std::exp(-3.0f * kLn10 * roundTripAt(hz, cw, dsp::LoopSat::latencySamples(hz, sampleRate_))
                                 / (t60 * sampleRate_))
                            / loopMagnitudeAt(cw));
    }
    g = std::max(0.0f, g);

    // Howl zone: lift the small-signal peak gain P = g·max|H| toward
    // kHowlPeakGain (> 1). The LoopSat's compression then holds the level.
    const float howl = std::clamp(s.howl, 0.0f, 1.0f);
    if (howl > 0.0f && maxMag > 0.0f) {
        const float p0 = g * maxMag;
        const float p  = p0 + (drive::kHowlPeakGain - p0) * std::sqrt(howl);
        g = p / maxMag;
    }
    g_ = g;
    loopSat_.set(s.loopSatAmount, s.loopSatKPos, s.loopSatKNeg);

    modDepth_ = std::max(0.0f, s.modDepth) * antires::kMicroModNorm;
    lfoDepth_ = std::max(0.0f, s.lfoDepth);
    lfoE_     = 2.0f * std::sin(map::kPi * std::max(0.0f, s.lfoHz) / sampleRate_); // magic-circle step

    // High path: no dispersion to speak of, simple T60 from its own trip.
    lhCur_ = kHighDelayRatio * lCur_;
    gHigh_ = std::min(kMaxGain, std::exp(-3.0f * kLn10 * lhCur_ / (kHighT60Ratio * s.t60Seconds * sampleRate_)));
}

float Spring::chainGroupDelaySamples(float freqHz) const
{
    return mPos_ * map::stretchedAllpassGroupDelaySamples(a_, k_, freqHz, sampleRate_);
}

float Spring::roundTripSamples(float freqHz) const
{
    return roundTripAt(freqHz, std::cos(2.0f * map::kPi * freqHz / sampleRate_),
                       dsp::LoopSat::latencySamples(freqHz, sampleRate_));
}

float Spring::roundTripAt(float freqHz, float cw, float loopSatLatency) const
{
    // Butterworth LPF group delay well below its cutoff ≈ sqrt(2) / (2 pi fC).
    const float lpfDelay = 1.41421356f * sampleRate_ / (2.0f * map::kPi * settings_.transitionHz);
    return lCur_ + chainGroupDelaySamples(freqHz) + damping_.groupDelay(cw) + lpfDelay + loopSatLatency;
}

float Spring::loopMagnitude(float freqHz) const
{
    return loopMagnitudeAt(std::cos(2.0f * map::kPi * freqHz / sampleRate_));
}

float Spring::loopMagnitudeAt(float cw) const
{
    return std::sqrt(dc_.magnitudeSquared(cw) * chirpLowpass_.magnitudeSquared(cw) * damping_.magnitudeSquared(cw));
}

float Spring::t60AtSeconds(float freqHz) const
{
    const float perTrip = g_ * loopMagnitude(freqHz);
    if (perTrip <= 0.0f) return 0.0f;
    return -3.0f * roundTripSamples(freqHz) / (sampleRate_ * std::log10(perTrip));
}

void Spring::clearStage(int j)
{
    float* ring = rings_ + size_t(j) * size_t(ringMask_ + 1);
    std::fill(ring, ring + ringMask_ + 1, 0.0f);
    thiranY1_[size_t(j)] = 0.0f;
}

float Spring::readLow(float delay) const
{
    // Linear interpolation between the two nearest samples: cheap fractional
    // delay so L can glide (ADR 0012). Its slight HF loss is folded into the
    // dark Loop anyway.
    const int   di = int(delay);
    const float fr = delay - float(di);
    int i0 = lowW_ - di;
    if (i0 < 0) i0 += lowSize_;
    int i1 = i0 - 1;
    if (i1 < 0) i1 += lowSize_;
    return lowBuf_[i0] + fr * (lowBuf_[i1] - lowBuf_[i0]);
}

inline void Spring::advanceGlides()
{
    // Slew-limited L: a DECAY move becomes a smooth tape-speed bend, never a
    // jump (ADR 0012). The high path follows the same tank length.
    const float dl = lTarget_ - lCur_;
    if (dl > kLoopSlewPerSample) lCur_ += kLoopSlewPerSample;
    else if (dl < -kLoopSlewPerSample) lCur_ -= kLoopSlewPerSample;
    else lCur_ = lTarget_; // land exactly, so "at rest" is detectable
    lhCur_ = kHighDelayRatio * lCur_;

    // The pickup offset glides too (it follows TENSION and SPRINGS, see
    // SpringSettings::tapOffsetSeconds): a jump in the read point would click.
    const float dt = tapOffsetTarget_ - tapOffset_;
    if (dt > kTapSlewPerSample) tapOffset_ += kTapSlewPerSample;
    else if (dt < -kTapSlewPerSample) tapOffset_ -= kTapSlewPerSample;
    else tapOffset_ = tapOffsetTarget_;

    // Stage count M glides one stage at a time: the stage at the edge is
    // cross-faded in/out, so TENSION never clicks. At rest mPos_ is a whole
    // number, so no stage is ever left half-mixed (which would comb-filter).
    if (mPos_ != float(mTarget_)) {
        if (mPos_ < float(mTarget_)) mPos_ = std::min(float(mTarget_), mPos_ + mRate_);
        else mPos_ = std::max(float(mTarget_), mPos_ - mRate_);
        const int active = int(std::ceil(mPos_));
        for (int j = active; j < mActive_; ++j) clearStage(j); // leaving stages start clean next time
        mActive_ = active;
    }
}

inline float Spring::advanceModulation()
{
    // Smoothed random: a new target every modHold_ samples, two one-poles.
    if (--modCount_ <= 0) {
        modCount_  = modHold_;
        modTarget_ = modRng_.bipolar();
    }
    modY1_ += modC_ * (modTarget_ - modY1_);
    modY2_ += modC_ * (modY1_ - modY2_);
    // Slow sine, "magic circle" oscillator: constant amplitude, 2 multiplies.
    lfoS_ += lfoE_ * lfoC_;
    lfoC_ -= lfoE_ * lfoS_;
    modNow_ = 1.0f + modDepth_ * modY2_ + lfoDepth_ * lfoS_;
    return modNow_;
}

inline float Spring::processLow(float in, float lMod, float tapMod)
{
    const float fb  = readLow(lMod);
    // Pickup ~half way: first echo after ~half a round trip (+ the fixed
    // stagger and WOBBLE's transport).
    float tapAt = tapRatio_ * lMod + tapOffset_ + tapMod;
    tapAt = tapAt < 2.0f ? 2.0f : (tapAt > lMod ? lMod : tapAt);
    const float tap = readLow(tapAt);

    float x = dc_.process(in + g_ * loopSat_.process(fb));

    // Spectral delay filter: M stretched allpass sections, each
    //   H(z) = (a + D(z)) / (1 + a D(z)),  D(z) = z^-N · Thiran(d) ≈ z^-K.
    // Schroeder form: v = x - a·D{v}, y = a·v + D{v}.
    const int   full = int(mPos_);
    const float frac = mPos_ - float(full);
    const int   iw   = ringW_;
    const int   ir0  = (iw - n_) & ringMask_;
    const int   ir1  = (iw - n_ - 1) & ringMask_;
    const size_t stride = size_t(ringMask_ + 1);
    const float a = a_, eta = eta_;
    // D{v} of a section needs only last sample's state, not this sample's x.
    // So the next section's D{v} is worked out while this section's x chain
    // waits on its multiply-adds (the M7 issues in order: without other work
    // in between, a section was a 7-step chain at ~21 cycles; this way ~14,
    // firmware/m3_bench.cpp "pipe"). Same arithmetic, same order per value.
    if (full > 0) {
        float* ring = rings_;
        float  d    = eta * (ring[ir0] - thiranY1_[0]) + ring[ir1];
        for (int j = 0; j < full - 1; ++j) {
            float* const next = ring + stride;
            const float  dn   = eta * (next[ir0] - thiranY1_[size_t(j + 1)]) + next[ir1];
            thiranY1_[size_t(j)] = d;
            const float v = x - a * d;
            ring[iw] = v;
            x = a * v + d;
            d    = dn;
            ring = next;
        }
        thiranY1_[size_t(full - 1)] = d;
        const float v = x - a * d;
        ring[iw] = v;
        x = a * v + d;
    }
    if (frac > 0.0f && full < kMaxStages) {
        float* ring = rings_ + size_t(full) * stride;
        const float dOut = eta * (ring[ir0] - thiranY1_[size_t(full)]) + ring[ir1];
        thiranY1_[size_t(full)] = dOut;
        const float v = x - a * dOut;
        ring[iw] = v;
        x += frac * (a * v + dOut - x);
    }
    ringW_ = (iw + 1) & ringMask_;

    x = chirpLowpass_.process(x);
    x = damping_.process(x);

    lowBuf_[lowW_] = x;
    if (++lowW_ == lowSize_) lowW_ = 0;
    return tap;
}

inline float Spring::processHigh(float in, float lhMod)
{
    const int   di = int(lhMod);
    const float fr = lhMod - float(di);
    int i0 = highW_ - di;
    if (i0 < 0) i0 += highSize_;
    int i1 = i0 - 1;
    if (i1 < 0) i1 += highSize_;
    const float fb = highBuf_[i0] + fr * (highBuf_[i1] - highBuf_[i0]);
    // Pickup: the output reads earlier along the line than the feedback.
    const float pm = kHighPickup * lhMod;
    const int   pi = int(pm);
    const float pf = pm - float(pi);
    int p0 = highW_ - pi;
    if (p0 < 0) p0 += highSize_;
    int p1 = p0 - 1;
    if (p1 < 0) p1 += highSize_;
    const float out = highBuf_[p0] + pf * (highBuf_[p1] - highBuf_[p0]);

    float h = in + gHigh_ * fb;
    for (int j = 0; j < kHighStages; ++j) { // first-order allpass: y = a(x - y1) + x1
        const float y = kHighAllpassCoeff * (h - hapY1_[size_t(j)]) + hapX1_[size_t(j)];
        hapX1_[size_t(j)] = h;
        hapY1_[size_t(j)] = y;
        h = y;
    }
    h = highpass_.process(h);
    h = highCeiling_.process(h);

    highBuf_[highW_] = h;
    if (++highW_ == highSize_) highW_ = 0;
    return out;
}

void Spring::process(const float* in, const float* highIn, const float* lFrac, const float* lSamples,
                     const float* tapSamples, float* out, int n)
{
    // Read limits: the delay memory holds the longest L plus all modulation
    // (lowDelaySize); the clamp only guards against a caller passing more.
    const float lMax = float(lowSize_ - 2);
    for (int i = 0; i < n; ++i) {
        advanceGlides();
        // Tiny seeded noise (-200 dB) keeps every filter state far above the
        // denormal range once a tail has died away. Inaudible, deterministic.
        const float nz   = kDenormalNoise * rng_.bipolar();
        const float x    = in[i] + nz;
        const float xh   = highIn ? highIn[i] + nz : x;
        const float mod  = advanceModulation() + (lFrac ? lFrac[i] : 0.0f);
        float       lMod = lCur_ * mod + (lSamples ? lSamples[i] : 0.0f);
        lMod = lMod < 2.0f ? 2.0f : (lMod > lMax ? lMax : lMod);
        const float low  = processLow(x, lMod, tapSamples ? tapSamples[i] : 0.0f);
        const float high = processHigh(xh, lhCur_); // high path unmodulated, see "Micro-mod floor"
        out[i] = low + highPathLevel_ * high;
    }
}

} // namespace rv
