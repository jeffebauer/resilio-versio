#include "dsp/Drive.h"

#include <algorithm>
#include <cmath>

namespace rv::dsp {

drive::Voice blendVoice(const std::array<float, 3>& w)
{
    using V = drive::Voice;
    static constexpr float V::*kFields[] = {
        &V::bandHpHz, &V::bandLpHz, &V::transKPos, &V::transKNeg, &V::fluxCutDb, &V::driveDbMin, &V::driveDbMax,
        &V::tapeAmount, &V::tapeK, &V::preEmphDb, &V::smearHzMax, &V::loopAmount, &V::loopKPos,
        &V::loopKNeg, &V::outK, &V::outAsym, &V::outLpHz, &V::compRef, &V::trimDb};
    static_assert(sizeof(kFields) / sizeof(kFields[0]) * sizeof(float) == sizeof(V), "blendVoice misses a Voice field");
    V out{};
    for (size_t a = 0; a < 3; ++a)
        for (auto f : kFields) out.*f += w[a] * (drive::kVoice[a].*f);
    return out;
}

float driveInLevelGain(const drive::Voice& v, float preGain, float amplitude)
{
    // 16 points over one period (both halves: the transducer is asymmetric),
    // at phases (i + ½)/16: sin of those is ± these four values. A table, so
    // a DRIVE CV sweep (re-modelled every control tick) costs no sinf calls.
    // The flux and emphasis shelves are left out (unity at the low
    // frequencies that saturate); Voice::compRef is calibrated with them in.
    static constexpr float kSin[4] = {0.19509032f, 0.55557023f, 0.83146961f, 0.98078528f};
    constexpr int kPoints = 16;
    double in = 0.0, out = 0.0;
    for (int i = 0; i < kPoints; ++i) {
        const int q = i % 8; // 0..7 over a half period: rises then falls
        const float s = kSin[q < 4 ? q : 7 - q] * (i < 8 ? 1.0f : -1.0f);
        const float x = amplitude * s;
        const float t = asymClip(preGain * x, v.transKPos, v.transKNeg);
        const float y = t + v.tapeAmount * (softClip(v.tapeK * t) / v.tapeK - t);
        in += double(x) * x;
        out += double(y) * y;
    }
    return in > 0.0 ? float(std::sqrt(out / in)) : 1.0f;
}

DriveInSettings driveInSettings(const drive::Voice& v, float drive)
{
    DriveInSettings s;
    s.voice   = v;
    s.preGain = drive::dbToGain(drive::drivePreGainDb(v, drive));
    const float level = driveInLevelGain(v, s.preGain, v.compRef);
    s.makeup  = drive::dbToGain(v.trimDb) / std::max(level, 1.0e-3f);
    s.smearHz = drive::smearHz(v, drive);
    return s;
}

// ---- DriveIn ------------------------------------------------------------------

void DriveIn::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    dc_.setCutoff(drive::kDriveDcHz, sampleRate);
    emphDb_ = fluxDb_ = hpHz_ = lpHz_ = smearHz_ = -1.0f;
    reset();
}

void DriveIn::reset()
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
    kPos_  = v.transKPos;
    kNeg_  = v.transKNeg;
    tapeK_ = v.tapeK;
    if (snap) {
        preGain_.snap(s.preGain);
        makeup_.snap(s.makeup);
        tapeAmt_.snap(v.tapeAmount);
    } else {
        preGain_.aim(s.preGain, interval);
        makeup_.aim(s.makeup, interval);
        tapeAmt_.aim(v.tapeAmount, interval);
    }
}

// ---- Tilt ---------------------------------------------------------------------

void Tilt::set(float tone, bool snap, int interval)
{
    if (tone != tone_) { // two pow() per TONE move, none at rest
        const float t    = drive::toneTiltDb(tone);
        const float comp = drive::toneTiltCompDb(tone);
        loGain_ = drive::dbToGain((comp - 0.5f * t));
        hiGain_ = drive::dbToGain((comp + 0.5f * t));
        tone_   = tone;
    }
    const float lo = loGain_, hi = hiGain_;
    boost_ = drive::toneTiltDb(tone) > 0.0f;
    if (snap) {
        lo_.snap(lo);
        hi_.snap(hi);
    } else {
        lo_.aim(lo, interval);
        hi_.aim(hi, interval);
    }
}

// ---- DriveOut -----------------------------------------------------------------

void DriveOut::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    hp_.setCutoff(drive::kOutHpHz, sampleRate);
    lpHz_ = -1.0f;
    reset();
}

void DriveOut::reset()
{
    os_.reset();
    hp_.reset();
    lp_.reset();
}

void DriveOut::set(const drive::Voice& v, bool)
{
    kPos_ = v.outK;
    kNeg_ = v.outK * (1.0f + v.outAsym);
    if (v.outLpHz != lpHz_) {
        lp_.setLowpass(std::min(v.outLpHz, 0.45f * sampleRate_), 0.7071f, sampleRate_);
        lpHz_ = v.outLpHz;
    }
}

} // namespace rv::dsp
