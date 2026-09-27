// Core + host test suite (SPEC §6.2). Dependency-free: each test returns
// true on pass. Run via ctest or directly.

#include "Wav.h"
#include "dsp/Tank.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

void paramSpecIsWellFormed()
{
    std::set<std::string> keys;
    bool ok = true;
    for (size_t i = 0; i < rv::kParams.size(); ++i) {
        const auto& p = rv::kParams[i];
        ok &= static_cast<size_t>(p.id) == i; // table order == enum order
        ok &= keys.insert(p.key).second;      // unique keys
        ok &= p.defaultValue >= 0.0f && p.defaultValue <= 1.0f;
        ok &= p.smoothingMs > 0.0f;
        if (p.kind == rv::ParamKind::Switch3)
            ok &= p.choices[0] && p.choices[1] && p.choices[2];
    }
    check(ok, "ParamSpec: ordered, unique keys, defaults in 0-1, switch labels");
}

void switchMappingRoundTrips()
{
    bool ok = true;
    for (int pos = 0; pos < 3; ++pos)
        ok &= rv::normalisedToSwitch(rv::switchToNormalised(pos)) == pos;
    check(ok, "Switch3 position <-> Normalised round-trips");
}

void tankPassthroughIsIdentity()
{
    rv::Tank tank;
    tank.prepare(48000.0f, 48);
    tank.setParam(rv::ParamId::Mix, 0.0f);
    std::vector<float> l(4800), r(4800), ol(4800), orr(4800);
    for (int i = 0; i < 4800; ++i) { l[i] = float(i % 48) / 48.0f - 0.5f; r[i] = -l[i]; }
    for (int pos = 0; pos < 4800; pos += 48)
        tank.process(l.data() + pos, r.data() + pos, ol.data() + pos, orr.data() + pos, 48);
    check(l == ol && r == orr, "Tank mix=0 gives bit-identical dry passthrough");
}

void wavRoundTripIsBitIdentical()
{
    rv::wav::Audio a;
    a.sampleRate = 48000;
    a.bitsPerSample = 24;
    a.channels.assign(2, std::vector<float>(1000));
    for (int i = 0; i < 1000; ++i) {
        a.channels[0][i] = float((i * 7919) % 16777216 - 8388608) / 8388608.0f;
        a.channels[1][i] = -a.channels[0][i] * 0.5f;
    }
    // Quantise once so the reference is exactly representable in 24-bit.
    std::string err;
    const std::string p1 = "rv_test_a.wav", p2 = "rv_test_b.wav";
    rv::wav::Audio q, back;
    bool ok = rv::wav::write(p1, a, err) && rv::wav::read(p1, q, err)
           && rv::wav::write(p2, q, err) && rv::wav::read(p2, back, err);
    ok = ok && q.channels == back.channels && back.sampleRate == 48000 && back.bitsPerSample == 24;
    std::remove(p1.c_str());
    std::remove(p2.c_str());
    check(ok, "WAV 24-bit write/read round-trip is bit-identical");
}

} // namespace

int main()
{
    paramSpecIsWellFormed();
    switchMappingRoundTrips();
    tankPassthroughIsIdentity();
    wavRoundTripIsBitIdentical();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
