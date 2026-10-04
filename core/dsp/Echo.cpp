#include "dsp/Echo.h"
#include "dsp/SizeOpt.h"

#include <algorithm>
#include <cmath>

namespace rv::dsp {

RV_SIZE_OPT void TapeEcho::prepare(float sampleRate, uint32_t seed, float* tape, size_t tapeFloats)
{
    sampleRate_ = sampleRate;
    minDelay_   = float(2 * kGrid);
    const bool fits = tape != nullptr && tapeFloats > size_t(8 * kGrid) && tapeFloats < size_t(1u << 30);
    buf_  = fits ? tape : nullptr;
    size_ = fits ? int(tapeFloats) : 0;
    // The longest echo time: kMaxSeconds, or what this tape holds less its
    // margin (WOBBLE's swing, the wander, the interpolation).
    maxD_    = fits ? std::min(echo::kMaxSeconds * sampleRate, float(size_ - echo::kTapeMargin)) : minDelay_;
    maxD_    = std::max(maxD_, minDelay_);
    readMax_ = fits ? float(size_ - 4) : minDelay_; // every read stays on the tape, whatever the swing
    lp_.setLowpass(std::min(echo::kHeadLpHz, 0.45f * sampleRate), echo::kHeadLpQ, sampleRate);
    recLp_.setLowpass(std::min(echo::kRecLpHz, 0.45f * sampleRate), echo::kHeadLpQ, sampleRate);
    hp_.setCutoff(echo::kHeadHpHz, sampleRate);
    glide_      = 1.0f - std::exp(-float(kGrid) / (echo::kTimeGlideSeconds * sampleRate));
    maxStep_    = echo::kMaxSlew * float(kGrid);
    floorDepth_ = 0.001f * echo::kFloorMs * sampleRate;
    floorStep1_ = echo::kFloorHz1 * float(kGrid) / sampleRate;
    floorStep2_ = echo::kFloorHz2 * float(kGrid) / sampleRate;
    wow_.prepare(sampleRate, 0, seed, Wobble::Role::Transport);
    reset();
}

RV_SIZE_OPT void TapeEcho::clearTape()
{
    // Real-time safe: nothing is wiped (2 s of tape is ~1 ms of memory
    // writes, a whole audio block on the Daisy); what was recorded before
    // reads as blank until it has been recorded over (play()).
    filled_ = 0;
    lp_.reset();
    hp_.reset();
    recLp_.reset();
}

RV_SIZE_OPT void TapeEcho::reset()
{
    if (buf_) std::fill(buf_, buf_ + size_, 0.0f); // not real-time (prepare(), Tank::reset())
    w_ = 0;
    clearTape();
    dFrom_ = dTo_ = 0.0f;
    k_     = 0;
    ph1_ = 0.0f, ph2_ = 0.37f;
    floorFrom_ = floorTo_ = 0.0f;
    wow_.reset();
}

void TapeEcho::tick(float seconds, float wobble, bool snap)
{
    const float target = std::clamp(seconds * sampleRate_, minDelay_, maxD_);
    dFrom_ = dTo_;
    if (snap || dTo_ <= 0.0f) {
        dTo_ = dFrom_ = target;
    } else {
        const float step = std::clamp(glide_ * (target - dTo_), -maxStep_, maxStep_);
        dTo_ += step;
    }
    // The tape's own wander (EchoVoicing.h kFloorMs), on the grid; ramped
    // per sample with the time (play()).
    floorFrom_ = floorTo_;
    ph1_ += floorStep1_;
    ph2_ += floorStep2_;
    if (ph1_ >= 1.0f) ph1_ -= 1.0f;
    if (ph2_ >= 1.0f) ph2_ -= 1.0f;
    floorTo_ = floorDepth_ * (0.6f * std::sin(2.0f * map::kPi * ph1_) + 0.4f * std::sin(2.0f * map::kPi * ph2_ + 1.0f));
    if (snap) floorFrom_ = floorTo_;
    wow_.setAmount(wobble, echo::kWowScale);
    k_ = 0;
}

void TapeEcho::play(float* out, int n)
{
    if (!buf_) {
        std::fill(out, out + n, 0.0f);
        return;
    }
    const float inv = 1.0f / float(kGrid);
    for (int i = 0; i < n; ++i) {
        const float t = float(k_ + i) * inv;
        float d = dFrom_ + t * (dTo_ - dFrom_) + floorFrom_ + t * (floorTo_ - floorFrom_) + wow_.next();
        d = d < minDelay_ ? minDelay_ : (d > readMax_ ? readMax_ : d);
        // Read at w - d: between the sample di back (a) and the one before it.
        const int   di   = int(d); // d > 0: truncation is floor
        const float frac = d - float(di);
        int a = w_ + i - di;
        if (a < 0) a += size_;
        const int b = a == 0 ? size_ - 1 : a - 1;
        float ya = buf_[a], yb = buf_[b];
        if (filled_ < size_) { // a fresh tape: what was there before reads as blank
            ya = di - i <= filled_ ? ya : 0.0f;
            yb = di + 1 - i <= filled_ ? yb : 0.0f;
        }
        const float y = ya + frac * (yb - ya);
        const float h = y - hp_.process(y); // playback head: thin ...
        out[i]        = lp_.process(h);     // ... and dark, every pass
    }
}

void TapeEcho::record(const float* in, int n)
{
    if (!buf_) return;
    constexpr float k = echo::kTapeDrive, invK = 1.0f / echo::kTapeDrive;
    for (int i = 0; i < n; ++i) {
        buf_[w_] = invK * softClip(k * recLp_.process(in[i]));
        if (++w_ == size_) w_ = 0;
    }
    filled_ = filled_ + n < size_ ? filled_ + n : size_;
    k_ += n;
}

} // namespace rv::dsp
