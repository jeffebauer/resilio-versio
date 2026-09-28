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

// Static level model of DriveIn's saturators for the automatic gain
// compensation: RMS of a sine of peak `amplitude` through
// tape(transducer(preGain · x)) divided by the sine's own RMS.
float driveInLevelGain(const drive::Voice& v, float preGain, float amplitude);

// ---- DriveIn: input transducer -> tape ---------------------------------------
//
//   x ─ LPF ─ LPF ─ HPF ─ × preGain ─┬ ×2 up ─────────────────────────────────────────────────┐
//                                    │  flux cut ─ asymClip                (magnetic transducer)  │
//                                    │  ─ pre-emph ─ tape clip ─ de-emph   (tape)                  │ at 2 fs
//                                    │  ─ flux restore                                            │
//                                    └ ×2 down ◄───────────────────────────────────────────────┘
//     ─ HF smear LPF ─ × makeup ─ DC block ─► out
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
    float preGain = 1.0f; // linear
    float makeup  = 1.0f; // linear, includes the ATTITUDE trim
    float smearHz = drive::kSmearOpenHz;
};

// DriveIn settings for Morph weights w and (smoothed) DRIVE, including the
// automatic gain compensation: preGain from the ADR 0014 curve, makeup =
// ATTITUDE trim / modelled level gain. Used by the Tank; public for tests.
DriveInSettings driveInSettings(const drive::Voice& v, float drive);

class DriveIn {
public:
    void prepare(float sampleRate);
    void reset();
    // Control rate. snap = jump (first tick / reset).
    void set(const DriveInSettings& s, bool snap, int interval);
    float process(float x)
    {
        // Low-passes before the high-pass: same response, but the high-pass
        // (poles near z = 1, where float rounding noise gets amplified) then
        // never sees loud highs, which would leave a -100 dBFS rounding hum.
        x = lp1_.process(x);
        x = lp2_.process(x);
        x = hp_.process(x);
        x *= preGain_.next();
        const float kP = kPos_, kN = kNeg_, tk = tapeK_, amt = tapeAmt_.next();
        float y = os_.process(x, [&](float u) {
            const float t = asymClip(fluxPre_.process(u), kP, kN);     // transducer (flux domain)
            const float p = preEmph_.process(t);                        // tape
            const float s = p + amt * (softClip(tk * p) / tk - p);
            return fluxPost_.process(deEmph_.process(s));               // restore highs
        });
        y = smear_.process(y);
        y *= makeup_.next();
        return dc_.process(y);
    }

private:
    float sampleRate_ = 48000.0f;
    Biquad hp_, lp1_, lp2_;
    Oversampler os_;
    FirstOrder fluxPre_, fluxPost_; // magnetic transducer shelves, oversampled rate
    FirstOrder preEmph_, deEmph_;   // tape emphasis, oversampled rate
    OnePoleLowpass smear_;
    DcBlocker dc_;
    Ramp preGain_, makeup_, tapeAmt_;
    float kPos_ = 1.0f, kNeg_ = 1.0f, tapeK_ = 1.0f;
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
        tone_ = -1.0f;
        reset();
    }
    void reset()
    {
        split_.reset();
        ceiling_.reset();
    }
    void set(float tone, bool snap, int interval);
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
        return lo * lo_.next() + hi + extra;
    }

private:
    OnePoleLowpass split_, ceiling_;
    Ramp lo_, hi_;
    bool boost_ = false;
    float tone_ = -1.0f, loGain_ = 1.0f, hiGain_ = 1.0f;
};

// ---- LoopSat: inside a Spring's feedback path -------------------------------
// y = x + amount · (asymClip(x) - x), oversampled. amount 0 (CLEAN) is
// exactly linear; the oversampler still runs so the Loop's latency never
// changes with ATTITUDE (a changing delay would bend pitch / click).
class LoopSat {
public:
    void reset() { os_.reset(); }
    void set(float amount, float kPos, float kNeg)
    {
        amount_ = amount;
        kPos_   = kPos;
        kNeg_   = kNeg;
    }
    float process(float x)
    {
        const float a = amount_, kP = kPos_, kN = kNeg_;
        return os_.process(x, [a, kP, kN](float u) { return u + a * (asymClip(u, kP, kN) - u); });
    }
    // Latency (samples) at freqHz, counted in the Loop round trip.
    static float latencySamples(float freqHz, float sampleRate) { return Oversampler::latencySamples(freqHz, sampleRate); }

private:
    Oversampler os_;
    float amount_ = 0.0f, kPos_ = 1.0f, kNeg_ = 1.0f;
};

// ---- DriveOut: output pickup, one per output channel -------------------------------
// Light soft clip (only bends near full scale) -> band-limit (HPF, which
// also removes the clip's DC, and a 2nd-order LPF). Unity small-signal gain.
class DriveOut {
public:
    void prepare(float sampleRate);
    void reset();
    void set(const drive::Voice& v, bool snap);
    float process(float x)
    {
        const float kP = kPos_, kN = kNeg_;
        float y = os_.process(x, [kP, kN](float u) { return asymClip(u, kP, kN); });
        y = hp_.process(y);
        return lp_.process(y);
    }

private:
    float sampleRate_ = 48000.0f;
    Oversampler os_;
    DcBlocker hp_;
    Biquad lp_;
    float kPos_ = 0.5f, kNeg_ = 0.5f, lpHz_ = -1.0f;
};

} // namespace rv::dsp
