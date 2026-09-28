#include "dsp/Tank.h"

#include "params/AntiRes.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace rv {

namespace {

// Decorrelator D (SPEC §4.3): two short allpasses at unrelated lengths.
// Same spectrum and tail as its input (the mid signal), different phase.
// Used only in the side (+D on L, -D on R), so it never reaches the mono sum.
constexpr std::array<float, 2> kDiffuserSeconds{{0.0023f, 0.0037f}};

int diffuserSize(float sampleRate, size_t i) { return int(std::ceil(kDiffuserSeconds[i] * sampleRate)) + 1; }

// Per-Spring noise seeds: fixed, so every run is reproducible.
constexpr std::array<uint32_t, Tank::kMaxSprings> kSeeds{{0x9E3779B9u, 0x7F4A7C15u, 0x2545F491u}};

} // namespace

// The Tank object itself must stay small: the Firmware keeps it as a global
// in DTCM (128 KB). Big buffers live in the pool.
static_assert(sizeof(Spring) < 2048, "Spring object grew: move state into the pool");
static_assert(Tank::kMaxSprings == modes::kNumSprings, "SpringModes.h tables are for 3 Springs");

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
    fadeStep_     = 1.0f / (kSpringsFadeSeconds * sampleRate);
    morphStep_    = float(kControlInterval) / (drive::kMorphSeconds * sampleRate);
    driveIn_.prepare(sampleRate);
    tilt_.prepare(sampleRate);
    for (auto& d : driveOut_) d.prepare(sampleRate);

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
    driveIn_.reset();
    tilt_.reset();
    for (auto& d : driveOut_) d.reset();
    compDrive_ = -1.0f;
    limitEnv_ = 0.0f;
    numPendingKicks_ = 0;
    tick_     = 0;
    primed_   = false;
}

void Tank::kick(int sampleOffset)
{
    if (numPendingKicks_ < kMaxPendingKicks)
        pendingKicks_[size_t(numPendingKicks_++)] = sampleOffset < 0 ? 0 : sampleOffset;
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

    // SPRINGS: a switch, so never smoothed here. A change starts a fade of
    // the output mix from the gains playing right now (mixCur_), so a flip
    // in the middle of a fade carries on smoothly from where it was.
    const int mode = normalisedToSwitch(values_[size_t(ParamId::Springs)]);
    if (snap) {
        mode_     = mode;
        mixCur_   = mixTo_ = mixFrom_ = modes::stereoMix(mode);
        trimCur_  = trimTo_ = trimFrom_ = modes::kModeTrim[size_t(mode)];
        mixScale_ = trimCur_ / std::sqrt(modes::mixPower(mixCur_));
        fadePos_  = 1.0f;
    } else if (mode != mode_) {
        mode_     = mode;
        mixFrom_  = mixCur_;
        trimFrom_ = trimCur_;
        mixTo_    = modes::stereoMix(mode);
        trimTo_   = modes::kModeTrim[size_t(mode)];
        fadePos_  = 0.0f;
    }

    // ATTITUDE Morph: glide the weights linearly toward the switch position
    // (ADR 0003). Moving the whole vector along a straight line keeps the sum
    // at 1, and a flip mid-Morph just turns toward the new corner from here.
    const int att = normalisedToSwitch(values_[size_t(ParamId::Attitude)]);
    std::array<float, 3> target{{0.0f, 0.0f, 0.0f}};
    target[size_t(att)] = 1.0f;
    if (snap) {
        attW_ = target;
    } else {
        float dist = 0.0f;
        for (size_t a = 0; a < 3; ++a) dist = std::max(dist, std::fabs(target[a] - attW_[a]));
        if (dist > 0.0f) {
            const float f = std::min(1.0f, morphStep_ / dist);
            for (size_t a = 0; a < 3; ++a) attW_[a] = f >= 1.0f ? target[a] : attW_[a] + f * (target[a] - attW_[a]);
        }
    }
    const drive::Voice voice = dsp::blendVoice(attW_);
    const float drive = smoothed_[size_t(ParamId::Drive)];

    // DriveIn settings and the DRIVE push on the later stages (ADR 0022):
    // only recomputed when DRIVE or the Morph moved (they cost a few exp).
    if (snap || drive != compDrive_ || attW_ != compW_) {
        driveInSettings_ = dsp::driveInSettings(voice, drive);
        push_      = drive::push(voice, drive);
        compDrive_ = drive;
        compW_     = attW_;
    }
    driveIn_.set(driveInSettings_, snap, kControlInterval);
    tilt_.set(tone, snap, kControlInterval);
    for (auto& d : driveOut_) d.set(voice, push_.out);
    {
        // DriveOut automatic makeup, linked across L/R so the image never
        // shifts: sqrt(level in / level out) of both channels together
        // (at least 1, at most kAutoMakeupMax), times the static wet makeup.
        const float in  = driveOut_[0].levelIn() + driveOut_[1].levelIn();
        const float out = driveOut_[0].levelOut() + driveOut_[1].levelOut();
        const float autoGain = std::clamp(std::sqrt(in / out), 1.0f, drive::kAutoMakeupMax);
        for (auto& d : driveOut_) d.setMakeup(autoGain * push_.wet, snap, kControlInterval);
    }

    SpringSettings base;
    base.loopDelaySeconds = map::decayLoopDelaySeconds(decay);
    base.t60Seconds       = map::decayT60Seconds(decay);
    base.transitionHz     = map::decayTransitionHz(decay);
    base.allpassCoeff     = map::boingCoefficient(boing);
    base.dampingHz        = map::toneDampingHz(tone);
    base.highPathLevel    = map::toneHighPathLevel(tone);
    base.loopSatAmount    = voice.loopAmount;
    // DRIVE pushes the LoopSat too (ADR 0022): the same curve, harder as
    // DRIVE rises (Voice::loopDriveDb). Its slope stays <= 1: Loop gain
    // can only go down, never up. Not inside the Howl zone, though: there
    // the LoopSat's hardness sets how loud the Howl settles (a harder curve
    // holds it lower), so the push fades out across the zone and the Howl
    // keeps its ADR 0019 voicing whatever DRIVE does. No jump at the edge.
    const float howlAmt   = drive::howlZone(decay) * attW_[2];
    const float loopPush  = drive::dbToGain(push_.loopDb * (1.0f - howlAmt));
    base.loopSatKPos      = voice.loopKPos * loopPush;
    base.loopSatKNeg      = voice.loopKNeg * loopPush;
    base.howl             = howlAmt;
    // AntiRes Micro-mod floor, always on (WOBBLE adds on top at M7), plus
    // the Howl zone's movement (ADR 0019), both on the same L-modulation hook.
    base.modDepth         = antires::kMicroModDepth + antires::kHowlModDepth * base.howl;
    base.lfoDepth         = antires::kHowlLfoDepth * base.howl;
    const int activeStages = modes::boingStages(boing, modes::kStageCap[size_t(mode_)]);
    for (size_t i = 0; i < springs_.size(); ++i) {
        // Same T60 for every Spring (g is designed from each Spring's own
        // round trip), so detuning changes pitch/texture, not tail length.
        SpringSettings s = base;
        s.loopDelaySeconds *= modes::kDetune[i].loopDelay;
        s.transitionHz     *= modes::kDetune[i].transition;
        s.allpassCoeff     *= modes::kDetune[i].allpassCoeff;
        s.tapRatio          = modes::kPickupTap[i];
        s.lfoHz             = antires::kHowlLfoHz * antires::kHowlLfoRatio[i];
        s.stages = modes::springActive(mode_, int(i)) ? activeStages : modes::kIdleStages;
        springs_[i].setSettings(s, snap);
    }
}

