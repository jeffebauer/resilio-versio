#include "dsp/Tank.h"
#include "dsp/ProfileHook.h"

#include "params/AntiRes.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"
#include "params/SplashVoicing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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
    limitHoldSamples_ = int(kLimitHoldS * sampleRate);
    fadeStep_     = 1.0f / (kSpringsFadeSeconds * sampleRate);
    morphStep_    = float(kControlInterval) / (drive::kMorphSeconds * sampleRate);
    driveIn_.prepare(sampleRate);
    tilt_.prepare(sampleRate);
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
    levelCoeff_ = 1.0f - std::exp(-1000.0f * float(kControlInterval) / (splash::kTankLevelSmoothMs * sampleRate));
    // Power ratios from dB (10^(dB/10) = dbToGain(2 dB): exp, not powf, for the Firmware's flash).
    satFloorMs_   = drive::dbToGain(2.0f * antires::kLoopSatQuietDb);
    satInvSpanMs_ = 1.0f / (satFloorMs_ * (drive::dbToGain(2.0f * antires::kLoopSatFadeDb) - 1.0f));
    for (auto& f : excHp_) f.setCutoff(drive::kExcHpHz, sampleRate);
    for (auto& f : excLp_) f.setCutoff(drive::kExcLpHz, sampleRate);
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
        decorrelator_[i].c    = modes::kDecorrCoeff;
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
    clangLp_.reset();
    clangEnv_ = 0.0f;
    kick_.reset();
    for (auto& w : wobble_) w.reset();
    transport_.reset();
    levelAcc_ = levelMs_ = 0.0f;
    for (auto& f : excHp_) f.reset();
    for (auto& f : excLp_) f.reset();
    excAccBroad_ = excAccBand_ = excBroad_ = excBand_ = 0.0f;
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
    numPendingKicks_ = 0;
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

void Tank::controlTick(bool snap)
{
    for (const auto& p : kParams) {
        const size_t i = static_cast<size_t>(p.id);
        if (snap || p.kind != ParamKind::Knob) smoothed_[i] = values_[i];
        else smoothed_[i] += tickCoeff_[i] * (values_[i] - smoothed_[i]);
    }
    const float decay = smoothed_[size_t(ParamId::Decay)];
    const float tension = smoothed_[size_t(ParamId::Tension)];
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
    if (snap || attW_ != voiceW_) { // the blends only when the Morph moved
        voice_  = dsp::blendVoice(attW_);
        kick_.setAttitude(attW_);
        voiceW_ = attW_;
    }
    const drive::Voice& voice = voice_;
    const float drive = smoothed_[size_t(ParamId::Drive)];
    const float splashAmt = smoothed_[size_t(ParamId::Splash)];

    // M7: Splash and Kick follow the Morph weights (their tables blend like
    // the drive voicing: no steps on an ATTITUDE flip); WOBBLE glides.
    splash_.set(attW_, splashAmt, splashDrive_, splashInput_); // DRIVE's gain on the Clang / Bite, the INPUT gain (below, on DRIVE moves)
    const float wobbleScale = splash::wobbleDecayScale(decay); // Loop depth eased at long DECAYs
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
            susAim_ = -std::min(cutDb, drive::kSusGentleMaxDb) * kDb;
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
        inTrimFrom_ = snap ? excTrimTo_ * susGain_ : inTrimTo_;
        inTrimTo_   = excTrimTo_ * susGain_;
    }
    // Jolt on a (control rate): after the detune, scaled per Spring like the
    // Loop-delay Jolt (splash::kJoltSpringScale, M8), clamped below.
    const float joltA = joltOn_ ? splash_.allpassDelta() : 0.0f;

    // DriveIn settings (the INPUT gain, ADR 0033) and the DRIVE push on the
    // pickups (ADR 0022): only recomputed when DRIVE or the Morph moved
    // (they cost a few exp).
    if (snap || drive != compDrive_ || attW_ != compW_) {
        driveInSettings_ = dsp::driveInSettings(voice, drive);
        push_      = drive::push(voice, drive);
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
        || mode_ != keyMode_) {
        keyDecay_    = decay;
        keyTension_  = tension;
        keyTone_     = tone;
        keyW_        = attW_;
        keyMode_     = mode_;
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
        const float a = std::clamp(baseAllpass_ * modes::kDetune[i].allpassCoeff + joltA * splash::kJoltSpringScale[i],
                                   -splash::kMaxAllpassMagnitude, splash::kMaxAllpassMagnitude);
        const bool waits = !heavyOk && i == turn
                        && (springs_[i].redesignPending()
                            || (springGen_[i] != baseGen_
                                && (springTension_[i] != keyTension_ || springTone_[i] != keyTone_)));
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
        const bool done  = springs_[i].setSettings(s, snap);
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
    base.transitionHz     = map::tensionTransitionHz(tension);
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
    susGDownCoeff_ = 1.0f - std::exp(-float(kControlInterval)
                                     / (sampleRate_ * std::max(drive::kSusGentleDownSeconds,
                                                               drive::kSusDownPerFill * base.t60Seconds / 13.8f)));
    activeStages_ = modes::tensionStages(tension, modes::kStageCap[size_t(mode_)]);
    // Spring A's Chirp-chain delay at the pickup alignment frequency: B and C
    // line their first echoes up on it (1 Spring = A alone, unchanged).
    alignA_ = modes::pickupChainSamples(base.allpassCoeff * modes::kDetune[0].allpassCoeff,
                                        base.transitionHz * modes::kDetune[0].transition, activeStages_, sampleRate_);
}

