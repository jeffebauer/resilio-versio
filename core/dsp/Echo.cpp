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
    setDiffuseVoicing(diffuse_); // the diffuser's lengths at this rate
    wear_.prepare(sampleRate, seed ^ 0x5EEDu);
    reset();
}

RV_SIZE_OPT void TapeEcho::setDiffuseVoicing(int v)
{
    diffuse_ = echo::kDiffuseBuilt && v > 0 && v < echo::kNumDiffuseVoicings ? v : 0;
    if (!echo::kDiffuseBuilt) return;
    const echo::DiffuseVoicing& d = echo::kDiffuse[diffuse_];
    const float maxD = float(kApSize - 4);
    for (int s = 0; s < echo::kDiffuseStages; ++s) {
        Allpass& a = ap_[s];
        a.depth = std::min(0.001f * d.modMs * sampleRate_, 0.25f * maxD);
        a.base  = std::clamp(0.001f * d.ms[s] * sampleRate_, a.depth + 2.0f, maxD - a.depth);
        a.step  = echo::kDiffuseModHz[s] / sampleRate_;
        a.ph    = 0.25f * float(s);
    }
    clearDiffuser();
}

RV_SIZE_OPT void TapeEcho::clearDiffuser()
{
    if (!echo::kDiffuseBuilt) return;
    for (auto& a : ap_) {
        std::fill(a.buf, a.buf + kApSize, 0.0f);
        a.w = 0;
    }
}

void TapeEcho::diffuse(float* x, int n)
{
    if (!echo::kDiffuseBuilt || diffuse_ == 0) return;
    const float g = echo::kDiffuse[diffuse_].g;
    for (auto& a : ap_) {
        for (int i = 0; i < n; ++i) {
            // Schroeder allpass, H = (-g + z^-D) / (1 - g z^-D), D drifting
            // slowly (a triangle, read with linear interpolation).
            a.ph += a.step;
            if (a.ph >= 1.0f) a.ph -= 1.0f;
            const float tri = a.ph < 0.5f ? 4.0f * a.ph - 1.0f : 3.0f - 4.0f * a.ph;
            const float d   = a.base + a.depth * tri;
            const int   di  = int(d);
            const float fr  = d - float(di);
            int r0 = a.w - di;
            if (r0 < 0) r0 += kApSize;
            const int   r1 = r0 == 0 ? kApSize - 1 : r0 - 1;
            const float dl = a.buf[r0] + fr * (a.buf[r1] - a.buf[r0]);
            const float v  = x[i] + g * dl;
            a.buf[a.w]     = v;
            if (++a.w == kApSize) a.w = 0;
            x[i] = dl - g * v;
        }
    }
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
    clearDiffuser();
    wear_.reset();
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
    // The wear's fixed delay (the worn tape's) is part of every pass: the tape is that much shorter.
    const float target = std::clamp(seconds * sampleRate_ - wear_.latencySamples(), minDelay_, maxD_);
    wear_.setDelaySeconds(dTo_ / sampleRate_); // a time-tracking BBD's clock follows the tape (swoops with it)
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
