// Kick timing (SPEC §7 M2): a Kick at absolute sample N must behave exactly
// like an input impulse at sample N, whatever the block size.

#include "dsp/Tank.h"

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

constexpr int kLength = 48000;
constexpr int kKickAt = 12345;

// Render kLength samples of silence at MIX 1, with either a Kick or an input
// impulse at kKickAt, using the given block size.
std::vector<float> render(int block, bool useKick)
{
    rv::Tank tank;
    tank.prepare(48000.0f, block);
    tank.setParam(rv::ParamId::Mix, 1.0f);
    std::vector<float> in(kLength, 0.0f), outL(kLength), outR(kLength);
    if (!useKick) in[kKickAt] = rv::Tank::kKickPlaceholder; // mono sum of L = R = x is x
    for (int pos = 0; pos < kLength; pos += block) {
        const int n = std::min(block, kLength - pos);
        if (useKick && kKickAt >= pos && kKickAt < pos + n) tank.kick(kKickAt - pos);
        tank.process(in.data() + pos, in.data() + pos, outL.data() + pos, outR.data() + pos, n);
    }
    return outL;
}

} // namespace

int main()
{
    const std::vector<float> reference = render(48, false);
    bool nonSilent = false;
    for (float v : reference) nonSilent |= v != 0.0f;
    check(nonSilent, "input impulse produces a tail");

    bool allMatch = true;
    for (int block : {1, 7, 32, 48, 128, 512, 1024}) {
        const bool match = render(block, true) == reference;
        allMatch &= match;
        std::printf("      block %4d: kick %s impulse\n", block, match ? "==" : "!=");
    }
    check(allMatch, "Kick at sample N == input impulse at sample N, bit-identical for all block sizes");

    // Offsets past the block end clamp to the last sample instead of vanishing.
    rv::Tank tank;
    tank.prepare(48000.0f, 64);
    tank.setParam(rv::ParamId::Mix, 1.0f);
    std::vector<float> in(48000, 0.0f), out(48000), outR(48000);
    tank.kick(1000);
    for (int pos = 0; pos < 48000; pos += 64) tank.process(in.data() + pos, in.data() + pos, out.data() + pos, outR.data() + pos, 64);
    bool heard = false;
    for (float v : out) heard |= v != 0.0f;
    check(heard, "Kick offset beyond the block is clamped, not dropped");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
