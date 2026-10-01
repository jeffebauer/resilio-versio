#pragma once
// Drive chain stages (SPEC §4.9, CONTEXT "Drive chain", "Tank-level stage"):
//   DriveIn  = input transducer -> tape          (Tank-level, mono, pre-tank)
//   Tilt     = TONE's pre-tank tilt EQ           (Tank-level, mono)
//   LoopSat  = saturator in a Spring's feedback  (per Spring; used by Spring)
//   DriveOut = output pickup                     (Tank-level, per output channel)
// Voicing numbers live in core/params/DriveVoicing.h. Everything here is
// one sample at a time, no allocation; coefficient setters run at control
// rate (the Tank's 32-sample grid) and may use libm.
//
// Smoothness (ADR 0003, 0015): anything that scales the signal (pre-gain,
// makeup, tilt gains, amounts) is ramped linearly per sample from one
// control tick to the next, so DRIVE / TONE moves and ATTITUDE Morphs never
// step. Filter coefficients change per tick in small steps (inaudible).

#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "params/DriveVoicing.h"

#include <array>
#include <cmath>

namespace rv::dsp {

// ---- Saturator curves ---------------------------------------------------------

// Smooth tanh-like soft clip: Padé approximant x(27 + x²)/(27 + 9x²), held at
// ±1 beyond |x| = 3. Its slope is ((9 - x²) / (3(3 + x²)))²: exactly 1 at 0,
// falling smoothly to exactly 0 at ±3, so the hold joins without a kink.
// Never steeper than 1 anywhere: a saturator built from it can only ever
// *reduce* gain, which is what keeps the Loop safe (gain < 1 stays < 1).
// One division, no libm: cheap on the Cortex-M7.
inline float softClip(float x)
{
    x = x > 3.0f ? 3.0f : (x < -3.0f ? -3.0f : x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
// Slope of softClip (for tests and the small-signal Loop analysis).
inline float softClipSlope(float x)
{
    if (x >= 3.0f || x <= -3.0f) return 0.0f;
    const float r = (9.0f - x * x) / (3.0f * (3.0f + x * x));
    return r * r;
}

// Asymmetric saturator: softClip(k·x)/k with a different hardness k for
// each half. Slope 1 at 0 from both sides (and curvature 0), so it is
// perfectly linear for small signals and joins smoothly at 0. The harder
// half flattens earlier: even harmonics, plus a little DC.
inline float asymClip(float x, float kPos, float kNeg)
{
    const float k = x >= 0.0f ? kPos : kNeg;
    return softClip(k * x) / k;
}
// The same with 1/k worked out beforehand (at control rate): a divide costs
// ~14 cycles on the M7 and nothing overlaps it (M3). Differs from the plain
// form in the last bit only.
inline float asymClip(float x, float kPos, float kNeg, float invPos, float invNeg)
{
    return x >= 0.0f ? softClip(kPos * x) * invPos : softClip(kNeg * x) * invNeg;
}

// ---- First-order shelf (tape pre-/de-emphasis) -------------------------------
// H(s) = (G s + wc) / (s + wc): gain 1 at DC, G far above wc. Bilinear with
// prewarp. invert() turns it into the exact inverse filter (poles <-> zeros),
// so de-emphasis(pre-emphasis(x)) == x when nothing sits between them.
struct FirstOrder {
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f, s = 0.0f;

    void setHighShelf(float hz, float gainDb, float sampleRate)
    {
        const float g  = drive::dbToGain(gainDb);
        const float w  = map::kPi * hz / sampleRate;
        const float t  = std::sin(w) / std::cos(w); // tan: prewarped wc / (2 fs) (tanf would add flash)
        const float a0 = 1.0f + t;
        b0 = (g + t) / a0;
        b1 = (t - g) / a0;
        a1 = (t - 1.0f) / a0;
    }
    void invert()
    {
        const float nb0 = 1.0f / b0, nb1 = a1 / b0, na1 = b1 / b0;
        b0 = nb0;
        b1 = nb1;
        a1 = na1;
    }
    float process(float x) // transposed direct form II
    {
        const float y = b0 * x + s;
        s = b1 * x - a1 * y;
        return y;
    }
    void reset() { s = 0.0f; }
};

// Linear per-sample ramp between control ticks: at each tick, aim(target)
// starts a straight line from the last target to the new one, covering it in
// exactly one control interval. Deterministic on the fixed tick grid.
struct Ramp {
    float value = 0.0f, target = 0.0f, step = 0.0f;
    void snap(float v) { value = target = v; step = 0.0f; }
    void aim(float t, int interval)
    {
        value = target; // land exactly on the previous target
        target = t;
        step = (t - value) / float(interval);
    }
    float next() { value += step; return value; }
};

// ---- Blended voicing ----------------------------------------------------------
// Voice (DriveVoicing.h) blended by the ATTITUDE Morph weights.
drive::Voice blendVoice(const std::array<float, 3>& w);

// ---- DriveIn: input transducer -> tape ---------------------------------------
//
//   x ─ LPF ─ LPF ─ HPF ─ × preGain ─┬ ×2 up ─────────────────────────────────────────────────┐
//                                    │  flux cut ─ asymClip                (magnetic transducer)  │
//                                    │  ─ pre-emph ─ tape clip ─ de-emph   (tape)                  │ at 2 fs
//                                    │  ─ flux restore                                            │
//                                    └ ×2 down ◄───────────────────────────────────────────────┘
//     ─ HF smear LPF ─ × makeup ─ DC block ─► out
//
// Automatic gain compensation (SPEC §4.9, ADR 0022): makeup = ATTITUDE
// trim / preGain (so small signals come out at unity) × the measured
// squash: DriveIn follows the slow mean-square level just after the
// pre-gain and just after the saturators (kAutoMakeupSeconds), and the
// square root of their ratio is how much the saturators took away. The
// Tank updates it every control tick. So a hot hit is flattened (peaks
// rounded, body brought up: compression) but the average level stays
// where it was at DRIVE 0, for quiet and loud material alike (the level DRIVE
// adds on purpose, ADR 0033, is the Tank's, on the Springs' output). The two
// followers fall together between hits, so their ratio remembers the last
// hit's squash: the next hit gets the right makeup straight away.
//
// Band-limit first (a spring driver coil loses lows and highs), then the
// saturators. The transducer saturates on flux (DriveVoicing.h kFluxHz):
// highs are cut going in and restored at the very end, so lows saturate
// first, in the transducer and the tape alike. (Restoring them before the
// tape would let the tape square off loud highs: harsh, and aliasing.) The steep 24 dB/oct low-pass also keeps high frequencies out
// of the saturators, which is most of why x2 oversampling is enough.
// Tape: the pre-emphasis boosts highs before the saturator and the
// de-emphasis (its exact inverse) cuts them after, so highs saturate first
// and come back rounded, like tape. tapeAmount crossfades the tape clip in
// and out *between* the emphasis filters: at 0 the pair cancels exactly, so
// CLEAN has no tape colour and the Morph in or out is seamless.
struct DriveInSettings {
    drive::Voice voice{};
    float inputGain = 1.0f; // the INPUT gain G (ADR 0033), linear: what the Splash hears (x · G)
    float preGain = 1.0f;   // into the saturators, linear: G × the ATTITUDE's voicing offset
    float heard   = 1.0f;   // G^kInputHeard, linear: the level DRIVE adds (the Tank applies it on the Springs' output)
    float makeup  = 1.0f;   // linear: ATTITUDE trim / preGain (the measured squash is added by DriveIn)
    float smearHz = drive::kSmearOpenHz;
};

// DriveIn settings for Morph weights w and (smoothed) DRIVE: the INPUT gain
// G and its heard share, preGain from the ADR 0014 curve, makeup = ATTITUDE
// trim / preGain. Used by the Tank; public for tests.
DriveInSettings driveInSettings(const drive::Voice& v, float drive);

class DriveIn {
public:
    void prepare(float sampleRate);
    void reset();
    // Control rate. snap = jump (first tick / reset).
    void set(const DriveInSettings& s, bool snap, int interval);
    // push / back: the Splash's Bite (ADR 0032): the saturators get the
    // signal x push, and back (<= 1) is taken off right after them, before
    // the makeup's output follower, so the automatic makeup measures only
    // what the saturators squashed (the bitten hit compared with how it
    // would have come out unbitten) and gives back nothing more. 1 / 1 = the
    // plain path, bit for bit.
    float process(float x, float push = 1.0f, float back = 1.0f)
    {
        // Low-passes before the high-pass: same response, but the high-pass
        // (poles near z = 1, where float rounding noise gets amplified) then
        // never sees loud highs, which would leave a -100 dBFS rounding hum.
        x = lp1_.process(x);
        x = lp2_.process(x);
        x = hp_.process(x);
        x *= preGain_.next();
        envIn_.process(x * x + kEnvFloor);
        x *= push;
        const float kP = kPos_, kN = kNeg_, iP = invPos_, iN = invNeg_, tk = tapeK_, itk = invTapeK_,
                    amt = tapeAmt_.next();
        float y = back * os_.process(x, [&](float u) {
            const float t = asymClip(fluxPre_.process(u), kP, kN, iP, iN); // transducer (flux domain)
            const float p = preEmph_.process(t);                            // tape
            const float s = p + amt * (softClip(tk * p) * itk - p);
            return fluxPost_.process(deEmph_.process(s));               // restore highs
        });
        envOut_.process(y * y + kEnvFloor);
        y = smear_.process(y);
        y *= makeup_.next();
        return dc_.process(y);
    }
    // Keeps the level followers away from denormals in silence (-120 dBFS).
    static constexpr float kEnvFloor = 1.0e-12f;

private:
    float sampleRate_ = 48000.0f;
    Biquad hp_, lp1_, lp2_;
    Oversampler os_;
    FirstOrder fluxPre_, fluxPost_; // magnetic transducer shelves, oversampled rate
    FirstOrder preEmph_, deEmph_;   // tape emphasis, oversampled rate
    OnePoleLowpass smear_;
    DcBlocker dc_;
    Ramp preGain_, makeup_, tapeAmt_;
    OnePoleLowpass envIn_, envOut_; // automatic gain compensation
    float kPos_ = 1.0f, kNeg_ = 1.0f, tapeK_ = 1.0f;
    float invPos_ = 1.0f, invNeg_ = 1.0f, invTapeK_ = 1.0f;
    // Last designed values (skip redesigns at rest).
    float emphDb_ = -1.0f, fluxDb_ = -1.0f, hpHz_ = -1.0f, lpHz_ = -1.0f, smearHz_ = -1.0f;
};

// ---- Tilt: TONE's pre-tank tilt EQ (DriveVoicing.h "TONE tilt") ------------------
class Tilt {
public:
    void prepare(float sampleRate)
    {
        split_.setCutoff(drive::kTiltPivotHz, sampleRate);
        ceiling_.setCutoff(std::min(drive::kTiltCeilingHz, 0.45f * sampleRate), sampleRate);
        sampleRate_ = sampleRate;
        tone_ = -1.0f;
        reset();
    }
    void reset()
    {
        split_.reset();
        ceiling_.reset();
        lowCut_.reset();
        order_.reset();
        lowCutB_.reset();
        orderB_.reset();
        hit_.snap(0.0f);
    }
    void set(float tone, bool snap, int interval);
    // Big Knob voicing (DriveVoicing.h "Big Knob TONE voicings"; Renderer
    // only, the firmware and plugin keep drive::kToneDefaultVoicing).
    void setVoicing(int v)
    {
        voicing_ = v;
        tone_    = -1.0f; // redesign on the next set()
    }
    int voicing() const { return voicing_; }
    float process(float x)
    {
        const float lo = split_.process(x);
        const float hi = x - lo;
        // Highs × hiGain, but a boost (hiGain > 1) only applies below the
        // ceiling: the extra (hiGain - 1)·hi goes through a low-pass. A cut
        // (CCW) applies to all highs. boost_ switches only at noon, where the
        // extra is 0, so it never causes a jump.
        const float g = hi_.next();
        const float extra = (g - 1.0f) * (boost_ ? ceiling_.process(hi) : hi);
        if (!boost_) ceiling_.process(hi); // keep its state live for a smooth hand-over
        const float in = lo * lo_.next() + hi + extra;
        const float y  = lowCut_.process(in); // bright-side low cut (drive::bigKnob)
        // Big Knob's 1st-order section: k 0 (noon, CCW, voicing 0) passes y
        // through exactly.
        const float k  = k_.next();
        const float out = y - k * order_.process(y);
        if (voicing_ != drive::kToneVoicingHits) return out;
        // Voicing 5: the gentle bump's path, blended in while a hit lasts.
        const float yb = lowCutB_.process(in);
        const float ob = yb - k * orderB_.process(yb);
        return out + hit_.next() * (ob - out);
    }
    // Voicing 5: how much of the bumped path (0..1), once per control tick.
    void setHitBlend(float target, int interval) { hit_.aim(target, interval); }

private:
    OnePoleLowpass split_, ceiling_, order_, orderB_;
    Biquad lowCut_, lowCutB_;
    float sampleRate_ = 48000.0f;
    Ramp lo_, hi_, k_, hit_;
    int voicing_ = drive::kToneDefaultVoicing;
    float kTarget_ = 0.0f;
    bool boost_ = false;
    float tone_ = -1.0f, loGain_ = 1.0f, hiGain_ = 1.0f;
};

// ---- LoopSat: inside a Spring's feedback path -------------------------------
//
//   x ─ HF cut ─ ×2 up ─ y = u + amount · (asymClip(u) - u) ─ ×2 down ─ HF restore ─► out
//
// amount 0 (CLEAN) is exactly linear; the oversampler still runs so the
// Loop's latency never changes with ATTITUDE (a changing delay would bend
// pitch / click).
//
// Flux shelves (M8, HighsLater re-tune): like the two transducers, the
// LoopSat saturates on flux: highs are cut by drive::kLoopFluxDb above
// drive::kLoopFluxHz going in and restored by the exact inverse coming out.
// Small signals pass unchanged (the pair cancels, so the Loop's gain, T60
// and round trip are exactly as designed, and CLEAN is untouched); a loud
// Loop squashes on its body (lows, low mids) while the top of the Chirp
// band and anything above fC saturate less. Why: at DRIVE 1 the pushed
// curve (ADR 0022's LoopSat push, removed by ADR 0033) was hard enough that a loud high tone in the
// Loop (a 0 dBFS 5 kHz sine leaks through the fC low-pass at ~-11 dB)
// was squared off, and its 19th harmonic (95 kHz at the doubled rate)
// folded back to 1 kHz, where the Loop rings: -69 dBFS, -53 dB re the
// tone in KICKED (test_drive, limit -60). With the shelves it is under
// -88 dBFS. The shelves run at the base rate (cheap: they are linear).
class LoopSat {
public:
    // Designs the flux shelves. Without it (standalone tests) they pass
    // everything unchanged.
    void prepare(float sampleRate)
    {
        fluxPre_.setHighShelf(drive::kLoopFluxHz, -drive::kLoopFluxDb, sampleRate);
        fluxPost_ = fluxPre_;
        fluxPost_.invert();
    }
    void reset()
    {
        os_.reset();
        fluxPre_.reset();
        fluxPost_.reset();
    }
    void set(float amount, float kPos, float kNeg)
    {
        amount_ = amount;
        kPos_   = kPos;
        kNeg_   = kNeg;
        invPos_ = 1.0f / kPos; // set() runs per redesign, process() per sample
        invNeg_ = 1.0f / kNeg;
    }
    // Blend only, hardness unchanged: the Tank's quiet-tail fade (AntiRes.h
    // "LoopSat quiet-tail fade"), a per-tick change without a Loop redesign.
    void setAmount(float amount) { amount_ = amount; }
    float process(float x)
    {
        const float a = amount_, kP = kPos_, kN = kNeg_, iP = invPos_, iN = invNeg_;
        const float y = os_.process(fluxPre_.process(x),
                                    [a, kP, kN, iP, iN](float u) { return u + a * (asymClip(u, kP, kN, iP, iN) - u); });
        return fluxPost_.process(y);
    }
    // Latency (samples) at freqHz, counted in the Loop round trip (the
    // shelves add none: they cancel).
    static float latencySamples(float freqHz, float sampleRate) { return Oversampler::latencySamples(freqHz, sampleRate); }

private:
    Oversampler os_;
    FirstOrder fluxPre_, fluxPost_;
    float amount_ = 0.0f, kPos_ = 1.0f, kNeg_ = 1.0f, invPos_ = 1.0f, invNeg_ = 1.0f;
};

// ---- DriveOut: output pickup, one per output channel -------------------------------
//
//   x ─ LPF ─ HF cut ─ ×2 up ─ asymClip ─ ×2 down ─ HF restore ─ HPF ─ × makeup ─► out
//
// A second magnetic transducer, after the springs. It is the one stage
// that hears the *finished* tail, so what it does is heard as is, not
// smeared by the springs: that makes it where DRIVE's grit and "pushed"
// density become obvious (ADR 0022). Its hardness rises with DRIVE
// (Voice::outDriveDb); at DRIVE 0 it only bends near full scale.
// Like the input transducer it saturates on flux (lows first): highs are
// cut by kOutFluxDb above kOutFluxHz going in and restored after, so it
// thickens and grits the body of the tail without fizz (and without
// squaring off highs that would alias). The pickup's band-limit (LPF) comes
// first for the same reason. The HPF also removes the
// asymmetric clip's DC.
//
// Automatic makeup (the level stays put, ADR 0022): the pickup measures
// its own input and output level (slow mean square, kAutoMakeupSeconds)
// and the Tank turns the ratio into a makeup gain, linked across both
// channels. Squashed peaks stay squashed (that is the compressed, pushed
// sound), but the average level comes back, whatever the material: quiet
// pads and loud hits alike, so DRIVE never works as a volume knob. Slow
// enough not to pump on single hits; it just "breathes" a little, like a
// tape machine's level after a loud passage.
class DriveOut {
public:
    void prepare(float sampleRate);
    void reset();
    // Control rate: voicing blended by the Morph; push = drive::Push::out,
    // fluxCutDb = drive::Push::outFluxDb (the pickup's flux cut, M8).
    // amount = drive::Push::outAmount (parallel blend of the saturation).
    void set(const drive::Voice& v, float push, float fluxCutDb = drive::kOutFluxDb, float amount = 1.0f);
    // Makeup gain (linear), set at control rate by the Tank from both
    // channels' levels (levelIn / levelOut), ramped per sample.
    void setMakeup(float g, bool snap, int interval) { snap ? makeup_.snap(g) : makeup_.aim(g, interval); }
    // Slow (kAutoMakeupSeconds) mean-square level going into and coming out
    // of the saturator: their ratio is how much the pickup squashed.
    float levelIn() const { return envIn_.y; }
    float levelOut() const { return envOut_.y; }
    float process(float x)
    {
        const float kP = kPos_, kN = kNeg_, iP = invPos_, iN = invNeg_, a = amount_;
        x = lp_.process(x);
        envIn_.process(x * x + kEnvFloor);
        // Full saturation (DRIVEN, KICKED, CLEAN at DRIVE 1) skips the blend:
        // the plain curve is ~13 ns/sample cheaper on the desktop (M8 CPU).
        const float u0 = fluxPre_.process(x);
        float y = fluxPost_.process(
            a >= 1.0f ? os_.process(u0, [kP, kN, iP, iN](float u) { return asymClip(u, kP, kN, iP, iN); })
                      : os_.process(u0, [kP, kN, iP, iN, a](float u) { return u + a * (asymClip(u, kP, kN, iP, iN) - u); }));
        envOut_.process(y * y + kEnvFloor);
        return makeup_.next() * hp_.process(y);
    }

private:
    float sampleRate_ = 48000.0f;
    Oversampler os_;
    FirstOrder fluxPre_, fluxPost_;
    DcBlocker hp_;
    Biquad lp_;
    Ramp makeup_;
    OnePoleLowpass envIn_, envOut_;
    // Keeps the level followers away from denormals in silence (-120 dBFS).
    static constexpr float kEnvFloor = 1.0e-12f;
    float kPos_ = 0.5f, kNeg_ = 0.5f, amount_ = 1.0f, lpHz_ = -1.0f, fluxDb_ = -1.0f;
    float invPos_ = 2.0f, invNeg_ = 2.0f;
    void setFlux(float cutDb);
};

} // namespace rv::dsp
