#include "dsp/Tank.h"
#include "dsp/SizeOpt.h"
#include "dsp/ProfileHook.h"

#include "params/AntiRes.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"
#include "params/SplashVoicing.h"

#include <algorithm>
#include <cmath>

namespace rv {

namespace {

// Decorrelator D (SPEC §4.3): short allpasses at unrelated lengths (SpringModes.h).
// Same spectrum and tail as its input (the mid signal), different phase.
// Used only in the side (+D on L, -D on R), so it never reaches the mono sum.
constexpr auto kDiffuserSeconds = modes::kDecorrSeconds;

int diffuserSize(float sampleRate, size_t i) { return int(std::ceil(kDiffuserSeconds[i] * sampleRate)) + 1; }

// Per-Spring noise seeds: fixed, so every run is reproducible.
constexpr std::array<uint32_t, Tank::kMaxSprings> kSeeds{{0x9E3779B9u, 0x7F4A7C15u, 0x2545F491u}};
// M7 seeds (distinct from the Spring noise seeds; the components scramble
// them with dsp::mixSeed).
constexpr uint32_t kSplashSeed = 0x51A5E001u;
constexpr uint32_t kKickSeed   = 0x4B1C0002u;
constexpr std::array<uint32_t, Tank::kMaxSprings> kWobbleSeeds{{0x0B0B1E01u, 0x0B0B1E02u, 0x0B0B1E03u}};
constexpr uint32_t kTransportSeed = 0x0B0B1E04u;
constexpr uint32_t kEchoSeed      = 0x0B0B1E05u; // the tape's WOBBLE (echo mode)

// Limiter backstop: identity below the knee, then dsp::softClip scaled into
// the room between knee and threshold. softClip has slope 1 and no curvature
// at 0, so the join at the knee has no corner in slope or curvature (a
// curvature step is a tick on held tones too), and it holds exactly at the
// threshold from 3x the room above the knee.
constexpr float kClatterWetGain = Tank::kWetGain * splash::kClatterWet;

inline float softLimit(float x)
{
    constexpr float kRoom = Tank::kLimitThreshold - Tank::kLimitKnee;
    const float     e     = std::fabs(x) - Tank::kLimitKnee;
    if (e <= 0.0f) return x;
    const float y = Tank::kLimitKnee + kRoom * dsp::softClip(e * (1.0f / kRoom));
    return x < 0.0f ? -y : y;
}

} // namespace

#if defined(RV_PROFILE_HOOKS)
void (*prof::markHook)(int) = nullptr;
#endif

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
#ifndef RV_FIXED_VOICINGS // the firmware hands the Tank its pool (main.cpp): no malloc / free linked (flash)
    std::free(ownedPool_);
    std::free(ownedTape_);
#endif
    ownedPool_   = nullptr;
    ownedFloats_ = 0;
    ownedTape_   = nullptr;
    ownedTapeFloats_ = 0;
}

// ---- Tank voicings (params/TankVoicing.h): pool layout ---------------------
namespace {
// Most Sweep sections any TENSION asks for, and its lowest top (sizes its rings).
int sweepMaxStages()
{
    const auto& t = tankv::tuning();
    const float most = t.sweepStagesNoon * std::max({1.0f, t.sweepTightScale, t.sweepLooseScale});
    return std::clamp(int(std::ceil(most)) + 1, 1, 64);
}
float sweepMinFcHz() { return map::kTransitionMinHz * tankv::tuning().sweepFcRatio; }
// Loop sections a voicing uses at most (its rings; 2-Spring cap 64).
int loopMaxStages(int v)
{
    if (!tankv::hasSweep(v)) return Spring::kMaxStages;
    const auto& t = tankv::tuning();
    const float frac = std::max(t.loopFracNoon, t.loopFracLoose);
    return std::min(Spring::kMaxStages, map::kMinStages + int(std::ceil(float(Spring::kMaxStages - map::kMinStages) * frac)) + 1);
}
// Loop diffuser buffer (voicing 3), Spring s, diffuser k: its delay + 2.
int diffBufSize(float sampleRate, size_t s, size_t k)
{
    const auto& t = tankv::tuning();
    return int(std::ceil(t.diffMs[k] * t.diffSpringScale[s] * 0.001f * sampleRate)) + 2;
}
RV_SIZE_OPT size_t diffFloats(float sampleRate)
{
    size_t n = 0;
    for (size_t s = 0; s < size_t(Tank::kMaxSprings); ++s)
        for (size_t k = 0; k < size_t(tankv::kNumDiffusers); ++k) n += size_t(diffBufSize(sampleRate, s, k));
    return n;
}
// Voicing 6's D2 (the Springs' difference decorrelated), stage k: its delay + 2.
int wideBufSize(float sampleRate, size_t k)
{
    return int(std::ceil(tankv::tuning().wideDecorrMs[k] * 0.001f * sampleRate)) + 2;
}
size_t wideFloats(float sampleRate)
{
    size_t n = 0;
    for (size_t k = 0; k < 3; ++k) n += size_t(wideBufSize(sampleRate, k));
    return n;
}
RV_SIZE_OPT size_t layoutFloats(float sampleRate, int loopStages, bool sweep, bool diffusers, bool wide)
{
    size_t n = size_t(Tank::kMaxSprings) * Spring::requiredFloats(sampleRate, loopStages);
    for (size_t i = 0; i < kDiffuserSeconds.size(); ++i) n += size_t(diffuserSize(sampleRate, i));
    if (sweep) n += dsp::Sweep::requiredFloats(sweepMaxStages(), sweepMinFcHz(), sampleRate);
    if (diffusers) n += diffFloats(sampleRate);
    if (wide) n += wideFloats(sampleRate);
    return n;
}
// What this build holds: the firmware only its default voicing; desktop
// builds every voicing (full Loop rings, the Sweep and the diffusers).
constexpr bool kPoolSweep     = RV_TANKV_BUILT >= 1;
constexpr bool kPoolDiffusers = RV_TANKV_BUILT >= 3;
constexpr bool kPoolWide      = RV_TANKV_BUILT >= 6;
#ifdef RV_FIXED_VOICINGS
int poolLoopStages() { return loopMaxStages(tankv::kDefaultVoicing); }
#else
int poolLoopStages() { return Spring::kMaxStages; }
#endif
} // namespace

RV_SIZE_OPT size_t Tank::requiredPoolFloats(float sampleRate)
{
    return layoutFloats(sampleRate, poolLoopStages(), kPoolSweep, kPoolDiffusers, kPoolWide);
}

RV_SIZE_OPT size_t Tank::poolFloatsForVoicing(float sampleRate, int v)
{
    return layoutFloats(sampleRate, loopMaxStages(v), tankv::hasSweep(v), tankv::hasDiffusion(v), tankv::hasWide(v));
}

RV_SIZE_OPT void Tank::prepare(float sampleRate, int maxBlockSize)
{
#ifndef RV_FIXED_VOICINGS
    const size_t need = requiredPoolFloats(sampleRate);
    if (need > ownedFloats_) {
        releaseOwnedPool();
        // malloc (not new/vector): no exceptions, returns null on failure.
        ownedPool_ = static_cast<float*>(std::malloc(need * sizeof(float)));
        if (ownedPool_) ownedFloats_ = need;
    }
    const size_t tapeNeed = requiredTapeFloats(sampleRate);
    if (tapeNeed > ownedTapeFloats_) {
        std::free(ownedTape_);
        ownedTape_       = static_cast<float*>(std::malloc(tapeNeed * sizeof(float)));
        ownedTapeFloats_ = ownedTape_ ? tapeNeed : 0;
    }
#endif // the firmware passes its own pool and tape (below); without a pool the Tank stays silent
    prepare(sampleRate, maxBlockSize, ownedPool_, ownedFloats_, ownedTape_, ownedTapeFloats_);
}

RV_SIZE_OPT void Tank::prepare(float sampleRate, int maxBlockSize, float* pool, size_t poolFloats, float* tape,
                               size_t tapeFloats)
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
    limitHoldSamples_ = int(kLimitHoldS * sampleRate);
    outBits_.prepare(sampleRate, 0xB175u);
    fadeStep_     = 1.0f / (kSpringsFadeSeconds * sampleRate);
    s3Step_       = float(kControlInterval) / (springs3::kGlideSeconds * sampleRate);
    morphStep_    = float(kControlInterval) / (drive::kMorphSeconds * sampleRate);
    driveIn_.prepare(sampleRate);
    tilt_.prepare(sampleRate);
    hitRelease_ = std::exp(-float(kControlInterval) / (drive::kHitBumpReleaseS * sampleRate));
    for (auto& d : driveOut_) d.prepare(sampleRate);
    splash_.prepare(sampleRate, kSplashSeed);
    kick_.prepare(sampleRate, kKickSeed);
    clangLp_.setCutoff(splash::strong(splash_.voicing()).clangHz, sampleRate);
    clangAtt_ = 1.0f - std::exp(-1000.0f / (splash::kEnvFastAttackMs * sampleRate));
    clangRel_ = 1.0f - std::exp(-1000.0f / (splash::kEnvFastReleaseMs * sampleRate));
    dcNoon_ = drive::driveCurve(0.5f);
    dcRef_  = drive::driveCurve(splash::kSplashRefDrive);
    for (size_t i = 0; i < wobble_.size(); ++i) wobble_[i].prepare(sampleRate, int(i), kWobbleSeeds[i]);
    transport_.prepare(sampleRate, 0, kTransportSeed, dsp::Wobble::Role::Transport);
    // Echo mode (EchoVoicing.h): the tape, the clock, and the DECAY that
    // gives the fixed springs their T60 (Mappings.h decayT60Seconds inverted).
    static_assert(dsp::TapeEcho::kGrid == kControlInterval, "the echo steps on the Tank's control grid");
    echo_.prepare(sampleRate, kEchoSeed, tape, tapeFloats);
    clock_.prepare(sampleRate);
    springsDecay_ = std::log(echo::kSpringsT60Seconds / map::kT60MinSeconds)
                  / std::log(map::kT60MaxSeconds / map::kT60MinSeconds);
    levelCoeff_ = 1.0f - std::exp(-1000.0f * float(kControlInterval) / (splash::kTankLevelSmoothMs * sampleRate));
    // Power ratios from dB (10^(dB/10) = dbToGain(2 dB): exp, not powf, for the Firmware's flash).
    satFloorMs_   = drive::dbToGain(2.0f * antires::kLoopSatQuietDb);
    satInvSpanMs_ = 1.0f / (satFloorMs_ * (drive::dbToGain(2.0f * antires::kLoopSatFadeDb) - 1.0f));
    for (auto& f : excHp_) f.setCutoff(drive::kExcHpHz, sampleRate);
    for (auto& f : excLp_) f.setCutoff(drive::kExcLpHz, sampleRate);
    for (auto& f : bkHp_) f.setCutoff(drive::kExcHpHz, sampleRate);
    toneReturn_.prepare(sampleRate);
    for (auto& f : trHp_) f.setCutoff(drive::kExcHpHz, sampleRate);
    excCoeff_ = 1.0f - std::exp(-float(kControlInterval) / (drive::kExcSeconds * sampleRate));
    excGate_  = drive::dbToGain(2.0f * drive::kExcGateDb); // a power
    {
        const float tick = float(kControlInterval) / sampleRate;
        susFastCoeff_  = 1.0f - std::exp(-tick / drive::kSusFastSeconds);
        susPeakRelease_ = std::exp(-tick / drive::kSusSlowSeconds);
        susUpCoeff_    = 1.0f - std::exp(-tick / drive::kSusUpSeconds);
        susLetGoCoeff_ = 1.0f - std::exp(-tick / drive::kSusLetGoSeconds);
        susDownCoeff_  = 1.0f - std::exp(-tick / drive::kSusDownSeconds);
        susOnsetDownCoeff_ = 1.0f - std::exp(-tick / drive::kSusOnsetDownSeconds);
        susFillCoeff_  = 1.0f - std::exp(-tick * 13.8f / (map::decayT60Seconds(0.5f) * drive::kSusFillScale)); // DECAY sets it (updateBaseSettings)
        // Power ratios from dB, as above.
        susHeldRatio_  = drive::dbToGain(-2.0f * drive::kSusHeldDropDb);
        susTarget_     = drive::dbToGain(2.0f * drive::kSusTargetDb);
        susStillRatio_ = drive::dbToGain(-2.0f * drive::kSusStillDropDb);
        susKRelease_   = tick * drive::kSusKReleaseDbPerS * (2.0f * 2.302585093f / 20.0f); // ln of a power, per tick
        // Round 3, the gentle voicing (DriveVoicing.h "Sustain trim voicings").
        susGUpCoeff_    = 1.0f - std::exp(-tick / drive::kSusGentleUpSeconds);
        susGLetGoCoeff_ = 1.0f - std::exp(-tick / drive::kSusGentleLetGoSeconds);
        susGDownCoeff_  = 1.0f - std::exp(-tick / drive::kSusGentleDownSeconds);
        susGTarget_     = drive::dbToGain(2.0f * drive::kSusGentleTargetDb);
        susGNeedRelease_ = tick * drive::kSusGentleNeedReleaseDbPerS * (2.302585093f / 20.0f); // ln of a gain, per tick
    }
    clatDelay_ = std::clamp(int(splash::kClatterSideMs * 0.001f * sampleRate + 0.5f), 1, int(kClatterSideMax));
    // THROW's send ramp and the Hold's ducking follower (ThrowHold.h).
    thrOpenStep_  = 1.0f / (throwhold::kThrowOpenSeconds * sampleRate);
    thrCloseStep_ = 1.0f / (throwhold::kThrowCloseSeconds * sampleRate);
    duckAtt_ = 1.0f - std::exp(-1.0f / (throwhold::kDuckKeyAttackSeconds * sampleRate));
    duckRel_ = 1.0f - std::exp(-1.0f / (throwhold::kDuckKeyReleaseSeconds * sampleRate));
    {
        const float tick = float(kControlInterval) / sampleRate;
        duckDbAtt_     = 1.0f - std::exp(-tick / throwhold::kDuckAttackSeconds);
        duckDbRel_     = 1.0f - std::exp(-tick / throwhold::kDuckReleaseSeconds);
        duckHoldTicks_ = int(throwhold::kDuckHoldSeconds / tick + 0.5f);
        for (auto& k : duckKey_) k.setLowpass(throwhold::kDuckKeyHz, 0.70710678f, sampleRate);
    }
#if RV_TANKV_BUILT >= 2 // Tank voicings 2 and 4 (TankVoicing.h)
    dBass_.setCutoff(tankv::tuning().togetherBassHz, sampleRate);
#endif
#if RV_TANKV_BUILT >= 4
    gentleHp_.setHighpass(tankv::tuning().gentleHpHz, tankv::tuning().gentleHpQ, sampleRate);
    gentleShelf_.setLowShelf(tankv::tuning().gentleShelfHz, tankv::tuning().gentleShelfDb, sampleRate);
#endif
#if RV_TANKV_BUILT >= 5 // voicing 5+: the transducers
    tdIn_.setLowpass(std::min(tankv::tuning().tdInHz, 0.45f * sampleRate), tankv::tuning().tdInQ, sampleRate);
    for (auto& f : tdOut_) f.setLowpass(std::min(tankv::tuning().tdOutHz, 0.45f * sampleRate), tankv::tuning().tdOutQ, sampleRate);
    tdEvenAvg_.setCutoff(20.0f, sampleRate);
    tdTrim_ = drive::dbToGain(tankv::tuning().tdTrimDb);
