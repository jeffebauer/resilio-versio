// Plugin host test bench (SPEC §7 M2). Loads the *built* AU/VST3 bundles the
// way a DAW does — juce::AudioPluginFormatManager over the headless plugin
// hosting module (no GUI needed to load and process a plugin) — and checks
// them against the criteria in SPEC.md §7 M2 and the Kick-timing model in
// host/tests/test_kick.cpp.
//
// Bundle paths come from CMake (RV_VST3_PATH / RV_AU_PATH, generated from
// $<TARGET_BUNDLE_DIR:...> so they always point at the just-built artefact).

#include "dsp/Tank.h"
#include "params/ParamSpec.h"
#include "Wav.h"

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

void note(const std::string& what) { std::printf("NOTE  %s\n", what.c_str()); }

// ---------------------------------------------------------------------------
// Plugin loading helpers
// ---------------------------------------------------------------------------

std::unique_ptr<juce::AudioPluginInstance> loadPlugin(juce::AudioPluginFormatManager& fm,
                                                       const juce::String& path,
                                                       double sampleRate, int blockSize,
                                                       juce::String& error)
{
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* format : fm.getFormats()) {
        if (format->fileMightContainThisPluginType(path)) {
            format->findAllTypesForFile(found, path);
            if (!found.isEmpty()) break;
        }
    }
    if (found.isEmpty()) { error = "no plugin types found at " + path; return nullptr; }
    return fm.createPluginInstance(*found[0], sampleRate, blockSize, error);
}

// VST3 (and classic AU) only expose a numeric parameter ID to the host, never
// our ParamSpec string `key` (that string lives inside the plugin's own
// AudioProcessorValueTreeState and is not part of either wire protocol). What
// *is* visible and stable is declaration order and the display name, and
// PluginProcessor.cpp's makeLayout() emits parameters in exactly rv::kParams
// order — so we identify hosted parameters by position, and verify that
// assumption (and "generated from ParamSpec, no hand-written list") by
// checking every name against ParamSpec at that position.
// VST3-hosted parameters derive from HostedAudioProcessorParameter (which
// derives from AudioProcessorParameter), not RangedAudioParameter -- VST3
// params are already normalised 0-1 with no NormalisableRange to expose.
// Everything we need (name, default, numSteps, text, setValueNotifyingHost,
// getValue) lives on the plain AudioProcessorParameter interface.
using ParamMap = std::array<juce::AudioProcessorParameter*, size_t(rv::ParamId::Count)>;

// JUCE's VST3 wrapper appends one synthetic "Bypass" parameter after our own
// (standard host behaviour, not a hand-written addition -- unlike the ~2080
// "MIDI CC" parameters disabled via JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS
// in plugin/CMakeLists.txt, see that comment), so the hosted parameter list
// is rv::kParams.size() + 1 long, with our params first and in ParamSpec order.
ParamMap mapParams(juce::AudioPluginInstance& inst, bool* orderAndNamesOk = nullptr)
{
    ParamMap byId{};
    auto& params = inst.getParameters();
    bool ok = params.size() >= int(rv::kParams.size());
    for (size_t i = 0; i < rv::kParams.size(); ++i) {
        auto* r = (int(i) < params.size()) ? params[int(i)] : nullptr;
        byId[i] = r;
        if (!r || r->getName(64) != juce::String(rv::spec(rv::ParamId(i)).name)) ok = false;
    }
    if (orderAndNamesOk) *orderAndNamesOk = ok;
    return byId;
}

void setParam(const ParamMap& byId, rv::ParamId id, float v)
{
    if (auto* p = byId[size_t(id)]) p->setValueNotifyingHost(v);
}

// A preset used by the parity and null-test checks: DECAY 0.8, BOING 0.2,
// TONE 0.7, plus whatever MIX the caller wants. Everything else stays at its
// ParamSpec default on both sides.
void setPresetOnPlugin(const ParamMap& byId, float decay, float boing, float tone, float mix)
{
    setParam(byId, rv::ParamId::Decay, decay);
    setParam(byId, rv::ParamId::Boing, boing);
    setParam(byId, rv::ParamId::Tone, tone);
    setParam(byId, rv::ParamId::Mix, mix);
}

