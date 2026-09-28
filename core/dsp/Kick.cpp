#include "dsp/Kick.h"

#include <cmath>

namespace rv::dsp {

namespace {

// sin(2π p) for p in [0, 1): parabola with one refinement step (max error
// ~0.1 %, harmonics below −56 dB). No libm per sample.
inline float fastSin01(float p)
{
    const float x = 2.0f * p - 1.0f;          // [-1, 1): sin(2πp) = -sin(πx)
    const float y = 4.0f * x * (1.0f - (x < 0.0f ? -x : x));
    return -(y * (0.775f + 0.225f * (y < 0.0f ? -y : y)));
}

} // namespace

void KickVoice::prepare(float sampleRate, uint32_t seed)
{
    sampleRate_   = sampleRate;
    seed_         = mixSeed(seed);
    mergeSamples_ = int(splash::kKickMergeMs * 0.001f * sampleRate);
    idleSamples_  = int(0.05f * sampleRate);
    for (auto& h : loopHp_) h.setHighpass(splash::kKickLoopHpHz, 0.707f, sampleRate);
    burstLp_.setCutoff(splash::kBurstHpHz, sampleRate);
    setAttitude({{0.0f, 1.0f, 0.0f}});
    reset();
}

void KickVoice::reset()
{
    for (auto& v : voices_) v = Voice{};
    nextVoice_ = 0;
    for (auto& h : loopHp_) h.reset();
    burstLp_.reset();
    rng_.seed(seed_);
    numPending_ = 0;
    quiet_      = idleSamples_;
    sinceLast_  = 1 << 30;
    joltAt_     = -1;
    started_    = 0;
}

void KickVoice::setAttitude(const std::array<float, 3>& weights) { params_ = splash::blendKick(weights); }

void KickVoice::trigger(int sampleOffset)
{
    if (numPending_ < kMaxPending) pending_[size_t(numPending_++)] = sampleOffset < 0 ? 0 : sampleOffset;
}

void KickVoice::start(Voice& v)
{
    const float fs = sampleRate_;
    v.active     = true;
    v.phase      = 0.0f; // sine starts from 0: no step at the onset
    v.fEnd       = params_.thumpEndHz / fs;
    v.glide      = (params_.thumpStartHz - params_.thumpEndHz) / fs;
    v.glideCoeff = std::exp(-1000.0f / (splash::kThumpGlideMs * fs));
    v.freq       = v.fEnd + v.glide;
    v.thumpEnv   = params_.thumpGain;
    v.thumpDecay = std::exp(-1000.0f / (params_.thumpDecayMs * fs));
    v.burstEnv   = params_.burstGain;
    v.burstDecay = std::exp(-1000.0f / (params_.burstDecayMs * fs));
}

void KickVoice::process(float* loopOut, float* directOut, int n)
{
    joltAt_ = -1;
    // Idle (the usual case): nothing pending, no voice, filters settled and
    // cleared. Silence without running the filters. The idle state is
    // entered on a fixed sample (idleSamples_ after the last voice ended),
    // so this shortcut never depends on the block size.
    if (numPending_ == 0 && quiet_ >= idleSamples_) {
        for (int i = 0; i < n; ++i) loopOut[i] = directOut[i] = 0.0f;
        sinceLast_ = sinceLast_ + n < (1 << 30) ? sinceLast_ + n : (1 << 30);
        return;
    }
    for (int i = 0; i < n; ++i) {
        // Onsets on their exact sample.
        for (int k = 0; k < numPending_; ++k) {
            const int at = pending_[size_t(k)] < n ? pending_[size_t(k)] : n - 1;
            if (at != i || sinceLast_ < mergeSamples_) continue;
            start(voices_[size_t(nextVoice_)]);
            nextVoice_ = (nextVoice_ + 1) % kVoices;
            sinceLast_ = 0;
            ++started_;
            if (joltAt_ < 0) joltAt_ = i;
        }
        if (sinceLast_ < (1 << 30)) ++sinceLast_;

        float thump = 0.0f, burst = 0.0f;
        for (auto& v : voices_) {
            if (!v.active) continue;
            // Phase first: the onset sample is sin(2π f/fs), tiny but not 0,
            // so both outputs start on the Kick's exact sample.
            v.phase += v.freq;
            if (v.phase >= 1.0f) v.phase -= 1.0f;
            thump += v.thumpEnv * fastSin01(v.phase);
            burst += v.burstEnv;
            v.glide *= v.glideCoeff;
            v.freq = v.fEnd + v.glide;
            v.thumpEnv *= v.thumpDecay;
            v.burstEnv *= v.burstDecay;
            if (v.burstEnv < 1.0e-7f) v.burstEnv = 0.0f; // done after ~50 ms, never denormal
            if (v.thumpEnv < 1.0e-6f) v.active = false; // burst is far shorter: gone long before
        }
        // Burst = broadband noise, its own lows removed (x − LP(x)).
        // Noise only while a burst sounds: the Rng advances on the same samples for any block size.
        const float nz = burst != 0.0f ? burst * rng_.bipolar() : 0.0f;
        const float b  = nz - burstLp_.process(nz);
        loopOut[i]   = loopHp_[1].process(loopHp_[0].process(thump + b));
        directOut[i] = thump;
        if (voices_[0].active || voices_[1].active) {
            quiet_ = 0;
        } else if (quiet_ < idleSamples_ && ++quiet_ == idleSamples_) {
            // 50 ms after the thump died (< -120 dB) the high-pass tail is
            // far below -180 dB: clear it exactly.
            for (auto& h : loopHp_) h.reset();
            burstLp_.reset();
        }
    }
    numPending_ = 0;
}

} // namespace rv::dsp
