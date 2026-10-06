// rv_render `--set key=value` parsing (rv::paramsjson::applySetArg).
// Regression: every value used to go through strtof, so
// attitude=KICKED became CLEAN and springs=2 became 3 Springs, silently.
// Checks labels, numbers and rejections in-process, then runs the real
// rv_render binary to prove --set attitude=KICKED renders the same as a
// --preset with "attitude": "KICKED" (and not like CLEAN).

#include "Json.h"
#include "ParamsJson.h"
#include "Wav.h"
#include "dsp/Tank.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

bool set(rv::Tank& tank, const char* arg)
{
    std::string error;
    return rv::paramsjson::applySetArg(tank, arg, error);
}

void labelsAndNumbers()
{
    rv::Tank tank;
    tank.prepare(48000.0f, 48);

    check(set(tank, "attitude=KICKED") && tank.param(rv::ParamId::Attitude) == 1.0f, "attitude=KICKED sets the right position");
    check(set(tank, "attitude=CLEAN") && tank.param(rv::ParamId::Attitude) == 0.0f, "attitude=CLEAN sets the left position");
    check(set(tank, "attitude=DRIVEN") && tank.param(rv::ParamId::Attitude) == 0.5f, "attitude=DRIVEN sets the centre position");
    check(set(tank, "springs=1") && tank.param(rv::ParamId::Springs) == 0.0f, "springs=1 is the label '1' (1 Spring), not a number");
    check(set(tank, "springs=2") && tank.param(rv::ParamId::Springs) == 0.5f, "springs=2 is 2 Springs, not clamped to 3");
    check(set(tank, "springs=3") && tank.param(rv::ParamId::Springs) == 1.0f, "springs=3 (the old label) is TANK ECHO");
    check(set(tank, "decay=0.8") && std::fabs(tank.param(rv::ParamId::Decay) - 0.8f) < 1e-6f, "numeric knob value accepted");
    check(set(tank, "mix=1") && tank.param(rv::ParamId::Mix) == 1.0f, "integer knob value accepted");

    // The v1.0.43 panel names (ADR 0044): same positions as the old labels above.
    check(set(tank, "attitude=VALVE") && tank.param(rv::ParamId::Attitude) == 1.0f, "attitude=VALVE sets the right position (was KICKED)");
    check(set(tank, "attitude=TAPE") && tank.param(rv::ParamId::Attitude) == 0.5f, "attitude=TAPE sets the centre position (was DRIVEN)");
    check(set(tank, "attitude=AMP") && tank.param(rv::ParamId::Attitude) == 1.0f, "attitude=AMP (the short-lived name) is VALVE");
    check(set(tank, "springs=ECHO") && tank.param(rv::ParamId::Springs) == 1.0f, "springs=ECHO is the right position (was 3)");
}

void rejections()
{
    rv::Tank tank;
    tank.prepare(48000.0f, 48);
    std::string error;

    check(!rv::paramsjson::applySetArg(tank, "attitude=kicked", error), "label is case-sensitive: 'kicked' rejected");
    check(error.find("CLEAN, TAPE or VALVE") != std::string::npos, "unknown-label error lists the valid labels");
    check(!set(tank, "springs=echo"), "springs=echo rejected (case-sensitive)");
    check(!set(tank, "attitude=HOT"), "unknown label rejected");
    check(!set(tank, "attitude=1"), "switch given a bare number rejected");
    check(!set(tank, "attitude=0.5"), "switch given a Normalised number rejected");
    check(!set(tank, "springs=0.5"), "springs=0.5 rejected (not a label)");
    check(!set(tank, "springs=4"), "springs=4 rejected");
    check(!set(tank, "decay=KICKED"), "knob given text rejected");
    check(!set(tank, "decay=0.8x"), "knob given trailing junk rejected");
    check(!set(tank, "decay="), "knob given nothing rejected");
    check(!set(tank, "decay=nan"), "knob given nan rejected");
    check(!set(tank, "nope=0.5"), "unknown param rejected");
    check(!set(tank, "decay"), "missing '=' rejected");
    check(tank.param(rv::ParamId::Attitude) == 0.5f, "rejected values leave the param at its default");
}

