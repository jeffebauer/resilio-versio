---
title: "Changelog: Resilio Versio releases"
description: "What changed in each Resilio Versio release, newest first: the Wellspring-fitted spring sound, TANK ECHO, King Tubby's Big Knob, the throw, the held bed and the new panel names."
slug: "changelog"
order: 5
---

# Changelog

Newest first. Versions are named by date and build (`vYYYY.MM.DD-<build>`). Every release has the firmware for the Versio and the plugin for macOS, unless it says otherwise.

<!-- LATEST: show the current version, date and download buttons here, from the release data. -->

## v2026.10.06-a6c70a4 — 6 October 2026

The springs got a big rework, and there's a whole new mode on the TANK switch. Roughly in order of how much you'll notice:

**New names** (same sound, same positions; saved sets keep their settings)
- **BLEND** (was MIX), as on Noise Engineering's own Versio modules.
- **TANK 1 · 2 · ECHO** (was SPRINGS 1 · 2 · 3): the third position is the echo, and echo isn't a spring.
- **ATTITUDE CLEAN · TAPE · VALVE** (was CLEAN · DRIVEN · KICKED), named for the saturation: TAPE is tape saturation, the dub colour; VALVE is a cranked valve stage inside the tank, with rattle, coarse grit and the howl.
- **THROW / TAP**: the button throws in TANK 1–2 and taps the echo's tempo in ECHO.
- A DAW may show the old names on a device you already have until it rescans the plugin or you load a fresh one.

**A new spring sound**
- The tank was rebuilt against recordings of a Teaching Machines Wellspring (a desktop BBD delay and stereo spring reverb): gentler highs from the first moment, and repeats that darken and blur into a wash instead of ticking like a delay.
- Then a final fit from a second recording session: a softer hit up front, the main ring sitting lower and warmer, and lows centred with the width growing as the tail goes on. The springs' level also sits more evenly across TONE and DRIVE.

**TANK ECHO: a tape echo feeding the springs**
- A quarter of each repeat goes through the springs and the rest comes straight off the tape, wide like two playback heads, so every repeat stays a distinct hit with a spring halo. The repeats wear like old tape: loud, bright ones come back thicker and duller, each pass a little darker.
- DECAY sets the echo's feedback; at the very top it holds a steady, saturated loop instead of running away.
- TENSION sets the echo time; moving it bends the repeats like a Space Echo's rate knob.
- Patch a clock into the gate, or tap the button, and TENSION steps through 1/2 down to 1/16, dotted values included. The tempo holds when the clock stops, so stopping and starting your sequencer doesn't bend the echo; one lone pulse sends it back to free time. The plugin follows the DAW's tempo and shows the note value.

**TONE: King Tubby's Big Knob**
- The right half of TONE is modelled on the stepped high-pass filter on Tubby's MCI desk: a steep low cut up to 800 Hz, with the filter's nasal bump on sharp hits. It sweeps smoothly, so you can ride it by hand or with CV.
- It sits after the springs, the way Black Ark's low cut sat on the spring return: sweep it and the tail you're hearing goes thin straight away.

**THROW / TAP: the button and the gate**
- The Kick is gone. In TANK 1–2, hold the button (or send a gate) and the springs hear the input only while it's held: drench one snare, leave the next dry, and the tail always rings on.
- Throw mode switches on at the first press or gate, so the button works with nothing patched. To leave it: tap, then press and hold for 2 seconds (the LEDs blink white).
- In TANK ECHO the button taps the tempo and the gate is the echo's clock. The LEDs flash purple on each tap, then pulse purple on your beat for 4 seconds.
- In the plugin, held MIDI notes act as the gate, so a clip can sequence throws to the sample.

**The hold at the top of DECAY (CLEAN and TAPE)**
- Turn DECAY all the way up and the tail holds: a near-endless bed for dub techno breakdowns that new sounds layer into. It ducks under kick and bass only. VALVE keeps its howl.

**Grit in TAPE and VALVE**
- The wet goes through a 24 kHz µ-law converter: the companded grit of early digital delays and samplers. TAPE is 12-bit (a fine grain), VALVE 10-bit (clearly gritty). CLEAN stays clean, and the dry is never touched.

**SPLASH is stronger**
- The top quarter of the knob is much bigger, and SPLASH works fully with DRIVE down, so a quiet mixer send still splashes.

**Smaller fixes**
- BLEND fully right is now 100 % wet on the module.
- Everything fits the Versio: under 80 % of the chip at its busiest and inside its 128 KB of flash, checked on the module.

## v2026.10.02-cef6a77-candidate-F — 2 October 2026 (pre-release)

A sound candidate, plugin only, named "Resilio Versio F" so it installed next to the 1 October build for side-by-side listening. Its changes (the Wellspring-fitted tank, the Big Knob, the stronger SPLASH) were refined and released in v2026.10.06. No firmware.

## v2026.10.01-1d18fce — 1 October 2026

The first shared build: firmware for the Versio and the plugin for macOS. Controls then: MIX, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE; SPRINGS 1 · 2 · 3; ATTITUDE CLEAN · DRIVEN · KICKED; a KICK button that struck the tank.
