// Renderer: offline CLI Host. WAV in -> Core -> WAV out (SPEC §6.2).
// M1 adds presets, automation, sweeps, metrics/sidecars and --analyze
// (docs/m1-contracts.md Stream B).
//
// Usage:
//   rv_render <in.wav> <out.wav> [--set key=value ...] [--preset p.json]
//             [--auto a.json] [--block N] [--sidecar]
//   rv_render --sweep sweep.json --out-dir DIR
//   rv_render --analyze <in.wav> [--sidecar-out x.json] [--channel L|R|mix]
//   Hidden, Renderer-only key (ParamsJson.h): wobble_voicing = 0 / 1 / 2 / 3 (A / B / C / D,
//   core/params/WobbleVoicing.h), in --set, a --preset, or a sweep base / grid.

#include "Automation.h"
#include "Json.h"
#include "Metrics.h"
#include "ParamsJson.h"
#include "Sidecar.h"
#include "Spectrogram.h"
#include "Sweep.h"
#include "Wav.h"
#include "dsp/Tank.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

namespace {

using rv::wav::Audio;

void usage(const char* argv0)
{
    std::fprintf(stderr,
        "usage: %s <in.wav> <out.wav> [--set key=value ...] [--preset p.json] [--auto a.json] [--block N] [--sidecar]\n"
        "       %s --sweep sweep.json --out-dir DIR\n"
        "       %s --analyze <in.wav> [--sidecar-out x.json] [--channel L|R|mix]\n",
        argv0, argv0, argv0);
}

bool applySet(rv::Tank& tank, const std::string& arg)
{
    const auto eq = arg.find('=');
    if (eq == std::string::npos) return false;
    const std::string key = arg.substr(0, eq);
    const float value     = std::strtof(arg.c_str() + eq + 1, nullptr);
    if (rv::paramsjson::applyHidden(tank, key, value)) return true; // e.g. wobble_voicing=2
    for (const auto& p : rv::kParams) {
        if (key == p.key) { tank.setParam(p.id, value); return true; }
    }
    return false;
}

// Mono downmix used by every analysis metric (sum of channels / channel
// count, matching the metrics contract).
std::vector<float> downmix(const Audio& a)
{
    const size_t frames = a.frames();
    std::vector<float> mono(frames, 0.0f);
    const size_t ch = a.channels.size();
    for (size_t i = 0; i < frames; ++i) {
        double sum = 0.0;
        for (size_t c = 0; c < ch; ++c) sum += double(a.channels[c][i]);
        mono[i] = ch ? float(sum / double(ch)) : 0.0f;
    }
    return mono;
}

std::string isoTimestamp()
{
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

// Renders `in` through `tank`, applying automation at <=16-sample
// granularity (sample-accurate kicks): breakpoints re-interpolated at
// every micro-block boundary, kicks fired at their exact sample offset
// within the micro-block they land in.
Audio renderWithAutomation(rv::Tank& tank, const Audio& in, int block, const rv::automation::Automation* autom)
{
    const std::vector<float>& srcL = in.channels[0];
    const std::vector<float>& srcR = in.channels.size() > 1 ? in.channels[1] : in.channels[0];
    const size_t frames = in.frames();
    const float sr = float(in.sampleRate);

    // Output is always stereo, like the Versio's outputs and the Plugin:
    // the Tank places Springs across L/R even for a mono input.
    Audio out = in;
    out.channels.assign(2, std::vector<float>(frames));
    float* dstR = out.channels[1].data();

    std::vector<size_t> kickSamples;
    if (autom) {
        for (double t : autom->kicksSeconds) kickSamples.push_back(size_t(std::lround(t * double(sr))));
    }
    size_t nextKick = 0;

    const int microBlock = autom ? std::min(block, 16) : block;
    for (size_t pos = 0; pos < frames; pos += size_t(microBlock)) {
        const int n = int(std::min<size_t>(size_t(microBlock), frames - pos));
        if (autom) {
            const double t = double(pos) / double(sr);
            for (const auto& track : autom->tracks)
                tank.setParam(track.key, float(rv::automation::valueAt(track, t)));
        }
        while (nextKick < kickSamples.size() && kickSamples[nextKick] < pos + size_t(n)) {
            if (kickSamples[nextKick] >= pos) tank.kick(int(kickSamples[nextKick] - pos));
            ++nextKick;
        }
        tank.process(srcL.data() + pos, srcR.data() + pos, out.channels[0].data() + pos, dstR + pos, n);
    }
    return out;
}

// Builds a Tank prepared with defaults, applies preset then --set
// overrides. Returns false with `error` set on a bad preset/--set.
bool buildTank(rv::Tank& tank, float sampleRate, int block, const std::string& presetPath,
               const std::vector<std::string>& sets, std::string& error)
{
    tank.prepare(sampleRate, block);
    if (!presetPath.empty()) {
        rv::json::Value preset;
        if (!rv::json::loadFile(presetPath, preset, error)) return false;
        if (!rv::paramsjson::applyPreset(tank, preset, error)) return false;
    }
    for (const auto& s : sets) {
        if (!applySet(tank, s)) { error = "bad --set: " + s; return false; }
    }
    return true;
}

// Analyzes `mono` and prints the one-line metrics summary. Returns the
// computed metrics + spectrogram for sidecar writing.
void analyzeAndReport(const Audio& audio, const std::vector<float>& mono, rv::metrics::Metrics& outM,
                       rv::spectrogram::Spectrogram& outSpec)
{
    outM = rv::metrics::compute(audio.channels, float(audio.sampleRate));
    outSpec = rv::spectrogram::compute(mono, float(audio.sampleRate));
    std::printf("%s\n", rv::metrics::summaryLine(outM).c_str());
}

int runAnalyze(const std::string& inPath, const std::string& sidecarOut, const std::string& channel)
{
    Audio in;
    std::string error;
    if (!rv::wav::read(inPath, in, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }

    std::vector<float> mono;
    if (channel == "L") mono = in.channels[0];
    else if (channel == "R") mono = in.channels.size() > 1 ? in.channels[1] : in.channels[0];
    else mono = downmix(in);

    // Metrics use the requested channel selection directly (as `mono`
    // stands in for the whole file here: pass it as a single-channel set
    // so nan/inf + clip counts and every analysis metric read that
    // selection, matching --channel L|R|mix). The stereo metrics
    // (docs/m4-contracts.md Stream E) are unaffected by --channel: pass
    // the file's real channels so they measure its actual stereo image
    // (e.g. a stereo wet take analyzed with --channel L still reports a
    // real stereo_correlation, not "mono").
    std::vector<std::vector<float>> single{mono};
    rv::metrics::Metrics m = rv::metrics::compute(single, float(in.sampleRate), &in.channels);
    rv::spectrogram::Spectrogram spec = rv::spectrogram::compute(mono, float(in.sampleRate));
    std::printf("%s\n", rv::metrics::summaryLine(m).c_str());

    if (!sidecarOut.empty()) {
        rv::json::Value params = rv::json::Value::makeObject(); // no Tank params for a raw analysis
        const double durationS = double(in.frames()) / double(in.sampleRate);
        rv::json::Value side = rv::sidecar::build(inPath, in.sampleRate, durationS, params, m, spec);
        if (!rv::json::saveFile(sidecarOut, side, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
        std::printf("sidecar -> %s\n", sidecarOut.c_str());
    }
    return 0;
}

int runSweep(const std::string& sweepPath, const std::string& outDir)
{
    std::string error;
    rv::sweep::Config cfg;
    if (!rv::sweep::loadFile(sweepPath, cfg, error)) { std::fprintf(stderr, "sweep: %s\n", error.c_str()); return 1; }

    Audio srcIn;
    if (!rv::wav::read(cfg.input, srcIn, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }

    // Append tail_seconds of silence so tails ring out.
    Audio tailed = srcIn;
    const size_t tailFrames = size_t(cfg.tailSeconds * double(srcIn.sampleRate));
    for (auto& ch : tailed.channels) ch.resize(ch.size() + tailFrames, 0.0f);

    std::system(("mkdir -p \"" + outDir + "\"").c_str());

    rv::json::Value manifest = rv::json::Value::makeObject();
    manifest.set("name", rv::json::Value::makeString(cfg.name));
    manifest.set("created", rv::json::Value::makeString(isoTimestamp()));
    manifest.set("input", rv::json::Value::makeString(cfg.input));
    manifest.set("ignore_flags", cfg.ignoreFlags);
    rv::json::Value renders = rv::json::Value::makeArray();

    const auto combos = rv::sweep::cartesian(cfg.grid);
    for (const auto& combo : combos) {
        rv::Tank tank;
        tank.prepare(float(tailed.sampleRate), 48);
        if (!rv::paramsjson::applyPreset(tank, cfg.base, error)) {
            std::fprintf(stderr, "sweep base preset: %s\n", error.c_str());
            return 1;
        }
        bool voiced = cfg.base.find(rv::paramsjson::kWobbleVoicingKey) != nullptr;
        for (const auto& [key, value] : combo) {
            if (rv::paramsjson::applyHidden(tank, key, value)) { voiced = true; continue; }
            rv::ParamId id;
            if (!rv::paramsjson::findParamId(key, id)) { std::fprintf(stderr, "sweep: unknown grid key '%s'\n", key.c_str()); return 1; }
            tank.setParam(id, float(value));
        }

        const std::string base = rv::sweep::fileBaseName(cfg.name, combo);
        const std::string wavName = base + ".wav";
        const std::string sidecarName = base + ".json";
        const std::string wavPath = outDir + "/" + wavName;
        const std::string sidecarPath = outDir + "/" + sidecarName;

        Audio out = renderWithAutomation(tank, tailed, 48, nullptr);
        if (!rv::wav::write(wavPath, out, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }

        std::vector<float> mono = downmix(out);
        rv::metrics::Metrics m; rv::spectrogram::Spectrogram spec;
        analyzeAndReport(out, mono, m, spec);

        rv::json::Value params = rv::sidecar::paramsToJson(tank);
        // A hidden voicing, when the sweep sets one, goes in as a letter (the
        // review page's A / B / C versions).
        if (voiced) params.set(rv::paramsjson::kWobbleVoicingKey, rv::json::Value::makeString(rv::paramsjson::wobbleVoicingLabel(tank)));
        const double durationS = double(out.frames()) / double(out.sampleRate);
        rv::json::Value side = rv::sidecar::build(wavName, out.sampleRate, durationS, params, m, spec);
        if (!rv::json::saveFile(sidecarPath, side, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }

        rv::json::Value entry = rv::json::Value::makeObject();
        entry.set("wav", rv::json::Value::makeString(wavName));
        entry.set("sidecar", rv::json::Value::makeString(sidecarName));
        entry.set("params", params);
        renders.push(entry);
        std::printf("rendered %s\n", wavName.c_str());
    }
    manifest.set("renders", renders);
    const std::string manifestPath = outDir + "/manifest.json";
    if (!rv::json::saveFile(manifestPath, manifest, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
    std::printf("manifest -> %s\n", manifestPath.c_str());
    return 0;
}

int runRender(const std::string& inPath, const std::string& outPath, int block, const std::string& presetPath,
              const std::string& autoPath, const std::vector<std::string>& sets, bool writeSidecar)
{
    Audio in;
    std::string error;
    if (!rv::wav::read(inPath, in, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }
    if (in.channels.size() > 2) { std::fprintf(stderr, "read: only mono or stereo input\n"); return 1; }

    rv::Tank tank;
    if (!buildTank(tank, float(in.sampleRate), block, presetPath, sets, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }

    rv::automation::Automation autom;
    const rv::automation::Automation* automPtr = nullptr;
    if (!autoPath.empty()) {
        if (!rv::automation::loadFile(autoPath, autom, error)) { std::fprintf(stderr, "auto: %s\n", error.c_str()); return 2; }
        automPtr = &autom;
    }

    Audio out = renderWithAutomation(tank, in, block, automPtr);
    if (!rv::wav::write(outPath, out, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
    std::printf("rendered %zu frames @ %d Hz -> %s\n", out.frames(), in.sampleRate, outPath.c_str());

    std::vector<float> mono = downmix(out);
    rv::metrics::Metrics m; rv::spectrogram::Spectrogram spec;
    analyzeAndReport(out, mono, m, spec);

    if (writeSidecar) {
        rv::json::Value params = rv::sidecar::paramsToJson(tank);
        const double durationS = double(out.frames()) / double(out.sampleRate);
        // Sidecar file name mirrors the WAV stem, next to it.
        std::string stem = outPath;
        const auto dot = stem.find_last_of('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        const std::string sidecarPath = stem + ".json";
        const auto slash = outPath.find_last_of('/');
        const std::string wavBase = slash == std::string::npos ? outPath : outPath.substr(slash + 1);
        rv::json::Value side = rv::sidecar::build(wavBase, out.sampleRate, durationS, params, m, spec);
        if (!rv::json::saveFile(sidecarPath, side, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
        std::printf("sidecar -> %s\n", sidecarPath.c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) { usage(argv[0]); return 2; }

    const std::string first = argv[1];
    if (first == "--sweep") {
        std::string sweepPath, outDir;
        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--out-dir" && i + 1 < argc) outDir = argv[++i];
            else sweepPath = a;
        }
        if (sweepPath.empty() || outDir.empty()) { usage(argv[0]); return 2; }
        return runSweep(sweepPath, outDir);
    }

    if (first == "--analyze") {
        if (argc < 3) { usage(argv[0]); return 2; }
        const std::string inPath = argv[2];
        std::string sidecarOut, channel = "mix";
        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--sidecar-out" && i + 1 < argc) sidecarOut = argv[++i];
            else if (a == "--channel" && i + 1 < argc) channel = argv[++i];
            else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
        }
        if (channel != "L" && channel != "R" && channel != "mix") { std::fprintf(stderr, "--channel must be L, R or mix\n"); return 2; }
        return runAnalyze(inPath, sidecarOut, channel);
    }

    if (argc < 3) { usage(argv[0]); return 2; }
    const std::string inPath = argv[1], outPath = argv[2];
    int block = 48; // matches the Versio's default block size
    std::string presetPath, autoPath;
    bool writeSidecar = false;
    std::vector<std::string> sets;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--set" && i + 1 < argc) sets.push_back(argv[++i]);
        else if (a == "--block" && i + 1 < argc) block = std::atoi(argv[++i]);
        else if (a == "--preset" && i + 1 < argc) presetPath = argv[++i];
        else if (a == "--auto" && i + 1 < argc) autoPath = argv[++i];
        else if (a == "--sidecar") writeSidecar = true;
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
    }
    if (block < 1) block = 1;

    return runRender(inPath, outPath, block, presetPath, autoPath, sets, writeSidecar);
}
