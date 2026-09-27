#include "dsp/Tank.h"

#include "params/Mappings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace rv {

namespace {

// Decorrelator for R when only one Spring plays (SPEC §4.3): two short
// allpasses at unrelated lengths. Same level and tail as L, different phase.
constexpr std::array<float, 2> kDiffuserSeconds{{0.0023f, 0.0037f}};

int diffuserSize(float sampleRate, size_t i) { return int(std::ceil(kDiffuserSeconds[i] * sampleRate)) + 1; }

// Per-Spring noise seeds: fixed, so every run is reproducible.
constexpr std::array<uint32_t, Tank::kMaxSprings> kSeeds{{0x9E3779B9u, 0x7F4A7C15u, 0x2545F491u}};

} // namespace

// The Tank object itself must stay small: the Firmware keeps it as a global
// in DTCM (128 KB). Big buffers live in the pool.
static_assert(sizeof(Spring) < 2048, "Spring object grew: move state into the pool");

Tank::~Tank() { releaseOwnedPool(); }

void Tank::releaseOwnedPool()
{
    std::free(ownedPool_);
    ownedPool_   = nullptr;
    ownedFloats_ = 0;
}

size_t Tank::requiredPoolFloats(float sampleRate)
{
    size_t n = size_t(kMaxSprings) * Spring::requiredFloats(sampleRate);
    for (size_t i = 0; i < kDiffuserSeconds.size(); ++i) n += size_t(diffuserSize(sampleRate, i));
    return n;
}

void Tank::prepare(float sampleRate, int maxBlockSize)
{
    const size_t need = requiredPoolFloats(sampleRate);
    if (need > ownedFloats_) {
        releaseOwnedPool();
        // malloc (not new/vector): no exceptions on the Firmware, returns null on failure.
        ownedPool_ = static_cast<float*>(std::malloc(need * sizeof(float)));
        if (ownedPool_) ownedFloats_ = need;
    }
    prepare(sampleRate, maxBlockSize, ownedPool_, ownedFloats_);
}

void Tank::prepare(float sampleRate, int maxBlockSize, float* pool, size_t poolFloats)
{
    sampleRate_   = sampleRate;
    maxBlockSize_ = maxBlockSize;
    for (const auto& p : kParams) {
        const size_t i = static_cast<size_t>(p.id);
        values_[i]     = p.defaultValue;
        // One-pole smoothing applied once per control tick (ADR 0015 times).
        tickCoeff_[i] = 1.0f - std::exp(-1000.0f * float(kControlInterval) / (p.smoothingMs * sampleRate));
    }
    mix_.setTime(spec(ParamId::Mix).smoothingMs, sampleRate);
    for (auto& s : shelfSplit_) s.setCutoff(kShelfHz, sampleRate);
    limitRelease_ = std::exp(-1.0f / (kLimitReleaseS * sampleRate));

    ok_ = pool != nullptr && poolFloats >= requiredPoolFloats(sampleRate);
    pool_       = ok_ ? pool : nullptr;
    poolFloats_ = ok_ ? requiredPoolFloats(sampleRate) : 0;
    if (ok_) bindPool(pool);
    reset();
}

void Tank::bindPool(float* pool)
{
    float* p = pool;
    for (size_t s = 0; s < springs_.size(); ++s) {
        springs_[s].prepare(sampleRate_, p, kSeeds[s]);
        p += Spring::requiredFloats(sampleRate_);
    }
    for (size_t i = 0; i < decorrelator_.size(); ++i) {
        decorrelator_[i].buf  = p;
        decorrelator_[i].size = diffuserSize(sampleRate_, i);
        p += decorrelator_[i].size;
    }
}

void Tank::reset()
{
    if (!ok_) return;
    for (auto& s : springs_) s.reset();
    for (auto& d : decorrelator_) {
        std::fill(d.buf, d.buf + d.size, 0.0f);
        d.w = 0;
    }
    for (auto& s : shelfSplit_) s.reset();
    limitEnv_ = 0.0f;
    tick_     = 0;
    primed_   = false;
}

void Tank::controlTick(bool snap)
{
    for (const auto& p : kParams) {
        const size_t i = static_cast<size_t>(p.id);
        if (snap || p.kind != ParamKind::Knob) smoothed_[i] = values_[i];
        else smoothed_[i] += tickCoeff_[i] * (values_[i] - smoothed_[i]);
    }
    const float decay = smoothed_[size_t(ParamId::Decay)];
    const float boing = smoothed_[size_t(ParamId::Boing)];
    const float tone  = smoothed_[size_t(ParamId::Tone)];

    SpringSettings s;
    s.loopDelaySeconds = map::decayLoopDelaySeconds(decay);
    s.t60Seconds       = map::decayT60Seconds(decay);
    s.transitionHz     = map::decayTransitionHz(decay);
    s.allpassCoeff     = map::boingCoefficient(boing);
    s.stages           = map::boingStages(boing);
    s.dampingHz        = map::toneDampingHz(tone);
    s.highPathLevel    = map::toneHighPathLevel(tone);
    for (int i = 0; i < activeSprings_; ++i) springs_[size_t(i)].setSettings(s, snap);
}

void Tank::process(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    if (!ok_) { // no memory: stay a clean passthrough rather than fail silently into noise
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        return;
    }
    if (!primed_) {
        controlTick(true);
        mix_.value = values_[size_t(ParamId::Mix)];
        primed_    = true;
    }

    float mono[kControlInterval];
    float wet[kControlInterval];
    int pos = 0;
    while (pos < numSamples) {
        if (tick_ == 0) controlTick(false); // fixed grid, independent of block size
        const int n = std::min(numSamples - pos, kControlInterval - tick_);

        // Real tanks are mono: sum the input (SPEC §4.3). Dry stays stereo.
        for (int i = 0; i < n; ++i) mono[i] = 0.5f * (inL[pos + i] + inR[pos + i]);
        springs_[0].process(mono, wet, n);

        for (int i = 0; i < n; ++i) {
            const float dryL = inL[pos + i], dryR = inR[pos + i]; // read before write: in may alias out
            float wl = kWetGain * wet[i];
            float wr = decorrelator_[1].process(decorrelator_[0].process(wl));

            // Gentle high-shelf cut: keep the part below kShelfHz, scale the rest.
            const float ll = shelfSplit_[0].process(wl), lr = shelfSplit_[1].process(wr);
            wl = ll + kShelfGain * (wl - ll);
            wr = lr + kShelfGain * (wr - lr);

            // Safety limiter (stereo-linked, instant attack): the envelope is
            // never below the current peak, so |out| <= threshold always.
            const float peak = std::max(std::fabs(wl), std::fabs(wr));
            limitEnv_ = std::max(peak, limitEnv_ * limitRelease_);
            const float gain = limitEnv_ > kLimitThreshold ? kLimitThreshold / limitEnv_ : 1.0f;
            wl *= gain;
            wr *= gain;

            const map::MixGains m = map::mixGains(mix_.process(values_[size_t(ParamId::Mix)]));
            outL[pos + i] = m.dry * dryL + m.wet * wl;
            outR[pos + i] = m.dry * dryR + m.wet * wr;
        }
        pos += n;
        tick_ = (tick_ + n) % kControlInterval;
    }
}

} // namespace rv
