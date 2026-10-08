---
title: "Resilio Versio: a dub spring reverb for the Noise Engineering Versio"
description: "Free firmware that turns the Noise Engineering Versio into a dub spring reverb with a tape echo. Throw snares into it, ride King Tubby's “Big Knob” and let the tail hang or howl. Also an AU/VST3 plugin for macOS."
slug: ""
order: 1
---

# Resilio Versio

**A dub-inspired spring reverb and tape echo for the Noise Engineering Versio platform.**

I wanted a spring reverb I could play the way dub engineers played theirs. Throw a snare into it, ride the filter like King Tubby, let the tail hang under the groove, or push it until it howls. There's a worn tape echo in front of it too. It's free firmware for the Versio, and the same sound runs as an AU/VST3 plugin on a Mac.

*Resilio*: Latin, "I leap back, rebound".

<!-- DEMOS: the demo player goes here (assets/audio/demos.json). -->

<!-- DOWNLOADS: two buttons, with version, date and size from the release data:
     "Firmware for Versio (.bin)" and "Plugin for macOS (AU/VST3)". Link: "How to install". -->

## What it sounds like

It sounds like a spring tank. Each hit lands as a bright clang, then the echoes sweep upward (that's the "boing", where the highs arrive after the lows), and the tail goes darker and smears into a wash.

## King Tubby's “Big Knob”

That filter sweep on Tubby's dubs came from an Altec broadcast filter.

In 1972 King Tubby bought an older MCI mixing desk from Dynamic Sounds, Byron Lee's studio in Kingston. Fitted to it was an Altec 9069B, a passive high-pass filter made of two capacitors and a coil, built for cutting rumble in broadcast and film work. On the desk it was a large red knob marked HI PASS FILTER. Tubby called it the “Big Knob”.

It cuts the lows at 18 dB per octave, in fixed steps from 70 Hz up to 7.5 kHz. Tubby switched it on the reverb and echo sends and returns as the track played. Each click thins the sound further, until a snare turns into a telephone ring and then a squeak, and you can hear each step click in.

**I modelled the right half of Resilio's TONE knob on it.** It has the same steep slope on the spring return, up to 800 Hz. I made it sweep smoothly rather than click through steps, so you can ride it by hand or with CV.

## Built around dub performance moves

In dub, the mixing desk is an instrument. Engineers threw single hits into the reverb, rode it, cut it and let it ring out, live, so no two passes were the same. Resilio is built for those moves.

- **The throw.** King Tubby, Dennis Bovell and Adrian Sherwood all did this on the send. You open the springs for one snare, close them again and let the tail ring on. The THROW / TAP button and the gate input both do it.
- **King Tubby's “Big Knob”.** The right half of TONE is modelled on the stepped high-pass filter on Tubby's MCI desk. It's a steep low cut up to 800 Hz, with the filter's nasal ring on sharp hits. It sweeps smoothly, so you can ride it by hand or with CV, and because it sits on the spring return, sweeping it thins the tail you're already hearing.
- **Tape echo into springs.** Nearly every dub rig paired a spring with a tape echo, from Tubby's homemade delay to Lee "Scratch" Perry's Space Echo. TANK ECHO puts a worn tape echo in front of the springs. Clock it from your sequencer or tap it in on the button.
- **The held bed.** At the top of DECAY, in CLEAN and TAPE, the tail holds as a near-endless bed that dips under your kick and bass, like a reverb sidechained to the kick in Basic Channel-style dub techno.
- **The kicked tank.** In VALVE, the top of DECAY lets the tank howl. It's spring feedback you can ride, rough and moving. Pull DECAY back and it falls into a normal tail.

[Read the manual](/manual)

## Colour: saturation and grit

The ATTITUDE switch sets the tank's colour. CLEAN keeps it polite, TAPE adds tape saturation and VALVE an overdriven valve stage. TAPE and VALVE also put the reverb through a bit-reduced µ-law converter, which gives it the grit of early digital delays and samplers. DRIVE sets how hard you push it.

## The panel

Resilio runs behind your Versio's printed panel, so the labels on your module won't match. Here's what's where.

<!-- PANEL: assets/panel/resilio-versio-panel.svg -->

- **4 LEDs** Meter the input and output
- **7 knobs** BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE
- **2 switches** TANK (1 · 2 · ECHO) and ATTITUDE (CLEAN · TAPE · VALVE)
- **1 button** THROW / TAP
- **7 CV inputs** One per knob, 0–5 V, added to the knob's position
- **1 gate input** Throws, or clocks the echo

## Help shape it

If something sounds wrong, or you want a sound it can't make yet, tell me. Ideas from the people playing it are how Resilio gets better.

- **Play it.** On the module or in your DAW, with your own sounds.
- **Tell me.** Open a GitHub issue with an idea for the sound, or a bug.
- **Hear it change.** I try ideas by ear, and credit the ones that ship.

## Safe to try

Resilio replaces the module's firmware completely, and it's just as easy to go back. You install it with Noise Engineering's own Firmware Swap web app, and the same app puts any Noise Engineering firmware back on. Resilio doesn't change anything else on the module.

[How to install](/install)

## The plugin

The same sound engine runs as an Audio Unit and VST3 plugin for macOS, laid out like the module's panel. You can sketch a patch in your DAW and throw from a MIDI clip, and in echo mode it follows your tempo. You don't need a Versio to use it.

## Status

This is the first public release. The firmware works and I play it on real hardware, but the sound may still change between versions. I'd like to hear what you think on [GitHub Issues](https://github.com/jeffebauer/resilio-versio/issues).

I'm a designer, not a DSP engineer. I made every sound decision by ear, and Claude (Anthropic's AI) wrote the DSP, the firmware and the tools. [Credits](/credits)

Resilio Versio is an independent project. It is not affiliated with or endorsed by Noise Engineering.
