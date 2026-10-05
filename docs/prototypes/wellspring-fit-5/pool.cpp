// Wellspring fit round 5: pool memory per Tank voicing (what the firmware's
// DTCM pool would need with that voicing as its only one; limit 30,000
// floats, firmware/main.cpp kTankPoolFloats) and the Tank object's size.
//   c++ -O1 -std=c++17 -Icore docs/prototypes/wellspring-fit-5/pool.cpp build-r/librv_core.a -o build-r/r5pool
#include "dsp/Tank.h"

#include <cstdio>

int main()
{
    std::printf("desktop requiredPoolFloats %zu\n", rv::Tank::requiredPoolFloats(48000.0f));
    for (int v = 0; v < rv::tankv::kNumVoicings; ++v)
        std::printf("voicing %d: %zu floats\n", v, rv::Tank::poolFloatsForVoicing(48000.0f, v));
    for (int st : {24, 41, 64}) std::printf("one Spring, %d rings: %zu floats\n", st, rv::Spring::requiredFloats(48000.0f, st));
    std::printf("sizeof(Tank) %zu B\n", sizeof(rv::Tank));
    return 0;
}
