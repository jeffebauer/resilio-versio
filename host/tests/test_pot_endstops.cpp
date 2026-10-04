// Release firmware knob end stops (firmware/PotEndStops.h): a pot that
// stops short of either end still gives exactly 0 / 1, noon stays noon, and
// the mapping is monotonic. Owner report, 4 Oct 2026: dry heard at MIX fully
// right on the module, not in the plugin.

#include "../../firmware/PotEndStops.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

} // namespace

int main()
{
    using rvpot::endStops;

    check(endStops(0.0f) == 0.0f, "fully CCW -> 0");
    check(endStops(0.02f) == 0.0f, "a pot stopping at 0.02 -> exactly 0");
    check(endStops(1.0f) == 1.0f, "fully CW -> 1");
    check(endStops(0.98f) == 1.0f, "a pot stopping at 0.98 -> exactly 1");
    check(endStops(-0.01f) == 0.0f && endStops(1.01f) == 1.0f, "out-of-range readings clamp");
    check(std::fabs(endStops(0.5f) - 0.5f) < 1e-6f, "noon stays noon");

    bool monotonic = true;
    float prev     = -1.0f;
    for (int i = 0; i <= 1000; ++i) {
        const float v = endStops(float(i) / 1000.0f);
        if (v < prev) monotonic = false;
        prev = v;
    }
    check(monotonic, "monotonic across the travel");

    // MIX is equal-power: at a 0.98 reading the dry used to be cos(0.98 pi/2)
    // = -30 dB; now it is fully off.
    const float dryBefore = std::cos(0.98f * 3.14159265f * 0.5f);
    const float dryAfter  = std::cos(endStops(0.98f) * 3.14159265f * 0.5f);
    char msg[160];
    std::snprintf(msg, sizeof msg, "MIX at a 0.98 pot: dry %.1f dB before, off after",
                  20.0f * std::log10(dryBefore));
    check(dryAfter < 1e-6f, msg);

    std::printf("%s\n", failures == 0 ? "ALL PASS" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
