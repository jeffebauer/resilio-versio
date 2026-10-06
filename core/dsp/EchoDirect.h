#pragma once
// Echo mode's repeats heard directly, next to the springs (ADR 0041
// amendment "Springs blend", owner 6 Oct 2026; numbers in EchoVoicing.h
// "Springs blend"). The shipped voicing (2, C-wide) and every build: the
// wide heads. Renderer / Plugin only (not RV_FIXED_VOICINGS): the ping-pong
// prototype's second tape, allocated only when a ping-pong voicing is chosen.
//
// Wide (two playback heads, the image centred): the tape's (mono) repeats
// split at kWideSplitHz; the lows go to L and R identically (centred, no
// comb in mono down there). Above it, L plays the repeats a touch darker
// (a one-pole at kWideDarkHz) and R plays them kWideMs later, at full
// brightness: the earlier head pulls the image one way, the brighter one the
// other, and kWideRTrim sets R's highs so L and R are equally loud on average.
// Ping-pong: a second tape (tapeR) the Tank cross-feeds with its own
// (Tank.cpp): the input goes on L's tape, L's playback x the feedback on R's,
// R's on L's. tapeR runs the same heads, WOBBLE (same seed) and wear.

#include "dsp/Echo.h"
#include "dsp/Filters.h"
#include "params/EchoVoicing.h"

#include <cstddef>
#include <cstdint>
#ifndef RV_FIXED_VOICINGS
#include <cstdlib>
#endif

namespace rv::dsp {

class EchoDirect {
public:
    EchoDirect() = default;
    EchoDirect(const EchoDirect&)            = delete;
    EchoDirect& operator=(const EchoDirect&) = delete;
#ifndef RV_FIXED_VOICINGS
    ~EchoDirect() { std::free(tape_); }
#endif

    void prepare(float sampleRate, [[maybe_unused]] uint32_t seed)
    {
        delay_ = int(echo::kWideMs * 0.001f * sampleRate + 0.5f);
        if (delay_ > kLine - 1) delay_ = kLine - 1;
        split_.setCutoff(echo::kWideSplitHz, sampleRate);
        dark_.setCutoff(echo::kWideDarkHz, sampleRate);
#ifndef RV_FIXED_VOICINGS
        sampleRate_ = sampleRate;
        seed_       = seed;
        prepared_   = true;
        prepareTape();
#endif
        reset();
    }
    void reset()
    {
#ifndef RV_FIXED_VOICINGS
        tapeR.reset();
#endif
        for (float& v : line_) v = 0.0f;
        w_ = 0;
        split_.reset();
        dark_.reset();
    }
    void clearTape()
    {
#ifndef RV_FIXED_VOICINGS
        tapeR.clearTape();
#endif
    }

#ifdef RV_FIXED_VOICINGS
    static constexpr int  voicing() { return echo::kSpringsBlendDefault; }
    static constexpr bool pingPong() { return false; }
#else
    // A ping-pong voicing allocates its tape here (a Renderer / test hook: not real-time safe).
    void setVoicing(int v)
    {
        voicing_ = v < 0 ? 0 : (v < echo::kNumSpringsBlendVoicings ? v : echo::kNumSpringsBlendVoicings - 1);
        if (prepared_) {
            prepareTape();
            tapeR.reset();
        }
    }
    int  voicing() const { return voicing_; }
    bool pingPong() const { return echo::kSpringsBlend[voicing_].style == echo::DirectStyle::PingPong && tape_ != nullptr; }
#endif
    bool  active() const { return echo::kSpringsBlend[voicing()].style != echo::DirectStyle::None; }
    float springs() const { return echo::kSpringsBlend[voicing()].springs; }
    // The wet's make-up for this voicing, per ATTITUDE (linear; EchoVoicing.h kSpringsBlend).
    float makeup(int att) const { return echo::kSpringsBlend[voicing()].makeup[att]; }
    // ... and its slope with the feedback (dB per unit of feedback, around kFeedbackNoon).
    float makeupFbDb() const { return echo::kSpringsBlend[voicing()].makeupFbDb; }

    // Wide: the mono repeats e -> L, R (n samples).
    void widen(const float* e, float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i) {
            const float lo = split_.process(e[i]);
            const float hi = e[i] - lo;
            line_[w_]      = hi;
            const float hd = line_[(w_ - delay_ + kLine) & (kLine - 1)];
            w_             = (w_ + 1) & (kLine - 1);
            l[i]           = lo + dark_.process(hi);
            r[i]           = lo + echo::kWideRTrim * hd;
        }
    }

#ifndef RV_FIXED_VOICINGS
    TapeEcho tapeR; // ping-pong: the right channel's tape
#endif

private:
    static constexpr int kLine = 1024; // >= kWideMs up to 128 kHz
    float          line_[kLine];
    int            w_ = 0, delay_ = 0;
    OnePoleLowpass split_{}, dark_{};
#ifndef RV_FIXED_VOICINGS
    int      voicing_ = echo::kSpringsBlendDefault;
    float*   tape_    = nullptr;
    size_t   tapeFloats_ = 0;
    float    sampleRate_ = 48000.0f;
    uint32_t seed_       = 0;
    bool     prepared_   = false;
    void     prepareTape()
    {
        if (echo::kSpringsBlend[voicing_].style != echo::DirectStyle::PingPong) return; // no second tape unless asked for
        const size_t need = echo::tapeFloats(sampleRate_);
        if (need > tapeFloats_) {
            std::free(tape_);
            tape_       = static_cast<float*>(std::malloc(need * sizeof(float)));
            tapeFloats_ = tape_ ? need : 0;
        }
        tapeR.prepare(sampleRate_, seed_, tape_, tapeFloats_);
    }
#endif
};

} // namespace rv::dsp
