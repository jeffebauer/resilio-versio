#pragma once
// RV_SIZE_OPT: build a control-rate function for size (-Os) in the firmware,
// where flash is the limit (ADR 0011: 128 KB). The per-sample DSP keeps -O3
// (firmware/Makefile). Mark only code that runs once per control tick or at
// set-up, never per sample. Desktop builds (and Clang) ignore it.
#if defined(RV_FIXED_VOICINGS) && defined(__GNUC__) && !defined(__clang__)
#define RV_SIZE_OPT __attribute__((optimize("Os")))
#else
#define RV_SIZE_OPT
#endif

// RV_NO_UNSWITCH: -O3 as everywhere else, without loop unswitching, for the
// Tank's per-sample block (Tank::process). Unswitching copied its whole input
// loop (TONE's tilt, the low cut, the coil, the Clang) once more for the one
// loop-invariant branch in it (the Clang's ceiling): 1.5 KB of flash for a
// predictable branch per sample (ADR 0038 Decision, "Flash").
#if defined(RV_FIXED_VOICINGS) && defined(__GNUC__) && !defined(__clang__)
#define RV_NO_UNSWITCH __attribute__((optimize("no-unswitch-loops")))
#else
#define RV_NO_UNSWITCH
#endif