#endif
#if RV_TANKV_BUILT >= 7 // voicing 7: the low cut's makeup followers, weighted like the Big Knob's
    for (auto& f : gmHp_) f.setCutoff(drive::kExcHpHz, sampleRate);
    {
        const tankv::LowCutStep lc = tankv::fLowCut(tankv::tuning(), fLowCut_);
        lcShHp_.setHighpass(lc.hpHz, lc.hpQ, sampleRate);
        lcShShelf_.setLowShelf(lc.shelfHz, lc.shelfDb, sampleRate);
    }
    tdEvenHp_.setHighpass(std::max(tankv::tuning().tdEvenHpHz, 1.0f), 0.7071f, sampleRate);
    tdDarkLp_.setCutoff(tankv::tuning().toneDarkLpHz, sampleRate);
#endif

    ok_ = pool != nullptr && poolFloats >= requiredPoolFloats(sampleRate);
    pool_       = ok_ ? pool : nullptr;
    poolFloats_ = ok_ ? requiredPoolFloats(sampleRate) : 0;
    if (ok_) bindPool(pool);
    reset();
}

RV_SIZE_OPT void Tank::bindPool(float* pool)
{
    float* p = pool;
    const int loopStages = poolLoopStages();
    for (size_t s = 0; s < springs_.size(); ++s) {
        springs_[s].prepare(sampleRate_, p, kSeeds[s], loopStages);
        p += Spring::requiredFloats(sampleRate_, loopStages);
    }
    for (size_t i = 0; i < decorrelator_.size(); ++i) {
        decorrelator_[i].buf  = p;
        decorrelator_[i].size = diffuserSize(sampleRate_, i);
        decorrelator_[i].c    = modes::kDecorrCoeff;
        p += decorrelator_[i].size;
    }
    // Tank voicings (TankVoicing.h): the Sweep's rings, the Loop diffusers.
#if RV_TANKV_BUILT >= 1
    sweep_.prepare(sampleRate_, p, sweepMaxStages(), sweepMinFcHz());
    p += dsp::Sweep::requiredFloats(sweepMaxStages(), sweepMinFcHz(), sampleRate_);
#endif
#if RV_TANKV_BUILT >= 3
    for (size_t s = 0; s < springs_.size(); ++s)
        for (size_t k = 0; k < size_t(tankv::kNumDiffusers); ++k) {
            diffSize_[s][k] = diffBufSize(sampleRate_, s, k);
            diffBuf_[s][k]  = p;
            p += diffSize_[s][k];
        }
#endif
#if RV_TANKV_BUILT >= 6
    for (size_t k = 0; k < wideDecorr_.size(); ++k) {
        wideDecorr_[k].buf  = p;
        wideDecorr_[k].size = wideBufSize(sampleRate_, k) - 1; // the Diffuser's delay is its size
        wideDecorr_[k].c    = tankv::tuning().wideDecorrCoeff;
        p += wideBufSize(sampleRate_, k);
    }
#endif
    applyTankVoicing();
}

void Tank::setTankVoicing([[maybe_unused]] int v)
{
#ifndef RV_FIXED_VOICINGS
    tankVoicing_ = std::clamp(v, 0, tankv::kNumVoicings - 1);
    if (!ok_) return;
    applyTankVoicing();
    reset(); // primed_ = false: the next process() snaps every setting (stage counts, mix) to the voicing
#endif
}

void Tank::setFLowCutVoicing([[maybe_unused]] int v)
{
#ifndef RV_FIXED_VOICINGS
    fLowCut_ = std::clamp(v, 0, tankv::kNumFLowCuts - 1);
    if (!ok_) return;
    applyTankVoicing();
    reset();
#endif
}

RV_SIZE_OPT void Tank::applyTankVoicing()
{
#if RV_TANKV_BUILT >= 1
    const auto& t = tankv::tuning();
    for (size_t s = 0; s < springs_.size(); ++s) {
#if RV_TANKV_BUILT >= 3
        float delays[tankv::kNumDiffusers];
        for (size_t k = 0; k < size_t(tankv::kNumDiffusers); ++k) delays[k] = t.diffMs[k] * t.diffSpringScale[s] * 0.001f * sampleRate_;
        springs_[s].setDiffusion(diffBuf_[s].data(), diffSize_[s].data(), delays,
                                 tankv::hasDiffusion(tankVoicing_) ? tankv::kNumDiffusers : 0, t.diffCoeff);
#endif
        springs_[s].setHighT60Ratio(tankv::hasGentle(tankVoicing_)        ? t.gentleHighT60Ratio
                                    : tankv::hasTransducers(tankVoicing_) ? t.tdHighT60Ratio // voicing 5+
                                                                          : Spring::kHighT60Ratio);
        springs_[s].setHighCeiling(tankv::hasTransducers(tankVoicing_) ? t.tdHighCeilHz : Spring::kHighCeilingHz);
        if (tankv::hasSweep(tankVoicing_)) springs_[s].setHighPathVoicing(t.hiXoverRatio, true, t.hiAlignMs);
        else springs_[s].setHighPathVoicing(Spring::kHighPassRatio, false, 0.0f);
    }
#if RV_TANKV_BUILT >= 4
    // The low cut: 4's, or 7's re-sized one (F round 2: one of its gentler
    // steps, Renderer-only; 0 = F's own).
    const bool lc7 = tankv::hasGentleMakeup(tankVoicing_);
    const tankv::LowCutStep lc = tankv::fLowCut(t, fLowCut_);
    gentleHp_.setHighpass(lc7 ? lc.hpHz : t.gentleHpHz, lc7 ? lc.hpQ : t.gentleHpQ, sampleRate_);
    gentleShelf_.setLowShelf(lc7 ? lc.shelfHz : t.gentleShelfHz, lc7 ? lc.shelfDb : t.gentleShelfDb, sampleRate_);
#endif
#if RV_TANKV_BUILT >= 7 && !defined(RV_FIXED_VOICINGS)
    // ... and the makeup's copy of it on the raw input (prepare(); the
    // firmware's step never changes).
    lcShHp_.setHighpass(lc.hpHz, lc.hpQ, sampleRate_);
    lcShShelf_.setLowShelf(lc.shelfHz, lc.shelfDb, sampleRate_);
#endif
#if RV_TANKV_BUILT >= 7
    tdTone_ = -1.0f; // the coil and pickup corners again (controlTick)
    hiT60Set_.fill(-1.0f);
#endif
    keyMode_ = -1; // the Springs' settings again (stage counts, pickups)
#endif
}

float Tank::loopDampingScale() const
{
    // The tank voicing's Loop damping (TankVoicing.h): 4's x1.5, 5+'s x3.6.
#if RV_TANKV_BUILT >= 7
    if (tankv::hasShipFixes(tankVoicing_)) { // TONE re-map: today's damping fully left, easing to 5's by noon
        const auto& t = tankv::tuning();
        const float w = tankv::toneDarkWeight(keyTone_, t.toneDarkCurve);
        // ... and toward tdDampingDecayMaxScale at the top of DECAY (the
        // highs never ring as long as a DECAY-max tail).
        const float u = std::clamp((keyDecay_ - t.tdDampingDecayFrom) / (1.0f - t.tdDampingDecayFrom), 0.0f, 1.0f);
        const float sc = t.tdDampingScale * std::exp(u * u * (3.0f - 2.0f * u) * std::log(t.tdDampingDecayMaxScale / t.tdDampingScale));
        return sc * std::exp(w * std::log(t.toneDarkDampingScale / sc));
    }
#endif
#if RV_TANKV_BUILT >= 5
    if (tankv::hasTransducers(tankVoicing_)) return tankv::tuning().tdDampingScale;
#endif
#if RV_TANKV_BUILT >= 4
    if (tankv::hasGentle(tankVoicing_)) return tankv::tuning().gentleDampingScale;
#endif
    return 1.0f;
}

RV_SIZE_OPT modes::StereoMix Tank::stereoMixFor(int mode) const
{
    // The SPRINGS 3 voicing's mix (modeMix), then the tank voicing's width.
    modes::StereoMix m = modeMix(mode);
    if (tankv::hasTogether(tankVoicing_)) {
        // Voicing 2: every Spring in both outputs (no side), the width from D.
        const auto& t = tankv::tuning();
        for (auto& sd : m.side) sd = 0.0f;
        m.decorr = mode == 0 ? t.togetherW1 : mode == 1 ? t.togetherW2 : t.togetherW3;
        if (tankv::hasWide(tankVoicing_) && mode > 0) {
            // Voicing 6: the Springs' difference back, through D2 (process()),
            // with D(mid) at wideW. 1 Spring has no difference: as 2.
            m.side[0] = mode == 2 ? t.wideSide3 : t.wideSide;
            m.side[1] = -m.side[0];
            m.decorr  = t.wideW;
#ifndef RV_FIXED_VOICINGS
            // F round 2 "coupled wide" / "swell" (Springs3Voicing.h fParts):
            // position 3's own side gains into D2. Renderer-only.
            if (mode == 2 && s3Voicing_ != springs3::kToday) {
                const auto ws = springs3::fParts(s3Voicing_).wideSides;
                if (ws[0] != 0.0f || ws[1] != 0.0f || ws[2] != 0.0f)
                    for (size_t k = 0; k < 3; ++k) m.side[k] = ws[k];
            }
#endif
        }
    }
    return m;
}

RV_SIZE_OPT void Tank::reset()
{
    if (!ok_) return;
#ifndef RV_FIXED_VOICINGS
    swFast_ = swSlow_ = swHold_ = 0.0f; // SPRINGS 3 "coupled swell"'s hit detector
#endif
    for (auto& s : springs_) s.reset();
#if RV_TANKV_BUILT >= 1
    sweep_.reset();
#endif
#if RV_TANKV_BUILT >= 2
    dBass_.reset();
#endif
#if RV_TANKV_BUILT >= 4
    gentleHp_.reset();
    gentleShelf_.reset();
#endif
#if RV_TANKV_BUILT >= 5
    tdIn_.reset();
    for (auto& f : tdOut_) f.reset();
    tdEvenAvg_.reset();
#endif
#if RV_TANKV_BUILT >= 6
    for (auto& d : wideDecorr_) {
        std::fill(d.buf, d.buf + d.size, 0.0f);
        d.w = 0;
    }
#endif
#if RV_TANKV_BUILT >= 7
    for (auto& f : gmHp_) f.reset();
    gmAccIn_ = gmAccOut_ = gmIn_ = gmOut_ = 0.0f;
    gmGain_ = 1.0f;
    lcShHp_.reset();
    lcShShelf_.reset();
    gmShAccIn_ = gmShAccOut_ = gmShIn_ = gmShOut_ = 0.0f;
    tdEvenHp_.reset();
    tdDarkLp_.reset();
    tdAccAll_ = tdAccLp_ = tdAll_ = tdLp_ = tdDarkDb_ = 0.0f;
#endif
    for (auto& d : decorrelator_) {
        std::fill(d.buf, d.buf + d.size, 0.0f);
        d.w = 0;
    }
    for (auto& s : shelfSplit_) s.reset();
    driveIn_.reset();
    tilt_.reset();
    hitBlend_ = 0.0f;
    for (auto& d : driveOut_) d.reset();
    splash_.reset();
    clangLp_.reset();
    clangEnv_ = 0.0f;
    clangCeilPush_ = 1.0f;
    splashInput_ = 1.0f;
    kick_.reset();
    for (auto& w : wobble_) w.reset();
    transport_.reset();
    echo_.reset();
    clock_.reset();
    numPendingClocks_ = numClockQ_ = 0;
    sampleClock_      = 0;
    echoW_ = echoWFrom_ = fbFrom_ = fbTo_ = ginFrom_ = ginTo_ = 0.0f;
    division_ = -1;
    levelAcc_ = levelMs_ = 0.0f;
    for (auto& f : excHp_) f.reset();
    for (auto& f : excLp_) f.reset();
    excAccBroad_ = excAccBand_ = excBroad_ = excBand_ = 0.0f;
    for (auto& f : bkHp_) f.reset();
    bkAccIn_ = bkAccOut_ = bkIn_ = bkOut_ = 0.0f;
    bkGain_  = 1.0f;
    toneReturn_.reset();
    for (auto& f : trHp_) f.reset();
    trAccIn_ = trAccOut_ = trIn_ = trOut_ = 0.0f;
    trGain_  = 1.0f;
    trMakeup_.snap(1.0f);
    trIdle_  = false; // the first tick decides
    // Power-up (and reset): the trim starts turned all the way down, so the
    // first sound can only come in too quiet, never too hot (the first chord
    // of a skank peaked ~3 dB over the rest: its attack reached the Springs
    // before the first control tick had heard it). One tick later it reads.
    excTrimFrom_ = excTrimTo_ = drive::dbToGain(-drive::kExcMaxDb);
    susFast_ = susFill_ = susFillIn_ = susFed_ = susHeld_ = susLn_ = susAim_ = susPeak_ = susPeakEnv_ = 0.0f;
    susGain_ = 1.0f;
    susFastPk_ = susStill_ = susKHold_ = susSince_ = 0.0f;
    susKHw_ = kSusNoK;
    susGNeed_ = kSusNoNeed;
    susGNeedHold_ = 0.0f;
    susEngaged_ = false;
    inTrimFrom_ = inTrimTo_ = excTrimTo_;
    clatBuf_.fill(0.0f);
    clatPos_ = 0;
    compDrive_ = -1.0f;
    limitEnv_  = 0.0f;
    limitHold_ = 0;
    limitGain_ = 1.0f;
    outBits_.reset();
    s3SeriesFrom_ = s3SeriesTo_ = s3WFrom_ = 0.0f;
    s3InLp_.reset();
    s3SendLp_.reset();
    numPendingKicks_ = 0;
    // THROW: off again until the gate's next first rising edge (power-up).
    numPendingGates_ = 0;
    gateHigh_ = throwOn_ = throwParamHigh_ = false;
    thrReleasing_ = releaseThrow_ = false;
    thrPos_    = 1.0f;
    thrRelPos_ = 0.0f;
    sendNow_ = 1.0f;
    holdZ_ = holdBed_ = 0.0f;
    holdSendFrom_ = holdSendTo_ = 1.0f;
    duckEnv_ = 0.0f;
    duckFrom_ = duckTo_ = 1.0f;
    duckDb_       = 0.0f;
    duckHoldLeft_ = 0;
    for (auto& k : duckKey_) k.reset();
    holdArmed_ = true;
    tick_     = 0;
    gridTick_ = 0;
    springTurn_ = 0;
    primed_   = false;
}

void Tank::kick(int sampleOffset)
{
    if (numPendingKicks_ < kMaxPendingKicks)
        pendingKicks_[size_t(numPendingKicks_++)] = sampleOffset < 0 ? 0 : sampleOffset;
}

bool Tank::exitThrowMode()
{
    if (!throwOn_ || thrReleasing_) return false;
    releaseThrow_ = true;
    return true;
}

// The throw switches on (the first rising edge, or the next one after
// exitThrowMode()). The ramp starts from the send now in effect, so the
// switch itself never steps the send.
void Tank::latchThrow(float hs)
{
    if (throwOn_ && !thrReleasing_) return;
    const bool  layer = holdVoicing_ == throwhold::kVoicingLayer;
    const float now   = layer ? (hs > 1.0e-6f ? sendNow_ / hs : 1.0f) : sendNow_;
    thrPos_       = std::clamp(now, 0.0f, 1.0f);
    throwOn_      = true;
    thrReleasing_ = false;
}

void Tank::gate(bool high, int sampleOffset)
{
    if (numPendingGates_ < kMaxPendingGates)
        pendingGates_[size_t(numPendingGates_++)] = GateEvent{sampleOffset < 0 ? 0 : sampleOffset, high};
}

