// Plugin Host. Parameters are generated from the ParamSpec table; this
// file never lists parameters by hand (SPEC §6.3).

#include "PluginEditor.h"
#include "dsp/Tank.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <utility>

namespace {

// echo: TENSION's text in clocked echo mode is its note value (EchoText.h),
// so the host's parameter display reads like the panel.
juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(const rv::plugin::EchoReadout& echo)
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
            auto attributes = juce::AudioParameterFloatAttributes{};
            if (p.id == rv::ParamId::Tension)
                attributes = attributes.withStringFromValueFunction([&echo](float v, int maxLength) {
                    // Otherwise JUCE's default text for a 0-1 range (7 decimals), unchanged.
                    const char* note = echo.name(v);
                    const juce::String text = note != nullptr ? juce::String(note) : juce::String(v, 7);
                    return maxLength > 0 ? text.substring(0, maxLength) : text;
                });
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float>(0.0f, 1.0f), p.defaultValue, attributes));
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
          state_(*this, nullptr, "params", makeLayout(panel_.echo))
    {
        for (const auto& p : rv::kParams)
            raw_[static_cast<size_t>(p.id)] = state_.getRawParameterValue(p.key);
    }

    void prepareToPlay(double sampleRate, int maxBlock) override
    {
        tank_.prepare(float(sampleRate), maxBlock);
        exitsSeen_ = 0; // the Tank's count starts again
        held_.fill(false);
        numHeld_ = 0;
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

        // SPRINGS 3 echo mode's clock (ADR 0041): the DAW's tempo, one beat
        // = its quarter note (the Versio's gate pulse); none = free time.
        float bpm = 0.0f;
        if (auto* head = getPlayHead())
            if (const auto pos = head->getPosition())
                if (const auto b = pos->getBpm()) bpm = float(*b);
        tank_.setHostTempo(bpm);

        const int n = buffer.getNumSamples();

        // The panel's button (ADR 0043, ButtonLink.h): its presses and
        // releases since the last block, at this block's start. The Tank does
        // the rest (throw, exit gesture, tap tempo).
        panel_.button.feed(tank_, n);
        // THROW (the throw_gate param above, ADR 0039) is the gate: the Tank
        // reads its changes at the block's start. MIDI notes are the gate too
        // (ADR 0043): held = high, sample-accurate, any note, velocity
        // ignored, any channel; a note-on is also a clock edge (SPRINGS 3, as
        // the jack's rising edge). So a MIDI clip sequences throws; the THROW
        // switch and the notes share the one gate (the last change wins).
        for (const auto m : midi) {
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) {
                auto& h = held_[size_t(msg.getNoteNumber() & 127)];
                if (!h) {
                    h = true;
                    if (numHeld_++ == 0) tank_.gate(true, m.samplePosition);
                }
                tank_.clock(m.samplePosition);
            } else if (msg.isNoteOff()) {
                auto& h = held_[size_t(msg.getNoteNumber() & 127)];
                if (h) {
                    h = false;
                    if (--numHeld_ == 0) tank_.gate(false, m.samplePosition);
                }
            } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                held_.fill(false);
                if (std::exchange(numHeld_, 0) > 0) tank_.gate(false, m.samplePosition);
            }
        }
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
        // TENSION's readout (EchoText.h): SPRINGS 3 and the zone playing.
        const float springs = raw_[static_cast<size_t>(rv::ParamId::Springs)]->load();
        panel_.echo.note(juce::roundToInt(springs) == 2, tank_.echoDivision());
        if (tank_.throwExits() != exitsSeen_) { // the button's exit gesture: the panel's LEDs blink
            exitsSeen_ = tank_.throwExits();
            panel_.noteThrowExited();
        }
        // Tap tempo on the LEDs (ADR 0043, firmware/TapLed.h).
        panel_.noteTaps(tank_.taps(), tank_.tappedBeatSamples() / float(getSampleRate()), tank_.tapping());
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

    rv::plugin::PanelLink panel_; // the button, the LED meters and TENSION's readout, shared with the editor (before state_: its layout reads panel_.echo)
    juce::AudioProcessorValueTreeState state_;
    std::array<std::atomic<float>*, static_cast<size_t>(rv::ParamId::Count)> raw_{};
    rv::Tank tank_;
    uint32_t exitsSeen_ = 0; // Tank::throwExits() last seen (audio thread)
    std::array<bool, 128> held_{}; // MIDI notes down (the gate is high while any is)
    int numHeld_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResilioVersioProcessor)
};

} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResilioVersioProcessor(); }
