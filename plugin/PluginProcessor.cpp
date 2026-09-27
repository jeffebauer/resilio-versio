// Plugin Host. Parameters are generated from the ParamSpec table; this
// file never lists parameters by hand (SPEC §6.3).

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

        // Any note-on = one Kick at its exact sample position; velocity ignored (ADR 0005).
        for (const auto m : midi)
            if (m.getMessage().isNoteOn())
                tank_.kick(m.samplePosition);

        const int n = buffer.getNumSamples();
        const float* inL = buffer.getReadPointer(0);
        const float* inR = getTotalNumInputChannels() > 1 ? buffer.getReadPointer(1) : inL; // mono in -> both sides
        tank_.process(inL, inR, buffer.getWritePointer(0), buffer.getWritePointer(1), n);
    }

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Resilio Versio"; }
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
    juce::AudioProcessorValueTreeState state_;
    std::array<std::atomic<float>*, static_cast<size_t>(rv::ParamId::Count)> raw_{};
    rv::Tank tank_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResilioVersioProcessor)
};

} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResilioVersioProcessor(); }