void Tank::clock(int sampleOffset)
{
    if (numPendingClocks_ < kMaxPendingKicks)
        pendingClocks_[size_t(numPendingClocks_++)] = sampleOffset < 0 ? 0 : sampleOffset;
}

void Tank::feedClocks()
{
    int k = 0;
    while (k < numClockQ_ && int32_t(clockQ_[size_t(k)] - sampleClock_) <= 0) clock_.edge(clockQ_[size_t(k++)]);
    if (k == 0) return;
    for (int j = k; j < numClockQ_; ++j) clockQ_[size_t(j - k)] = clockQ_[size_t(j)];
    numClockQ_ -= k;
}

RV_SIZE_OPT void Tank::echoTick(float decayKnob, float tensionKnob, bool fresh, bool snap)
{
    // SPRINGS 3 echo mode (EchoVoicing.h, ADR 0041); its glide is in
    // controlTick. The clock: the host's tempo (Plugin), else the gate's (lost after a
    // while without pulses), else none.
    clock_.update(sampleClock_);
    const float beat = hostBpm_ > 0.0f ? 60.0f / hostBpm_ : clock_.beatSamples() / sampleRate_;
    if (beat > 0.0f) {
        // TENSION's seven zones, long -> short, with a little hysteresis at
        // each border (kDivisionHysteresis).
        const float u = tensionKnob * float(echo::kNumDivisions);
        if (division_ < 0 || u < float(division_) - echo::kDivisionHysteresis
            || u > float(division_ + 1) + echo::kDivisionHysteresis)
            division_ = std::clamp(int(u), 0, echo::kNumDivisions - 1);
        float secs = echo::kDivisionBeats[size_t(division_)] * beat;
        while (secs > echo::kMaxSeconds) secs *= 0.5f; // longer than the tape: half
        echoSecs_ = secs;
    } else {
        division_ = -1;
        echoSecs_ = echo::kFreeLongSeconds
                  * std::exp(tensionKnob * std::log(echo::kFreeShortSeconds / echo::kFreeLongSeconds));
    }
    // DECAY = feedback: CLEAN and DRIVEN's curve, KICKED's (both persistent
    // at the top, KICKED from a little lower), blended with the ATTITUDE Morph.
    const float base = echo::feedbackBase(decayKnob);
    const float fb   = (attW_[0] + attW_[1]) * echo::feedbackRise(base, decayKnob, echo::kCleanFrom)
                     + attW_[2] * echo::feedbackRise(base, decayKnob, echo::kKickedFrom);
    fbFrom_ = snap ? fb : fbTo_;
    fbTo_   = fb;
    // The input onto the tape at the feedback's own gain: every repeat a step
    // down from the hit, the first included (EchoVoicing.h "The first repeat").
    const float gin = std::clamp(fb, echo::kFirstRepeatMin, echo::kFirstRepeatMax);
    ginFrom_ = snap ? gin : ginTo_;
    ginTo_   = gin;
    if (echoW_ > 0.0f || echoWFrom_ > 0.0f)
        echo_.tick(echoSecs_, smoothed_[size_t(ParamId::Wobble)], snap || fresh); // a fresh tape starts at the time
}

