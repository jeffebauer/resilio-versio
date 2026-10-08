---
title: "Changelog: Resilio Versio releases"
description: "What changed in each Resilio Versio release, newest first: the Wellspring-fitted spring sound, TANK ECHO, King Tubby's “Big Knob”, the throw, the held bed and the new panel names."
slug: "changelog"
order: 5
---

# Changelog

Newest first. Each version is named by its date and build (`vYYYY.MM.DD-<build>`). Each release includes the firmware for the Versio and the plugin for macOS, unless it says otherwise.

When a player suggests a change, I credit them in its entry. If you have an idea, [suggest a sound](https://github.com/jeffebauer/resilio-versio/issues/new?template=sound_idea.yml).

<!-- LATEST: show the current version, date and download buttons here, from the release data. -->

## 7 October 2026

<p class="release-version">v2026.10.07-0671d22</p>

The first public release. The repository, the downloads and this site are now open to everyone.

- The first chord after a run of drums no longer comes in up to 3 dB louder than the chords after it, because each new sound is now read on its own.
- On the plugin's panel, THROW sits clear of DRIVE's label, and GATE and SIZE share the bottom row.
- The download now includes the licence files and a link to the exact source. The code is MIT, and the plugin is AGPLv3 because it's built with JUCE.

## 6 October 2026

<p class="release-version">v2026.10.06-a6c70a4</p>

The springs got a big rework, and there's a whole new mode on the TANK switch. Roughly in order of how much you'll notice:

### New names

The sound and the positions haven't changed, and saved sets keep their settings.
- **BLEND** (was MIX), as on Noise Engineering's own Versio modules.
- **TANK 1 · 2 · ECHO** (was SPRINGS 1 · 2 · 3). The third position is the echo, and an echo isn't a spring.
- **ATTITUDE CLEAN · TAPE · VALVE** (was CLEAN · DRIVEN · KICKED), named for the saturation. TAPE is tape saturation, the dub colour. VALVE is a cranked valve stage inside the tank, with rattle, coarse grit and the howl.
- **THROW / TAP**. The button throws in TANK 1–2 and taps the echo's tempo in ECHO.
- A DAW may show the old names on a device you already have until it rescans the plugin or you load a fresh one.

### A new spring sound
- The tank was rebuilt against recordings of a Teaching Machines Wellspring, a desktop BBD delay and stereo spring reverb. The highs are gentler from the first moment, and the repeats darken and blur into a wash where they used to tick like a delay.
- A final fit from a second recording session followed. The hit up front is softer, the main ring sits lower and warmer, and the lows stay centred while the width grows as the tail goes on. The springs' level also sits more evenly across TONE and DRIVE.

### TANK ECHO: a tape echo feeding the springs
- A quarter of each repeat goes through the springs and the rest comes straight off the tape, wide like two playback heads, so each repeat stays a distinct hit with a spring halo. The repeats wear like old tape. Loud, bright ones come back thicker and duller, and each pass is a little darker.
- DECAY sets the echo's feedback. At the very top it holds a steady, saturated loop and doesn't run away.
- TENSION sets the echo time, and moving it bends the repeats like a Space Echo's rate knob.
- Patch a clock into the gate, or tap the button, and TENSION steps through 1/2 down to 1/16, dotted values included. The tempo holds when the clock stops, so stopping and starting your sequencer doesn't bend the echo. One lone pulse sends it back to free time. The plugin follows the DAW's tempo and shows the note value.

### TONE: King Tubby's “Big Knob”
- The right half of TONE is modelled on the stepped high-pass filter on Tubby's MCI desk. That gives a steep low cut up to 800 Hz, with the filter's nasal bump on sharp hits. It sweeps smoothly, so you can ride it by hand or with CV.
- It sits after the springs, the way Black Ark's low cut sat on the spring return, so when you sweep it the tail you're hearing goes thin straight away.

### THROW / TAP: the button and the gate
- The Kick is gone. In TANK 1–2, hold the button (or send a gate) and the springs hear the input only while it's held. Drench one snare, leave the next dry, and the tail rings on either way.
- Throw mode switches on at the first press or gate, so the button works with nothing patched. To leave it, tap, then press and hold for 2 seconds. The LEDs blink white.
- In TANK ECHO the button taps the tempo and the gate is the echo's clock. The LEDs flash purple on each tap, then pulse purple on your beat for 4 seconds.
- In the plugin, held MIDI notes act as the gate, so a clip can sequence throws to the sample.

### The hold at the top of DECAY (CLEAN and TAPE)
- Turn DECAY all the way up and the tail holds, giving you a near-endless bed for dub techno breakdowns that new sounds layer into. It ducks under kick and bass only. VALVE keeps its howl.

### Grit in TAPE and VALVE
- The wet goes through a 24 kHz µ-law converter, for the companded grit of early digital delays and samplers. TAPE is 12-bit, a fine grain, and VALVE is 10-bit and clearly gritty. CLEAN stays clean, and the dry signal isn't touched.

### SPLASH is stronger
- The top quarter of the knob is much bigger, and SPLASH works fully with DRIVE down, so a quiet mixer send still splashes.

### Smaller fixes
- BLEND fully right is now 100 % wet on the module.
- Everything fits the Versio. At its busiest it uses under 80 % of the chip, and it stays inside the 128 KB of flash. I checked both on the module.

## 2 October 2026 (pre-release)

<p class="release-version">v2026.10.02-cef6a77-candidate-F</p>

A sound candidate, plugin only, with no firmware. I named it "Resilio Versio F" so it would install next to the 1 October build and I could listen to them side by side. Its changes (the Wellspring-fitted tank, the “Big Knob”, the stronger SPLASH) were refined and released in v2026.10.06.

## 1 October 2026

<p class="release-version">v2026.10.01-1d18fce</p>

The first shared build, with firmware for the Versio and the plugin for macOS. The controls then were MIX, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE, SPRINGS 1 · 2 · 3, ATTITUDE CLEAN · DRIVEN · KICKED, and a KICK button that struck the tank.
