# Resilio Versio

**A dub spring reverb for the Noise Engineering Versio.** A simulated spring tank you can throw snares into, splash, drive, filter like King Tubby, hold forever, push into a howl, or feed from a worn tape echo.

*Resilio*: Latin, "I leap back, rebound". By [Jesse Bauer](https://jessebauer.xyz).

Resilio Versio is firmware for the Noise Engineering Versio. Not affiliated with or endorsed by Noise Engineering.

Resilio is alternative firmware for the [Versio](https://noiseengineering.us/products/versio), Noise Engineering's open Eurorack platform (an Electro-Smith Daisy Seed behind seven knobs, two switches, a button and a gate). It replaces the module's firmware entirely and goes back just as easily. The same sound also runs as an AU/VST3 plugin, so a patch on the desk can be sketched in a DAW first.

> **Status:** working firmware, played on real hardware. Firmware and plugin builds are on the [Releases](https://github.com/jeffebauer/resilio-versio/releases/latest) page.

## What it sounds like

A spring tank, not a room. Every hit lands in the springs as its own **splash**: a bright clang on the attack, then echoes that sweep upward (the highs arrive after the lows, the "boing" of a real spring), then a tail that darkens and blurs into a wash instead of ticking like a delay.

- **Warm at rest, splashy when pushed.** Gentle highs from the first moment, repeats that darken as they go, the bass centred and the width growing as the tail rings on.
- **Every knob position usable.** No dead zones, no cliff edge into runaway feedback; the tail always fades unless you ask it not to.
- **Grit you choose.** ATTITUDE is named for the saturation. CLEAN is a polite, linear tank. TAPE adds tape saturation and a subtle 12-bit grain. VALVE is a spring in a cranked valve amp: hard, lopsided saturation inside the tank, rattle on hits, 10-bit grit, and allowed to howl. The dry signal stays clean in every mode.
- **Nothing added to your playing.** SPLASH makes your hits hit harder (their own highs, their own transients); it doesn't layer samples or bursts on top. Ghost notes in a groove stay quiet.

## Roots

Dub treats the mixing desk as an instrument played live. The reverb isn't a background room: it's thrown at single hits, ridden, filtered and muted, then left to ring on its own, and every pass comes out different. Resilio is built around those moves.

- **The throw.** The classic dub send move (Tubby, Dennis Bovell, Adrian Sherwood): open the springs for one snare, close them, and let the tail ring on. On Resilio the button and the gate input both throw.
- **King Tubby's "Big Knob".** The right half of TONE is modelled on the stepped high-pass on Tubby's MCI desk (an Altec 9069B): a steep low cut up to 800 Hz, with the filter's nasal ring on sharp hits. One deliberate departure: the Altec clicks between fixed steps, and Resilio's sweeps smoothly instead, so it can be ridden by hand or modulated by CV without zipper steps. It sits on the spring return, the way Black Ark's low cut did, so sweeping it thins the tail you're hearing straight away.
- **Tape echo into springs.** Nearly every dub rig paired a spring with tape echo: Tubby's homemade delay, Lee "Scratch" Perry's Space Echo, Sherwood, later Pole and Echospace. TANK ECHO is that pairing.
- **Dub techno's held bed.** At the top of DECAY (in CLEAN and TAPE) the tail holds as a near-infinite bed that ducks under kick and bass only, like a reverb sidechained to the kick in Basic Channel-style dub techno.
- **The kicked tank.** VALVE's Howl is the rideable spring feedback of a tank pushed too far: pull DECAY back and it falls into a normal tail.

## How it was shaped

Resilio was designed by ear. [Jesse Bauer](https://jessebauer.xyz), a designer and dub enthusiast, brought the musical goals and made every sonic decision; Claude (Anthropic's AI) was the engineering partner, writing the DSP, the firmware and the tooling, and turning each technical question into a musical one.

- **Modelled on physics.** The springs follow Välimäki, Parker and Abel's *Parametric Spring Reverberation Effect* (JAES, 2010): each spring is a feedback loop around a chain of "stretched" allpass filters that spread each echo in time by frequency, which is what makes the chirp. On top: one or two detuned springs spread across the stereo field (TANK 1 or 2; TANK ECHO is a tape echo feeding two), a drive chain (input transducer, tape, saturation inside the loop, pickup), splash from the hits themselves, and a layered defence against single-tone ringing.
- **Fitted to a real tank.** The tank was tuned, round after round, against recordings of the owner's Teaching Machines Wellspring (a desktop BBD delay and stereo spring reverb): its softer hit, its warmer main ring, where its width sits. A Strymon Magneto was a second reference.
- **Every change heard before it shipped.** Each candidate sound became a level-matched listening page (A against B, in every ATTITUDE, on rim hits, a reggae skank and a held pad), and the owner picked. Over forty decisions are recorded, with their reasons, in [`docs/adr/`](docs/adr/).
- **Desktop first, hardware for the feel.** Sound work happens in an offline renderer and the plugin; the module is where CPU is measured and the final feel is judged. The firmware runs inside 128 KB of flash and under 80 % of the chip at its busiest moment.

## The instrument

Resilio runs behind the stock Versio panel, so the printed labels don't match yet. Pots are numbered in reading order, top to bottom, left to right. Every knob has a CV input (0–5 V, added to the knob).

| Control | What it does |
|---|---|
| **P1 BLEND** | Dry ↔ wet. Fully right is 100 % wet, for a send/return. Fully left is a clean passthrough |
| **P2 DECAY** | How long the tail rings, from a slap (~0.4 s) to a long wash (~9 s). The top of the knob: the **Hold** in CLEAN and TAPE (a held bed that ducks under kick and bass), the **Howl** in VALVE |
| **P3 TONE** | Left: warm, dark dub. Noon: the Wellspring-fit sound. Right: King Tubby's Big Knob, thinner and more telephone-like the further you go |
| **P4 SPLASH** | How hard hits hit the springs: a bright clang from the hit's own highs, and in TAPE/VALVE a harder bite on drums. Works at any DRIVE, so a quiet mixer send still splashes |
| **P5 TENSION** | Which tank is fitted. Right: tight, quick repeats, a small bright chirp. Left: loose, slow repeats, a big dark boing. Turning it while the tail rings bends the pitch, like tightening a string |
| **P6 WOBBLE** | Pitch movement both ways from a still noon. Left: tape-like drift, a random wow with flutter on top. Right: a steady warble |
| **P7 DRIVE** | The tank's input: up to +24 dB, so a quiet send drives it like a hot one. Colour and grit build as you turn it up; the tail never gets shorter |
| **TANK** switch | **1**: one spring, sparse and the most splashy. **2**: two springs, the classic tank. **ECHO**: echo mode (below) |
| **ATTITUDE** switch | **CLEAN** / **TAPE** (tape saturation, the dub colour) / **VALVE** (a cranked valve stage: rattle, grit, Howl). Flipping it reshapes the tail already ringing |
| **THROW / TAP** button | TANK 1–2: a **throw** by hand (held = the springs hear the input). TANK ECHO: **tap tempo** |
| **Gate in** | TANK 1–2: **throw** from a sequencer. TANK ECHO: the echo's **clock** |
| **LEDs** | In L, In R, Out L, Out R level meters (green → amber, red on input clipping or when the output limiter works). White when throw mode goes off; purple on tap tempo |

**Echo mode (TANK ECHO).** A worn tape echo feeding the two-spring tank. A quarter of each repeat goes through the springs and the rest comes straight off the tape, wide like two playback heads, so every repeat stays a distinct hit with a spring halo. The repeats wear like old tape: loud, bright ones come back thicker and duller, each pass a little darker, with no digital fizz.
- **DECAY** is the feedback: from one repeat to long builds, and at the very top a steady, saturated loop that never runs away.
- **TENSION** is the echo time: 2 s to 80 ms free, or 1/2 down to 1/16 (dotted values included) of a clock, a tapped tempo or the DAW's tempo. Moving it bends the repeats like a Space Echo's rate knob.
- **WOBBLE** moves the tape too, so each repeat wavers a little more than the one before.
- A clock's tempo **holds** when its pulses stop, so stopping and starting a sequencer doesn't bend the echo.

Full player's guide: [`docs/manual.md`](docs/manual.md). Dub starting points: [`docs/presets.md`](docs/presets.md).

## A few dub moves

- **Throw the snare.** TANK 2, TAPE, BLEND fully right on a send. Hold the button on the beats you want drenched; let go and the tail rings on.
- **Big Knob sweep.** Ride TONE from noon to fully right as a tail rings: it thins to a telephone splash. Back down for the warmth.
- **Skank into echo.** TANK ECHO, clocked from your sequencer, TENSION on dotted 1/8. DECAY around 2 o'clock for a trail of repeats, each one splashing into the springs.
- **Hold the bed.** CLEAN, DECAY fully up. Play a chord, let it bloom into a held wash, and keep playing: the bed ducks under your kick and bass.
- **Kick the tank.** VALVE, DRIVE past 3 o'clock, DECAY into the top: ride the Howl, then pull DECAY back and let it fall away.

## Getting it

**On the Versio:** download the firmware ([`resilio-versio-firmware.bin`](https://github.com/jeffebauer/resilio-versio/releases/latest/download/resilio-versio-firmware.bin)) from this repository's [Releases](https://github.com/jeffebauer/resilio-versio/releases/latest) and install it with Noise Engineering's [Firmware Swap](https://noiseengineering.us/portal/firmware) web app (Chrome): *Select Custom File*, *Connect*, *Change Firmware*. Use USB power only: **never connect USB and Eurorack power at the same time.** To go back, run the same app and pick any Noise Engineering firmware; Resilio changes nothing else on the module. Step by step: [`docs/manual.md`](docs/manual.md#installing-noise-engineering-firmware-swap).

**In a DAW:** the AU/VST3 plugin for macOS 12 or newer, Apple Silicon or Intel ([`resilio-versio-plugin-macos.zip`](https://github.com/jeffebauer/resilio-versio/releases/latest/download/resilio-versio-plugin-macos.zip); install steps in its read-me) has the same panel, the same sound and the same LEDs. It follows the DAW's tempo in echo mode, and held MIDI notes act as the gate.

## For builders

One DSP core with no platform code, three hosts: an offline renderer (WAV in, WAV out, parameter sweeps and measurements), the JUCE plugin, and the Versio firmware (libDaisy). Every host reads the same parameter table, so a setting in the plugin sounds identical on the module.

- Building and flashing your own: [`docs/building.md`](docs/building.md)
- The specification: [`SPEC.md`](SPEC.md); vocabulary: [`CONTEXT.md`](CONTEXT.md); decisions: [`docs/adr/`](docs/adr/)
- Firmware variants, CPU runs and flash budget: [`firmware/README.md`](firmware/README.md)

## Feedback

Found a bug, or have an idea for the sound? Open an issue on [GitHub Issues](https://github.com/jeffebauer/resilio-versio/issues/new/choose). There are two short templates: a bug report (where it happened, the version, the settings in panel names, ideally a recording) and a sound idea.

## Licence

- **Code, tools and docs:** MIT, see [`LICENSE`](LICENSE).
- **Plugin binaries:** AGPLv3. The plugin is built on [JUCE](https://juce.com), used under its AGPLv3 licence, so the plugin as distributed is AGPLv3 (full text in [`LICENSES/AGPL-3.0.txt`](LICENSES/AGPL-3.0.txt)). Each release's source is this repository at that release's tag.
- **Firmware:** our MIT code with libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0). No JUCE, and no Noise Engineering code.
- Third-party notices: [`NOTICE`](NOTICE).

## Acknowledgements

- Spring reverb modelling: V. Välimäki, J. Parker and J. S. Abel; J. Parker; S. Bilbao (full references in [`SPEC.md` §11](SPEC.md)).
- [Electro-Smith](https://electro-smith.com) for libDaisy, and [Noise Engineering](https://noiseengineering.us) for the open Versio platform.
- The engineers whose moves this is built around: King Tubby, Lee "Scratch" Perry, Scientist, Dennis Bovell, Adrian Sherwood, and the dub techno lineage after them.

## Trademarks

Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.
