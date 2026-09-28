#include "dsp/Tank.h"

#include "params/AntiRes.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"
#include "params/SplashVoicing.h"

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
// M7 seeds (distinct from the Spring noise seeds; the components scramble
// them with dsp::mixSeed).
constexpr uint32_t kSplashSeed = 0x51A5E001u;
constexpr uint32_t kKickSeed   = 0x4B1C0002u;
constexpr std::array<uint32_t, Tank::kMaxSprings> kWobbleSeeds{{0x0B0B1E01u, 0x0B0B1E02u, 0x0B0B1E03u}};

// Limiter backstop: identity below the knee, then dsp::softClip scaled into
// the room between knee and threshold. softClip has slope 1 and no curvature
// at 0, so the join at the knee has no corner in slope or curvature (a
// curvature step is a tick on held tones too), and it holds exactly at the
// threshold from 3x the room above the knee.
inline float softLimit(float x)
{
    constexpr float kRoom = Tank::kLimitThreshold - Tank::kLimitKnee;
    const float     e     = std::fabs(x) - Tank::kLimitKnee;
    if (e <= 0.0f) return x;
    const float y = Tank::kLimitKnee + kRoom * dsp::softClip(e * (1.0f / kRoom));
    return x < 0.0f ? -y : y;
}

} // namespace