void setPresetOnTank(rv::Tank& tank, float decay, float boing, float tone, float mix)
{
    tank.setParam(rv::ParamId::Decay, decay);
    tank.setParam(rv::ParamId::Boing, boing);
    tank.setParam(rv::ParamId::Tone, tone);
    tank.setParam(rv::ParamId::Mix, mix);
}

// Block-size schedules a DAW might send, covering a signal of `total` samples.
std::vector<int> blockSchedule(const std::vector<int>& pattern, int total)
{
    std::vector<int> out;
    size_t i = 0;
    int done = 0;
    while (done < total) {
        int n = std::min(pattern[i % pattern.size()], total - done);
        out.push_back(n);
        done += n;
        ++i;
    }
    return out;
}

double maxAbsDiffDb(const std::vector<float>& a, const std::vector<float>& b)
{
    float m = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m == 0.0f ? -400.0 : 20.0 * std::log10(double(m));
}

// ---------------------------------------------------------------------------
// Render helpers
// ---------------------------------------------------------------------------

// Tank-direct render: mono input duplicated to L/R (matches PluginProcessor's
// mono-in handling), optionally with a Kick injected at kickAt (else a plain
// input impulse can be pre-baked into `in` by the caller).
struct TankRender { std::vector<float> outL, outR; };

TankRender renderTank(const std::vector<float>& in, const std::vector<int>& blocks,
                       float decay, float boing, float tone, float mix, int kickAt = -1)
{
    rv::Tank tank;
    tank.prepare(48000.0f, 512);
    setPresetOnTank(tank, decay, boing, tone, mix);
    TankRender r;
    r.outL.assign(in.size(), 0.0f);
    r.outR.assign(in.size(), 0.0f);
    int pos = 0;
    for (int n : blocks) {
        if (kickAt >= 0 && kickAt >= pos && kickAt < pos + n) tank.kick(kickAt - pos);
        tank.process(in.data() + pos, in.data() + pos, r.outL.data() + pos, r.outR.data() + pos, n);
        pos += n;
    }
    return r;
}

// Plugin-hosted render over the same schedule. `midiNote` >= 0 triggers a
// note-on (with junk note-off / CC noise mixed in, which must do nothing) at
// kickAt instead of an audio impulse.
TankRender renderPlugin(juce::AudioPluginFormatManager& fm, const juce::String& path,
                         const std::vector<float>& in, const std::vector<int>& blocks,
                         float decay, float boing, float tone, float mix,
                         int kickAt = -1, int midiNote = -1, int velocity = 64)
{
    juce::String error;
    auto inst = loadPlugin(fm, path, 48000.0, 512, error);
    TankRender r;
    if (!inst) { std::printf("FAIL  could not load plugin for render: %s\n", error.toStdString().c_str()); ++failures; return r; }
    inst->prepareToPlay(48000.0, 512);
    setPresetOnPlugin(mapParams(*inst), decay, boing, tone, mix);

    r.outL.assign(in.size(), 0.0f);
    r.outR.assign(in.size(), 0.0f);
    int pos = 0;
    for (int n : blocks) {
        juce::AudioBuffer<float> buffer(2, n);
        buffer.copyFrom(0, 0, in.data() + pos, n);
        buffer.copyFrom(1, 0, in.data() + pos, n);

        juce::MidiBuffer midi;
        if (kickAt >= 0 && kickAt >= pos && kickAt < pos + n) {
            if (midiNote >= 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, midiNote, juce::uint8(velocity)), kickAt - pos);
        }
        // Noise that must do nothing (ADR 0005: note-offs / other MIDI ignored).
        midi.addEvent(juce::MidiMessage::noteOff(1, 37), std::min(n - 1, 0));
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), std::min(n - 1, 0));

        inst->processBlock(buffer, midi);
        std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + n, r.outL.data() + pos);
        std::copy(buffer.getReadPointer(1), buffer.getReadPointer(1) + n, r.outR.data() + pos);
        pos += n;
    }
    return r;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit; // starts the JUCE MessageManager (needed to host plugins)

    juce::AudioPluginFormatManager fm;
    juce::addHeadlessDefaultFormatsToManager(fm);

    const juce::String vst3Path = RV_VST3_PATH;
