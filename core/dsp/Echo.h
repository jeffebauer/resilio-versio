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
#include "dsp/EchoWear.h"
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

    // Diffuse repeats (EchoVoicing.h kDiffuse): 0 none ... 3 heavy. Clears
    // the diffuser. diffuse() smears the feedback (in place) before it goes
    // back on the tape; a no-op at voicing 0.
    void setDiffuseVoicing(int v);
    int  diffuseVoicing() const { return diffuse_; }
    void diffuse(float* x, int n);

    // Wear (EchoVoicing.h "Wear", dsp/EchoWear.h): 0 none ... 4 crushed.
    void setWearVoicing(int v) { wear_.setVoicing(v); }
    void setBbdVoicing(int v) { wear_.setBbdVoicing(v); }
    int  bbdVoicing() const { return wear_.bbdVoicing(); }
    float bbdClockHz() const { return wear_.bbdClockHz(); }
    int  wearVoicing() const { return wear_.voicing(); }
    bool wearActive() const { return wear_.active(); }
    // On the feedback (in place) / on the input (the wear's fixed delay).
    void wear(float* x, int n) { wear_.process(x, n); }
    void delayInput(float* x, int n) { wear_.delayInput(x, n); }

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
    // The diffuser (only built where a voicing can use it: EchoVoicing.h kDiffuseBuilt).
    static constexpr int kApSize = echo::kDiffuseBuilt ? 2048 : 1; // >= kDiffuseMaxMs at 96 kHz + the drift
    struct Allpass {
        float buf[kApSize];
        int   w = 0;
        float base = 1.0f, depth = 0.0f, ph = 0.0f, step = 0.0f;
    };
    Allpass ap_[echo::kDiffuseStages];
    int     diffuse_ = echo::kDiffuseDefault;
    TapeWear wear_;
    void    clearDiffuser();
    OnePoleLowpass hp_{};
    Wobble         wow_;
};

} // namespace rv::dsp
