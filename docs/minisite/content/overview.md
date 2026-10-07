---
title: "Resilio Versio: a dub spring reverb for the Noise Engineering Versio"
description: "Free alternative firmware that turns the Noise Engineering Versio into a dub spring reverb: throw snares into it, ride King Tubby's Big Knob, feed it from a worn tape echo, hold it forever or push it into a howl. Also as an AU/VST3 plugin for macOS."
slug: ""
order: 1
---

# Resilio Versio

**A dub spring reverb for the Noise Engineering Versio.**

A simulated spring tank you can throw snares into, splash, drive, filter like King Tubby, hold forever, push into a howl, or feed from a worn tape echo. Free firmware for the Versio Eurorack module, and the same sound as an AU/VST3 plugin for macOS.

*Resilio*: Latin, "I leap back, rebound".

<!-- DEMOS: the demo player goes here (assets/audio/demos.json). -->

<!-- DOWNLOADS: two buttons, with version, date and size from the release data:
     "Firmware for the Versio (.bin)" and "Plugin for macOS (AU/VST3)". Link: "How to install". -->

## What it sounds like

A spring tank, not a room. Every hit lands in the springs as its own **splash**: a bright clang on the attack, then echoes that sweep upward (the highs arrive after the lows: the "boing" of a real spring), then a tail that darkens and blurs into a wash instead of ticking like a delay.

## Built around dub performance moves

Dub treats the mixing desk as an instrument played live. The reverb isn't a background room: it's thrown at single hits, ridden, filtered and muted, then left to ring on its own, and every pass comes out different.

- **The throw.** The classic send move of King Tubby, Dennis Bovell and Adrian Sherwood: open the springs for one snare, close them, let the tail ring on. The THROW / TAP button and the gate input both throw.
- **King Tubby's Big Knob.** The right half of TONE is modelled on the stepped high-pass filter on Tubby's MCI desk: a steep low cut up to 800 Hz, with the filter's nasal ring on sharp hits. It sweeps smoothly rather than in steps, so you can ride it by hand or with CV, and it sits on the spring return, so sweeping it thins the tail you're hearing straight away.
- **Tape echo into springs.** Nearly every dub rig paired a spring with tape echo, from Tubby's homemade delay to Lee "Scratch" Perry's Space Echo. TANK ECHO is that pairing: a worn tape echo feeding the springs, clockable from your sequencer or tapped in on the button.
- **The held bed.** At the top of DECAY (in CLEAN and TAPE) the tail holds as a near-endless bed that dips under your kick and bass only, like a reverb sidechained to the kick in Basic Channel-style dub techno.
- **The kicked tank.** In VALVE the top of DECAY lets the tank howl: rideable spring feedback, rough and moving, never a clean tone. Pull DECAY back and it falls into a normal tail.

[Read the manual](/manual)

## The panel

Resilio runs behind your Versio's printed panel, so the labels on your module won't match. Here's the map.

<!-- PANEL: assets/panel/resilio-versio-panel.svg -->

- **Seven knobs**, each with a CV input: BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE
- **Two switches**: TANK (1 · 2 · ECHO) and ATTITUDE (CLEAN · TAPE · VALVE)
- **One button**: THROW / TAP
- **A gate input** that throws, or clocks the echo
- **Four LEDs** that meter the input and output

## Safe to try

Resilio replaces the module's firmware completely, and goes back just as easily. You install it with Noise Engineering's own Firmware Swap web app, and you go back to any Noise Engineering firmware with the same app. Resilio changes nothing else on the module.

[How to install](/install)

## The plugin

The same sound engine runs as an Audio Unit and VST3 plugin for macOS, laid out like the module's panel. Sketch a patch in your DAW, throw from a MIDI clip, follow the DAW's tempo in echo mode. No Versio needed.

## Status

Pre-release: working firmware, played on real hardware. The sound may still change between versions. Feedback is welcome on [GitHub Issues](https://github.com/jeffebauer/resilio-versio/issues).

Resilio is designed by ear: a designer and dub enthusiast made every sonic decision, with Claude (Anthropic's AI) as the engineering partner writing the DSP, the firmware and the tools. [Credits](/credits)

Resilio Versio is an independent project. It is not affiliated with or endorsed by Noise Engineering.
