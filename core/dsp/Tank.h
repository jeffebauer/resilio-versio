#pragma once
// The Tank: 1–3 Springs plus Tank-level stages (CONTEXT.md).
// M0: passthrough only. Proves Core builds and runs identically in every Host.

#include "params/ParamSpec.h"

#include <array>
#include <cstddef>

namespace rv {

class Tank {
public:
    void prepare(float sampleRate, int maxBlockSize)
    {
        sampleRate_   = sampleRate;
        maxBlockSize_ = maxBlockSize;
        for (const auto& p : kParams)
            values_[static_cast<size_t>(p.id)] = p.defaultValue;
    }

    void setParam(ParamId id, float normalised)
    {
        if (normalised < 0.0f) normalised = 0.0f;
        if (normalised > 1.0f) normalised = 1.0f;
        values_[static_cast<size_t>(id)] = normalised;
    }

    float param(ParamId id) const { return values_[static_cast<size_t>(id)]; }

    // Kick event at sample offset within the next process() block.
    void kick(int /*sampleOffset*/) {}

    void process(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
    }

    float sampleRate() const { return sampleRate_; }
    int   maxBlockSize() const { return maxBlockSize_; }

private:
    float sampleRate_   = 48000.0f;
    int   maxBlockSize_ = 48;
    std::array<float, static_cast<size_t>(ParamId::Count)> values_{};
};

} // namespace rv