// The Tank object itself must stay small: the Firmware keeps it as a global
// in DTCM (128 KB). Big buffers live in the pool.
static_assert(sizeof(Spring) < 2048, "Spring object grew: move state into the pool");
static_assert(Tank::kMaxSprings == modes::kNumSprings, "SpringModes.h tables are for 3 Springs");
// The M7 components keep their own control grid, counted from reset() like
// the Tank's: same length, so the two stay aligned for any block size.
static_assert(splash::kControlInterval == Tank::kControlInterval, "M7 control grid must match the Tank's");

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
    limitAttack_  = 1.0f - std::exp(-1.0f / (kLimitAttackS * sampleRate));
    fadeStep_     = 1.0f / (kSpringsFadeSeconds * sampleRate);
    morphStep_    = float(kControlInterval) / (drive::kMorphSeconds * sampleRate);
    driveIn_.prepare(sampleRate);
    tilt_.prepare(sampleRate);
    for (auto& d : driveOut_) d.prepare(sampleRate);
    splash_.prepare(sampleRate, kSplashSeed);
    kick_.prepare(sampleRate, kKickSeed);
    for (size_t i = 0; i < wobble_.size(); ++i) wobble_[i].prepare(sampleRate, int(i), kWobbleSeeds[i]);
    levelCoeff_ = 1.0f - std::exp(-1000.0f * float(kControlInterval) / (splash::kTankLevelSmoothMs * sampleRate));

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
    splash_.reset();
    kick_.reset();
    for (auto& w : wobble_) w.reset();
    hfGainFrom_ = hfGainTo_ = 1.0f;
    levelAcc_ = levelMs_ = 0.0f;
    compDrive_ = -1.0f;
    limitEnv_  = 0.0f;
    limitGain_ = 1.0f;
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
    const float splashAmt = smoothed_[size_t(ParamId::Splash)];

    // M7: Splash and Kick follow the Morph weights (their tables blend like
    // the drive voicing: no steps on an ATTITUDE flip); WOBBLE glides.
    splash_.set(attW_, splashAmt);
    kick_.setAttitude(attW_);
    for (auto& w : wobble_) w.setAmount(smoothed_[size_t(ParamId::Wobble)]);
    // Tank level for KICKED's energy-dependent rattle: smoothed RMS of the
    // wet mid over the last tick, scaled by SPLASH.
    levelMs_ += levelCoeff_ * (levelAcc_ * (1.0f / float(kControlInterval)) - levelMs_);
    levelAcc_ = 0.0f;
    splash_.setTankLevel(std::sqrt(levelMs_) * splashAmt);
    // CLEAN HF emphasis: ramp from the last tick's gain to the new one.
    const float hfGain = splashOn_ ? splash_.highPathGain() : 1.0f;
    hfGainFrom_ = snap ? hfGain : hfGainTo_;
    hfGainTo_   = hfGain;
    // Jolt on a (control rate, <= 0): after the detune, clamped below.
    const float joltA = joltOn_ ? splash_.allpassDelta() : 0.0f;

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
    // AntiRes Micro-mod floor, always on (WOBBLE adds on top, per sample), plus
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
        s.allpassCoeff      = std::clamp(s.allpassCoeff * modes::kDetune[i].allpassCoeff + joltA,
                                         -splash::kMaxAllpassMagnitude, splash::kMaxAllpassMagnitude);
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

    float mono[kControlInterval], driven[kControlInterval], high[kControlInterval];
    float clatter[kControlInterval], jolt[kControlInterval], kickLoop[kControlInterval], kickDirect[kControlInterval];
    float lFrac[kControlInterval], lSamples[kControlInterval];
    float wet[kMaxSprings][kControlInterval];
    int pos = 0;
    while (pos < numSamples) {
        if (tick_ == 0) controlTick(false); // fixed grid, independent of block size
        const int n = std::min(numSamples - pos, kControlInterval - tick_);

        // Real tanks are mono: sum the input (SPEC §4.3). Dry stays stereo.
        // DriveIn (transducer -> tape) first: the Splash listens here.
        for (int i = 0; i < n; ++i) driven[i] = driveIn_.process(0.5f * (inL[pos + i] + inR[pos + i]));

        // Kick: onsets on their exact sample (offsets clamp to the block).
        for (int k = 0; k < numPendingKicks_; ++k) {
            const int at = std::min(pendingKicks_[size_t(k)], numSamples - 1) - pos;
            if (at >= 0 && at < n) kick_.trigger(at);
        }
        kick_.process(kickLoop, kickDirect, n);
        // A Kick forces a maximal Splash on its own sample (SPEC §4.6).
        if (kick_.joltOffset() >= 0) splash_.strike(1.0f, kick_.joltOffset());
        splash_.process(driven, clatter, jolt, n);
        if (!splashOn_) std::fill(clatter, clatter + n, 0.0f); // test hooks (Tank.h)
        if (!joltOn_) std::fill(jolt, jolt + n, 0.0f);

        // Spring inputs: TONE's tilt, plus the Kick's high-passed Loop feed
        // (post-drive); the high path also gets the Clatter (the same for every
        // Spring: their detuned high paths decorrelate it), and in CLEAN a
        // little transient lift (ramped across the tick: no zipper).
        const float hfStep = (hfGainTo_ - hfGainFrom_) * (1.0f / float(kControlInterval));
        for (int i = 0; i < n; ++i) {
            mono[i] = tilt_.process(driven[i]) + kickLoop[i];
            high[i] = (mono[i] + clatter[i]) * (hfGainFrom_ + hfStep * float(tick_ + i));
        }
        for (size_t s = 0; s < springs_.size(); ++s) {
            const float scale = splash::kJoltSpringScale[s];
            for (int i = 0; i < n; ++i) {
                lFrac[i]    = scale * jolt[i];
                lSamples[i] = wobble_[s].next();
            }
            springs_[s].process(mono, high, lFrac, lSamples, wet[s], n);
        }

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
            levelAcc_ += mid * mid;
            // D: mid with its phase scrambled (always running, so its state
            // is live whatever the mode). It goes into L and R with opposite
            // signs, so it widens the image and cancels exactly in mono.
            const float d = mixCur_.decorr * decorrelator_[1].process(decorrelator_[0].process(mid));
            // Output pickup (DriveOut), one per channel.
            // The Kick's direct thump joins the mid here: centred, mono-safe,
            // coloured by the pickups like the tank body moving under them.
            const float body = mid + kWetGain * kickDirect[i];
            float wl = driveOut_[0].process(body + side + d);
            float wr = driveOut_[1].process(body - side - d);

            // Gentle high-shelf cut: keep the part below kShelfHz, scale the rest.
            const float ll = shelfSplit_[0].process(wl), lr = shelfSplit_[1].process(wr);
            wl = ll + kShelfGain * (wl - ll);
            wr = lr + kShelfGain * (wr - lr);

            // Safety limiter (stereo-linked). The envelope jumps to each new
            // peak and releases slowly; the gain glides down to knee/envelope
            // over ~1 ms and follows the (smooth) release straight back. An
            // instant gain would pin every rising peak flat at the threshold:
            // a corner in the waveform, i.e. a tick per new peak on held tones
            // (it was the DECAY-0.5 held-chord tick). The overshoot the glide
            // lets through is caught by a soft clip: identity up to the knee,
            // then a smooth curve that holds at the threshold T (softLimit).
            const float peak = std::max(std::fabs(wl), std::fabs(wr));
            limitEnv_ = std::max(peak, limitEnv_ * limitRelease_);
            const float gainTarget = limitEnv_ > kLimitKnee ? kLimitKnee / limitEnv_ : 1.0f;
            if (gainTarget < limitGain_) limitGain_ += limitAttack_ * (gainTarget - limitGain_);
            else limitGain_ = gainTarget;
            wl = softLimit(wl * limitGain_);
            wr = softLimit(wr * limitGain_);

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
