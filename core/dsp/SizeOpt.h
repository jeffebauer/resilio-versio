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