RV_SIZE_OPT void Tank::controlTick(bool snap)
{
#if RV_TANKV_BUILT >= 1
    snapNow_ = snap;
#endif
    for (const auto& p : kParams) {
        const size_t i = static_cast<size_t>(p.id);
        if (snap || p.kind != ParamKind::Knob) smoothed_[i] = values_[i];
        else smoothed_[i] += tickCoeff_[i] * (values_[i] - smoothed_[i]);
    }
    float decay = smoothed_[size_t(ParamId::Decay)];
    float tension = smoothed_[size_t(ParamId::Tension)];
    const float tone  = smoothed_[size_t(ParamId::Tone)];

    // SPRINGS: a switch, so never smoothed here. A change starts a fade of
    // the output mix from the gains playing right now (mixCur_), so a flip
    // in the middle of a fade carries on smoothly from where it was.
    int mode = normalisedToSwitch(values_[size_t(ParamId::Springs)]);
    springsPos_ = mode; // the panel's position (the gate's role, ThrowHold.h gateRole)
    // Position 3 = echo mode (ADR 0041): the echo glides in (echoTick, after
    // the Morph below: the feedback follows ATTITUDE), and the Springs play
    // position 2 at the fixed tank: DECAY and TENSION are the echo's, so the
    // Springs glide from the knobs' tank to EchoVoicing.h's with the echo.
    // The echo fades in on a fresh tape, and out, over springs3::kGlideSeconds.
    const float decayKnob = decay, tensionKnob = tension;
    bool echoFresh = false;
    {
        const bool  echoPos = echoMode_ && mode == 2;
        const float target  = echoPos ? 1.0f : 0.0f;
        echoFresh = target > 0.0f && echoW_ <= 0.0f;
        if (echoFresh) echo_.clearTape();
        echoWFrom_ = snap ? target : echoW_;
        echoW_     = snap ? target : (target > echoW_ ? std::min(target, echoW_ + s3Step_) : std::max(target, echoW_ - s3Step_));
        if (echoPos) mode = 1;
        if (echoW_ > 0.0f) { // exactly the knobs otherwise (positions 1 and 2, bit for bit)
            // (1 - w) x + w y: exactly the fixed tank once the glide is done.
            decay   = (1.0f - echoW_) * decay + echoW_ * springsDecay_;
            tension = (1.0f - echoW_) * tension + echoW_ * echo::kSpringsTension;
        }
    }
    if (snap) {
        mode_     = mode;
        mixCur_   = mixTo_ = mixFrom_ = stereoMixFor(mode);
        trimCur_  = trimTo_ = trimFrom_ = modeTrim(mode);
        mixScale_ = trimCur_ / std::sqrt(modes::mixPower(mixCur_));
        fadePos_  = 1.0f;
    } else if (mode != mode_) {
        mode_     = mode;
        mixFrom_  = mixCur_;
        trimFrom_ = trimCur_;
        mixTo_    = stereoMixFor(mode);
        trimTo_   = modeTrim(mode);
        fadePos_  = 0.0f;
    }
    // SPRINGS 3 palette (Springs3Voicing.h): the Springs glide into position
    // 3's voicing (and back out) over springs3::kGlideSeconds, while the
    // output mix fades as above. At 0 everything is today's, bit for bit.
    if (springs3::kPaletteBuilt) {
        const float target = mode_ == 2 && s3Voicing_ != springs3::kToday ? 1.0f : 0.0f;
        s3WFrom_ = snap ? target : s3W_;
        s3W_ = snap ? target : (target > s3W_ ? std::min(target, s3W_ + s3Step_) : std::max(target, s3W_ - s3Step_));
        const springs3::Voicing& v3 = springs3::voicing(s3Voicing_);
        const float series = s3W_ * v3.series;
        // The Sustain trim's ceiling (ADR 0035): voicings that build up more
        // on held notes may trim further, so the tank still tames itself
        // rather than leaving it to the limiter (Springs3Voicing.h).
        s3SusMaxDb_ = s3W_ > 0.0f ? drive::kSusGentleMaxDb + s3W_ * (v3.sustainMaxDb - drive::kSusGentleMaxDb)
                                  : drive::kSusGentleMaxDb;
        s3SeriesFrom_ = snap ? series : s3SeriesTo_;
        s3SeriesTo_   = series;
        // Coupled Loops (voicings 8, 10): the rotation follows the glide. It
        // lets go in the Howl zone (KICKED, top of DECAY; ADR 0019): coupled,
        // the three Loops locked into one frozen howl (the M6 Howl grid's
        // movement check failed 3 of 18), so there the Springs howl apart,
        // as today.
        const float howl = drive::howlZone(decay) * attW_[2];
#ifndef RV_FIXED_VOICINGS
        const float turn = springs3::hasInputWeights(s3Voicing_) ? s3SwellTurn_ : 1.0f; // "coupled swell" (updateBaseSettings)
#else
        constexpr float turn = 1.0f;
#endif
        const CoupleMatrix m = coupleMatrix(s3W_ * (1.0f - howl) * v3.couplingAngle * turn, v3.couplingKind);
        s3CoupleFrom_ = snap ? m : s3CoupleTo_;
        s3CoupleTo_   = m;
        s3Coupled_    = v3.couplingKind != springs3::kCoupleNone && (s3WFrom_ > 0.0f || s3W_ > 0.0f);
#ifndef RV_FIXED_VOICINGS
        if (springs3::hasInputWeights(s3Voicing_)) { // "coupled swell": the input split, ramped per sample (processCoupled);
            // let go in the Howl zone with the coupling (Spring A alone took the
            // bursts there and the Howl froze: M6 Howl grid 17/18, movement)
            const float sw = s3W_ * s3SwellAmt_ * (1.0f - howl);
            s3SwellFrom_ = snap ? sw : s3SwellTo_;
            s3SwellTo_   = sw;
        }
#endif
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
    outBits_.setTarget(att, snap); // the output's bit depth fades on its own (OutputBits.h)
    echoTick(decayKnob, tensionKnob, echoFresh, snap);
    if (snap || attW_ != voiceW_) { // the blends only when the Morph moved
        voice_  = dsp::blendVoice(attW_);
        kick_.setAttitude(attW_);
        voiceW_ = attW_;
    }
    const drive::Voice& voice = voice_;
    const float drive = smoothed_[size_t(ParamId::Drive)];

    // HOLD (ADR 0040, ThrowHold.h): CLEAN and DRIVEN only (KICKED keeps its
    // Howl there: weight exactly 0 once the Morph reaches KICKED). The zone
    // sets the Springs' T60 (updateBaseSettings); the bed weight sets the
    // freeze / layer send and the ducking, ramped over the next tick.
    {
        const float z = throwhold::zone(decay);
        // Armed when DECAY enters the zone outside KICKED; KICKED inside the
        // zone disarms it until DECAY leaves the zone (the Howl flip calms
        // into the plain DECAY tail, as before the Hold; ADR 0040).
        if (z <= 0.0f) holdArmed_ = true;
        else if (attW_[2] > 0.0f) holdArmed_ = false;
        const float notKicked = holdNotKicked();
        holdZ_   = z * notKicked;
        holdBed_ = throwhold::bedWeight(z) * notKicked;
        const float hs = throwhold::holdSend(holdVoicing_, holdBed_);
        holdSendFrom_ = snap ? hs : holdSendTo_;
        holdSendTo_   = hs;
        float target = 0.0f; // dB of dip asked for by the key
        if (holdBed_ > 0.0f) {
            // The key's peak level (dBFS) -> the dip, x the bed weight.
            constexpr float kDb = 20.0f / 2.302585093f;
            const float envDb = kDb * std::log(std::max(duckEnv_, 1.0e-9f));
            const float amt = std::clamp((envDb - throwhold::kDuckFloorDb) / (throwhold::kDuckFullDb - throwhold::kDuckFloorDb), 0.0f, 1.0f);
            target = throwhold::kDuckDepthDb[duckVoicing_] * holdBed_ * amt;
        }
        // Falls fast, holds, then comes back on a short curve.
        if (snap) duckDb_ = target, duckHoldLeft_ = 0;
        else if (target >= duckDb_) {
            duckDb_ += duckDbAtt_ * (target - duckDb_);
            duckHoldLeft_ = duckHoldTicks_;
        } else if (duckHoldLeft_ > 0) --duckHoldLeft_;
        else duckDb_ += duckDbRel_ * (target - duckDb_);
        if (duckDb_ < 1.0e-4f) duckDb_ = 0.0f;
        const float duck = duckDb_ > 0.0f ? drive::dbToGain(-duckDb_) : 1.0f;
        duckFrom_ = snap ? duck : duckTo_;
        duckTo_   = duck;
    }
    const float splashAmt = smoothed_[size_t(ParamId::Splash)];

    // M7: Splash and Kick follow the Morph weights (their tables blend like
    // the drive voicing: no steps on an ATTITUDE flip); WOBBLE glides.
#if RV_TANKV_BUILT >= 7
    if (tankv::hasShipFixes(tankVoicing_)) {
        // 7: the transducers darken the crash against the tail most at low
        // DRIVE; the Clang and the Clatter are lifted there (TankVoicing.h
        // tdSplashLiftDb), easing to none by DRIVE 0.8 (process()).
        const auto& t = tankv::tuning();
        const float u = std::clamp((t.tdSplashLiftTo - smoothed_[size_t(ParamId::Drive)]) / (t.tdSplashLiftTo - t.tdSplashLiftFrom), 0.0f, 1.0f);
        splashLift_ = drive::dbToGain((1.0f - attW_[2]) * t.tdSplashLiftDb * u + attW_[2] * t.tdSplashLiftKickedDb);
    }
#endif
    splash_.set(attW_, splashAmt, splashDrive_, splashInput_); // DRIVE's gain on the Clang / Bite, the INPUT gain (below, on DRIVE moves)
    float wobbleScale = splash::wobbleDecayScale(decay); // Loop depth eased at long DECAYs
#if RV_TANKV_BUILT >= 7
    if (tankv::hasShipFixes(tankVoicing_) && smoothed_[size_t(ParamId::Wobble)] < 0.5f) {
        // 7: WOBBLE's left side (random wow, in the Loops) a little gentler
        // at the top of DECAY, where its wow builds up over more round trips
        // in 7's denser tail than the right side's (TankVoicing.h).
        const auto& t = tankv::tuning();
        const float u = std::clamp((decay - t.tdDampingDecayFrom) / (1.0f - t.tdDampingDecayFrom), 0.0f, 1.0f);
        wobbleScale *= 1.0f + u * u * (3.0f - 2.0f * u) * (t.tdWobbleLeftDecayMax - 1.0f);
    }
#endif
    for (auto& w : wobble_) w.setAmount(smoothed_[size_t(ParamId::Wobble)], wobbleScale);
    transport_.setAmount(smoothed_[size_t(ParamId::Wobble)]);
    // Tank level for KICKED's energy-dependent rattle (and the LoopSat fade
    // below): smoothed RMS of the wet mid over the last tick, scaled by
    // SPLASH. The mid is summed after DRIVE's heard gain (ADR 0033), so it is
    // divided back out here: the level is the Springs' own, whatever DRIVE.
    const float invHeard = 1.0f / driveInSettings_.heard;
    const float tickMs = levelAcc_ * (invHeard * invHeard / float(kControlInterval)); // last tick, unsmoothed
    levelMs_ += levelCoeff_ * (tickMs - levelMs_);
    levelAcc_ = 0.0f;
    splash_.setTankLevel(std::sqrt(levelMs_) * splashAmt);
    // Excitation trim (M8, DriveVoicing.h): slow band / full power of the
    // driven input -> input trim, held below the gate, ramped over the tick.
    const float tickIn = excAccBroad_ * (1.0f / float(kControlInterval)); // raw input power, last tick
    {
        constexpr float kInv = 1.0f / float(kControlInterval);
        excBroad_ += excCoeff_ * (tickIn - excBroad_);
        susFast_ += susFastCoeff_ * (tickIn - susFast_);
        excBand_ += excCoeff_ * (excAccBand_ * kInv - excBand_);
        excAccBroad_ = excAccBand_ = 0.0f;
        constexpr float kMaxLog = drive::kExcMaxDb * (2.302585093f / 20.0f);
        float trim = excTrimTo_;
        if (excBroad_ > excGate_) {
            const float l = 0.5f * drive::kExcStrength
                          * std::log(drive::kExcRefShare * excBroad_ / std::max(excBand_, 1.0e-12f));
            trim = std::exp(std::clamp(l, -kMaxLog, kMaxLog));
        }
        excTrimFrom_ = snap ? trim : excTrimTo_;
        excTrimTo_   = trim;
    }
    // Sustain trim (M8, DriveVoicing.h): while the input is held, ease the
    // Springs' input down so the wet's peaks, where the limiter reads them,
    // sit no higher than kSusTargetDb while it arrives and kSusSettledLiftDb
    // above that once settled (at DRIVE 0; DRIVE's heard gain rides on top,
    // ADR 0033); let go as soon as it isn't held.
    // Feed-forward, so it can't hunt: the tank's build-up gain K for this
    // sound = the wet's peak envelope over the envelope of what went into
    // the Springs (the raw input x the Sustain trim squared, lagged the way
    // the tank fills), both released alike. K doesn't move with the trim,
    // only with the sound and the settings, so the trim that lands the peaks
    // on the target is read off it.
    {
        // The wet's peak envelope: the last tick's peak, or the envelope
        // released over kSusSlowSeconds (a tick is 0.7 ms, far shorter than
        // a low note's cycle: the envelope holds its peaks between them).
        susPeakEnv_ = std::max(susPeak_, susPeakEnv_ * susPeakRelease_);
        susPeak_    = 0.0f;
        // DRIVE's heard gain divided out (ADR 0033): the Springs' own level,
        // so DRIVE's few extra dB stay a deliberate throw, not trimmed away.
        const float pk2 = susPeakEnv_ * susPeakEnv_ * (invHeard * invHeard);
        susFill_   += susFillCoeff_ * (susGain_ * susGain_ * tickIn - susFill_); // what the tank has been fed, as it fills
        susFillIn_ += susFillCoeff_ * (tickIn - susFillIn_);                     // the same, untrimmed
        // ... with the wet's envelope release (squared: a power), so a rise
        // or a fall reads the same on both sides of K.
        susFed_ = std::max(susFill_, susFed_ * susPeakRelease_ * susPeakRelease_);
        const float dt   = float(kControlInterval) / sampleRate_;
        const bool  gentle = susVoicing_ == drive::kSusVoicingGentle; // round 3
        const bool  held = susOn_ && susVoicing_ != drive::kSusVoicingOff && excBroad_ > excGate_ && susFast_ >= susHeldRatio_ * excBroad_;
        // Held two ways: within kSusHeldDropDb of its slow level for
        // kSusOnsetSeconds, or sooner if its fast level has stayed within
        // kSusStillDropDb of its own peak since it began for kSusStillSeconds
        // (an organ or a pad holds its level; a hit or a stab is already
        // falling away by then). Latched until the input stops being held.
        susFastPk_  = held ? std::max(susFastPk_, susFast_) : 0.0f;
        susStill_   = held && susFast_ >= susStillRatio_ * susFastPk_ ? susStill_ + dt : 0.0f;
        susHeld_    = held ? susHeld_ + dt : 0.0f;
        susEngaged_ = held && (susEngaged_ || susHeld_ >= drive::kSusOnsetSeconds || susStill_ >= drive::kSusStillSeconds);
        if (susEngaged_) {
            constexpr float kMaxLn = drive::kSusMaxDb * (2.302585093f / 20.0f);
            constexpr float kBand  = drive::kSusSteadyDb * (2.302585093f / 20.0f);
            // K is a high-water mark: the highest build-up read while this
            // sound is held, kept for kSusKHoldSeconds, then let down at
            // kSusKReleaseDbPerS. WOBBLE's Drift moves a held note on and off
            // the tank's modes (fully left a drone's wet swings ~10 dB), so the
            // trim answers the loudest swell, then sits still through the rest
            // instead of chasing each one.
            const float kLn = std::log(std::max(pk2, 1.0e-24f) / std::max(susFed_, 1.0e-24f));
            if (kLn >= susKHw_) { // a new high (or the first read: susKHw_ starts at kSusNoK)
                susKHw_   = kLn;
                susKHold_ = drive::kSusKHoldSeconds;
            } else if (susKHold_ > 0.0f) {
                susKHold_ -= dt;
            } else {
                susKHw_ = std::max(kLn, susKHw_ - susKRelease_);
            }
            susSince_ += dt;
            if (gentle) {
            // Round 3, gentle (DriveVoicing.h "Sustain trim voicings"): a
            // safety net, not a level rider. `now` is the trim that would put
            // the peaks on the target this tick (this tick's K: with no trim
            // it reads the wet's real peaks); kept as a low-water mark, the
            // deepest met, held kSusGentleNeedHoldSeconds, then let up at
            // kSusGentleNeedReleaseDbPerS, so the trim answers the loudest
            // swell and holds (no dip and swell). (Round 2's high-water K
            // times a later, louder input read swells that never came.)
            constexpr float kDb = 2.302585093f / 20.0f;
            const float now = 0.5f * (std::log(susGTarget_ / std::max(susFillIn_, 1.0e-24f)) - kLn);
            if (now <= susGNeed_) {
                susGNeed_     = now;
                susGNeedHold_ = drive::kSusGentleNeedHoldSeconds;
            } else if (susGNeedHold_ > 0.0f) {
                susGNeedHold_ -= dt;
            } else {
                susGNeed_ = std::min(now, susGNeed_ + susGNeedRelease_);
            }
            // That swell's peak untrimmed (dBFS) -> the cut: none up to
            // kSusGentleFromDb (the limiter would hardly work), then a ramp
            // that lands the peaks on the target by kSusGentleFullDb, then
            // target-keeping, at most kSusGentleMaxDb. No step anywhere, so a
            // sound that creeps up a little moves the trim a little.
            constexpr float kFrom  = drive::kSusGentleFromDb, kFull = drive::kSusGentleFullDb, kTgt = drive::kSusGentleTargetDb;
            constexpr float kSlope = (kFull - kTgt) / (kFull - kFrom);
            const float peakDb = kTgt - susGNeed_ * (1.0f / kDb);
            const float cutDb  = peakDb <= kFrom ? 0.0f : peakDb < kFull ? (peakDb - kFrom) * kSlope : peakDb - kTgt;
            // The Big Knob's makeup (DriveVoicing.h, Renderer voicings 1-3;
            // 0 dB otherwise) is ours to take back on top: it lifted what
            // reaches the Springs, so the trim may cut that much deeper.
            // (A SPRINGS 3 palette voicing may move the ceiling itself.)
            const float bkDb = std::max(0.0f, std::log(bkGain_)) * (1.0f / kDb);
            const float susMaxDb = springs3::kPaletteBuilt ? s3SusMaxDb_ : drive::kSusGentleMaxDb;
            susAim_ = -std::min(cutDb, susMaxDb + bkDb) * kDb;
            // One smooth glide down (~0.3 s), a slow one up.
            susLn_ += (susAim_ < susLn_ ? susGDownCoeff_ : susGUpCoeff_) * (susAim_ - susLn_);
            } else {
            // Peaks^2 with a trim t = K x t^2 x (the input, as the tank fills
            // with it): t^2 = target^2 / (K x in). The input's fill-time
            // level: on a swell the trim keeps up.
            float want = 0.5f * (std::log(susTarget_ / std::max(susFillIn_, 1.0e-24f)) - susKHw_);
            // Arriving (the first kSusSettleSeconds): aim at the target itself
            // and move down at kSusOnsetDownSeconds, so a held sound's first
            // peaks are caught. Settled: kSusSettledLiftDb more room (K is the
            // loudest swell met, so the usual peaks sit lower), a steady band
            // of +-kSusSteadyDb, and
            // outside it move only as far as its edge (a swell 1.5 dB past the
            // band costs 1.5 dB of trim, not 2.5), down at kSusDownSeconds.
            const bool  settled = susSince_ >= drive::kSusSettleSeconds;
            const float edge    = settled ? kBand : 0.0f;
            if (settled) want += drive::kSusSettledLiftDb * (2.302585093f / 20.0f);
            if (want < susLn_ - kBand) susAim_ = want + edge;
            else if (want > susLn_ + kBand) susAim_ = want - edge;
            susAim_ = std::clamp(susAim_, -kMaxLn, 0.0f);
            const float down = settled ? susDownCoeff_ : std::max(susDownCoeff_, susOnsetDownCoeff_);
            susLn_ += (susAim_ < susLn_ ? down : susUpCoeff_) * (susAim_ - susLn_);
          }
        } else {
            susLn_ -= (gentle ? susGLetGoCoeff_ : susLetGoCoeff_) * susLn_;
            if (susLn_ > -1.0e-6f) susLn_ = 0.0f; // let go all the way (and no denormals)
            susAim_   = 0.0f;
            susKHw_   = kSusNoK;
            susKHold_ = susSince_ = 0.0f;
            susGNeed_ = kSusNoNeed;
            susGNeedHold_ = 0.0f;
        }
        susGain_    = susLn_ < 0.0f ? std::exp(susLn_) : 1.0f;
    }
    // Big Knob makeup (DriveVoicing.h, voicings 1-3, TONE right of noon):
    // slow power into and out of the Tilt; give back a share of what the low
    // cut took out, held in silence. 1 (exactly) otherwise. With the Big
    // Knob after the Springs (the default) the Tilt has no low cut above
    // 20 Hz, so this only follows the tilt itself (as the prototype the
    // owner picked did); the return filter has its own makeup below.
    {
        constexpr float kInv = 1.0f / float(kControlInterval);
        bkIn_ += excCoeff_ * (bkAccIn_ * kInv - bkIn_);
        bkOut_ += excCoeff_ * (bkAccOut_ * kInv - bkOut_);
        bkAccIn_ = bkAccOut_ = 0.0f;
        if (tilt_.voicing() == drive::kToneVoicingToday || tone <= 0.5f) {
            bkGain_ = 1.0f;
        } else if (bkIn_ > excGate_) {
            constexpr float kMaxLog = drive::kBigKnobMakeupMaxDb * (2.302585093f / 20.0f);
            const float l = 0.5f * drive::kBigKnobMakeupShare * std::log(bkIn_ / std::max(bkOut_, 1.0e-12f))
                          + (2.302585093f / 20.0f) * drive::bigKnobPreTrimDb(tilt_.place(), tilt_.voicing(), tone, attW_, drive);
            bkGain_ = std::exp(std::clamp(l, -kMaxLog, kMaxLog));
        }
        // The Big Knob on the wet (DriveVoicing.h "TONE placement"): the
        // return filter's own makeup (the wet's power into / out of it,
        // slow, held while the wet is silent).
        trIn_ += excCoeff_ * (trAccIn_ * kInv - trIn_);
        trOut_ += excCoeff_ * (trAccOut_ * kInv - trOut_);
        trAccIn_ = trAccOut_ = 0.0f;
        if (tilt_.place() == drive::kTonePlacePre || tilt_.voicing() == drive::kToneVoicingToday || tone <= 0.5f) {
            trGain_ = 1.0f;
        } else if (trIn_ > excGate_) {
            constexpr float kMaxLog = drive::kPostMakeupMaxDb * (2.302585093f / 20.0f);
            const float l = 0.5f * drive::kPostMakeupShare * std::log(trIn_ / std::max(trOut_, 1.0e-12f))
                          + (2.302585093f / 20.0f) * drive::bigKnobPostTrimDb(tilt_.place(), tilt_.voicing(), tone);
            trGain_ = std::exp(std::clamp(l, -kMaxLog, kMaxLog));
        }
        if (tilt_.place() != drive::kTonePlacePre) {
            if (snap) trMakeup_.snap(trGain_);
            else trMakeup_.aim(trGain_, kControlInterval);
        }
        float trim = excTrimTo_ * susGain_ * bkGain_;
#if RV_TANKV_BUILT >= 7
        // Voicing 7: give back what the low cut took out (power in / out of
        // it, slow, held in silence), up to gentleMakeupMaxDb.
        gmIn_ += excCoeff_ * (gmAccIn_ * kInv - gmIn_);
        gmOut_ += excCoeff_ * (gmAccOut_ * kInv - gmOut_);
        gmAccIn_ = gmAccOut_ = 0.0f;
        if (!tankv::hasGentleMakeup(tankVoicing_)) {
            gmGain_ = 1.0f;
        } else if (gmIn_ > excGate_) {
            const float kMaxLog = tankv::tuning().gentleMakeupMaxDb * (2.302585093f / 20.0f);
            float l = std::log(gmIn_ / std::max(gmOut_, 1.0e-12f));
            if (tankv::hasShipFixes(tankVoicing_)) {
                // The same reading on the raw input (before DRIVE: KICKED's
                // own low products, which the low cut removes, made the makeup
                // grow with DRIVE): the smaller of the two.
                // The raw input above ~90 Hz (the Excitation trim's high-
                // passes), into and out of a copy of the low cut.
                gmShIn_ += excCoeff_ * (gmShAccIn_ * kInv - gmShIn_);
                gmShOut_ += excCoeff_ * (gmShAccOut_ * kInv - gmShOut_);
                l = std::min(l, std::log(gmShIn_ / std::max(gmShOut_, 1.0e-12f)));
            }
            gmGain_ = std::exp(std::clamp(0.5f * tankv::tuning().gentleMakeupShare * l, 0.0f, kMaxLog));
        }
        gmShAccIn_ = gmShAccOut_ = 0.0f;
        trim *= gmGain_;
        // TONE re-map (7): the coil darker left of noon, the pickup open right
        // of it, and the level that keeps (on the Springs' input, ramped).
        if (tankv::hasShipFixes(tankVoicing_)) {
            const float driveNow = smoothed_[size_t(ParamId::Drive)];
            const float kickedOpen = attW_[2] * driveNow * driveNow * driveNow; // KICKED's coil saturation
            if (snap || tone != tdTone_ || kickedOpen != tdDrive_) {
                const auto& t = tankv::tuning();
                const float wd = tankv::toneDarkWeight(tone, t.toneDarkCurve);
                const float wb = tankv::toneBrightWeight(tone, t.toneBrightCurve);
                const float inHz  = t.tdInHz * std::exp(wd * std::log(t.toneDarkInHz / t.tdInHz)
                                                        + 0.693147f * t.tdDriveOpenOct * kickedOpen);
                const float outHz = t.tdOutHz * std::exp(wb * std::log(t.toneBrightOutHz / t.tdOutHz));
                tdIn_.setLowpass(std::min(inHz, 0.45f * sampleRate_), t.tdInQ, sampleRate_);
                for (auto& f : tdOut_) f.setLowpass(std::min(outHz, 0.45f * sampleRate_), t.tdOutQ, sampleRate_);
                if (t.tdEvenHpHz > 0.0f)
                    tdEvenHp_.setHighpass(t.tdEvenHpHz * std::exp(wb * std::log(t.toneBrightEvenHpHz / t.tdEvenHpHz)), 0.7071f, sampleRate_);
                toneTrim_ = drive::dbToGain(wb * t.toneBrightDb);
                tdWd_     = wd;
                tdTone_   = tone;
                tdDrive_  = kickedOpen;
            }
            const auto& t = tankv::tuning();
            // Left of noon the tank darkens: give back what it takes, by how
            // much of the input is highs (power in / out of a low-pass at
            // toneDarkLpHz, x toneDarkShare, up to toneDarkDb): a snare gets
            // most of it back, a held pad (little up there) next to nothing.
            tdAll_ += excCoeff_ * (tdAccAll_ * kInv - tdAll_);
            tdLp_ += excCoeff_ * (tdAccLp_ * kInv - tdLp_);
            tdAccAll_ = tdAccLp_ = 0.0f;
            if (tdAll_ > excGate_) {
                const float lossDb = (10.0f / 2.302585093f) * std::log(tdAll_ / std::max(tdLp_, 1.0e-12f));
                tdDarkDb_ = tdWd_ * std::min(t.toneDarkDb, t.toneDarkShare * lossDb);
            }
            trim *= toneTrim_ * drive::dbToGain(tdDarkDb_ - t.tdDriveOpenDb * kickedOpen);
        }
#endif
        inTrimFrom_ = snap ? trim : inTrimTo_;
        inTrimTo_   = trim;
    }
    // Jolt on a (control rate): after the detune, scaled per Spring like the
    // Loop-delay Jolt (splash::kJoltSpringScale, M8), clamped below.
    const float joltA = joltOn_ ? splash_.allpassDelta() : 0.0f;

    // DriveIn settings (the INPUT gain, ADR 0033) and the DRIVE push on the
    // pickups (ADR 0022): only recomputed when DRIVE or the Morph moved
    // (they cost a few exp).
    const bool bigKnobPush = tilt_.voicing() == drive::kToneVoicingDriven;
    if (snap || drive != compDrive_ || attW_ != compW_ || (bigKnobPush && tone != compTone_)) {
        driveInSettings_ = dsp::driveInSettings(voice, drive);
        if (bigKnobPush) {
            // Big Knob voicing 3 (DriveVoicing.h): the lows pushed harder
            // into the transducer, highs and small signals as before.
            const float p = drive::bigKnob(drive::kToneVoicingDriven, tone).pushDb;
            const float g = drive::dbToGain(p);
            driveInSettings_.voice.fluxCutDb += p;
            driveInSettings_.preGain *= g;
            driveInSettings_.makeup /= g;
        }
        compTone_  = tone;
        push_      = drive::push(voice, drive);
        if constexpr (splash::kVoicingsBuilt) { // the Clang's ceiling credit for the pickups' push (SplashVoicing.h)
            const float share = attW_[0] * splash::kCeilPushShare[0] + attW_[1] * splash::kCeilPushShare[1]
                              + attW_[2] * splash::kCeilPushShare[2];
            clangCeilPush_ = std::exp(share * std::log(push_.out));
        }
        splashDrive_ = splash::splashDriveGain(drive::driveCurve(drive), dcNoon_, dcRef_);
        splashInput_ = driveInSettings_.inputGain;
        compDrive_ = drive;
        compW_     = attW_;
    }
    driveIn_.set(driveInSettings_, snap, kControlInterval);
    if (snap) {
        heardGain_.snap(driveInSettings_.heard);
        inputGain_.snap(driveInSettings_.inputGain);
    } else {
        heardGain_.aim(driveInSettings_.heard, kControlInterval);
        inputGain_.aim(driveInSettings_.inputGain, kControlInterval);
    }
    tilt_.set(tone, snap, kControlInterval);
    // Big Knob voicing 5 (DriveVoicing.h): the bump follows sharp hits (the
    // Splash's reading since the last tick), up at once, back over
    // kHitBumpReleaseS. On the tick grid, so any block size gives the same.
    {
        const float h = splash_.takeHitMax() * drive::kHitBumpGain;
        hitBlend_ = h > hitBlend_ * hitRelease_ ? h : hitBlend_ * hitRelease_;
        if (tilt_.voicing() == drive::kToneVoicingHits)
            tilt_.setHitBlend(hitBlend_ < 1.0f ? hitBlend_ : 1.0f, kControlInterval);
        if (tilt_.place() != drive::kTonePlacePre) { // the Big Knob on the wet (DriveVoicing.h "TONE placement")
            toneReturn_.set(tilt_.voicing(), tone, snap, kControlInterval);
            if (tilt_.voicing() == drive::kToneVoicingHits)
                toneReturn_.setHitBlend(hitBlend_ < 1.0f ? hitBlend_ : 1.0f, kControlInterval);
            // Skip the stage while it's an exact pass-through (noon and left,
            // makeup at 1). The followers' into-side keeps running (out =
            // in there), so the makeup starts from the right reading.
            const bool idle = toneReturn_.idle() && trMakeup_.value == 1.0f && trMakeup_.target == 1.0f;
            if (trIdle_ && !idle) {
                toneReturn_.clearFilters();
                trHp_[2] = trHp_[0];
                trHp_[3] = trHp_[1];
            }
            trIdle_ = idle;
        }
    }
    // The pickups' hardness is divided by the level DRIVE adds (ADR 0033), so
    // they bend the louder tail exactly as ADR 0022 voiced them: DRIVE's
    // extra level passes the pickups as level, not as extra grit (CLEAN
    // stays a tint; KICKED's squash doesn't run into the makeup's ceiling).
    for (auto& d : driveOut_) d.set(voice, push_.out / driveInSettings_.heard, push_.outFluxDb, push_.outAmount);
    {
        // DriveOut automatic makeup, linked across L/R so the image never
        // shifts: sqrt(level in / level out) of both channels together
        // (at least 1, at most kAutoMakeupMax), times the static wet makeup.
        const float in  = driveOut_[0].levelIn() + driveOut_[1].levelIn();
        const float out = driveOut_[0].levelOut() + driveOut_[1].levelOut();
        const float autoGain = std::clamp(std::sqrt(in / out), 1.0f, drive::kAutoMakeupMax);
        for (auto& d : driveOut_) d.setMakeup(autoGain * push_.wet, snap, kControlInterval);
    }

    // The Springs' settings (all but the Jolt on the allpass coefficient)
    // follow DECAY, TENSION, TONE, the Morph and SPRINGS only (not DRIVE since
    // ADR 0033: it no longer pushes the LoopSat). At rest
    // nothing is worked out again (M3 run 12: their exp/log/cos were a fixed
    // cost on every tick); after a move, only when a Spring takes new
    // settings, on its turn (TENSION's allpass coefficient, which every
    // Spring gets on every tick, at once: it's cheap).
    if (snap || decay != keyDecay_ || tension != keyTension_ || tone != keyTone_ || attW_ != keyW_
        || mode_ != keyMode_ || s3W_ != keyS3W_) {
        keyDecay_    = decay;
        keyTension_  = tension;
        keyTone_     = tone;
        keyW_        = attW_;
        keyMode_     = mode_;
        keyS3W_      = s3W_;
        if (springs3::kPaletteBuilt) updateShapes();
        baseAllpass_ = map::tensionCoefficient(tension);
        baseDirty_   = true;
        ++baseGen_;
    }

    // AntiRes: LoopSat quiet-tail fade (AntiRes.h). How hard the LoopSat
    // would bend the tail right now = the wet mid's power (the rattle's
    // smoothed level, or the last tick's if louder: a hit into a quiet tank
    // gets its saturation back at once) x the curve's hardness squared. It
    // scales every Spring's LoopSat blend: full above the fade, none below
    // its floor. A few multiplies per tick, nothing per sample. (The wet mid
    // is measured before DRIVE's heard gain, so the fade doesn't move with
    // DRIVE either, ADR 0033.)
    {
        const float kSat     = std::max(voice.loopKPos, voice.loopKNeg);
        const float satLevel = std::max(levelMs_, tickMs) * kSat * kSat;
        const float satGate  = std::clamp((satLevel - satFloorMs_) * satInvSpanMs_, 0.0f, 1.0f);
        for (auto& sp : springs_) sp.setLoopSatGate(satGate);
    }

    // One Spring per tick takes its new settings (all three on a snap). A
    // change reaches Springs B and C up to two ticks (1.3 ms) after A, up
    // to ~6 ms while TENSION or TONE move (below): well inside every
    // parameter's glide (TENSION 60 ms, DECAY 80 ms). It spreads the
    // Loop gain redesign (the costliest control work) so no audio block
    // carries all three (M3: that burst lifted the peak CPU ~12 points above
    // the average). M3 run 12: a TENSION move takes a Spring two ticks
    // (Spring::setSettings), and a Spring whose fC or damping moved (a heavy
    // redesign step) waits out every third tick. Ticks come every 32
    // samples, so at the Versio's 48-sample block every other block holds
    // two ticks (grid ticks 3n and 3n + 1); skipping 3n + 1 keeps it to one
    // heavy step per block. On the fixed tick grid, so any block size
    // renders the same. The turn moves on once the Spring has its settings.
    const bool   heavyOk = snap || gridTick_ != 1;
    const size_t turn    = size_t(springTurn_);
    bool turnDone = true;
    for (size_t i = 0; i < springs_.size(); ++i) {
        const float a = std::clamp(baseAllpass_ * shape_[i].allpassCoeff + joltA * splash::kJoltSpringScale[i],
                                   -splash::kMaxAllpassMagnitude, splash::kMaxAllpassMagnitude);
        const bool waits = !heavyOk && i == turn
                        && (springs_[i].redesignPending()
                            || (springGen_[i] != baseGen_
                                && (springTension_[i] != keyTension_ || springTone_[i] != keyTone_
                                    || springS3W_[i] != keyS3W_)));
        if (!snap && (i != turn || waits)) {
            // Off turn: only the Jolt's (and TENSION's) allpass coefficient,
            // so a hit's pitch lurch reaches every Spring on the same tick
            // (staggering it read as extra undulation on KICKED hits, owner).
            springs_[i].setAllpassCoeff(a);
            if (waits) turnDone = false;
            continue;
        }
        if (springGen_[i] != baseGen_) {
            if (baseDirty_) {
                updateBaseSettings(keyDecay_, keyTension_, keyTone_, voice);
                baseDirty_ = false;
            }
            updateSpringSettings(i);
        }
        SpringSettings s = springSet_[i];
        s.allpassCoeff   = a;
        // Echo mode built: Spring C is never run or heard (process()), so
        // it only keeps its turn (the same answers, so A and B take their
        // settings on the same ticks), without the maths.
        const bool done  = i == 2 && echoMode_ && !snap ? springs_[i].setSettingsUnheard(s) : springs_[i].setSettings(s, snap);
        if (i == turn) turnDone = done;
    }
    if (turnDone) springTurn_ = (springTurn_ + 1) % int(kMaxSprings);
    if (!snap) gridTick_ = (gridTick_ + 1) % 3;
}

void Tank::updateBaseSettings(float decay, float tension, float tone, const drive::Voice& voice)
{
    SpringSettings& base = baseSet_;
    base = SpringSettings{};
    // TENSION picks the tank (L, fC, a and M together); DECAY sets T60 and
    // nothing else (ADR 0026).
    base.loopDelaySeconds = map::tensionLoopDelaySeconds(tension);
    base.t60Seconds       = map::decayT60Seconds(decay);
    {
        // HOLD (ADR 0040): the zone glides T60 out toward minutes, in CLEAN
        // and DRIVEN; the Spring lets its Loop gain cap follow (hold), and
        // the high path keeps the plain DECAY's T60 (ThrowHold.h).
        const float hz = throwhold::zone(decay) * holdNotKicked();
        if (hz > 0.0f) {
            base.highT60Seconds = base.t60Seconds;
            base.t60Seconds     = throwhold::t60Seconds(base.t60Seconds, hz);
            base.hold           = hz;
        }
    }
    base.transitionHz     = map::tensionTransitionHz(tension);
    const springs3::Voicing& v3 = springs3::voicing(s3Voicing_);
    if (springs3::kPaletteBuilt && s3W_ > 0.0f) {
        // Position 3's voicing (Springs3Voicing.h): the whole tank longer or
        // shorter and its Chirp lower or higher. L is capped at the loosest
        // tank's before the per-Spring detune: the delay memory holds that
        // (Spring.cpp), and the Springs stay detuned against each other. A
        // shortened tank is floored (the pan tank's tight end).
        const float l = base.loopDelaySeconds;
        const float lv = std::clamp(l * v3.lengthScale, std::min(l, v3.minLoopSeconds), std::max(l, map::kLoopDelayMaxSeconds));
        base.loopDelaySeconds = l + s3W_ * (lv - l);
        base.transitionHz *= 1.0f + s3W_ * (v3.transitionScale - 1.0f);
    }
    // F round 2 "coupled swell": how much of the input goes into Spring A
    // follows how many round trips the tail lasts (T60 / L): a short or
    // loose tank has no time to hand it over (Springs3Voicing.h).
#ifndef RV_FIXED_VOICINGS
    if (springs3::hasInputWeights(s3Voicing_)) {
        const float u = std::clamp((base.t60Seconds / base.loopDelaySeconds - springs3::kSwellTripsFrom)
                                       / (springs3::kSwellTripsTo - springs3::kSwellTripsFrom), 0.0f, 1.0f);
        s3SwellAmt_ = u * u * (3.0f - 2.0f * u);
        // ... and the turn per trip grows with the trip's length (square
        // root): a loose tank hands A's energy over in fewer, bigger turns,
        // so the swell takes less different times across TENSION.
        s3SwellTurn_ = std::min(1.0f, std::sqrt(base.loopDelaySeconds / map::kTensionMidLoopDelaySeconds));
        swFastAtt_  = 1.0f - std::exp(-1.0f / (springs3::kSwellFastAttS * sampleRate_));
        swSlowC_    = 1.0f - std::exp(-1.0f / (springs3::kSwellSlowS * sampleRate_));
        swHoldStep_ = 1.0f / (springs3::kSwellHoldS * sampleRate_);
    }
#endif
    // In series: the send into the second tank follows DECAY (Springs3Voicing.h).
    if (springs3::kPaletteBuilt && v3.series > 0.0f)
        s3SeriesSend_ = v3.seriesSend
                      * std::exp(springs3::kSeriesExponent * std::log(springs3::kSeriesRefT60 * base.loopDelaySeconds
                                                                      / (springs3::kSeriesRefLoop * base.t60Seconds)));
    base.allpassCoeff     = map::tensionCoefficient(tension);
    base.dampingHz        = map::toneDampingHz(tone);
    base.highPathLevel    = map::toneHighPathLevel(tone);
    base.loopSatAmount    = voice.loopAmount;
    // The LoopSat keeps its ATTITUDE's fixed, gentle hardness: DRIVE no
    // longer pushes it (ADR 0033; ADR 0022's push squashed the tail on every
    // round trip and made DRIVE shorten it). Its slope stays <= 1: Loop
    // gain can only go down, never up. In the Howl zone that hardness sets
    // how loud the Howl settles (ADR 0019), as before (the push used to fade
    // out across the zone for that reason).
    const float howlAmt   = drive::howlZone(decay) * attW_[2];
    base.loopSatKPos      = voice.loopKPos;
    base.loopSatKNeg      = voice.loopKNeg;
    base.howl             = howlAmt;
    // AntiRes Micro-mod floor, always on (WOBBLE adds on top, per sample), plus
    // the Howl zone's movement (ADR 0019), both on the same L-modulation hook.
    base.modDepth         = antires::microModDepth(base.loopDelaySeconds) + antires::kHowlModDepth * base.howl;
    base.lfoDepth         = antires::kHowlLfoDepth * base.howl;
    // Sustain trim: the tank's fill time (its power builds and dies with
    // T60 / 13.8, 60 dB in T60), x kSusFillScale, for the input side of K
    // (DriveVoicing.h: the full fill time read K high while a sound arrived).
    susFillCoeff_ = 1.0f - std::exp(-float(kControlInterval) * 13.8f / (sampleRate_ * base.t60Seconds * drive::kSusFillScale));
    // ... and it moves down at no more than twice the rate the tank fills: a
    // long tail fills slowly, and its first echoes (which answer at once)
    // would otherwise read as a full tank and trim a held sound too far,
    // then let it back up over seconds (a slow swell at DECAY max).
    susDownCoeff_ = 1.0f - std::exp(-float(kControlInterval)
                                    / (sampleRate_ * std::max(drive::kSusDownSeconds,
                                                              drive::kSusDownPerFill * base.t60Seconds / 13.8f)));
    float susGlide = std::max(drive::kSusGentleDownSeconds, drive::kSusDownPerFill * base.t60Seconds / 13.8f);
#if RV_TANKV_BUILT >= 7
    // F round 2's gentler low cut: held sounds trimmed a little sooner (TankVoicing.h kFSusGlideScale).
    if (tankv::hasShipFixes(tankVoicing_)) susGlide *= tankv::fLowCut(tankv::tuning(), fLowCut_).susGlideScale;
#endif
    susGDownCoeff_ = 1.0f - std::exp(-float(kControlInterval) / (sampleRate_ * susGlide));
    int cap = modes::kStageCap[size_t(mode_)];
    activeStages_ = modes::tensionStages(tension, cap);
#if RV_TANKV_BUILT >= 1
    sweepAlign_   = 0.0f;
    if (tankv::hasSweep(tankVoicing_)) {
        // Voicing 1 (TankVoicing.h): the shared Sweep carries most of the
        // Chirp, so the Loops keep fewer sections (tight end: the floor).
        const auto& t = tankv::tuning();
        const float u = map::tensionLooseness(tension);
        activeStages_ = map::kMinStages
                      + int(float(cap - map::kMinStages) * map::anchorLin(0.0f, t.loopFracNoon, t.loopFracLoose, u) + 0.5f);
        const int st = int(t.sweepStagesNoon * map::anchorLin(t.sweepTightScale, 1.0f, t.sweepLooseScale, u) + 0.5f);
        sweep_.set(base.transitionHz * t.sweepFcRatio, t.sweepCoeff, st, snapNow_);
        // Its delay at TankVoicing.h sweepAlignHz, less what Spring A's
        // shorter Loop chain no longer delays there: every pickup moves back
        // by that, so the first echo keeps today's time (ADR 0029).
        const float aA  = base.allpassCoeff * modes::kDetune[0].allpassCoeff;
        const float kA  = map::stretchK(base.transitionHz * modes::kDetune[0].transition, sampleRate_);
        const int   was = modes::tensionStages(tension, cap);
        sweepAlign_ = sweep_.groupDelaySamples(t.sweepAlignHz)
                    - float(was - activeStages_) * map::stretchedAllpassGroupDelaySamples(aA, kA, t.sweepAlignHz, sampleRate_);
    }
#endif
    if (springs3::kPaletteBuilt) {
        // The tank voicing's position (stages, A's Chirp-chain delay), without
        // the SPRINGS 3 voicing: round 2's reference for the repeat timing
        // (updateSpringSettings, keepTodaysTiming).
        todayStages_ = activeStages_;
        alignToday_  = modes::pickupChainSamples(base.allpassCoeff * modes::kDetune[0].allpassCoeff,
                                                 map::tensionTransitionHz(tension) * modes::kDetune[0].transition,
                                                 todayStages_, sampleRate_);
    }
    // A SPRINGS 3 voicing's own stage count (round 1's cap, 9's boost): on
    // the tank voicings without the Sweep only (the Sweep sets the Loops'
    // share; those SPRINGS 3 voicings are Renderer-only references).
    if (springs3::kPaletteBuilt && mode_ == 2 && s3W_ > 0.0f && !tankv::hasSweep(tankVoicing_)) {
        cap += int(std::lround(s3W_ * float(v3.stageCap - cap)));
        activeStages_ = modes::tensionStages(tension, cap);
        // Diffuse (voicing 9): more stages where TENSION runs fewer than the cap,
        // so the loosest tank (the CPU worst case) is unchanged.
        if (v3.stageBoost > 0.0f)
            activeStages_ += int(std::lround(s3W_ * v3.stageBoost * float(cap - activeStages_)));
    }
    // Spring A's Chirp-chain delay at the pickup alignment frequency: B and C
    // line their first echoes up on it (1 Spring = A alone, unchanged).
    alignA_ = modes::pickupChainSamples(base.allpassCoeff * shape_[0].allpassCoeff,
                                        base.transitionHz * shape_[0].transition, activeStages_, sampleRate_);
}

void Tank::updateSpringSettings(size_t i)
{
    // g is designed from each Spring's own round trip, so the L/fC/a
    // detune changes pitch/texture, not tail length; the damping and
    // decay detune (SpringModes.h) make each Spring fade its own way.
    // In position 3 the Spring's shape may be a voicing's (Springs3Voicing.h,
    // shape_); otherwise it is SpringModes.h's detune.
    const springs3::Shape& sh = shape_[i];
    SpringSettings s = baseSet_;
    s.loopDelaySeconds *= sh.loopDelay;
    s.transitionHz     *= sh.transition;
    s.dampingHz        *= sh.damping; // Spring.cpp clamps to 0.45 fs
    s.dampingHz        *= loopDampingScale(); // the tank voicing (1 today)
    if (springs3::kPaletteBuilt && s3W_ > 0.0f && springs3::voicing(s3Voicing_).dampingCapHz > 0.0f)
        s.dampingHz = std::min(s.dampingHz, springs3::voicing(s3Voicing_).dampingCapHz * modes::kDetune[i].damping);
    s.t60Seconds       *= sh.decay;
    s.highT60Seconds   *= sh.decay; // the Hold's (0 otherwise)
    s.highPathLevel    *= sh.highPath;
#if RV_TANKV_BUILT >= 5
    if (tankv::hasTransducers(tankVoicing_)) s.highPathLevel *= tankv::tuning().tdHighLevel; // voicing 5+
#endif
    s.tapRatio          = modes::kPickupTap[i];
    s.stages            = modes::springActive(mode_, int(i)) ? activeStages_ : modes::kIdleStages;
    // Pickup: tapRatio lines the first echoes up along the delay line;
    // the offset lines up the Chirp chains too (SpringModes.h "Pickup
    // position"), plus a fixed trim. Uses a without the Jolt, so it
    // moves with TENSION and SPRINGS only, and the Spring glides to it.
    // (allpassCoeff itself, with the Jolt, goes on every tick: controlTick.)
    const float aNoJolt = baseSet_.allpassCoeff * sh.allpassCoeff;
    s.tapOffsetSeconds  = sh.pickupOffset
                       + (alignA_ - modes::pickupChainSamples(aNoJolt, s.transitionHz, s.stages, sampleRate_)) / sampleRate_
#if RV_TANKV_BUILT >= 1
                       - sweepAlign_ / sampleRate_ // voicing 1: the Sweep's delay (0 otherwise)
#endif
        ;
    if (springs3::kPaletteBuilt && s3W_ > 0.0f && springs3::voicing(s3Voicing_).keepTiming)
        keepTodaysTiming(i, s);
#if RV_TANKV_BUILT >= 7
    if (tankv::hasShipFixes(tankVoicing_)) { // TONE re-map: the high path today's length fully left
        const auto& t = tankv::tuning();
        const float w = tankv::toneDarkWeight(keyTone_, t.toneDarkCurve);
        const float r = t.tdHighT60Ratio + w * (t.toneDarkHighT60Ratio - t.tdHighT60Ratio);
        if (r != hiT60Set_[i]) {
            springs_[i].setHighT60Ratio(r);
            hiT60Set_[i] = r;
        }
    }
#endif
    s.lfoHz             = antires::kHowlLfoHz * antires::kHowlLfoRatio[i];
    springSet_[i]       = s;
    springGen_[i]       = baseGen_;
    springTension_[i]   = keyTension_;
    springTone_[i]      = keyTone_;
    springS3W_[i]       = keyS3W_;
}

namespace {
// One-pole low-pass group delay (samples) at kPickupAlignHz, as the Spring's
// damping filter (dsp::OnePoleLowpass, clamped at 0.45 fs as Spring.cpp).
RV_SIZE_OPT float dampingDelayAtAlign(float dampingHz, float sampleRate)
{
    dsp::OnePoleLowpass lp;
    lp.setCutoff(std::min(dampingHz, 0.45f * sampleRate), sampleRate);
    return lp.groupDelay(std::cos(2.0f * map::kPi * modes::kPickupAlignHz / sampleRate));
}
} // namespace

RV_SIZE_OPT void Tank::keepTodaysTiming(size_t i, SpringSettings& s) const
{
    // Round 2 (Springs3Voicing.h keepTiming): the Spring keeps today's
    // round trip and first echo at kPickupAlignHz. The voicing changed the
    // time its Chirp chain (+ fC low-pass + damping) takes there by dX
    // samples: the delay takes dX back, and the pickup moves so the first
    // echo (chain + tapRatio x L + offset) lands where today's does.
    const modes::Detune& d = modes::kDetune[i];
    const float a0     = baseSet_.allpassCoeff * d.allpassCoeff;
    const float fc0    = map::tensionTransitionHz(keyTension_) * d.transition;
    const int   m0     = modes::springActive(mode_, int(i)) ? todayStages_ : modes::kIdleStages;
    const float chain0 = modes::pickupChainSamples(a0, fc0, m0, sampleRate_);
    const float x0     = chain0 + dampingDelayAtAlign(map::toneDampingHz(keyTone_) * d.damping * loopDampingScale(), sampleRate_);
    const float aV     = baseSet_.allpassCoeff * shape_[i].allpassCoeff;
    const float xV     = modes::pickupChainSamples(aV, s.transitionHz, s.stages, sampleRate_)
                   + dampingDelayAtAlign(s.dampingHz, sampleRate_);
    const float dX     = x0 - xV;
    // Today's L (the voicing keeps the detuned length) plus dX, within the
    // delay memory (the loosest right Spring can't grow: it then repeats up
    // to ~1 % early, reported by test_springs3 "timing").
    const float l0   = s.loopDelaySeconds;
    const float lMax = map::kLoopDelayMaxSeconds * modes::kMaxLoopDelayDetune;
    s.loopDelaySeconds = std::min(l0 + dX / sampleRate_, std::max(l0, lMax));
    const float dL   = (s.loopDelaySeconds - l0) * sampleRate_;
    float off0 = modes::kPickupOffsetSeconds[i] + (alignToday_ - chain0) / sampleRate_;
#if RV_TANKV_BUILT >= 1
    off0 -= sweepAlign_ / sampleRate_; // the tank voicing's Sweep (0 otherwise)
#endif
    s.tapOffsetSeconds = off0 + (dX - s.tapRatio * dL) / sampleRate_;
}

RV_SIZE_OPT Tank::CoupleMatrix Tank::coupleMatrix(float angle, int kind)
{
    CoupleMatrix m{};
    m[0] = m[4] = m[8] = 1.0f;
    if (angle == 0.0f || kind == springs3::kCoupleNone) return m;
    const float c = std::cos(angle), sn = std::sin(angle);
    if (kind == springs3::kCoupleLeftRight) {
        // A (0) and B (1) turn into each other; C keeps its own.
        m[0] = c, m[1] = -sn;
        m[3] = sn, m[4] = c;
        return m;
    }
    // Rodrigues: a rotation by `angle` about the axis u (springs3::
    // kCoupleAxis). A lopsided axis, so every blend of the three returns
    // turns, their sum too: about (1, 1, 1) the sum stayed put, and a
    // resonance the three Springs share held on while the rest decayed
    // (test_springs3 "ringing": a steady tone at 680 Hz).
    const auto& u = springs3::kCoupleAxis;
    const float k = 1.0f - c;
    const float cross[3][3] = {{0.0f, -u[2], u[1]}, {u[2], 0.0f, -u[0]}, {-u[1], u[0], 0.0f}};
    for (int r = 0; r < 3; ++r)
        for (int col = 0; col < 3; ++col)
            m[size_t(r * 3 + col)] = (r == col ? c : 0.0f) + sn * cross[r][col] + k * u[r] * u[col];
    return m;
}

void Tank::processCoupled(const float* mono, float* const* clat, const float* jolt, const float* tapSamples, float* wobA,
                          float (*wet)[kControlInterval], int tick, int n)
{
    // The Springs' inputs as in process() (same order of the WOBBLE calls),
    // then one sample at a time across the three Springs: each Loop's return
    // (g x LoopSat(feedback)), turned by the coupling rotation, goes into the
    // Loops' inputs. The rotation ramps per sample from the last tick's to
    // this one's (a blend of two rotations never gains: |blend| <= 1).
    float lFrac[kMaxSprings][kControlInterval], lSamples[kMaxSprings][kControlInterval];
    float loopIn[kMaxSprings][kControlInterval], high[kMaxSprings][kControlInterval];
    const float inv = 1.0f / float(kControlInterval);
    for (size_t s = 0; s < springs_.size(); ++s) {
        const float scale = splash::kJoltSpringScale[s];
        const float* c = clat[s];
        for (int i = 0; i < n; ++i) {
            lFrac[s][i]    = scale * jolt[i];
            lSamples[s][i] = s == 0 ? (wobA[i] = wobble_[0].next()) : wobble_[s].next(wobA[i]);
            loopIn[s][i]   = mono[i] + splash::kClatterLoop * c[i];
            high[s][i]     = mono[i] + splash::kClatterHigh * c[i];
        }
    }
#ifndef RV_FIXED_VOICINGS
    // F round 2 "coupled swell" (Springs3Voicing.h fParts inputWeight): each
    // Spring its own share of a hit, gliding in with the voicing.
    if (springs3::hasInputWeights(s3Voicing_)) {
        const auto iw = springs3::fParts(s3Voicing_).inputWeight;
        // Hits only: short power against long power opens the split on an
        // attack and holds it while the hit rings into the Springs; a held
        // sound (fast = slow) goes into all three evenly, as coupled does, so
        // its level is today's. On a hit Spring A gets the input, louder by
        // kSwellInputGain (what waits in A is lost to its own damping until
        // the coupling hands it on), B and C a trace.
        float a[kControlInterval];
        for (int i = 0; i < n; ++i) {
            const float x2 = mono[i] * mono[i]; // short and long power (a held sound: equal)
            swFast_ += swFastAtt_ * (x2 - swFast_);
            swSlow_ += swSlowC_ * (x2 - swSlow_);
            const float tr = std::clamp((swFast_ / (swSlow_ + 1.0e-10f) - springs3::kSwellRatioFrom)
                                            / (springs3::kSwellRatioTo - springs3::kSwellRatioFrom), 0.0f, 1.0f);
            swHold_ = std::max(tr, swHold_ - swHoldStep_);
            a[i] = (s3SwellFrom_ + (s3SwellTo_ - s3SwellFrom_) * float(tick + i) * inv) * swHold_;
        }
        for (size_t s = 0; s < springs_.size(); ++s) {
            const float k = iw[s] * springs3::kSwellInputGain - 1.0f;
            for (int i = 0; i < n; ++i) {
                loopIn[s][i] += a[i] * k * mono[i];
                high[s][i] += a[i] * k * mono[i];
            }
        }
    }
#endif
    for (int i = 0; i < n; ++i) {
        const float t = float(tick + i) * inv;
        float r[kMaxSprings];
        for (size_t s = 0; s < springs_.size(); ++s)
            r[s] = springs_[s].coupledReturn(lFrac[s][i], lSamples[s][i], tapSamples[i]);
        for (size_t s = 0; s < springs_.size(); ++s) {
            float mixed = 0.0f;
            for (size_t k = 0; k < springs_.size(); ++k) {
                const size_t e = s * size_t(kMaxSprings) + k;
                mixed += (s3CoupleFrom_[e] + t * (s3CoupleTo_[e] - s3CoupleFrom_[e])) * r[k];
            }
            wet[s][i] = springs_[s].coupledFinish(loopIn[s][i], high[s][i], mixed);
        }
    }
}

RV_SIZE_OPT modes::StereoMix Tank::modeMix(int mode) const
{
    return springs3::kPaletteBuilt && mode == 2 && s3Voicing_ != springs3::kToday ? springs3::voicing(s3Voicing_).mix
                                                        : modes::stereoMix(mode);
}

RV_SIZE_OPT float Tank::modeTrim(int mode) const
{
    return springs3::kPaletteBuilt && mode == 2 && s3Voicing_ != springs3::kToday
               ? modes::kModeTrim[size_t(mode)] * springs3::voicing(s3Voicing_).trim
               : modes::kModeTrim[size_t(mode)];
}

RV_SIZE_OPT void Tank::updateShapes()
{
    // Today's detune at s3W_ = 0 (exactly: no arithmetic on it), the
    // voicing's shape at 1, a straight blend while gliding.
    const springs3::Voicing& v3 = springs3::voicing(s3Voicing_);
    s3InLp_.setCutoff(std::max(v3.inputLowCutHz, 1.0f), sampleRate_);
    s3SendLp_.setCutoff(std::max(v3.seriesLowCutHz, 1.0f), sampleRate_);
    for (size_t i = 0; i < shape_.size(); ++i) {
        const springs3::Shape from = springs3::fromDetune(i);
        if (s3W_ <= 0.0f) {
            shape_[i] = from;
            continue;
        }
        const springs3::Shape& to = v3.spring[i];
        const float w = s3W_;
        shape_[i] = {from.loopDelay + w * (to.loopDelay - from.loopDelay),
                     from.transition + w * (to.transition - from.transition),
                     from.allpassCoeff + w * (to.allpassCoeff - from.allpassCoeff),
                     from.damping + w * (to.damping - from.damping),
                     from.decay + w * (to.decay - from.decay),
                     from.highPath + w * (to.highPath - from.highPath),
                     from.pickupOffset + w * (to.pickupOffset - from.pickupOffset)};
    }
}

RV_NO_UNSWITCH void Tank::process(const float* inL, const float* inR, float* outL, float* outR, int numSamples)
{
    if (!ok_) { // no memory: stay a clean passthrough rather than fail silently into noise
        for (int i = 0; i < numSamples; ++i) {
            outL[i] = inL[i];
            outR[i] = inR[i];
        }
        numPendingKicks_ = numPendingClocks_ = 0;
        numPendingGates_ = 0;
        return;
    }
    // THROW from the ParamSpec switch (Plugin, Renderer): a change is a gate
    // change at the block's start.
    {
        const bool p = values_[size_t(ParamId::Throw)] >= 0.5f;
        if (p != throwParamHigh_) {
            throwParamHigh_ = p;
            if (p && !gateHigh_) latchThrow(holdSendFrom_);
            gateHigh_ = p;
        }
    }
    // Throw mode off (a long press of KICK, ADR 0039): crossfade from the
    // throw's send to the plain one over the open ramp, then unlatch.
    if (releaseThrow_) {
        releaseThrow_ = false;
        if (throwOn_ && !thrReleasing_) {
            thrReleasing_ = true;
            thrRelPos_    = 0.0f;
        }
    }
    int gateIdx = 0; // next pending gate change
    // Echo mode's clock: this block's gate edges, on their exact samples,
    // queued; each reaches the clock at the first control tick at or after
    // it (feedClocks), so any block size reads the same tempo at the same tick.
    for (int k = 0; k < numPendingClocks_; ++k) {
        const uint32_t at = sampleClock_ + uint32_t(std::min(pendingClocks_[size_t(k)], numSamples - 1));
        if (numClockQ_ == kMaxPendingKicks) break; // full: extras dropped (as kick())
        int j = numClockQ_++;
        for (; j > 0 && int32_t(clockQ_[size_t(j - 1)] - at) > 0; --j) clockQ_[size_t(j)] = clockQ_[size_t(j - 1)];
        clockQ_[size_t(j)] = at;
    }
    numPendingClocks_ = 0;
    if (!primed_) {
        feedClocks();
        controlTick(true);
        mix_.value = values_[size_t(ParamId::Mix)];
        primed_    = true;
    }

    float mono[kControlInterval], driven[kControlInterval], high[kControlInterval], loopIn[kControlInterval];
    float xin[kControlInterval], det[kControlInterval], clang[kControlInterval], bite[kControlInterval], clangToday[kControlInterval];
    float clatter[kControlInterval], clatterB[kControlInterval], clatterC[kControlInterval], jolt[kControlInterval], kickLoop[kControlInterval], kickDirect[kControlInterval];
    float lFrac[kControlInterval], lSamples[kControlInterval], tapSamples[kControlInterval], wobA[kControlInterval],
        trem[kControlInterval];
    float wet[kMaxSprings][kControlInterval], send[kControlInterval], sendG[kControlInterval];
    float echoPlay[kControlInterval], echoGain[kControlInterval], echoRec[kControlInterval];
    int pos = 0;
    while (pos < numSamples) {
        if (tick_ == 0) { // fixed grid, independent of block size
            feedClocks();
            controlTick(false);
        }
        prof::mark(prof::kControl);
        const int n = std::min(numSamples - pos, kControlInterval - tick_);

        // THROW and HOLD (ADR 0039, 0040, ThrowHold.h): the send's gain per
        // sample, in front of everything that hears the input (the Splash
        // included: a thrown snare splashes), only while either is in play
        // (otherwise nothing here runs and the input is as before, bit for
        // bit). Gate changes land on their sample; the throw latches on at
        // the first rising edge. Thrown, the send follows the gate (an open
        // throw overrides the freeze: it is how new sound gets into a held
        // bed; the layer voicing keeps its lower send under the throw).
        // HOLD's ducking: the key's follower runs per sample below; the dip
        // is read on the tick (controlTick) and ramped over it.
        const bool  duckLive  = holdBed_ > 0.0f || duckFrom_ != 1.0f || duckTo_ != 1.0f;
        const float duckStep = (duckTo_ - duckFrom_) * (1.0f / float(kControlInterval));
        const bool sendLive = throwOn_ || gateIdx < numPendingGates_ || holdSendFrom_ != 1.0f || holdSendTo_ != 1.0f;
        if (sendLive) {
            const float hStep = (holdSendTo_ - holdSendFrom_) * (1.0f / float(kControlInterval));
            const bool  layer = holdVoicing_ == throwhold::kVoicingLayer;
            const bool  throwRole = throwhold::gateRole(springsPos_, echoMode_) == throwhold::GateRole::Throw;
            for (int i = 0; i < n; ++i) {
                while (gateIdx < numPendingGates_ && std::min(pendingGates_[size_t(gateIdx)].at, numSamples - 1) <= pos + i) {
                    const bool high = pendingGates_[size_t(gateIdx++)].high;
                    if (high && !gateHigh_ && throwRole) latchThrow(holdSendFrom_ + hStep * float(tick_ + i));
                    gateHigh_ = high;
                }
                const float hs = holdSendFrom_ + hStep * float(tick_ + i);
                float g = hs;
                if (throwOn_) {
                    // Where the gate is the echo's clock (SPRINGS 3, ADR
                    // 0041) the throw rests open: the send glides open and
                    // follows the gate again back in positions 1-2.
                    const bool open = throwRole ? gateHigh_ : true;
                    thrPos_ = open ? std::min(1.0f, thrPos_ + thrOpenStep_) : std::max(0.0f, thrPos_ - thrCloseStep_);
                    const float t = throwhold::smooth01(thrPos_);
                    g = layer ? t * hs : t;
                    if (thrReleasing_) { // leaving throw mode: glide to the plain send, then unlatch
                        thrRelPos_ = std::min(1.0f, thrRelPos_ + thrOpenStep_);
                        g += throwhold::smooth01(thrRelPos_) * (hs - g);
                        if (thrRelPos_ >= 1.0f) {
                            g        = hs; // exactly the plain send from here on
                            throwOn_ = thrReleasing_ = false;
                        }
                    }
                }
                // Layer: what goes into the bed dips with it (kicks and bass
                // don't pile up in the bed during the dip).
                if (layer && duckLive) g *= duckFrom_ + duckStep * float(tick_ + i);
                sendG[i] = g;
            }
            sendNow_ = sendG[n - 1];
        }

        // SPRINGS 3 echo mode (EchoVoicing.h): the tape's playback, faded in
        // with the glide, joins the mono input before the Splash, so each
        // repeat hits the springs as a new hit would. The tape records the
        // input and its own playback x the feedback (series: the feedback
        // stays on the tape).
        const bool echoRun = echoW_ > 0.0f || echoWFrom_ > 0.0f;
        if (echoRun) {
            echo_.play(echoPlay, n);
            const float gStep = (echoW_ - echoWFrom_) * (1.0f / float(kControlInterval));
            const float fStep = (fbTo_ - fbFrom_) * (1.0f / float(kControlInterval));
            const float iStep = (ginTo_ - ginFrom_) * (1.0f / float(kControlInterval));
            // The feedback (diffused and worn when a prototype voicing asks,
            // EchoVoicing.h), and the input at the first repeat's gain.
            float fbs[kControlInterval], xs[kControlInterval];
            const bool diffuse = echo::kDiffuseBuilt && echo_.diffuseVoicing() != 0;
            // KICKED only (with the Morph): its runaway zone is where the tape saturates.
            const float trim = diffuse ? 1.0f - attW_[2] * (1.0f - echo::kDiffuse[echo_.diffuseVoicing()].trim) : 1.0f;
            for (int i = 0; i < n; ++i) {
                fbs[i] = trim * (fbFrom_ + fStep * float(tick_ + i)) * echoPlay[i];
                xs[i]  = (ginFrom_ + iStep * float(tick_ + i)) * (0.5f * (inL[pos + i] + inR[pos + i]));
            }
            if (diffuse) echo_.diffuse(fbs, n);
            if (echo_.wearActive()) {
                echo_.wear(fbs, n);
                echo_.delayInput(xs, n);
            }
            for (int i = 0; i < n; ++i) {
                const float w = echoWFrom_ + gStep * float(tick_ + i);
                echoGain[i]   = w;
                echoRec[i]    = w * xs[i] + fbs[i];
                echoPlay[i] *= w;
            }
            prof::mark(prof::kSpringC); // echo mode: the echo's share (Spring C doesn't run)
        }

        // Real tanks are mono: sum the input (SPEC §4.3). Dry stays stereo.
        // The Splash listens first, to the input after the INPUT gain G and
        // before any saturation (ADR 0032, 0033).
        for (int i = 0; i < n; ++i) {
            float x = 0.5f * (inL[pos + i] + inR[pos + i]);
            if (duckLive) { // the key: the input's lows (kick, bass)
                const float k = duckKey_[1].process(duckKey_[0].process(x));
                const float a = k < 0.0f ? -k : k;
                duckEnv_ += (a > duckEnv_ ? duckAtt_ : duckRel_) * (a - duckEnv_);
            }
            if (sendLive) x *= sendG[i];
            // Echo mode: the Springs (and the Splash) hear the tape's output,
            // the input plus its repeats; the Excitation trim below still
            // reads the input itself (after the send).
            const float xe = echoRun ? x + echoPlay[i] : x;
            xin[i] = xe;
            det[i] = xe * inputGain_.next();
            // Excitation trim followers on the raw input (what the dry path
            // carries), full band and weighted like the whole chain's response.
            float w = x - excHp_[0].process(x);
            w -= excHp_[1].process(w);
#if RV_TANKV_BUILT >= 7
            if (tankv::hasShipFixes(tankVoicing_)) { // 7: the low cut's makeup, read on the raw input too (controlTick)
                const float sh = lcShShelf_.process(lcShHp_.process(w)); // above ~90 Hz, as its followers
                gmShAccIn_ += w * w;
                gmShAccOut_ += sh * sh;
            }
#endif
            w = excLp_[1].process(excLp_[0].process(w));
            excAccBroad_ += x * x;
            excAccBand_ += w * w;
        }

        // Kick: onsets on their exact sample (offsets clamp to the block).
        for (int k = 0; k < numPendingKicks_; ++k) {
            const int at = std::min(pendingKicks_[size_t(k)], numSamples - 1) - pos;
            if (at >= 0 && at < n) kick_.trigger(at);
        }
        kick_.process(kickLoop, kickDirect, n);
        // A Kick forces a maximal Splash on its own sample (SPEC §4.6).
        if (kick_.joltOffset() >= 0) splash_.strike(1.0f, kick_.joltOffset());
        float* const clat[kMaxSprings] = {clatter, clatterB, clatterC};
        splash_.process(det, clang, bite, clatter, clatterB, clatterC, jolt, n, splash::kVoicingsBuilt ? clangToday : nullptr);
        if (!splashOn_) { // test hooks (Tank.h)
            for (auto* c : clat) std::fill(c, c + n, 0.0f);
            std::fill(clang, clang + n, 0.0f);
            std::fill(bite, bite + n, 0.0f);
        }
        if (!joltOn_) std::fill(jolt, jolt + n, 0.0f);
#if RV_TANKV_BUILT >= 7
        if (splashLift_ != 1.0f) { // 7: SPLASH at low DRIVE (controlTick)
            const float k = splashLift_;
            for (int i = 0; i < n; ++i) {
                clatter[i] *= k;
                clatterB[i] *= k;
                clatterC[i] *= k;
            }
        }
#endif
        prof::mark(prof::kSplash);

        // DriveIn (transducer -> tape). The Bite (ADR 0032, SplashVoicing.h):
        // a short, cracking hit goes into the saturators harder, x (1 + b),
        // and most of the push is taken back right after them
        // (splash::kBiteTakeBack, three quarters since the owner's 30 Sep
        // listen: "a bit hot/distorted"): grit on the hit, little level. Taken back inside DriveIn, before its
        // makeup's output follower, so the makeup gives back only what the
        // saturators squashed. b = 0 on everything else (and always in
        // CLEAN): the plain path, bit for bit.
        for (int i = 0; i < n; ++i) {
            const float b = bite[i];
            const float back = b > 0.0f ? splash::biteBack(1.0f + b) : 1.0f;
            driven[i] = driveIn_.process(xin[i], 1.0f + b, back); // one call site: DriveIn inlines once
        }
        prof::mark(prof::kDriveIn);

        // Spring inputs: TONE's tilt and the excitation trim; the Clang (ADR
        // 0032): the hit's own highs, above splash::kClangHz, fed harder into
        // the springs while it lasts, x + c (x - LP(x)); plus the Kick's
        // high-passed Loop feed (post-drive, not clanged: a Kick has its own
        // crash). Each Spring also gets its own Clatter stream (the Kick's
        // crash: same burst envelope, independent noise, every spring clangs
        // on its own), into the Loop (dispersed into the Chirp, decays with
        // the tail) and the high path (fast echoes), splash::kClatterLoop /
        // kClatterHigh.
        // The Kick's Loop feed is divided by the level DRIVE adds on the
        // Springs' output (heardGain_), so a Kick stays the same size at any
        // DRIVE (ADR 0005: fixed strength, ATTITUDE only).
        const float excStep = (inTrimTo_ - inTrimFrom_) * (1.0f / float(kControlInterval));
        const float kickScale = 1.0f / driveInSettings_.heard;
        // The Clang's ceiling rises with KICKED's pickup push (clangCeilPush_):
        // driven KICKED pickups squash a big splash on their own (SplashVoicing.h).
        const float clangCeil = splash_.clangCeiling() * clangCeilPush_;
#if RV_TANKV_BUILT >= 5
        const float tdEven = tankv::hasTransducers(tankVoicing_) ? tankv::tuning().tdEven : 0.0f;
#endif
        for (int i = 0; i < n; ++i) {
            const float d  = driven[i];
            const float t  = tilt_.process(d);
            // Big Knob makeup followers: lows the tank doesn't hear (below
            // ~90 Hz, the Excitation trim's high-pass) don't count.
            float wi = d - bkHp_[0].process(d);
            wi -= bkHp_[1].process(wi);
            float wo = t - bkHp_[2].process(t);
            wo -= bkHp_[3].process(wo);
            bkAccIn_ += wi * wi;
            bkAccOut_ += wo * wo;
#if RV_TANKV_BUILT >= 7
            if (tankv::hasShipFixes(tankVoicing_)) { // TONE re-map's makeup: how much of the input is highs (controlTick)
                const float lp = tdDarkLp_.process(wi);
                tdAccAll_ += wi * wi;
                tdAccLp_ += lp * lp;
            }
#endif
            float x = t * (inTrimFrom_ + excStep * float(tick_ + i));
#if RV_TANKV_BUILT >= 4
            if (tankv::hasLowCut(tankVoicing_)) {      // voicings 4, 7: the low cut
                const float in = x;
                x = gentleShelf_.process(gentleHp_.process(x));
#if RV_TANKV_BUILT >= 7
                float gi = in - gmHp_[0].process(in);   // makeup followers (7): above ~90 Hz
                gi -= gmHp_[1].process(gi);
                float go = x - gmHp_[2].process(x);
                go -= gmHp_[3].process(go);
                gmAccIn_ += gi * gi;
                gmAccOut_ += go * go;
#else
                (void)in;
#endif
            }
#endif
            const float lo = clangLp_.process(x);
            float c = clang[i];
            if (splash::kVoicingsBuilt && clangCeil > 0.0f) { // SPLASH stronger voicings: the Clang's ceiling (SplashVoicing.h)
                const float hi = x - lo, a = dsp::absSel(hi); // (Select.h: no branch on the chip)
                clangEnv_ += dsp::selGt(a, clangEnv_, clangAtt_, clangRel_) * (a - clangEnv_);
                const float cmax = clangCeil / (clangEnv_ + 1.0e-9f);
                c = c < cmax ? c : cmax;
                c = c > clangToday[i] ? c : clangToday[i]; // never below today's Clang
            }
#if RV_TANKV_BUILT >= 7
            c *= splashLift_; // 7: SPLASH at low DRIVE (controlTick), past the ceiling
#endif
#if RV_TANKV_BUILT >= 5
            if (tankv::hasTransducers(tankVoicing_)) { // voicing 5+: the input coil (the Kick's knock bypasses it)
                float u = x + c * (x - lo);
                if (tdEven != 0.0f) { // the coil's even-order colour
                    // A plain square (only doubled frequencies: nothing above
                    // Nyquist for what DriveIn lets through) less its slow
                    // average (no thump), at a gain that eases off as the
                    // slow level rises (so loud input isn't torn up).
                    const float u2  = u * u;
                    const float avg = tdEvenAvg_.process(u2);
                    float sq = u2 - avg;
#if RV_TANKV_BUILT >= 7
                    // 7: the doubled frequencies only, not the difference
                    // tones between them (tdEvenHpHz).
                    if (tankv::hasShipFixes(tankVoicing_) && tankv::tuning().tdEvenHpHz > 0.0f) sq = tdEvenHp_.process(sq);
#endif
                    u += tdEven / (1.0f + tankv::tuning().tdEvenEase * avg) * sq;
                }
                mono[i] = tdIn_.process(u) + kickScale * kickLoop[i];
                continue;
            }
#endif
            mono[i] = x + c * (x - lo) + kickScale * kickLoop[i];
        }
        // Voicing 1: the shared Sweep, once for every Spring (Loop and high path).
#if RV_TANKV_BUILT >= 1
        if (tankv::hasSweep(tankVoicing_)) sweep_.process(mono, n);
#endif
        // One transport for every pickup: the first echoes move together. Its
        // flutter tremolo (WOBBLE left, WobbleVoicing.h) scales the wet below.
        transport_.process(tapSamples, trem, n);
        prof::mark(prof::kTilt);
        // SPRINGS 3 "in series" (Springs3Voicing.h): Springs B and C hear
        // Spring A's output instead of the input, faded in and out with the
        // voicing's glide (per sample across the tick, so it never steps).
        const springs3::Voicing& v3 = springs3::voicing(s3Voicing_);
        const bool  series     = springs3::kPaletteBuilt && (s3SeriesFrom_ > 0.0f || s3SeriesTo_ > 0.0f);
        const float seriesSend = s3SeriesSend_;
        const float seriesStep = (s3SeriesTo_ - s3SeriesFrom_) * (1.0f / float(kControlInterval));
        const bool  sendCut    = series && v3.seriesLowCutHz > 0.0f;
        // The voicing's low cut on every Spring's input (the pan tank),
        // faded in and out with the glide.
        if (springs3::kPaletteBuilt && v3.inputLowCutHz > 0.0f && (s3WFrom_ > 0.0f || s3W_ > 0.0f)) {
            const float step = (s3W_ - s3WFrom_) * (1.0f / float(kControlInterval));
            for (int i = 0; i < n; ++i) mono[i] -= (s3WFrom_ + step * float(tick_ + i)) * s3InLp_.process(mono[i]);
        }
        if (springs3::kPaletteBuilt && s3Coupled_) {
            processCoupled(mono, clat, jolt, tapSamples, wobA, wet, tick_, n);
        } else
        {
        // Spring C is heard nowhere with echo mode built (Tank.h "SPRINGS
        // switching"): it doesn't run, and plays silence.
        const size_t numRun = echoMode_ ? size_t(kMaxSprings - 1) : springs_.size();
        if (numRun < springs_.size()) std::fill(wet[2], wet[2] + n, 0.0f);
        for (size_t s = 0; s < numRun; ++s) {
            const float scale = splash::kJoltSpringScale[s];
            const float* c = clat[s];
            for (int i = 0; i < n; ++i) {
                lFrac[i]    = scale * jolt[i];
                lSamples[i] = s == 0 ? (wobA[i] = wobble_[0].next()) : wobble_[s].next(wobA[i]); // B, C share A's at low WOBBLE
                loopIn[i]   = mono[i] + splash::kClatterLoop * c[i];
                high[i]     = mono[i] + splash::kClatterHigh * c[i];
            }
            if (series && s > 0) {
                // Only the Loops: the high paths keep the input, so the fast
                // high echoes aren't echoed twice (a 5-6 kHz ring on tight
                // tanks at DECAY max). The send is low-cut once, for B and C.
                if (s == 1)
                    for (int i = 0; i < n; ++i) send[i] = sendCut ? wet[0][i] - s3SendLp_.process(wet[0][i]) : wet[0][i];
                for (int i = 0; i < n; ++i) {
                    const float k = s3SeriesFrom_ + seriesStep * float(tick_ + i);
                    loopIn[i] = mono[i] + k * (seriesSend * send[i] - mono[i]) + splash::kClatterLoop * c[i];
                }
            }
            springs_[s].process(loopIn, high, lFrac, lSamples, tapSamples, wet[s], n);
            prof::mark(prof::Section(prof::kSpringA + int(s)));
        }
        }

#if RV_TANKV_BUILT >= 5
        // Voicing 5+: the Springs keep more of their highs (less Loop damping),
        // so their tail comes back louder; tdTrimDb takes that back.
        // 7: after the pickups instead (below), so DriveOut squashes the
        // tail as it does today: before them, the trimmed tail met DRIVE's
        // pickups 2.5 dB lower, so DRIVE grew it and its splash more.
        const bool  trimAfter = tankv::hasShipFixes(tankVoicing_);
        const float wetGain = kWetGain * (tankv::hasTransducers(tankVoicing_) && !trimAfter ? tdTrim_ : 1.0f);
        const float outTrim = trimAfter ? tdTrim_ : 1.0f;
        const float kickIn  = trimAfter ? kWetGain / tdTrim_ : kWetGain; // the Kick's thump comes out as before
#else
        constexpr float wetGain = kWetGain;
        constexpr float outTrim = 1.0f;
        constexpr float kickIn  = kWetGain;
#endif
        float wetL[kControlInterval], wetR[kControlInterval]; // the wet up to the mu-law box (ADR 0042 amendment)
        for (int i = 0; i < n; ++i) {
            // DRIVE's heard share (ADR 0033): the tail comes back louder by
            // G^kInputHeard. Here, on the Springs' output, rather than into
            // them: the LoopSat, the AntiRes fade and the Howl see the same
            // level at every DRIVE (the tail's length and colour don't move
            // with DRIVE), and the pickups' hardness is divided by the same
            // gain (controlTick), so they bend the louder tail as before.
            float wg = wetGain * heardGain_.next() * trem[i];
            if (echoRun) wg *= 1.0f + echoGain[i] * (echo::kTrim - 1.0f); // echo mode's level (EchoVoicing.h kTrim), with the glide
            const float src[modes::kNumSources] = {wg * wet[0][i], wg * wet[1][i], wg * wet[2][i]};

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
            float dd = mid;
            for (auto& ap : decorrelator_) dd = ap.process(dd);
#if RV_TANKV_BUILT >= 2
            if (tankv::hasTogether(tankVoicing_) && !tankv::hasWide(tankVoicing_)) dd -= dBass_.process(dd); // voicing 2: bass centred
#endif
            float d = mixCur_.decorr * dd;
            float sd = side;
#if RV_TANKV_BUILT >= 6
            if (tankv::hasWide(tankVoicing_)) {
                // Voicing 6: the side through D2 (its own decorrelator), into
                // the same bass-cut difference X: L = mid + X, R = mid - X.
                float s2 = side;
                for (auto& ap : wideDecorr_) s2 = ap.process(s2);
                d   = mixCur_.decorr * dd + s2;
                d  -= dBass_.process(d); // bass centred
                sd  = 0.0f;
            }
#endif
            // Output pickup (DriveOut), one per channel.
            // The Kick's direct thump joins the mid here: centred, mono-safe,
            // coloured by the pickups like the tank body moving under them.
            const float body = mid + kickIn * kickDirect[i];
            float inl = body + sd + d, inr = body - sd - d;
#if RV_TANKV_BUILT >= 5
            if (tankv::hasTransducers(tankVoicing_)) { // voicing 5+: the output pickup's treble loss
                inl = tdOut_[0].process(inl);
                inr = tdOut_[1].process(inr);
            }
#endif
            float wl = outTrim * driveOut_[0].process(inl);
            float wr = outTrim * driveOut_[1].process(inr);
            // Clatter share straight to the wet (M8 round 1; 0 since round 2,
            // splash::kClatterWet: it read as a hi-hat on top of the reverb):
            // the crash on top of the tail, after the pickups (an asymmetric
            // pickup would turn the burst's envelope into lows: KICKED Kick's low end, ADR
            // 0016). Mid, plus a copy delayed by kClatterSideMs in the side
            // (band noise a millisecond apart is uncorrelated): a wide crash
            // that sums to the plain burst in mono.
            if (kClatterWetGain > 0.0f) {
                const float cw = kClatterWetGain * clatter[i];
                const float cs = splash::kClatterSide * clatBuf_[size_t(clatPos_)];
                clatBuf_[size_t(clatPos_)] = cw;
                if (++clatPos_ >= clatDelay_) clatPos_ = 0;
                wl += cw + cs;
                wr += cw - cs;
            }

            // Gentle high-shelf cut: keep the part below kShelfHz, scale the rest.
            const float ll = shelfSplit_[0].process(wl), lr = shelfSplit_[1].process(wr);
            wetL[i] = ll + kShelfGain * (wl - ll);
            wetR[i] = lr + kShelfGain * (wr - lr);
        }
        // The mu-law box (ADR 0042, amended 5 Oct 2026: the wet only, before
        // TONE's return filter, so turning TONE right thins its grit; then the
        // limiter). In CLEAN (and voicing 0) it leaves the wet untouched.
        outBits_.process(wetL, wetR, n);
        for (int i = 0; i < n; ++i) {
            const float dryL = inL[pos + i], dryR = inR[pos + i]; // read before write: in may alias out
            float       wl = wetL[i], wr = wetR[i];
            if (tilt_.place() != drive::kTonePlacePre && trIdle_) {
                // Noon and left of it: an exact pass-through; only the
                // makeup's into-follower runs (out = in).
                const float si = wl + wr;
                float wi = si - trHp_[0].process(si);
                wi -= trHp_[1].process(wi);
                trAccIn_ += wi * wi;
                trAccOut_ += wi * wi;
            } else if (tilt_.place() != drive::kTonePlacePre) {
                // The Big Knob on the return (DriveVoicing.h "TONE
                // placement", ADR 0036 amendment): the tail you hear thins
                // at once. Its makeup followers (L + R above ~90 Hz, so the
                // Kick's sub thump doesn't count) and its makeup.
                const float si = wl + wr;
                toneReturn_.process(wl, wr);
                const float so = wl + wr;
                float wi = si - trHp_[0].process(si);
                wi -= trHp_[1].process(wi);
                float wo = so - trHp_[2].process(so);
                wo -= trHp_[3].process(wo);
                trAccIn_ += wi * wi;
                trAccOut_ += wo * wo;
                const float g = trMakeup_.next();
                wl *= g;
                wr *= g;
            }

            // Safety limiter (stereo-linked). The envelope jumps to each new
            // peak, holds 30 ms (Tank.h kLimitHoldS) and releases slowly; the gain glides down to knee/envelope
            // over ~1 ms and follows the (smooth) release straight back. An
            // instant gain would pin every rising peak flat at the threshold:
            // a corner in the waveform, i.e. a tick per new peak on held tones
            // (it was the DECAY-0.5 held-chord tick). The overshoot the glide
            // lets through is caught by a soft clip: identity up to the knee,
            // then a smooth curve that holds at the threshold T (softLimit).
            const float peak = std::max(std::fabs(wl), std::fabs(wr));
            susPeak_ = std::max(susPeak_, peak); // Sustain trim: the peaks the limiter reads
            if (peak >= limitEnv_ * kLimitHoldRefresh) {  // hold (kLimitHoldS)
                limitEnv_  = std::max(peak, limitEnv_);
                limitHold_ = limitHoldSamples_;
            } else if (limitHold_ > 0) {
                --limitHold_;
            } else {
                limitEnv_ = std::max(peak, limitEnv_ * limitRelease_);
            }
            const float gainTarget = limitEnv_ > kLimitKnee ? kLimitKnee / limitEnv_ : 1.0f;
            if (gainTarget < limitGain_) limitGain_ += limitAttack_ * (gainTarget - limitGain_);
            else limitGain_ = gainTarget;
            wl = softLimit(wl * limitGain_);
            wr = softLimit(wr * limitGain_);
            if (duckLive) { // HOLD: the held wet dips under the input's kick and bass (after the limiter: it reads the bed's own level)
                const float dg = duckFrom_ + duckStep * float(tick_ + i);
                wl *= dg;
                wr *= dg;
            }

            const float mixNow = mix_.process(values_[size_t(ParamId::Mix)]);
            if (mixNow != mixAt_) { // two square roots, only while MIX moves
                mixGains_ = map::mixGains(mixNow);
                mixAt_    = mixNow;
            }
            const map::MixGains m = mixGains_;
            outL[pos + i] = m.dry * dryL + m.wet * wl;
            outR[pos + i] = m.dry * dryR + m.wet * wr;
        }
        prof::mark(prof::kOutput);
        if (echoRun) echo_.record(echoRec, n); // the record head: after this step's playback (Echo.h)
        pos += n;
        sampleClock_ += uint32_t(n);
        tick_ = (tick_ + n) % kControlInterval;
    }
    numPendingKicks_ = 0;
    numPendingGates_ = 0;
}

} // namespace rv
