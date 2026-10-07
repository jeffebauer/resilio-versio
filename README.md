# Resilio Versio

![Resilio Versio: a render of the Versio module with Resilio's panel art, lit from above on a dark stage.](docs/minisite/assets/renders/hero-monolith.jpg)

**A dub-inspired spring reverb and tape echo for the Noise Engineering Versio.** I wanted a spring reverb I could play the way dub engineers played theirs. Throw a snare into it, ride the filter like King Tubby, let the tail hang, or push it until it howls, with a worn tape echo in front when you want one. It's free firmware for the Versio Eurorack module, and the same sound runs as an AU/VST3 plugin on macOS.

### → [resilio-versio.vercel.app](https://resilio-versio.vercel.app): listen, download, and read the manual

[Download](https://resilio-versio.vercel.app/#download) · [Install](https://resilio-versio.vercel.app/install/) · [Manual](https://resilio-versio.vercel.app/manual/) · [Starting points](https://resilio-versio.vercel.app/presets/) · [Changelog](https://resilio-versio.vercel.app/changelog/) · [FAQ](https://resilio-versio.vercel.app/faq/)

*Resilio*: Latin, "I leap back, rebound". By [Jesse Bauer](https://jessebauer.xyz). Not affiliated with or endorsed by Noise Engineering.

## What it is

Resilio is alternative firmware for the [Versio](https://noiseengineering.us/products/versio), Noise Engineering's open Eurorack platform. It replaces the module's firmware entirely, and you can go back just as easily with Noise Engineering's own Firmware Swap web app.

It sounds like a spring tank. Each hit lands in the springs as its own splash, a bright clang and then echoes that sweep upward (the spring's "boing"), and the tail darkens into a wash. I built it around the moves of dub, where the mixing desk is played live:

- Throw one snare into the springs and let the tail ring on, from the button or the gate input.
- The right half of TONE is King Tubby's “Big Knob”, a steep low cut modelled on the Altec filter on his desk.
- TANK ECHO puts a worn tape echo in front of the springs, clocked or tapped.
- At the top of DECAY the tail holds as a bed that ducks under your kick and bass, like dub techno's sidechained reverb. In VALVE the tank howls there.
- ATTITUDE picks the colour: CLEAN, TAPE (tape saturation and 12-bit µ-law grain) or VALVE (a cranked valve stage and 10-bit grit). It only colours the reverb, and your dry signal isn't touched.

There are seven knobs, each with a CV input, plus two switches, a button and a gate. The [manual](https://resilio-versio.vercel.app/manual/) covers every control, and [Starting points](https://resilio-versio.vercel.app/presets/) has settings for classic dub and dub techno sounds.

> **Status:** first public release. The firmware works and I've played it on real hardware, but the sound may still change between versions.

## Get it

Download from the [website](https://resilio-versio.vercel.app/#download) or this repository's [Releases](https://github.com/jeffebauer/resilio-versio/releases/latest):

- **Firmware for Versio** ([`resilio-versio-firmware.bin`](https://github.com/jeffebauer/resilio-versio/releases/latest/download/resilio-versio-firmware.bin)). Install it in Chrome with Noise Engineering's [Firmware Swap](https://noiseengineering.us/portal/firmware), with the module powered from USB only. **Never connect USB and Eurorack power at the same time.**
- **Plugin for macOS** ([`resilio-versio-plugin-macos.zip`](https://github.com/jeffebauer/resilio-versio/releases/latest/download/resilio-versio-plugin-macos.zip)). AU and VST3 for macOS 12 or newer, Apple Silicon or Intel, with the same panel and sound. It follows your DAW's tempo, and MIDI notes act as the gate.

Step-by-step instructions, including how to go back to Noise Engineering's firmware, are on the [install page](https://resilio-versio.vercel.app/install/).

## How it was made

I designed Resilio by ear. I'm a designer and dub enthusiast, and I set the musical goals and made every sound decision, while Claude (Anthropic's AI) wrote the DSP, the firmware and the tools. The springs follow Välimäki, Parker and Abel's physical model, and I tuned them round after round against recordings of a real spring tank. Before a sound shipped, I heard it against the last one in level-matched listening tests. Over forty decisions are written up with their reasons in [`docs/adr/`](docs/adr/).

## For builders

One DSP core with no platform code, and three hosts: an offline renderer (WAV in, WAV out), the JUCE plugin and the Versio firmware (libDaisy). They all read the same parameter table, so a setting in the plugin sounds the same on the module. The firmware fits in 128 KB of flash and stays under 80 % of the chip at its busiest moment.

- Building and flashing your own: [`docs/building.md`](docs/building.md)
- The specification is [`SPEC.md`](SPEC.md), the vocabulary is in [`CONTEXT.md`](CONTEXT.md), and the decisions are in [`docs/adr/`](docs/adr/)
- Firmware variants, CPU runs and flash budget: [`firmware/README.md`](firmware/README.md)

## Feedback

I'd love to hear how it sounds in your hands, because I refine Resilio with the people playing it. You can [suggest a sound](https://github.com/jeffebauer/resilio-versio/issues/new?template=sound_idea.yml) or [report a bug](https://github.com/jeffebauer/resilio-versio/issues/new?template=bug_report.yml), each with a short form. I try ideas out and listen to them before anything ships, and the ones that make it are credited in the [changelog](https://resilio-versio.vercel.app/changelog/). There's more in the [FAQ](https://resilio-versio.vercel.app/faq/#how-do-i-give-feedback-or-report-a-bug).

## Licence

- **Code, tools and docs:** MIT, see [`LICENSE`](LICENSE).
- **Plugin binaries:** AGPLv3. The plugin is built on [JUCE](https://juce.com), used under its AGPLv3 licence (full text in [`LICENSES/AGPL-3.0.txt`](LICENSES/AGPL-3.0.txt)). Each release's source is this repository at that release's tag.
- **Firmware:** my MIT code with libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0). No JUCE, and no Noise Engineering code.
- Third-party notices: [`NOTICE`](NOTICE).

## Acknowledgements

- Spring reverb modelling: V. Välimäki, J. Parker and J. S. Abel; J. Parker; S. Bilbao (full references in [`SPEC.md` §11](SPEC.md)).
- [Electro-Smith](https://electro-smith.com) for libDaisy, and [Noise Engineering](https://noiseengineering.us) for the open Versio platform.
- The engineers whose moves this is built around: King Tubby, Lee "Scratch" Perry, Scientist, Dennis Bovell, Adrian Sherwood, and the dub techno lineage after them.

## Trademarks

Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.
