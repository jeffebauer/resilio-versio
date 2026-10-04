// Plugin Host. Parameters are generated from the ParamSpec table; this
// file never lists parameters by hand (SPEC §6.3).

#include "PluginEditor.h"
#include "dsp/Tank.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : rv::kParams) {
        const juce::ParameterID id{p.key, 1};
        if (p.kind == rv::ParamKind::Switch3) {
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                id, p.name, juce::StringArray{p.choices[0], p.choices[1], p.choices[2]},
                rv::normalisedToSwitch(p.defaultValue)));
        } else if (p.kind == rv::ParamKind::Toggle) {
            // THROW (ADR 0039): the gate, automatable. Raw value 0 / 1.
            layout.add(std::make_unique<juce::AudioParameterBool>(id, p.name, p.defaultValue >= 0.5f));
        } else {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(0.0f, 1.0f), p.defaultValue));
        }
    }
    return layout;
}

class ResilioVersioProcessor final : public juce::AudioProcessor {
public:
    ResilioVersioProcessor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state_(*this, nullptr, "params", makeLayout())
    {
        for (const auto& p : rv::kParams)
            raw_[static_cast<size_t>(p.id)] = state_.getRawParameterValue(p.key);
    }

    void prepareToPlay(double sampleRate, int maxBlock) override
    {
        tank_.prepare(float(sampleRate), maxBlock);
        setLatencySamples(0); // reported to the host once Core adds latency (M2 criterion)
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::stereo()
            && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;

        for (const auto& p : rv::kParams) {
            const float v = raw_[static_cast<size_t>(p.id)]->load();
            tank_.setParam(p.id, p.kind == rv::ParamKind::Switch3 ? rv::switchToNormalised(int(v)) : v);
        }

        // The panel's KICK button: one Kick at the start of this block.
        if (panel_.takeKick())
            tank_.kick(0);
        // THROW (the throw_gate param above, ADR 0039) is the gate: the Tank
        // reads its changes at the block's start. MIDI notes stay Kicks.
        // Any note-on = one Kick at its exact sample position; velocity ignored (ADR 0005).
        for (const auto m : midi)
            if (m.getMessage().isNoteOn())
                tank_.kick(m.samplePosition);

        const int n = buffer.getNumSamples();
        const float* inL = buffer.getReadPointer(0);
        const float* inR = getTotalNumInputChannels() > 1 ? buffer.getReadPointer(1) : inL; // mono in -> both sides

        // LED meters (ADR 0031): abs peaks of the input before the Tank and
        // the output after it, and the safety limiter's gain, as the
        // firmware's audio callback takes them. The panel reads them.
        using Link = rv::plugin::PanelLink;
        panel_.notePeak(Link::kInL, peakOf(inL, n));
        panel_.notePeak(Link::kInR, peakOf(inR, n));
        tank_.process(inL, inR, buffer.getWritePointer(0), buffer.getWritePointer(1), n);
        panel_.notePeak(Link::kOutL, peakOf(buffer.getReadPointer(0), n));
        panel_.notePeak(Link::kOutR, peakOf(buffer.getReadPointer(1), n));
        panel_.noteLimiterGain(tank_.limiterGain());
    }

    juce::AudioProcessorEditor* createEditor() override { return new rv::plugin::PanelEditor(*this, state_, panel_); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; } // max DECAY (ADR 0001)

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override
    {
        if (auto xml = state_.copyState().createXml())
            copyXmlToBinary(*xml, dest);
    }

    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size))
            state_.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    static float peakOf(const float* x, int n)
    {
        float p = 0.0f;
        for (int i = 0; i < n; ++i) p = std::max(p, std::abs(x[i]));
        return p;
    }

    juce::AudioProcessorValueTreeState state_;
    std::array<std::atomic<float>*, static_cast<size_t>(rv::ParamId::Count)> raw_{};
    rv::Tank tank_;
    rv::plugin::PanelLink panel_; // KICK button and LED meters, shared with the editor

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResilioVersioProcessor)
};

} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResilioVersioProcessor(); }