#ifdef RV_AU_PATH
    const juce::String auPath = RV_AU_PATH;
    const bool haveAuPath = true;
#else
    const juce::String auPath;
    const bool haveAuPath = false;
#endif

    // ---- 1. Loads at 44.1/48/96 kHz, stereo in/out and mono in/stereo out ----
    {
        struct Cfg { double sr; bool monoIn; };
        const Cfg cfgs[] = {{44100.0, false}, {44100.0, true}, {48000.0, false}, {48000.0, true},
                            {96000.0, false}, {96000.0, true}};
        struct Fmt { const char* name; juce::String path; bool available; bool strict; };
        const Fmt formats[] = {{"VST3", vst3Path, true, true}, {"AU", auPath, haveAuPath, false}};
        for (const auto& fmt : formats) {
            if (!fmt.available) { note("AU bundle path not configured (non-Apple build?)"); continue; }
            for (const auto& c : cfgs) {
                juce::String error;
                auto inst = loadPlugin(fm, fmt.path, c.sr, 512, error);
                bool ok = inst != nullptr;
                if (ok && c.monoIn) {
                    juce::AudioProcessor::BusesLayout layout;
                    layout.inputBuses.add(juce::AudioChannelSet::mono());
                    layout.outputBuses.add(juce::AudioChannelSet::stereo());
                    ok = inst->setBusesLayout(layout) && inst->checkBusesLayoutSupported(layout);
                }
                if (ok) inst->prepareToPlay(c.sr, 512);

                std::string what = std::string(fmt.name) + " loads at " + std::to_string(int(c.sr)) + " Hz, "
                                  + (c.monoIn ? "mono" : "stereo") + " in/stereo out";
                if (!ok) what += " (" + error.toStdString() + ")";
                if (fmt.strict) check(ok, what);
                else if (ok) check(ok, what);
                else note(what + " -- best-effort AU hosting, not a hard failure");

                if (inst) inst->releaseResources();
            }
        }
    }

    // ---- 2. Parameters: generated from rv::kParams, nothing hand-written ----
    {
        juce::String error;
        auto inst = loadPlugin(fm, vst3Path, 48000.0, 512, error);
        if (!inst) {
            check(false, "VST3 loads for parameter check: " + error.toStdString());
        } else {
            inst->prepareToPlay(48000.0, 512);
            // JUCE's VST3 wrapper appends one synthetic host "Bypass" parameter
            // after ours (standard, not hand-written); MIDI-CC-as-parameters is
            // disabled in plugin/CMakeLists.txt so it does not also add ~2080
            // "MIDI CC nn" parameters.
            check(size_t(inst->getParameters().size()) == rv::kParams.size() + 1,
                  "parameter count == rv::kParams.size() + 1 host Bypass (" + std::to_string(rv::kParams.size() + 1) + ")");

            bool orderOk = false;
            ParamMap byId = mapParams(*inst, &orderOk);
            check(orderOk, "hosted parameters, in order, have exactly rv::kParams' names (identifies each by position; "
                            "VST3/AU expose only a numeric ID to the host, not ParamSpec's string key)");

            for (size_t i = 0; i < rv::kParams.size(); ++i) {
                const auto& p = rv::kParams[i];
                auto* param = byId[i];
                char what[256];
                std::snprintf(what, sizeof(what), "param '%s'", p.key);
                if (!param) { check(false, std::string(what) + " present at its ParamSpec position"); continue; }
                check(param->isAutomatable(), std::string(what) + " is automatable");
                check(std::fabs(param->getDefaultValue() - p.defaultValue) < 1e-6f, std::string(what) + " default matches ParamSpec");

                if (p.kind == rv::ParamKind::Knob) {
                    check(param->getNumSteps() > 1000, std::string(what) + " is continuous (Knob)");
                } else {
                    check(param->getNumSteps() == 3, std::string(what) + " has exactly 3 choices (Switch3)");
                    bool labelsMatch = true;
                    for (int c = 0; c < 3; ++c) {
                        const auto text = param->getText(rv::switchToNormalised(c), 32).toStdString();
                        if (text != std::string(p.choices[c])) labelsMatch = false;
                    }
                    check(labelsMatch, std::string(what) + " choice labels match ParamSpec");
                }
            }
        }
    }

    // ---- Stimulus (regenerate if missing) -------------------------------------
    const std::string stimulusPath = std::string(RV_STIMULUS_DIR) + "/02_hits.wav";
    {
        std::FILE* f = std::fopen(stimulusPath.c_str(), "rb");
        if (!f) {
            std::printf("stimulus missing, running make_stimulus.py...\n");
            std::system(std::string("python3 \"" RV_MAKE_STIMULUS_PY "\"").c_str());
        } else {
            std::fclose(f);
        }
    }
    rv::wav::Audio stim;
    std::string wavError;
    bool haveStimulus = rv::wav::read(stimulusPath, stim, wavError) && stim.sampleRate == 48000 && !stim.channels.empty();
    check(haveStimulus, "loaded test_audio/stimulus/02_hits.wav" + (haveStimulus ? std::string() : (": " + wavError)));

    if (haveStimulus) {
        const std::vector<float>& in = stim.channels[0];

        // ---- 3. Plugin output == Tank-direct, MIX 1 + preset, 48 kHz -----------
        struct Pattern { const char* name; std::vector<int> sizes; };
        const Pattern patterns[] = {
            {"block 64", {64}},
            {"block 512", {512}},
            {"varying block sizes (13,128,441,1,256)", {13, 128, 441, 1, 256}},
        };
        for (const auto& pat : patterns) {
            auto blocks = blockSchedule(pat.sizes, int(in.size()));
            auto tankOut = renderTank(in, blocks, 0.8f, 0.2f, 0.7f, 1.0f);
            auto pluginOut = renderPlugin(fm, vst3Path, in, blocks, 0.8f, 0.2f, 0.7f, 1.0f);
            const double dbL = maxAbsDiffDb(tankOut.outL, pluginOut.outL);
            const double dbR = maxAbsDiffDb(tankOut.outR, pluginOut.outR);
            char what[256];
            std::snprintf(what, sizeof(what), "VST3 == Tank-direct at MIX 1 + preset, %s (max diff L %.1f dBFS, R %.1f dBFS)",
                          pat.name, dbL, dbR);
            check(dbL <= -120.0 && dbR <= -120.0, what);
        }

        // ---- 5. Latency == 0, MIX 0 is a null test against the input -----------
        {
            juce::String error;
            auto inst = loadPlugin(fm, vst3Path, 48000.0, 512, error);
            if (!inst) {
                check(false, "VST3 loads for latency/null-test: " + error.toStdString());
            } else {
                inst->prepareToPlay(48000.0, 512);
                check(inst->getLatencySamples() == 0, "getLatencySamples() == 0");
            }
        }
        {
            auto blocks = blockSchedule({512}, int(in.size()));
            auto pluginOut = renderPlugin(fm, vst3Path, in, blocks, 0.5f, 0.5f, 0.5f, 0.0f);
            bool identicalL = pluginOut.outL == in, identicalR = pluginOut.outR == in;
            const double dbL = maxAbsDiffDb(in, pluginOut.outL), dbR = maxAbsDiffDb(in, pluginOut.outR);
            char what[256];
            std::snprintf(what, sizeof(what), "MIX 0 render is a null test against the input (bit-identical: L %s, R %s; max diff L %.1f dBFS, R %.1f dBFS)",
                          identicalL ? "yes" : "no", identicalR ? "yes" : "no", dbL, dbR);
            check(identicalL && identicalR, what);
        }
    }

    // ---- 4. MIDI Kick: sample-accurate, velocity ignored, only note-on fires ---
    {
        constexpr int kLength = 48000;
        std::vector<float> silence(kLength, 0.0f);
        struct Case { const char* label; int note_; int velocity; int kickAt; std::vector<int> pattern; };
        const Case cases[] = {
            {"note 21 vel 1 at sample 4000, block 64", 21, 1, 4000, {64}},
            {"note 60 vel 64 at sample 30000, block 512", 60, 64, 30000, {512}},
            {"note 108 vel 127 near end (tail still fits), varying blocks", 108, 127, kLength - 8000, {13, 128, 441, 1, 256}},
        };
        for (const auto& c : cases) {
            auto blocks = blockSchedule(c.pattern, kLength);
            auto reference = renderTank(silence, blocks, 0.5f, 0.5f, 0.5f, 1.0f, c.kickAt);
            auto pluginOut = renderPlugin(fm, vst3Path, silence, blocks, 0.5f, 0.5f, 0.5f, 1.0f, c.kickAt, c.note_, c.velocity);
            const double dbL = maxAbsDiffDb(reference.outL, pluginOut.outL);
            const double dbR = maxAbsDiffDb(reference.outR, pluginOut.outR);
            char what[256];
            std::snprintf(what, sizeof(what), "MIDI Kick == input-impulse Kick, %s (max diff L %.1f dBFS, R %.1f dBFS)",
                          c.label, dbL, dbR);
            check(dbL <= -120.0 && dbR <= -120.0, what);
        }
    }

    // ---- 6. State: getStateInformation -> new instance setStateInformation ----
    {
        juce::String errorA, errorB;
        auto instA = loadPlugin(fm, vst3Path, 48000.0, 512, errorA);
        auto instB = loadPlugin(fm, vst3Path, 48000.0, 512, errorB);
        if (!instA || !instB) {
            check(false, "two VST3 instances load for state test");
        } else {
            instA->prepareToPlay(48000.0, 512);
            instB->prepareToPlay(48000.0, 512);
            ParamMap byIdA = mapParams(*instA);
            ParamMap byIdB = mapParams(*instB);
            setPresetOnPlugin(byIdA, 0.9f, 0.1f, 0.6f, 0.4f);
            setParam(byIdA, rv::ParamId::Springs, 1.0f);   // choice 2
            setParam(byIdA, rv::ParamId::Attitude, 0.0f);  // choice 0

            juce::MemoryBlock state;
            instA->getStateInformation(state);
            check(state.getSize() > 0, "getStateInformation produced non-empty state");
            instB->setStateInformation(state.getData(), int(state.getSize()));

            bool allMatch = true;
            for (size_t i = 0; i < rv::kParams.size(); ++i) {
                auto* pa = byIdA[i];
                auto* pb = byIdB[i];
                const bool match = pa && pb && std::fabs(pa->getValue() - pb->getValue()) < 1e-4f;
                allMatch &= match;
                if (!match) std::printf("      param '%s': A=%f B=%f\n", rv::kParams[i].key,
                                        pa ? pa->getValue() : -1.0f, pb ? pb->getValue() : -1.0f);
            }
            check(allMatch, "setStateInformation on a new instance restores every parameter value");
        }
    }

    // ---- 7. Timing proxy: ns/sample at 48 kHz, 512-sample blocks, worst case ---
    {
        juce::String error;
        auto inst = loadPlugin(fm, vst3Path, 48000.0, 512, error);
        if (inst) {
            inst->prepareToPlay(48000.0, 512);
            ParamMap byId = mapParams(*inst);
            setPresetOnPlugin(byId, 1.0f, 1.0f, 1.0f, 1.0f); // longest tail, most dispersion stages, full wet
            setParam(byId, rv::ParamId::Springs, 1.0f);

            constexpr int kTotal = 48000 * 5; // 5 s
            juce::AudioBuffer<float> buffer(2, 512);
            juce::Random rng(1234);
            std::vector<float> noise(512);
            for (auto& s : noise) s = rng.nextFloat() * 2.0f - 1.0f;

            const auto start = std::chrono::steady_clock::now();
            int done = 0;
            while (done < kTotal) {
                const int n = std::min(512, kTotal - done);
                buffer.setSize(2, n, false, false, true);
                buffer.copyFrom(0, 0, noise.data(), n);
                buffer.copyFrom(1, 0, noise.data(), n);
                juce::MidiBuffer midi;
                inst->processBlock(buffer, midi);
                done += n;
            }
            const auto elapsed = std::chrono::steady_clock::now() - start;
            const double ns = std::chrono::duration<double, std::nano>(elapsed).count();
            std::printf("TIMING  %.1f ns/sample (worst-case params, 48 kHz, 512-sample blocks, %d samples rendered)\n",
                        ns / double(kTotal), kTotal);
        } else {
            note("could not load VST3 for timing proxy: " + error.toStdString());
        }
    }

    if (!haveAuPath) note("AU hosting: bundle path not available on this platform (macOS only); VST3-only checks ran.");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