bool readWav(const std::string& path, rv::wav::Audio& a)
{
    std::string error;
    return rv::wav::read(path, a, error);
}

bool sameAudio(const rv::wav::Audio& a, const rv::wav::Audio& b)
{
    if (a.channels.size() != b.channels.size() || a.frames() != b.frames()) return false;
    for (size_t c = 0; c < a.channels.size(); ++c)
        for (size_t i = 0; i < a.frames(); ++i)
            if (a.channels[c][i] != b.channels[c][i]) return false;
    return true;
}

int run(const std::string& args)
{
    const std::string cmd = "./rv_render " + args + " > /dev/null 2>&1";
    return std::system(cmd.c_str());
}

// End to end through the rv_render binary (the test runs in the build dir).
void cliMatchesPreset()
{
    const std::string dir = "test_set_args_tmp";
    std::system(("mkdir -p " + dir).c_str());

    // Loud decaying noise hits, so DRIVEN/KICKED saturation has work to do.
    rv::wav::Audio in;
    in.sampleRate = 48000;
    in.channels.assign(1, std::vector<float>(48000, 0.0f));
    uint32_t seed = 1;
    for (size_t i = 0; i < in.frames(); ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float noise = float(int32_t(seed)) / 2147483648.0f;
        const size_t t = i % 12000;
        in.channels[0][i] = t < 2400 ? 0.9f * noise * std::exp(-float(t) / 600.0f) : 0.0f;
    }
    std::string error;
    const std::string inPath = dir + "/in.wav";
    check(rv::wav::write(inPath, in, error), "cli: stimulus written");

    std::ofstream(dir + "/kicked.json") << R"({ "attitude": "KICKED", "springs": "2" })";

    check(run(inPath + " " + dir + "/set.wav --set attitude=KICKED --set springs=2") == 0, "cli: --set with labels exits 0");
    check(run(inPath + " " + dir + "/preset.wav --preset " + dir + "/kicked.json") == 0, "cli: --preset exits 0");
    check(run(inPath + " " + dir + "/clean.wav --set attitude=CLEAN --set springs=2") == 0, "cli: --set attitude=CLEAN exits 0");
    check(run(inPath + " " + dir + "/valve.wav --set attitude=VALVE --set springs=2") == 0, "cli: --set attitude=VALVE exits 0");

    rv::wav::Audio viaSet, viaPreset, clean, valve;
    const bool read = readWav(dir + "/set.wav", viaSet) && readWav(dir + "/preset.wav", viaPreset) && readWav(dir + "/clean.wav", clean)
                   && readWav(dir + "/valve.wav", valve);
    check(read, "cli: renders read back");
    if (read) {
        check(sameAudio(viaSet, viaPreset), "cli: --set attitude=KICKED renders identically to --preset KICKED");
        check(!sameAudio(viaSet, clean), "cli: --set attitude=KICKED differs from CLEAN");
        check(sameAudio(viaSet, valve), "cli: --set attitude=VALVE renders identically to the old label KICKED (ADR 0044)");
    }

    check(run(inPath + " " + dir + "/bad.wav --set attitude=HOT") != 0, "cli: unknown label exits non-zero");
    check(run(inPath + " " + dir + "/bad.wav --set attitude=1") != 0, "cli: bare number on a switch exits non-zero");
    check(run(inPath + " " + dir + "/bad.wav --set decay=high") != 0, "cli: text on a knob exits non-zero");

    std::system(("rm -rf " + dir).c_str());
}

} // namespace

int main()
{
    labelsAndNumbers();
    rejections();
    cliMatchesPreset();
    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "all passed", failures, failures == 1 ? "" : "s");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
