#include "dsp/Drive.h"
#include "dsp/SizeOpt.h"

#include <algorithm>
#include <cmath>

namespace rv::dsp {

RV_SIZE_OPT drive::Voice blendVoice(const std::array<float, 3>& w)
{
    using V = drive::Voice;
    static constexpr float V::*kFields[] = {
        &V::bandHpHz, &V::bandLpHz, &V::transKPos, &V::transKNeg, &V::fluxCutDb, &V::driveDbMin, &V::driveDbMax,
        &V::tapeAmount, &V::tapeK, &V::preEmphDb, &V::smearHzMax, &V::loopAmount, &V::loopKPos,
        &V::loopKNeg, &V::outDriveDb, &V::outFluxOpenDb, &V::outAmount0, &V::outK, &V::outAsym, &V::outLpHz, &V::wetMakeupDb,
        &V::trimDb};
    static_assert(sizeof(kFields) / sizeof(kFields[0]) * sizeof(float) == sizeof(V), "blendVoice misses a Voice field");
    V out{};
    for (size_t a = 0; a < 3; ++a)
        for (auto f : kFields) out.*f += w[a] * (drive::kVoice[a].*f);
    return out;
}

DriveInSettings driveInSettings(const drive::Voice& v, float drive)
{
    DriveInSettings s;
    s.voice   = v;
    const float inputDb = drive::inputGainDb(drive);
    s.inputGain = drive::dbToGain(inputDb);
    s.preGain   = drive::dbToGain(drive::drivePreGainDb(v, drive));
    // The level DRIVE adds on purpose (ADR 0033); DriveIn gives back the rest.
    s.heard  = drive::dbToGain(drive::kInputHeard * inputDb);
    s.makeup = drive::dbToGain(v.trimDb) / s.preGain;
    s.smearHz = drive::smearHz(v, drive);
    return s;
}

// ---- DriveIn ------------------------------------------------------------------

RV_SIZE_OPT void DriveIn::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    dc_.setCutoff(drive::kDriveDcHz, sampleRate);
    envIn_.setCutoff(1.0f / (2.0f * map::kPi * drive::kAutoMakeupSeconds), sampleRate);
    envOut_ = envIn_;
    emphDb_ = fluxDb_ = hpHz_ = lpHz_ = smearHz_ = -1.0f;
    reset();
}

RV_SIZE_OPT void DriveIn::reset()
{
    hp_.reset();
    lp1_.reset();
    lp2_.reset();
    os_.reset();
    preEmph_.reset();
    deEmph_.reset();
    fluxPre_.reset();
    fluxPost_.reset();
    smear_.reset();
    dc_.reset();
    envIn_.y = envOut_.y = kEnvFloor;
}

void DriveIn::set(const DriveInSettings& s, bool snap, int interval)
{
    const drive::Voice& v = s.voice;
    const float nyq = 0.45f * sampleRate_;
    // Filters are redesigned only when their cutoff moved (at rest: never).
    if (v.bandHpHz != hpHz_) {
        hp_.setHighpass(v.bandHpHz, 0.7071f, sampleRate_);
        hpHz_ = v.bandHpHz;
    }
    if (v.bandLpHz != lpHz_) {
        // Two 2nd-order sections at the same cutoff (Q 0.54 and 1.31: a
        // 4th-order Butterworth), 24 dB/oct.
        lp1_.setLowpass(std::min(v.bandLpHz, nyq), 0.5412f, sampleRate_);
        lp2_.setLowpass(std::min(v.bandLpHz, nyq), 1.3066f, sampleRate_);
        lpHz_ = v.bandLpHz;
    }
    if (s.smearHz != smearHz_) {
        smear_.setCutoff(std::min(s.smearHz, nyq), sampleRate_);
        smearHz_ = s.smearHz;
    }
    if (v.preEmphDb != emphDb_) {
        // Redesign the pair, keeping each filter's running state so a Morph
        // (which moves preEmphDb a little per tick) stays continuous.
        const float sPre = preEmph_.s, sDe = deEmph_.s;
        const float osRate = sampleRate_ * float(Oversampler::kFactor);
        preEmph_.setHighShelf(drive::kPreEmphHz, v.preEmphDb, osRate);
        deEmph_ = preEmph_;
        deEmph_.invert();
        preEmph_.s = sPre;
        deEmph_.s  = sDe;
        emphDb_    = v.preEmphDb;
    }
    if (v.fluxCutDb != fluxDb_) { // same idea: exact inverse pair, state kept
        const float sPre = fluxPre_.s, sPost = fluxPost_.s;
        const float osRate = sampleRate_ * float(Oversampler::kFactor);
        fluxPre_.setHighShelf(drive::kFluxHz, -v.fluxCutDb, osRate);
        fluxPost_ = fluxPre_;
        fluxPost_.invert();
        fluxPre_.s  = sPre;
        fluxPost_.s = sPost;
        fluxDb_     = v.fluxCutDb;
    }
    kPos_     = v.transKPos;
    kNeg_     = v.transKNeg;
    tapeK_    = v.tapeK;
    invPos_   = 1.0f / kPos_;
    invNeg_   = 1.0f / kNeg_;
    invTapeK_ = 1.0f / tapeK_;
    // Measured squash (see "Automatic gain compensation"), at least 1 (never
    // turns a clean signal down) and at most preGain (never louder than the
    // signal would be with no pre-gain at all).
    const float squash = std::clamp(std::sqrt(envIn_.y / envOut_.y), 1.0f, std::max(1.0f, s.preGain));
    const float makeup = s.makeup * squash;
    if (snap) {
        preGain_.snap(s.preGain);
        makeup_.snap(makeup);
        tapeAmt_.snap(v.tapeAmount);
    } else {
        preGain_.aim(s.preGain, interval);
        makeup_.aim(makeup, interval);
        tapeAmt_.aim(v.tapeAmount, interval);
    }
}

