#pragma once
// The output's bit depth per ATTITUDE (ADR 0042, owner 4 Oct 2026; method
// and measurements in docs/prototypes/output-mulaw/README.md). The owner liked the echo branch's
// 8-bit mu-law repeats (echo_bits_voicing D) but heard it as subtle there,
// and asked for the same "box" on the whole output in DRIVEN and KICKED:
//   CLEAN  untouched (bit for bit as today, MIX 0 still a clean passthrough)
//   DRIVEN 24 kHz / 12-bit mu-law
//   KICKED 24 kHz / 10-bit mu-law (8-bit until 5 Oct 2026: owner, "8-bit on
//          KICKED has good character, but maybe sounds a little digital. Try
//          10-bit instead of 8 for a little more grit than 12, but not as
//          much as 8"; ADR 0042 amendment)
// every SPRINGS position. Since 5 Oct 2026 (ADR 0042 amendment) the wet
// only, before TONE's return filter; the dry is untouched. dsp/OutputBits.h.
// The owner picked it on every panel (renders/feat_output_mulaw, 4 Oct 2026):
// the default everywhere. Renderer key output_bits_voicing 0 renders "before
// the box" (the pre-ADR 0042 reference, and the tests' hook for reading the
// Tank before it); the firmware builds only kOutputBitsDefault
// (RV_FIXED_VOICINGS: the hook compiles out, costing nothing).

namespace rv::outbits {

struct Depth {
    float bits; // per full scale, both sides (mu-law, mu 255)
};
// Per ATTITUDE (CLEAN, DRIVEN, KICKED); CLEAN's is never used (the box is bypassed).
constexpr Depth kDepth[3] = {{24.0f}, {12.0f}, {10.0f}};

constexpr int   kNumVoicings     = 2;
// The box's full scale (Tank units). Since it moved before the limiter (ADR
// 0042 amendment, 5 Oct 2026) it sees the wet before the limiter, which
// runs hotter than 0 dBFS: up to 1.95 in DRIVEN and 2.29 in KICKED on the
// stimulus at DRIVE 0.4 / 1, SPLASH 1, every SPRINGS (06_noise_bursts the
// worst). A clip at the box's 24 kHz would fold its harmonics back into the
// band (new pitches), so full scale sits above them: 2.5 (+8 dB). The
// bottom step moves up with it (KICKED's lowest ~-79 dBFS, DRIVEN's ~-91).
constexpr float kFullScale       = 2.5f;
constexpr float kFadeSeconds     = 0.020f; // an ATTITUDE flip crossfades the box settings (ADR 0003: no click)
// TPDF dither (+-1 step) down to the last step, fading out between 1 and 0.5
// steps of signal; nearest rounding, so under half a step is exactly 0. (The
// echo's loop fades it from 4 steps and rounds toward zero below: here,
// with no loop, those undithered last steps made tails end in a pitched
// fizz; OutputBits.h.)
constexpr float kDitherFullLsb   = 1.0f;
constexpr float kDitherOffLsb    = 0.5f;
// The signal's level for the dither (in the compressed domain, where even a
// loud low note is only near zero for microseconds around its crossings).
// The echo's 40 ms kept a step of dither hiss going 0.1-0.25 s after the
// input stopped; 1 ms lets the output reach exact silence within ~10 ms.
constexpr float kEnvReleaseMs    = 1.0f;

#ifdef RV_OUTPUT_BITS_DEFAULT
constexpr int kOutputBitsDefault = RV_OUTPUT_BITS_DEFAULT;
#else
constexpr int kOutputBitsDefault = 1; // owner, 4 Oct 2026: B on every panel (ADR 0042)
#endif

} // namespace rv::outbits
