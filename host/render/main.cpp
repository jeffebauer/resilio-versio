// Renderer: offline CLI Host. WAV in -> Core -> WAV out (SPEC §6.2).
// M0: passthrough with --set overrides. JSON presets, automation, sweeps
// and metrics arrive with M1.
//
// Usage: rv_render <in.wav> <out.wav> [--set key=value ...] [--block N]

#include "Wav.h"
#include "dsp/Tank.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

bool applySet(rv::Tank& tank, const std::string& arg)
{
    const auto eq = arg.find('=');
    if (eq == std::string::npos) return false;
    const std::string key = arg.substr(0, eq);
    const float value     = std::strtof(arg.c_str() + eq + 1, nullptr);
    for (const auto& p : rv::kParams) {
        if (key == p.key) { tank.setParam(p.id, value); return true; }
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <in.wav> <out.wav> [--set key=value ...] [--block N]\n", argv[0]);
        return 2;
    }
    const std::string inPath = argv[1], outPath = argv[2];
    int block = 48; // matches the Versio's default block size

    rv::wav::Audio in;
    std::string error;
    if (!rv::wav::read(inPath, in, error)) { std::fprintf(stderr, "read: %s\n", error.c_str()); return 1; }
    if (in.channels.size() > 2) { std::fprintf(stderr, "read: only mono or stereo input\n"); return 1; }

    std::vector<std::string> sets;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--set" && i + 1 < argc) sets.push_back(argv[++i]);
        else if (a == "--block" && i + 1 < argc) block = std::atoi(argv[++i]);
        else { std::fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
    }
    if (block < 1) block = 1;

    rv::Tank tank;
    tank.prepare(float(in.sampleRate), block);
    for (const auto& s : sets) {
        if (!applySet(tank, s)) { std::fprintf(stderr, "bad --set: %s\n", s.c_str()); return 2; }
    }

    // Mono input feeds both sides, like the Versio's L->R normalling.
    const std::vector<float>& srcL = in.channels[0];
    const std::vector<float>& srcR = in.channels.size() > 1 ? in.channels[1] : in.channels[0];
    const size_t frames = in.frames();

    rv::wav::Audio out = in;
    out.channels.assign(in.channels.size(), std::vector<float>(frames));
    std::vector<float> scratchR(frames);
    float* dstR = in.channels.size() > 1 ? out.channels[1].data() : scratchR.data();

    for (size_t pos = 0; pos < frames; pos += size_t(block)) {
        const int n = int(std::min<size_t>(size_t(block), frames - pos));
        tank.process(srcL.data() + pos, srcR.data() + pos, out.channels[0].data() + pos, dstR + pos, n);
    }

    if (!rv::wav::write(outPath, out, error)) { std::fprintf(stderr, "write: %s\n", error.c_str()); return 1; }
    std::printf("rendered %zu frames @ %d Hz -> %s\n", frames, in.sampleRate, outPath.c_str());
    return 0;
}
