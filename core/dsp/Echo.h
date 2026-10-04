#pragma once
// The tape echo of SPRINGS 3's echo mode (ADR 0041; numbers and the musical
// story in params/EchoVoicing.h).
//
// One delay line (the tape), in memory the host hands over (Tank::prepare:
// the Renderer and Plugin malloc it, the firmware passes a buffer in AXI
// SRAM). The Tank drives it on its 32-sample control grid: tick() once per
// grid step (echo time, WOBBLE), then for the samples up to the next step
// play() (the playback head: the tape read at the echo time plus WOBBLE's
// offset, linear interpolation, then the head's high-pass and low-pass) and
// record() (the record head: a low-pass, softClip saturation, write, advance). play()
// runs before record() in a step and the delay is never shorter than two
// steps, so a step's reads never meet its own writes.
//
// The echo time glides (echo::kTimeGlideSeconds, one-pole on the delay, at
// most echo::kMaxSlew samples per sample) and is ramped per sample between
// grid steps, so a change bends the repeats' pitch, never clicks.
// Block-size independent: everything advances per sample or per grid step.

#include "dsp/Drive.h"
#include "dsp/Filters.h"
#include "dsp/Wobble.h"
#include "params/EchoVoicing.h"

#include <cstddef>
#include <cstdint>

namespace rv::dsp {

class TapeEcho {
public:
    static constexpr int kGrid = 32; // the Tank's control interval (static_assert in Tank.cpp)

    // Heads, WOBBLE, and the tape (tapeFloats of memory, cleared here; null
    // or too short for two grid steps = no tape: the echo plays silence).
    void prepare(float sampleRate, uint32_t seed, float* tape, size_t tapeFloats);
    void reset();     // tape, heads and glide cleared; WOBBLE restarted
    void clearTape(); // a fresh tape (real-time safe) and the heads: when echo mode comes in

    // Control grid: the echo time it aims for (seconds), WOBBLE (Normalised,
    // bipolar), snap = jump there (first tick).
    void tick(float seconds, float wobble, bool snap);
    // The delay the glide has reached (samples, at this grid step), for tests.
    float delaySamples() const { return dTo_; }
    // Longest echo time this tape holds (seconds).
    float maxSeconds() const { return maxD_ / sampleRate_; }

    // n <= kGrid - (samples since tick): the playback head for the next n
    // samples (filtered), not advancing the tape.
    void play(float* out, int n);
    // The record head: writes softClip(k LP(x)) / k for each of n samples and
    // advances the tape.
    void record(const float* in, int n);

    bool ok() const { return buf_ != nullptr; }
    const Wobble& wow() const { return wow_; }

private:
    float* buf_  = nullptr;
    int    size_ = 0, w_ = 0, filled_ = 0; // filled_: samples recorded since the tape was last made fresh
    float  sampleRate_ = 48000.0f;
    float  dFrom_ = 0.0f, dTo_ = 0.0f, glide_ = 0.0f, maxStep_ = 0.0f, minDelay_ = 64.0f, maxD_ = 0.0f, readMax_ = 0.0f;
    int    k_ = 0; // samples since tick()
    float  floorDepth_ = 0.0f, floorStep1_ = 0.0f, floorStep2_ = 0.0f; // the tape's own wander (kFloorMs)
    float  ph1_ = 0.0f, ph2_ = 0.0f, floorFrom_ = 0.0f, floorTo_ = 0.0f;
    Biquad         lp_{}, recLp_{}; // playback head, record head
    OnePoleLowpass hp_{};
    Wobble         wow_;
};

} // namespace rv::dsp
