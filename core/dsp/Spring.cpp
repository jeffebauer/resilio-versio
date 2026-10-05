#include "dsp/Spring.h"
#include "dsp/SizeOpt.h"
#include "dsp/StretchedChain.h"

#include "params/AntiRes.h"
#include "params/SplashVoicing.h"
#include "params/SpringModes.h"
#include "params/ThrowHold.h"
#include "params/WobbleVoicing.h"

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
// kJoltMaxLoopFrac of L) and WOBBLE (at most wobble::maxDepthSamples()).
constexpr float kLongestLoopSeconds = map::kLoopDelayMaxSeconds * modes::kMaxLoopDelayDetune;
constexpr float kJoltMaxLoopFrac    = 0.0125f;
RV_SIZE_OPT int lowDelaySize(float sampleRate)
{
    return int(std::ceil(kLongestLoopSeconds * sampleRate * (1.02f + kJoltMaxLoopFrac)))
         + int(std::ceil(wobble::maxDepthSamples(sampleRate))) + 8;
}
RV_SIZE_OPT int highDelaySize(float sampleRate)
{
    return int(std::ceil(Spring::kHighDelayRatio * kLongestLoopSeconds * sampleRate * 1.02f)) + 8;
}
// Each stretched section's ring holds K+1 samples; K is largest at the lowest
// (detuned) fC.
RV_SIZE_OPT int ringSize(float sampleRate)
{
    const float kMax = map::stretchK(map::kTransitionMinHz * modes::kMinTransitionDetune, sampleRate);
    return nextPow2(int(std::ceil(kMax * 1.1f)) + 3);
}

} // namespace

RV_SIZE_OPT size_t Spring::requiredFloats(float sampleRate, [[maybe_unused]] int maxStages)
{
#if RV_TANKV_BUILT >= 1
    const int rings = std::clamp(maxStages, 1, kMaxStages);
#else
    constexpr int rings = kMaxStages; // the firmware without voicing 1
#endif
    return size_t(lowDelaySize(sampleRate)) + size_t(highDelaySize(sampleRate)) + size_t(rings) * size_t(ringSize(sampleRate));
}

RV_SIZE_OPT void Spring::prepare(float sampleRate, float* pool, uint32_t noiseSeed, int maxStages)
{
    sampleRate_ = sampleRate;
#if RV_TANKV_BUILT >= 1
    maxStages_  = std::clamp(maxStages, 1, kMaxStages);
#else
    (void)maxStages; // the firmware without voicing 1: always kMaxStages
#endif
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
    static_assert(kDesignFcRatios.size() == size_t(kNumFcPoints), "Spring.h caches one value per fC point");
    for (size_t k = 0; k < kDesignHz.size(); ++k) {
        ptCos_[k]     = std::cos(2.0f * map::kPi * kDesignHz[k] / sampleRate_);
        ptLatency_[k] = dsp::LoopSat::latencySamples(kDesignHz[k], sampleRate_);
    }
    // Everything else in the caches is per fC / damping: work it out afresh.
    designFc_ = designDampHz_ = magFc_ = lfoHzSet_ = -1.0f;
    pending_  = false;
    stale_    = false;
    modHold_ = std::max(1, int(antires::kMicroModHoldSeconds * sampleRate));
    modC_    = 1.0f - std::exp(-1.0f / (antires::kMicroModHoldSeconds * sampleRate));

    reset();
    setSettings(settings_, true);
}

