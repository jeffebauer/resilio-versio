#pragma once
// The output's bit depth per ATTITUDE (PROTOTYPE, owner 4 Oct 2026;
// docs/prototypes/output-mulaw/README.md). The owner liked the echo branch's
// 8-bit mu-law repeats (echo_bits_voicing D) but heard it as subtle there,
// and asked for the same "box" on the whole output in DRIVEN and KICKED:
//   CLEAN  untouched (bit for bit as today, MIX 0 still a clean passthrough)
//   DRIVEN 24 kHz / 12-bit mu-law
//   KICKED 24 kHz / 8-bit mu-law
// every SPRINGS position, dry and wet both (after MIX). dsp/OutputBits.h.
// Renderer key output_bits_voicing (0 = today, 1 = the owner's spec); the
// firmware builds only kOutputBitsDefault (RV_FIXED_VOICINGS).

namespace rv::outbits {

struct Depth {
    float bits; // per full scale, both sides (mu-law, mu 255)
};
// Per ATTITUDE (CLEAN, DRIVEN, KICKED); CLEAN's is never used (the box is bypassed).
constexpr Depth kDepth[3] = {{24.0f}, {12.0f}, {8.0f}};

constexpr int   kNumVoicings     = 2;
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
constexpr int kOutputBitsDefault = 0;
#endif

} // namespace rv::outbits
