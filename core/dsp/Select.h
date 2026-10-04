#pragma once
// Branch-free choices for the firmware's per-sample code (perf/run16).
//
// GCC compiles most `x > y ? a : b` on floats as a branch. Where the choice
// follows the signal (its sign, an envelope's attack or release) the branch
// is a coin flip for the M7's branch predictor, and every branch splits the
// code, so the M7 can't overlap one chain of maths with the next. RV_VSEL
// (GCC, Cortex-M7 FPv5: the firmware) does the choice with the FPU's VSEL
// instead: the same IEEE comparison, the same result for every input (NaN
// included). Every other build keeps the plain C.

#if defined(__GNUC__) && !defined(__clang__) && defined(__ARM_ARCH_7EM__) && defined(__ARM_FP) && __ARM_FP == 14
#define RV_VSEL 1
#else
#define RV_VSEL 0
#endif

namespace rv::dsp {

// x > y ? a : b
inline float selGt(float x, float y, float a, float b)
{
#if RV_VSEL
    float r;
    asm("vcmpe.f32 %[x], %[y]\n\t"
        "vmrs APSR_nzcv, fpscr\n\t"
        "vselgt.f32 %[r], %[a], %[b]"
        : [r] "=t"(r)
        : [x] "t"(x), [y] "t"(y), [a] "t"(a), [b] "t"(b)
        : "cc");
    return r;
#else
    return x > y ? a : b;
#endif
}

// x < 0 ? -x : x, exactly (unlike fabs, keeps -0 as -0)
inline float absSel(float x) { return selGt(0.0f, x, -x, x); }

} // namespace rv::dsp