void Tank::updateSpringSettings(size_t i)
{
    // g is designed from each Spring's own round trip, so the L/fC/a
    // detune changes pitch/texture, not tail length; the damping and
    // decay detune (SpringModes.h) make each Spring fade its own way.
    SpringSettings s = baseSet_;
    s.loopDelaySeconds *= modes::kDetune[i].loopDelay;
    s.transitionHz     *= modes::kDetune[i].transition;
    s.dampingHz        *= modes::kDetune[i].damping; // Spring.cpp clamps to 0.45 fs
    s.t60Seconds       *= modes::kDetune[i].decay;
    s.tapRatio          = modes::kPickupTap[i];
    s.stages            = modes::springActive(mode_, int(i)) ? activeStages_ : modes::kIdleStages;
    // Pickup: tapRatio lines the first echoes up along the delay line;
    // the offset lines up the Chirp chains too (SpringModes.h "Pickup
    // position"), plus a fixed trim. Uses a without the Jolt, so it
    // moves with TENSION and SPRINGS only, and the Spring glides to it.
    // (allpassCoeff itself, with the Jolt, goes on every tick: controlTick.)
    const float aNoJolt = baseSet_.allpassCoeff * modes::kDetune[i].allpassCoeff;
    s.tapOffsetSeconds  = modes::kPickupOffsetSeconds[i]
                       + (alignA_ - modes::pickupChainSamples(aNoJolt, s.transitionHz, s.stages, sampleRate_)) / sampleRate_;
    s.lfoHz             = antires::kHowlLfoHz * antires::kHowlLfoRatio[i];
    springSet_[i]       = s;
    springGen_[i]       = baseGen_;
    springTension_[i]   = keyTension_;
    springTone_[i]      = keyTone_;
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

    float mono[kControlInterval], driven[kControlInterval], high[kControlInterval], loopIn[kControlInterval];
    float xin[kControlInterval], det[kControlInterval], clang[kControlInterval], bite[kControlInterval];
    float clatter[kControlInterval], clatterB[kControlInterval], clatterC[kControlInterval], jolt[kControlInterval], kickLoop[kControlInterval], kickDirect[kControlInterval];
    float lFrac[kControlInterval], lSamples[kControlInterval], tapSamples[kControlInterval], wobA[kControlInterval],
        trem[kControlInterval];
    float wet[kMaxSprings][kControlInterval];
    int pos = 0;
    while (pos < numSamples) {
        if (tick_ == 0) controlTick(false); // fixed grid, independent of block size
        prof::mark(prof::kControl);
        const int n = std::min(numSamples - pos, kControlInterval - tick_);

        // Real tanks are mono: sum the input (SPEC §4.3). Dry stays stereo.
        // The Splash listens first, to the input after the INPUT gain G and
        // before any saturation (ADR 0032, 0033).
        for (int i = 0; i < n; ++i) {
            const float x = 0.5f * (inL[pos + i] + inR[pos + i]);
            xin[i] = x;
            det[i] = x * inputGain_.next();
            // Excitation trim followers on the raw input (what the dry path
            // carries), full band and weighted like the whole chain's response.
            float w = x - excHp_[0].process(x);
            w -= excHp_[1].process(w);
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
        splash_.process(det, clang, bite, clatter, clatterB, clatterC, jolt, n);
        if (!splashOn_) { // test hooks (Tank.h)
            for (auto* c : clat) std::fill(c, c + n, 0.0f);
            std::fill(clang, clang + n, 0.0f);
            std::fill(bite, bite + n, 0.0f);
        }
        if (!joltOn_) std::fill(jolt, jolt + n, 0.0f);
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
        // The Clang's ceiling rises with the pickups' push (push_.out): driven
        // pickups squash a big splash on their own (SplashVoicing.h).
        const float clangCeil = splash_.clangCeiling() * push_.out, clangFloor = splash_.clangFloorShare();
        for (int i = 0; i < n; ++i) {
            const float x  = tilt_.process(driven[i]) * (inTrimFrom_ + excStep * float(tick_ + i));
            const float lo = clangLp_.process(x);
            float c = clang[i];
            if (clangCeil > 0.0f) { // SPLASH stronger voicings: the Clang's ceiling (SplashVoicing.h)
                const float hi = x - lo, a = hi < 0.0f ? -hi : hi;
                clangEnv_ += (a > clangEnv_ ? clangAtt_ : clangRel_) * (a - clangEnv_);
                const float cmax = clangCeil / (clangEnv_ + 1.0e-9f), cmin = c * clangFloor;
                c = c < cmax ? c : (cmax > cmin ? cmax : cmin);
            }
            mono[i] = x + c * (x - lo) + kickScale * kickLoop[i];
        }
        // One transport for every pickup: the first echoes move together. Its
        // flutter tremolo (WOBBLE left, WobbleVoicing.h) scales the wet below.
        transport_.process(tapSamples, trem, n);
        prof::mark(prof::kTilt);
        for (size_t s = 0; s < springs_.size(); ++s) {
            const float scale = splash::kJoltSpringScale[s];
            const float* c = clat[s];
            for (int i = 0; i < n; ++i) {
                lFrac[i]    = scale * jolt[i];
                lSamples[i] = s == 0 ? (wobA[i] = wobble_[0].next()) : wobble_[s].next(wobA[i]); // B, C share A's at low WOBBLE
                loopIn[i]   = mono[i] + splash::kClatterLoop * c[i];
                high[i]     = mono[i] + splash::kClatterHigh * c[i];
            }
            springs_[s].process(loopIn, high, lFrac, lSamples, tapSamples, wet[s], n);
            prof::mark(prof::Section(prof::kSpringA + int(s)));
        }

        for (int i = 0; i < n; ++i) {
            const float dryL = inL[pos + i], dryR = inR[pos + i]; // read before write: in may alias out
            // DRIVE's heard share (ADR 0033): the tail comes back louder by
            // G^kInputHeard. Here, on the Springs' output, rather than into
            // them: the LoopSat, the AntiRes fade and the Howl see the same
            // level at every DRIVE (the tail's length and colour don't move
            // with DRIVE), and the pickups' hardness is divided by the same
            // gain (controlTick), so they bend the louder tail as before.
            const float wg = kWetGain * heardGain_.next() * trem[i];
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
            const float d = mixCur_.decorr * dd;
            // Output pickup (DriveOut), one per channel.
            // The Kick's direct thump joins the mid here: centred, mono-safe,
            // coloured by the pickups like the tank body moving under them.
            const float body = mid + kWetGain * kickDirect[i];
            float wl = driveOut_[0].process(body + side + d);
            float wr = driveOut_[1].process(body - side - d);
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
            wl = ll + kShelfGain * (wl - ll);
            wr = lr + kShelfGain * (wr - lr);

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
        pos += n;
        tick_ = (tick_ + n) % kControlInterval;
    }
    numPendingKicks_ = 0;
}

} // namespace rv
