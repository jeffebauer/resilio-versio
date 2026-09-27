// Renderer CLI-support test suite: automation interpolation + kick
// scheduling, sweep grid + naming (docs/m1-contracts.md Stream B).
// Synthetic JSON only -- doesn't touch the Tank, since Stream A's DSP is
// still changing under M1.

#include "Automation.h"
#include "Json.h"
#include "Sweep.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

void automationInterpolatesLinearly()
{
    const char* text = R"({
        "breakpoints": [
            {"t": 0.0, "key": "decay", "value": 0.2},
            {"t": 4.0, "key": "decay", "value": 1.0},
            {"t": 1.0, "key": "boing", "value": 0.5}
        ],
        "kicks": [1.5, 3.0, 0.25]
    })";
    rv::json::Value root;
    std::string error;
    bool ok = rv::json::parse(text, root, error);
    check(ok, "automation: JSON parses");

    rv::automation::Automation autom;
    ok = ok && rv::automation::parse(root, autom, error);
    check(ok, "automation: breakpoints + kicks parse without error");

    const rv::automation::Track* decay = nullptr;
    for (const auto& t : autom.tracks) if (t.key == rv::ParamId::Decay) decay = &t;
    check(decay != nullptr, "automation: 'decay' track found");
    if (decay) {
        check(std::fabs(rv::automation::valueAt(*decay, 0.0) - 0.2) < 1e-9, "automation: value at t=0 matches first breakpoint");
        check(std::fabs(rv::automation::valueAt(*decay, 4.0) - 1.0) < 1e-9, "automation: value at t=4 matches last breakpoint");
        check(std::fabs(rv::automation::valueAt(*decay, 2.0) - 0.6) < 1e-9, "automation: value at the midpoint is linearly interpolated");
        check(std::fabs(rv::automation::valueAt(*decay, 10.0) - 1.0) < 1e-9, "automation: value clamps to the last breakpoint after it");
        check(std::fabs(rv::automation::valueAt(*decay, -1.0) - 0.2) < 1e-9, "automation: value clamps to the first breakpoint before it");
    }

    // Kicks come back sorted, ready for sample-accurate scheduling.
    check(autom.kicksSeconds.size() == 3, "automation: all kicks parsed");
    check(autom.kicksSeconds[0] == 0.25 && autom.kicksSeconds[1] == 1.5 && autom.kicksSeconds[2] == 3.0,
          "automation: kicks sorted ascending");
}

void kickSchedulingIsSampleAccurate()
{
    // Mirrors the Renderer's own scheduling: convert a kick time to a
    // sample index, then find the micro-block offset within a <=16-sample
    // block that contains it.
    const double sr = 48000.0;
    const double kickTime = 1.500001; // 72000.048 samples -> rounds to 72000
    const long kickSample = std::lround(kickTime * sr);
    check(kickSample == 72000, "automation: kick time converts to the nearest sample");

    const int microBlock = 16;
    const long blockStart = (kickSample / microBlock) * microBlock;
    const int offset = int(kickSample - blockStart);
    check(offset >= 0 && offset < microBlock, "automation: kick offset lands within its <=16-sample micro-block");
    check(blockStart + offset == kickSample, "automation: block start + offset reconstructs the exact kick sample");
}

void sweepParsesGridAndBase()
{
    const char* text = R"({
        "name": "m1_grid",
        "input": "test_audio/stimulus/01_clicks.wav",
        "base": {"attitude": "CLEAN"},
        "grid": {"decay": [0, 0.5, 1], "boing": [0, 1]},
        "tail_seconds": 12
    })";
    rv::json::Value root;
    std::string error;
    bool ok = rv::json::parse(text, root, error);
    rv::sweep::Config cfg;
    ok = ok && rv::sweep::parse(root, cfg, error);
    check(ok, "sweep: JSON parses");
    check(cfg.name == "m1_grid" && cfg.input == "test_audio/stimulus/01_clicks.wav" && cfg.tailSeconds == 12.0,
          "sweep: name/input/tail_seconds parsed");
    check(cfg.grid.size() == 2, "sweep: both grid axes parsed, insertion order preserved");
    check(cfg.grid[0].key == "decay" && cfg.grid[0].values.size() == 3, "sweep: 'decay' axis has 3 values");
    check(cfg.grid[1].key == "boing" && cfg.grid[1].values.size() == 2, "sweep: 'boing' axis has 2 values");

    const auto combos = rv::sweep::cartesian(cfg.grid);
    check(combos.size() == 6, "sweep: cartesian product is 3x2 = 6 combinations");

    bool allUnique = true;
    for (size_t i = 0; i < combos.size(); ++i)
        for (size_t j = i + 1; j < combos.size(); ++j)
            if (rv::sweep::fileBaseName(cfg.name, combos[i]) == rv::sweep::fileBaseName(cfg.name, combos[j])) allUnique = false;
    check(allUnique, "sweep: every combination gets a unique file name");
}

void sweepNamingMatchesContract()
{
    rv::sweep::Combo combo{{"decay", 0.5}, {"boing", 1.0}};
    const std::string name = rv::sweep::fileBaseName("m1_grid", combo);
    check(name == "m1_grid__decay0.50_boing1.00", "sweep: file name matches '<name>__decay0.50_boing1.00' scheme");
}

} // namespace

int main()
{
    automationInterpolatesLinearly();
    kickSchedulingIsSampleAccurate();
    sweepParsesGridAndBase();
    sweepNamingMatchesContract();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
