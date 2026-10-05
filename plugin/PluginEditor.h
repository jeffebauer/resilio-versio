#pragma once
// Plugin Host's panel: the Versio's controls at the Versio's positions
// (docs/briefs/plugin-panel-ui.md). Seven knobs, the two three-way toggles,
// the button (THROW / tap tempo, ADR 0043), the GATE stand-in and the four
// LED meters, all with JUCE's standard look.
// The parameters themselves still come only from the ParamSpec table.

#include "ButtonLink.h"
#include "EchoText.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

namespace rv::plugin {

// What the audio thread and the panel share, lock-free.
// Audio thread: notePeak() / noteLimiterGain() once per block, button.feed()
// at the start of a block. Panel (message thread): button.press() /
// release(), and takePeak() / takeLimiterGain() on its timer, which also
// clears them, the way firmware/main.cpp's main loop reads and clears gPeak /
// gLimiterGain.
// A block landing between a take's read and clear is lost; a meter can't
// show one block anyway.
struct PanelLink {
    enum Meter { kInL, kInR, kOutL, kOutR, kNumMeters };

    void notePeak(int m, float peak)
    {
        auto& p = peak_[static_cast<size_t>(m)];
        if (peak > p.load(std::memory_order_relaxed)) p.store(peak, std::memory_order_relaxed);
    }
    void noteLimiterGain(float gain)
    {
        if (gain < limiterGain_.load(std::memory_order_relaxed)) limiterGain_.store(gain, std::memory_order_relaxed);
    }
    ButtonLink button; // the panel's button (ADR 0043)
    EchoReadout echo;  // TENSION's note value in echo mode (EchoText.h)
    // Audio thread: throw mode was on and is now off (the LEDs blink).
    void noteThrowExited() { throwExited_.store(true, std::memory_order_release); }
    bool takeThrowExited() { return throwExited_.exchange(false, std::memory_order_acq_rel); }
    float takePeak(int m) { return peak_[static_cast<size_t>(m)].exchange(0.0f, std::memory_order_relaxed); }
    float takeLimiterGain() { return limiterGain_.exchange(1.0f, std::memory_order_relaxed); }

private:
    std::array<std::atomic<float>, kNumMeters> peak_{{{0.0f}, {0.0f}, {0.0f}, {0.0f}}};
    std::atomic<float> limiterGain_{1.0f}; // lowest Tank::limiterGain() since the last take
    std::atomic<bool>  throwExited_{false};
};

class PanelEditor final : public juce::AudioProcessorEditor {
public:
    PanelEditor(juce::AudioProcessor& owner, juce::AudioProcessorValueTreeState& state, PanelLink& link);
    ~PanelEditor() override;

    void resized() override;

private:
    class Panel;
    void setPanelScale(float scale); // 1, 1.5 or 2 x the 5 px/mm layout

    juce::AudioProcessorValueTreeState& state_;
    std::unique_ptr<Panel>              panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelEditor)
};

} // namespace rv::plugin