RV_SIZE_OPT void Spring::reset()
{
    std::fill(lowBuf_, lowBuf_ + lowSize_, 0.0f);
    std::fill(highBuf_, highBuf_ + highSize_, 0.0f);
    std::fill(rings_, rings_ + size_t(maxStages_) * size_t(ringMask_ + 1), 0.0f);
    lowW_ = highW_ = ringW_ = 0;
#if RV_TANKV_BUILT >= 3
    for (int k = 0; k < numFbDiff_; ++k) {
        std::fill(fbDiff_[size_t(k)].buf, fbDiff_[size_t(k)].buf + fbDiff_[size_t(k)].size, 0.0f);
        fbDiff_[size_t(k)].w = 0;
    }
#endif
    thiranY1_.fill(0.0f);
    hapX1_.fill(0.0f);
    hapY1_.fill(0.0f);
    dc_.reset();
    chirpLowpass_.reset();
    damping_.reset();
#if RV_TANKV_BUILT >= 8
    dampBq_.reset();
#endif
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

bool Spring::setSettings(const SpringSettings& s, bool snap)
{
    if (stale_) { // after setSettingsUnheard(): nothing staged or cached is current
        stale_    = false;
        pending_  = false;
        designFc_ = designDampHz_ = magFc_ = -1.0f;
        snap      = true;
    }
    if (pending_ && !snap) {
        // Second half of a TENSION redesign: install the filters staged on
        // the last call together with everything else as it is now. fC
        // stays the staged one; if TENSION has moved on since, the next
        // call starts another round.
        SpringSettings t = s;
        t.transitionHz = designFc_;
        settings_ = t;
        lTarget_  = std::clamp(t.loopDelaySeconds * sampleRate_ - diffDelay(), 4.0f, float(lowSize_ - 4));
        mTarget_  = std::clamp(t.stages, 1, maxStages_);
        prepareDamping(t.dampingHz);
        commitDesign();
        pending_ = false;
        return true;
    }

    // At rest (knobs still, glides finished) the coefficients can't change:
    // skip the redesign and save its transcendental maths.
    const bool same = s.loopDelaySeconds == settings_.loopDelaySeconds && s.t60Seconds == settings_.t60Seconds
                   && s.transitionHz == settings_.transitionHz && s.allpassCoeff == settings_.allpassCoeff
                   && s.stages == settings_.stages && s.dampingHz == settings_.dampingHz
                   && s.highPathLevel == settings_.highPathLevel && s.tapRatio == settings_.tapRatio
                   && s.tapOffsetSeconds == settings_.tapOffsetSeconds
                   && s.loopSatAmount == settings_.loopSatAmount && s.loopSatKPos == settings_.loopSatKPos
                   && s.loopSatKNeg == settings_.loopSatKNeg && s.howl == settings_.howl
                   && s.modDepth == settings_.modDepth && s.lfoDepth == settings_.lfoDepth && s.lfoHz == settings_.lfoHz
                   && s.hold == settings_.hold && s.highT60Seconds == settings_.highT60Seconds
#if RV_TANKV_BUILT >= 8
                   && s.eqHz == settings_.eqHz && s.eqDb == settings_.eqDb && s.eqQ == settings_.eqQ
                   && s.eqToLp == settings_.eqToLp
#endif
        ;
    if (!snap && same && lCur_ == lTarget_ && mPos_ == float(mTarget_)) return true;

    if (!snap && s.transitionHz != designFc_) {
        // fC moved (TENSION): the costly half now, the rest next call. What
        // only sets where a glide heads (L, the stage count, the pickup)
        // and the allpass coefficient (like setAllpassCoeff(): the Jolt
        // reaches every Spring on the same tick) go in at once; the
        // filters and g, which must match each other, next call.
        prepareTransition(s.transitionHz);
        a_               = s.allpassCoeff;
        lTarget_         = std::clamp(s.loopDelaySeconds * sampleRate_ - diffDelay(), 4.0f, float(lowSize_ - 4));
        mTarget_         = std::clamp(s.stages, 1, maxStages_);
        tapOffsetTarget_ = s.tapOffsetSeconds * sampleRate_;
        pending_         = true;
        return false;
    }

    settings_ = s;
    lTarget_  = std::clamp(s.loopDelaySeconds * sampleRate_ - diffDelay(), 4.0f, float(lowSize_ - 4));
    mTarget_  = std::clamp(s.stages, 1, maxStages_);
    if (snap) {
        lCur_ = lTarget_;
        mPos_ = float(mTarget_);
        for (int j = mTarget_; j < maxStages_; ++j) clearStage(j);
        mActive_ = mTarget_;
    }
    if (s.transitionHz != designFc_) prepareTransition(s.transitionHz); // snap
    prepareDamping(s.dampingHz);
    commitDesign();
    if (snap) tapOffset_ = tapOffsetTarget_;
    pending_ = false;
    return true;
}

void Spring::prepareTransition(float transitionHz)
{
    // Stretch K = N + d: N whole samples in the ring plus a first-order Thiran
    // allpass for the fraction d in [0.5, 1.5). Thiran is itself an allpass, so
    // the section stays exactly allpass for any real K and K can glide.
    stagedK_ = std::clamp(map::stretchK(transitionHz, sampleRate_), 1.6f, float(ringMask_ - 2));
    stagedN_ = int(stagedK_ - 0.5f);
    const float d = stagedK_ - float(stagedN_);
    stagedEta_ = (1.0f - d) / (1.0f + d);

    stagedLowpass_.setLowpass(transitionHz, 0.7071f, sampleRate_);
    stagedHighpass_.setHighpass(hiXover() * transitionHz, 0.7071f, sampleRate_); // today kHighPassRatio
    // Butterworth LPF group delay well below its cutoff ≈ sqrt(2) / (2 pi fC).
    lpfDelay_ = 1.41421356f * sampleRate_ / (2.0f * map::kPi * transitionHz);

    for (size_t j = 0; j < kDesignFcRatios.size(); ++j) {
        const float hz = kDesignFcRatios[j] * transitionHz;
        ptCos_[kNumDesignHz + j]     = std::cos(2.0f * map::kPi * hz / sampleRate_);
        ptLatency_[kNumDesignHz + j] = dsp::LoopSat::latencySamples(hz, sampleRate_);
        ptCosK_[kNumDesignHz + j]    = map::stretchedAllpassCos(stagedK_, hz, sampleRate_);
    }
    for (size_t k = 0; k < kDesignHz.size(); ++k) ptCosK_[k] = map::stretchedAllpassCos(stagedK_, kDesignHz[k], sampleRate_);
    designFc_ = transitionHz;
}

#if RV_TANKV_BUILT >= 8
namespace {
// Group delay (samples) of a biquad at w (cos w): the numerator's less the
// denominator's, each Re[(c1 e^-jw + 2 c2 e^-2jw) / (c0 + c1 e^-jw + c2 e^-2jw)].
float polyDelay(float c0, float c1, float c2, float cw, float sw)
{
    const float c2w = 2.0f * cw * cw - 1.0f, s2w = 2.0f * sw * cw;
    const float pr = c0 + c1 * cw + c2 * c2w, pi = -(c1 * sw + c2 * s2w);
    const float qr = c1 * cw + 2.0f * c2 * c2w, qi = -(c1 * sw + 2.0f * c2 * s2w);
    return (qr * pr + qi * pi) / std::max(pr * pr + pi * pi, 1.0e-20f);
}
float biquadDelay(const dsp::Biquad& b, float cw)
{
    const float sw = std::sqrt(std::fmax(0.0f, 1.0f - cw * cw));
    return polyDelay(b.b0, b.b1, b.b2, cw, sw) - polyDelay(1.0f, b.a1, b.a2, cw, sw);
}
// The round-5 damping biquad: an RBJ peaking cut eased toward the one-pole
// low-pass y += c (x - y) by u (coefficients blended: the stable region of
// (a1, a2) is convex, so every blend is stable; u 1 = that low-pass exactly).
void designLoopEq(dsp::Biquad& b, const SpringSettings& s, float dampingC, float sampleRate)
{
    const float A  = std::exp(s.eqDb * (2.302585093f / 40.0f));
    const float w  = 2.0f * map::kPi * std::min(s.eqHz, 0.45f * sampleRate) / sampleRate;
    const float al = std::sin(w) / (2.0f * s.eqQ), cw = std::cos(w);
    const float a0 = 1.0f + al / A;
    const float u  = std::clamp(s.eqToLp, 0.0f, 1.0f), v = 1.0f - u;
    b.b0 = v * (1.0f + al * A) / a0 + u * dampingC;
    b.b1 = v * (-2.0f * cw) / a0;
    b.b2 = v * (1.0f - al * A) / a0;
    b.a1 = v * (-2.0f * cw) / a0 - u * (1.0f - dampingC);
    b.a2 = v * (1.0f - al / A) / a0;
}
} // namespace
#endif

void Spring::prepareDamping(float dampingHz)
{
#if RV_TANKV_BUILT >= 8
    const SpringSettings& se = settings_;
    const bool eqSame = se.eqHz == designEq_[0] && se.eqDb == designEq_[1] && se.eqQ == designEq_[2] && se.eqToLp == designEq_[3];
    if (dampingHz == designDampHz_ && magFc_ == designFc_ && eqSame) return; // nothing moved
    designEq_[0] = se.eqHz, designEq_[1] = se.eqDb, designEq_[2] = se.eqQ, designEq_[3] = se.eqToLp;
#else
    if (dampingHz == designDampHz_ && magFc_ == designFc_) return; // neither moved
#endif
    if (dampingHz != designDampHz_) {
        stagedDamping_.setCutoff(std::min(dampingHz, 0.45f * sampleRate_), sampleRate_);
        designDampHz_ = dampingHz;
    }
#if RV_TANKV_BUILT >= 8
    stagedEqOn_ = se.eqHz > 0.0f;
    if (kR5Only || stagedEqOn_) designLoopEq(stagedBq_, se, stagedDamping_.c, sampleRate_);
#endif
    // Loop magnitude per trip, excluding g (as loopMagnitudeAt, with the
    // staged filters), and the damping's group delay, at every point.
    float maxMag = 0.0f;
    for (int p = 0; p < kNumPoints; ++p) {
        const float cw = ptCos_[size_t(p)];
#if RV_TANKV_BUILT >= 8
        if (kR5Only || stagedEqOn_) {
            ptDampDelay_[size_t(p)] = biquadDelay(stagedBq_, cw);
            ptMag_[size_t(p)] = std::sqrt(dc_.magnitudeSquared(cw) * stagedLowpass_.magnitudeSquared(cw)
                                          * stagedBq_.magnitudeSquared(cw));
            if (p < kNumDesignHz) maxMag = std::max(maxMag, ptMag_[size_t(p)]);
            continue;
        }
#endif
        ptDampDelay_[size_t(p)] = stagedDamping_.groupDelay(cw);
        ptMag_[size_t(p)] = std::sqrt(dc_.magnitudeSquared(cw) * stagedLowpass_.magnitudeSquared(cw)
                                      * stagedDamping_.magnitudeSquared(cw));
        if (p < kNumDesignHz) maxMag = std::max(maxMag, ptMag_[size_t(p)]);
    }
    maxMag_ = maxMag;
    magFc_  = designFc_;
}

namespace {
void copyCoefficients(dsp::Biquad& to, const dsp::Biquad& from) // keeps the running state
{
    to.b0 = from.b0;
    to.b1 = from.b1;
    to.b2 = from.b2;
    to.a1 = from.a1;
    to.a2 = from.a2;
}
} // namespace

void Spring::commitDesign()
{
    const SpringSettings& s = settings_;

    k_   = stagedK_;
    n_   = stagedN_;
    eta_ = stagedEta_;
    a_   = s.allpassCoeff;
    copyCoefficients(chirpLowpass_, stagedLowpass_);
    copyCoefficients(highpass_, stagedHighpass_);
    damping_.c = stagedDamping_.c;
#if RV_TANKV_BUILT >= 8
    if (stagedEqOn_ && !eqOn_) dampBq_.reset(); // into the biquad: from rest
    eqOn_ = stagedEqOn_;
    copyCoefficients(dampBq_, stagedBq_);
#endif
    highPathLevel_ = s.highPathLevel;
    tapRatio_      = std::clamp(s.tapRatio, 0.05f, 0.95f);
    tapOffsetTarget_ = s.tapOffsetSeconds * sampleRate_; // glides in advanceGlides()

    // Loop gain g from the target T60 and the *actual* round trip. A tail
    // loses 60 dB in T60 seconds; one trip takes RT seconds, so each trip may
    // lose 60 * RT / T60 dB in total. The filters already take |H(f)| of that,
    // g supplies the rest. Take the smallest g over the design points
    // (kDesignHz and kDesignFcRatios) so the slowest band hits T60 and no
    // band rings longer. Round trip = L + chain + damping + fC low-pass +
    // LoopSat oversampler latency (as roundTripSamples()).
    const float t60 = kT60DesignScale * s.t60Seconds;
    float g = kMaxGain;
    // Hold zone (ADR 0040): the cap moves with the zone weight from kMaxGain
    // to the one that puts the peak g|H| at throwhold::kPeakGain (< 1).
    const float hold = std::clamp(s.hold, 0.0f, 1.0f);
    if (hold > 0.0f && maxMag_ > 0.0f) g = kMaxGain + hold * (throwhold::kPeakGain / maxMag_ - kMaxGain);
    for (size_t p = 0; p < size_t(kNumPoints); ++p) {
        const float chain = mPos_ * map::stretchedAllpassGroupDelayFromCos(a_, k_, ptCosK_[p]);
        const float rt    = withDiff(lCur_) + chain + ptDampDelay_[p] + lpfDelay_ + ptLatency_[p];
        const float gf    = std::exp(-3.0f * kLn10 * rt / (t60 * sampleRate_)) / ptMag_[p];
        g = std::min(g, gf);
    }
    g = std::max(0.0f, g);
    if (hold > 0.0f && maxMag_ > 0.0f) g = std::min(g, throwhold::kPeakGain / maxMag_); // never P >= 1 while held

    // Howl zone: lift the small-signal peak gain P = g·max|H| toward
    // kHowlPeakGain (> 1). The LoopSat's compression then holds the level.
    const float howl = std::clamp(s.howl, 0.0f, 1.0f);
    const float maxMag = maxMag_;
    if (howl > 0.0f && maxMag > 0.0f) {
        const float p0 = g * maxMag;
        const float p  = p0 + (drive::kHowlPeakGain - p0) * std::sqrt(howl);
        g = p / maxMag;
    }
    g_ = g;
    loopSat_.set(s.loopSatAmount * satGate_, s.loopSatKPos, s.loopSatKNeg); // quiet-tail fade kept on a redesign

    modDepth_ = std::max(0.0f, s.modDepth) * antires::kMicroModNorm;
    lfoDepth_ = std::max(0.0f, s.lfoDepth);
#if RV_TANKV_BUILT >= 8
    if (kR5Only || fbDiffPre_) {
        // Round 5: the modulation moves L, and the round trip is L plus the
        // diffusers (8 ms, longer than voicing 3's): scaled up so the round
        // trip moves as much as before (the Howl's movement read 0.45-0.50 %,
        // ADR 0019's bar 0.5; 7 0.67-0.79).
        const float sc = withDiff(lTarget_) / std::max(lTarget_, 1.0f);
        modDepth_ *= sc;
        lfoDepth_ *= sc;
    }
#endif
    if (s.lfoHz != lfoHzSet_) { // fixed per Spring: one sin, not one per redesign
        lfoE_     = 2.0f * std::sin(map::kPi * std::max(0.0f, s.lfoHz) / sampleRate_); // magic-circle step
        lfoHzSet_ = s.lfoHz;
    }

    // High path: no dispersion to speak of, simple T60 from its own trip.
    lhCur_ = kHighDelayRatio * withDiff(lCur_);
    const float hiBase = s.highT60Seconds > 0.0f ? s.highT60Seconds : s.t60Seconds; // the Hold keeps the plain DECAY's
    gHigh_ = std::min(kMaxGain, std::exp(-3.0f * kLn10 * lhCur_ / (hiT60() * hiBase * sampleRate_)));

    // Tank voicing 1 (setHighPathVoicing): the high path's pickup lines its
    // first echo up with the Loop's at the crossover: the Loop's pickup plus
    // what its first echo passes (Chirp chain, fC low-pass, damping), minus
    // the high path's own allpasses and ceiling, plus a trim.
#if RV_TANKV_BUILT >= 1
    hiPick_ = -1.0f;
    if (hiAlignOn_) {
        const float fx  = hiXover_ * s.transitionHz;
        const float cw  = std::cos(2.0f * map::kPi * fx / sampleRate_);
#if RV_TANKV_BUILT >= 8
        const float dampDelay = (kR5Only || eqOn_) ? biquadDelay(dampBq_, cw) : damping_.groupDelay(cw);
#else
        const float dampDelay = damping_.groupDelay(cw);
#endif
        // (8+: the diffusers in front of the pickup delay the first echo by
        // about their delay, the pickup reads pickShift() earlier.)
#if RV_TANKV_BUILT >= 8
        const float loopArr = tapRatio_ * withDiff(lTarget_) + tapOffsetTarget_ - pickShift() + preDiffDelay()
#else
        const float loopArr = tapRatio_ * withDiff(lTarget_) + tapOffsetTarget_
#endif
                            + float(mTarget_) * map::stretchedAllpassGroupDelaySamples(a_, k_, fx, sampleRate_) + lpfDelay_
                            + dampDelay;
        const float highOwn = float(kHighStages) * map::stretchedAllpassGroupDelaySamples(kHighAllpassCoeff, 1.0f, fx, sampleRate_)
                            + highCeiling_.groupDelay(cw);
        hiPick_ = std::max(2.0f, loopArr - highOwn + hiAlignMs_ * 0.001f * sampleRate_);
    }
#endif
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
    // + the feedback diffusers' delay (Tank voicing 3; their average group
    // delay is their delay, and L was shortened by it).
    return withDiff(lCur_) + chainGroupDelaySamples(freqHz) + damping_.groupDelay(cw) + lpfDelay + loopSatLatency;
}

#ifndef RV_FIXED_VOICINGS // analysis only: not in the firmware (Drive.h)
float Spring::firstEchoSamples(float freqHz) const
{
    const float cw = std::cos(2.0f * map::kPi * freqHz / sampleRate_);
    const float lpfDelay = 1.41421356f * sampleRate_ / (2.0f * map::kPi * settings_.transitionHz);
    return chainGroupDelaySamples(freqHz) + damping_.groupDelay(cw) + lpfDelay + tapRatio_ * lCur_ + tapOffset_
         - pickShift() + preDiffDelay();
}
#endif

float Spring::loopMagnitude(float freqHz) const
{
    return loopMagnitudeAt(std::cos(2.0f * map::kPi * freqHz / sampleRate_));
}

float Spring::loopMagnitudeAt(float cw) const
{
#if RV_TANKV_BUILT >= 8
    if (kR5Only || eqOn_)
        return std::sqrt(dc_.magnitudeSquared(cw) * chirpLowpass_.magnitudeSquared(cw) * dampBq_.magnitudeSquared(cw));
#endif
    return std::sqrt(dc_.magnitudeSquared(cw) * chirpLowpass_.magnitudeSquared(cw) * damping_.magnitudeSquared(cw));
}

float Spring::t60AtSeconds(float freqHz) const
{
    const float perTrip = g_ * loopMagnitude(freqHz);
    if (perTrip <= 0.0f) return 0.0f;
    return -3.0f * roundTripSamples(freqHz) / (sampleRate_ * std::log10(perTrip));
}

RV_SIZE_OPT void Spring::setDiffusion(float* const* bufs, const int* sizes, const float* delays, int n, float c)
{
#if RV_TANKV_BUILT >= 3
    numFbDiff_   = std::clamp(n, 0, kMaxFbDiffusers);
    fbDiffC_     = c;
    fbDiffDelay_ = 0.0f;
    for (int k = 0; k < numFbDiff_; ++k) {
        FbDiffuser& f = fbDiff_[size_t(k)];
        f.buf  = bufs[k];
        f.size = sizes[k];
        f.d    = std::clamp(int(delays[k] + 0.5f), 1, sizes[k] - 1);
        f.w    = 0;
        fbDiffDelay_ += float(f.d);
    }
    reset();
    setSettings(settings_, true); // L, the pickup and g with the new round trip
#else
    (void)bufs, (void)sizes, (void)delays, (void)n, (void)c; // the firmware without voicing 3
#endif
}

void Spring::clearStage(int j)
{
    // Rings are section-interleaved (chirpSections below): stage j is
    // every maxStages_-th float.
    const size_t S = size_t(maxStages_);
    for (size_t p = 0; p <= size_t(ringMask_); ++p) rings_[p * S + size_t(j)] = 0.0f;
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
    lhCur_ = kHighDelayRatio * withDiff(lCur_); // the full round trip (voicing 3 shortens L)

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

namespace {
// The Chirp's spectral delay filter for one sample: M stretched allpass
// sections (dsp/StretchedChain.h: the rings section-interleaved, row p
// (ring position), section j at rings[p * S + j]; pipelined for the M7).
inline float chirpSections(float x, float* rings, size_t S, int iw, int ir0, int ir1, float* thiranY1, int full,
                           float frac, int maxStages, float a, float eta)
{
    return dsp::stretchedChain(x, rings + size_t(ir0) * S, rings + size_t(ir1) * S, rings + size_t(iw) * S, thiranY1,
                               full, frac, maxStages, a, eta);
}
} // namespace

inline float Spring::processLow(float in, float lMod, float tapMod)
{
    const float fb  = readLow(lMod);
    // Pickup ~half way: first echo after ~half a round trip (+ the fixed
    // stagger and WOBBLE's transport).
    // (Tank voicing 3: along the full round trip, L + the feedback diffusers.)
    float tapAt = tapRatio_ * withDiff(lMod) + tapOffset_ + tapMod - pickShift();
    tapAt = tapAt < 2.0f ? 2.0f : (tapAt > lMod ? lMod : tapAt);
    const float tap = readLow(tapAt);

    // Tank voicing 3: the feedback diffusers (after the pickup: every trip
    // smears the echo a little more; the first echo never passes them).
    // Voicing 8+ runs them in front of the pickup instead (below).
    float fbd = fb;
#if RV_TANKV_BUILT >= 8
    if (kR5Only || fbDiffPre_) {
    } else
#endif
#if RV_TANKV_BUILT >= 3
    if (numFbDiff_ == 3) {
        // The three (voicing 3+ always has three) written out, each one's
        // delayed sample read before the chain starts: the reads don't wait
        // on x, so on the M7 only the three multiply-adds stay in line
        // (perf/run16). The same steps as FbDiffuser::process, in order.
        FbDiffuser& f0 = fbDiff_[0];
        FbDiffuser& f1 = fbDiff_[1];
        FbDiffuser& f2 = fbDiff_[2];
        int r0 = f0.w - f0.d, r1 = f1.w - f1.d, r2 = f2.w - f2.d;
        if (r0 < 0) r0 += f0.size;
        if (r1 < 0) r1 += f1.size;
        if (r2 < 0) r2 += f2.size;
        const float z0 = f0.buf[r0], z1 = f1.buf[r1], z2 = f2.buf[r2];
        const float c  = fbDiffC_;
        float v = fbd - c * z0;
        f0.buf[f0.w] = v;
        fbd = c * v + z0;
        v = fbd - c * z1;
        f1.buf[f1.w] = v;
        fbd = c * v + z1;
        v = fbd - c * z2;
        f2.buf[f2.w] = v;
        fbd = c * v + z2;
        if (++f0.w == f0.size) f0.w = 0;
        if (++f1.w == f1.size) f1.w = 0;
        if (++f2.w == f2.size) f2.w = 0;
    } else {
        for (int k = 0; k < numFbDiff_; ++k) fbd = fbDiff_[size_t(k)].process(fbd, fbDiffC_);
    }
#endif

    float x = dc_.process(in + g_ * loopSat_.process(fbd));

    // Spectral delay filter (chirpSections).
    const int   full = int(mPos_);
    const float frac = mPos_ - float(full);
    const int   iw   = ringW_;
    x = chirpSections(x, rings_, size_t(maxStages_), iw, (iw - n_) & ringMask_, (iw - n_ - 1) & ringMask_,
                      thiranY1_.data(), full, frac, maxStages_, a_, eta_);
    ringW_ = (iw + 1) & ringMask_;

    x = chirpLowpass_.process(x);
#if RV_TANKV_BUILT >= 8
    x = dampAndDiffuse(x);
#else
    x = damping_.process(x);
#endif

    lowBuf_[lowW_] = x;
    if (++lowW_ == lowSize_) lowW_ = 0;
    return tap;
}

#if RV_TANKV_BUILT >= 8
// Voicing 8+: the damping (the round-5 biquad when on), then, if they sit
// in front of the pickup, the three diffusers (as processLow's, in order).
inline float Spring::dampAndDiffuse(float x)
{
    x = (kR5Only || eqOn_) ? dampBq_.process(x) : damping_.process(x);
    if ((kR5Only || fbDiffPre_) && numFbDiff_ == 3) {
        FbDiffuser& f0 = fbDiff_[0];
        FbDiffuser& f1 = fbDiff_[1];
        FbDiffuser& f2 = fbDiff_[2];
        int r0 = f0.w - f0.d, r1 = f1.w - f1.d, r2 = f2.w - f2.d;
        if (r0 < 0) r0 += f0.size;
        if (r1 < 0) r1 += f1.size;
        if (r2 < 0) r2 += f2.size;
        const float z0 = f0.buf[r0], z1 = f1.buf[r1], z2 = f2.buf[r2];
        const float c  = fbDiffC_;
        float v = x - c * z0;
        f0.buf[f0.w] = v;
        x = c * v + z0;
        v = x - c * z1;
        f1.buf[f1.w] = v;
        x = c * v + z1;
        v = x - c * z2;
        f2.buf[f2.w] = v;
        x = c * v + z2;
        if (++f0.w == f0.size) f0.w = 0;
        if (++f1.w == f1.size) f1.w = 0;
        if (++f2.w == f2.size) f2.w = 0;
    }
    return x;
}
#endif

// The Loop after its input sum, for the coupled Loops (coupledFinish): the
// same arithmetic as processLow's, kept as a copy so the firmware's hot loop
// (processLow, which never sees the coupling) compiles exactly as before.
void Spring::loopWrite(float x)
{
    // Spectral delay filter (chirpSections).
    const int   full = int(mPos_);
    const float frac = mPos_ - float(full);
    const int   iw   = ringW_;
    x = chirpSections(x, rings_, size_t(maxStages_), iw, (iw - n_) & ringMask_, (iw - n_ - 1) & ringMask_,
                      thiranY1_.data(), full, frac, maxStages_, a_, eta_);
    ringW_ = (iw + 1) & ringMask_;

    x = chirpLowpass_.process(x);
#if RV_TANKV_BUILT >= 8
    x = dampAndDiffuse(x);
#else
    x = damping_.process(x);
#endif

    lowBuf_[lowW_] = x;
    if (++lowW_ == lowSize_) lowW_ = 0;
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
#if RV_TANKV_BUILT >= 1
    const float pm = hiPick_ >= 0.0f ? std::min(hiPick_, lhMod) : kHighPickup * lhMod; // voicing 1: aligned
#else
    const float pm = kHighPickup * lhMod;
#endif
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

float Spring::coupledReturn(float lFrac, float lSamples, float tapSamples)
{
    // As process(), one sample, up to the Loop's input sum.
    advanceGlides();
    cNoise_         = kDenormalNoise * rng_.bipolar();
    const float mod = advanceModulation() + lFrac;
    float lMod      = lCur_ * mod + lSamples;
    const float lMax = float(lowSize_ - 2);
    lMod = lMod < 2.0f ? 2.0f : (lMod > lMax ? lMax : lMod);
    const float fb = readLow(lMod);
    // (Tank voicing 3: the pickup along the full round trip, and the
    // feedback diffusers, as processLow.)
    float tapAt = tapRatio_ * withDiff(lMod) + tapOffset_ + tapSamples - pickShift();
    tapAt = tapAt < 2.0f ? 2.0f : (tapAt > lMod ? lMod : tapAt);
    cTap_ = readLow(tapAt);
    float fbd = fb;
#if RV_TANKV_BUILT >= 3
    if (preDiffDelay() == 0.0f) // 8+ in front of the pickup: loopWrite runs them
        for (int k = 0; k < numFbDiff_; ++k) fbd = fbDiff_[size_t(k)].process(fbd, fbDiffC_);
#endif
    return g_ * loopSat_.process(fbd);
}

float Spring::coupledFinish(float in, float highIn, float loopReturn)
{
    const float x = in + cNoise_;
    loopWrite(dc_.process(x + loopReturn));
    const float high = processHigh(highIn + cNoise_, lhCur_);
    return cTap_ + highPathLevel_ * high;
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