// ---- Tilt ---------------------------------------------------------------------

void Tilt::set(float tone, bool snap, int interval)
{
    if (tone != tone_) { // two pow() per TONE move, none at rest
        const float t    = drive::toneTiltDb(tone);
        const drive::BigKnob b = drive::bigKnobPre(place_, voicing_, tone);
        const float comp = drive::toneTiltCompDb(tone);
        loGain_ = drive::dbToGain((comp - 0.5f * t));
        hiGain_ = drive::dbToGain((comp + 0.5f * t));
        lowCut_.setHighpass(b.hz, b.q, sampleRate_);
        order_.setCutoff(b.hz1, sampleRate_);
        if (voicing_ == drive::kToneVoicingHits && place_ == drive::kTonePlacePre) {
            const drive::BigKnob g = drive::bigKnobPre(place_, drive::kToneVoicingGentle, tone);
            lowCutB_.setHighpass(g.hz, g.q, sampleRate_);
            orderB_.setCutoff(g.hz1, sampleRate_);
        }
        kTarget_ = b.k;
        tone_    = tone;
    }
    const float lo = loGain_, hi = hiGain_;
    boost_ = drive::toneTiltDb(tone) > 0.0f;
    if (snap) {
        lo_.snap(lo);
        hi_.snap(hi);
        k_.snap(kTarget_);
    } else {
        lo_.aim(lo, interval);
        hi_.aim(hi, interval);
        k_.aim(kTarget_, interval);
    }
}

// ---- DriveOut -----------------------------------------------------------------

RV_SIZE_OPT void DriveOut::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    hp_.setCutoff(drive::kOutHpHz, sampleRate);
    fluxDb_ = -1.0f;
    setFlux(drive::kOutFluxDb);
    envIn_.setCutoff(1.0f / (2.0f * map::kPi * drive::kAutoMakeupSeconds), sampleRate);
    envOut_ = envIn_;
    lpHz_ = -1.0f;
    reset();
}

RV_SIZE_OPT void DriveOut::reset()
{
    os_.reset();
    fluxPre_.reset();
    fluxPost_.reset();
    hp_.reset();
    lp_.reset();
    envIn_.y = envOut_.y = kEnvFloor;
    makeup_.snap(1.0f);
}

void DriveOut::setFlux(float cutDb)
{
    if (cutDb == fluxDb_) return; // redesign only when DRIVE / the Morph moved it
    // Exact inverse pair, running state kept (as DriveIn's shelves), so a
    // DRIVE move stays continuous; linear signal passes unchanged.
    const float sPre = fluxPre_.s, sPost = fluxPost_.s;
    fluxPre_.setHighShelf(drive::kOutFluxHz, -cutDb, sampleRate_);
    fluxPost_ = fluxPre_;
    fluxPost_.invert();
    fluxPre_.s  = sPre;
    fluxPost_.s = sPost;
    fluxDb_     = cutDb;
}

void DriveOut::set(const drive::Voice& v, float push, float fluxCutDb, float amount)
{
    setFlux(fluxCutDb);
    amount_ = amount;
    // Hardness steps a little per control tick while DRIVE moves: sat(k·x)/k
    // changes smoothly with k, so the steps are far below audibility.
    kPos_   = v.outK * push;
    kNeg_   = v.outK * (1.0f + v.outAsym) * push;
    invPos_ = 1.0f / kPos_;
    invNeg_ = 1.0f / kNeg_;
    if (v.outLpHz != lpHz_) {
        lp_.setLowpass(std::min(v.outLpHz, 0.45f * sampleRate_), 0.7071f, sampleRate_);
        lpHz_ = v.outLpHz;
    }
}

} // namespace rv::dsp
