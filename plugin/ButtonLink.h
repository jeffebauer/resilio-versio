#pragma once
// The panel button's way to the Tank (ADR 0043), without JUCE so the plugin
// host test can check it against Tank::button() directly.
//
// The panel (message thread) counts presses and releases; the audio thread,
// at the start of each block, hands the Tank what happened since the last
// block: a press at the block's start, a release there too, or, when a
// whole click (press and release) fell between two blocks, the press at
// the block's start and the release at its last sample, so even a click
// shorter than a block throws (a few ms) and taps. The Tank does the rest:
// the hand throw, the exit gesture, tap tempo.

#include "dsp/Tank.h"

#include <atomic>
#include <cstdint>

namespace rv::plugin {

struct ButtonLink {
    // Panel (message thread).
    void press() { presses_.fetch_add(1, std::memory_order_release); }
    void release() { releases_.fetch_add(1, std::memory_order_release); }

    // Audio thread, before tank.process() of a block of n samples.
    void feed(Tank& tank, int n)
    {
        const uint32_t released = releases_.load(std::memory_order_acquire);
        const uint32_t pressed  = presses_.load(std::memory_order_acquire);
        const bool     down     = pressed != released; // held now
        const bool     newPress = pressed != pressesSeen_, newRelease = released != releasesSeen_;
        if (down && !downSeen_) {
            tank.button(true, 0);
        } else if (!down && downSeen_) {
            tank.button(false, 0);
        } else if (!down && newPress) { // a whole click between two blocks
            tank.button(true, 0);
            tank.button(false, n - 1);
        } else if (down && newRelease) { // let go and pressed again between two blocks
            tank.button(false, 0);
            tank.button(true, n > 1 ? 1 : 0);
        }
        pressesSeen_  = pressed;
        releasesSeen_ = released;
        downSeen_     = down;
    }

private:
    std::atomic<uint32_t> presses_{0}, releases_{0};
    uint32_t pressesSeen_ = 0, releasesSeen_ = 0; // audio thread only
    bool     downSeen_ = false;
};

} // namespace rv::plugin
