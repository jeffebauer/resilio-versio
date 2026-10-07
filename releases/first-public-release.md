**Resilio Versio: a dub spring reverb for the Noise Engineering Versio**

By [Jesse Bauer](https://jessebauer.xyz). Website: [resilio-versio.vercel.app](https://resilio-versio.vercel.app).

Resilio turns the Versio into a spring tank you play like a dub engineer: throw single snares into it, splash it, drive it, cut its lows like King Tubby's "Big Knob", hold its tail as a bed under the groove, push it into a howl, or feed it from a worn tape echo. The same sound runs as an AU/VST3 plugin, so you can try it in a DAW before flashing the module.

**New since the last pre-public build (v2026.10.06-a6c70a4)**

- The first chord after a run of drums no longer comes in up to 3 dB louder than the chords after it: a new sound is read on its own.

**What's in the download**

- **`resilio-versio-firmware.bin`**: the firmware for the Versio. Install it with Noise Engineering's Firmware Swap web app in Chrome (*Select Custom File*, *Connect*, *Change Firmware*). Use USB power only: never connect USB and Eurorack power at the same time. To go back, pick any Noise Engineering firmware in the same app. Step by step: [the player's guide](https://github.com/jeffebauer/resilio-versio/blob/main/docs/manual.md#installing-noise-engineering-firmware-swap).
- **`resilio-versio-plugin-macos.zip`**: the plugin for macOS 12 or newer, Apple Silicon or Intel, as VST3 and Audio Unit, with a read-me (install steps), the licence files and a copy of the firmware. It isn't signed by Apple yet, so the read-me shows the one Terminal line that lets macOS open it.
- **`SHA256SUMS.txt`**: checksums, if you want to check a download.

The same files are also attached under their dated names.

**On the panel**

BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE and DRIVE on the knobs; TANK (1, 2 or ECHO, a tape echo into two springs) and ATTITUDE (CLEAN, TAPE or VALVE) on the switches; THROW / TAP on the button. The Versio's printed labels don't match yet: the [README](https://github.com/jeffebauer/resilio-versio#the-instrument) has the map, and the [player's guide](https://github.com/jeffebauer/resilio-versio/blob/main/docs/manual.md) explains every control.

**Feedback**

Bugs and sound ideas go on [GitHub Issues](https://github.com/jeffebauer/resilio-versio/issues/new/choose). Settings in panel names and a short recording help most.

**Licence**

The code is MIT. The plugin is built with JUCE, used under the GNU AGPLv3, so the plugin binaries are AGPLv3; their source is this repository at this release's tag. The firmware uses libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0). Details: [`NOTICE`](https://github.com/jeffebauer/resilio-versio/blob/main/NOTICE).

Resilio Versio is firmware for the Noise Engineering Versio. Not affiliated with or endorsed by Noise Engineering. Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.
