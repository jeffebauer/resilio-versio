#pragma once
// PROTOTYPE (owner 6 Oct 2026; EchoVoicing.h "Springs blend",
// docs/prototypes/echo-springs-blend/README.md): echo mode's repeats heard
// directly, in stereo, next to the springs. Renderer / Plugin builds only:
// the firmware (RV_FIXED_VOICINGS) never includes it.
//
// Wide: the tape's (mono) repeats; L as they are, R kWideMs later and a
// touch darker, both above kWideSplitHz only (the lows identical in L and R).
// Ping-pong: a second tape (tapeR) the Tank cross-feeds with its own
// (Tank.cpp): the input goes on L's tape, L's playback x the feedback on R's,
// R's on L's. tapeR runs the same heads, WOBBLE (same seed) and wear as the
// Tank's tape.

#include "dsp/Echo.h"
#include "dsp/Filters.h"
#include "params/EchoVoicing.h"

#include <cstdlib>

namespace rv::dsp {

class EchoDirect {
public:
    EchoDirect() = default;
    EchoDirect(const EchoDirect&) = delete;
    EchoDirect& operator=(const EchoDirect&) = delete;
    ~EchoDirect() { std::free(tape_); }

    void prepare(float sampleRate, uint32_t seed)
    {
        const size_t need = echo::tapeFloats(sampleRate);
        if (need > tapeFloats_) {
            std::free(tape_);
            tape_       = static_cast<float*>(std::malloc(need * sizeof(float)));
            tapeFloats_ = tape_ ? need : 0;
        }
        tapeR.prepare(sampleRate, seed, tape_, tapeFloats_);
        delay_ = int(echo::kWideMs * 0.001f * sampleRate + 0.5f);
        if (delay_ > kLine - 1) delay_ = kLine - 1;
        split_.setCutoff(echo::kWideSplitHz, sampleRate);
        dark_.setCutoff(echo::kWideDarkHz, sampleRate);
        reset();
    }
    void reset()
    {
        tapeR.reset();
        for (float& v : line_) v = 0.0f;
        w_ = 0;
        split_.reset();
        dark_.reset();
    }
    void clearTape() { tapeR.clearTape(); }

    void setVoicing(int v)
    {
        voicing_ = v < 0 ? 0 : (v < echo::kNumSpringsBlendVoicings ? v : echo::kNumSpringsBlendVoicings - 1);
    }
    int   voicing() const { return voicing_; }
    bool  active() const { return voicing_ != 0; }
    bool  pingPong() const { return echo::kSpringsBlend[voicing_].style == echo::DirectStyle::PingPong; }
    float springs() const { return echo::kSpringsBlend[voicing_].springs; }

    // Wide: the mono repeats e -> L, R (n samples).
    void widen(const float* e, float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i) {
            const float lo = split_.process(e[i]);
            const float hi = e[i] - lo;
            line_[w_]      = hi;
            const float hd = line_[(w_ - delay_ + kLine) & (kLine - 1)];
            w_             = (w_ + 1) & (kLine - 1);
            l[i]           = e[i];
            r[i]           = lo + dark_.process(hd);
        }
    }

    TapeEcho tapeR; // ping-pong: the right channel's tape

private:
    static constexpr int kLine = 2048; // >= kWideMs at 192 kHz
    float*         tape_ = nullptr;
    size_t         tapeFloats_ = 0;
    float          line_[kLine]{};
    int            w_ = 0, delay_ = 0;
    int            voicing_ = 0;
    OnePoleLowpass split_{}, dark_{};
};

} // namespace rv::dsp