void Tank::process(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    if (!ok_) { // no memory: stay a clean passthrough rather than fail silently into noise
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        numPendingKicks_ = 0;
        return;
    }
    if (!primed_) {
        controlTick(true);
        mix_.value = values_[size_t(ParamId::Mix)];
        primed_    = true;
    }

    float mono[kControlInterval];
    float wet[kMaxSprings][kControlInterval];
    int pos = 0;
    while (pos < numSamples) {
        if (tick_ == 0) controlTick(false); // fixed grid, independent of block size
        const int n = std::min(numSamples - pos, kControlInterval - tick_);

        // Real tanks are mono: sum the input (SPEC §4.3). Dry stays stereo.
        for (int i = 0; i < n; ++i) mono[i] = 0.5f * (inL[pos + i] + inR[pos + i]);
        for (int k = 0; k < numPendingKicks_; ++k) {
            const int at = std::min(pendingKicks_[size_t(k)], numSamples - 1) - pos;
            if (at >= 0 && at < n) mono[at] += kKickPlaceholder;
        }
        // Tank-level input stages: DriveIn (transducer -> tape), then TONE's tilt.
        for (int i = 0; i < n; ++i) mono[i] = tilt_.process(driveIn_.process(mono[i]));
        for (size_t s = 0; s < springs_.size(); ++s) springs_[s].process(mono, wet[s], n);

        for (int i = 0; i < n; ++i) {
            const float dryL = inL[pos + i], dryR = inR[pos + i]; // read before write: in may alias out
            const float src[modes::kNumSources] = {kWetGain * wet[0][i], kWetGain * wet[1][i],
                                                   kWetGain * wet[2][i]};

            if (fadePos_ < 1.0f) {
                // SPRINGS fade, smoothstep-shaped t (3t² - 2t³): every gain
                // moves continuously and starts and ends with zero slope, so
                // nothing jumps. The mid/side gains fade linearly in t, and
                // the whole mix is rescaled to constant power on every sample
                // (1/sqrt(mixPower)), so there is no level dip half way
                // (a plain fade dips ~2 dB when one Spring hands over to
                // another). One square root per sample, only during the fade.
                fadePos_ = std::min(1.0f, fadePos_ + fadeStep_);
                const float t = fadePos_ * fadePos_ * (3.0f - 2.0f * fadePos_);
                for (int k = 0; k < modes::kNumSources; ++k) {
                    mixCur_.mid[k]  = mixFrom_.mid[k] + t * (mixTo_.mid[k] - mixFrom_.mid[k]);
                    mixCur_.side[k] = mixFrom_.side[k] + t * (mixTo_.side[k] - mixFrom_.side[k]);
                }
                mixCur_.decorr = mixFrom_.decorr + t * (mixTo_.decorr - mixFrom_.decorr);
                trimCur_       = trimFrom_ + t * (trimTo_ - trimFrom_);
                mixScale_      = trimCur_ / std::sqrt(modes::mixPower(mixCur_));
            }
            float mid = 0.0f, side = 0.0f;
            for (int k = 0; k < modes::kNumSources; ++k) {
                mid += mixCur_.mid[k] * src[k];
                side += mixCur_.side[k] * src[k];
            }
            mid *= mixScale_;
            side *= mixScale_;
            // D: mid with its phase scrambled (always running, so its state
            // is live whatever the mode). It goes into L and R with opposite
            // signs, so it widens the image and cancels exactly in mono.
            const float d = mixCur_.decorr * decorrelator_[1].process(decorrelator_[0].process(mid));
            // Output pickup (DriveOut), one per channel.
            float wl = driveOut_[0].process(mid + side + d);
            float wr = driveOut_[1].process(mid - side - d);

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
    numPendingKicks_ = 0;
}

} // namespace rv
