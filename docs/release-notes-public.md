# Public release notes (draft)

Draft text for the first public release (ADR 0045), and a short note for the three earlier releases that were shared with friends. Text only: nothing here has been posted. The owner edits, then the lead pastes it in.

---

## First public release

Release title: `Resilio Versio <tag>`. Pass this as the release notes (`tools/make_release.sh --notes`), or paste it above the read-me.

> **Resilio Versio: a dub spring reverb for the Noise Engineering Versio**
>
> By [Jesse Bauer](https://jessebauer.xyz).
>
> Resilio turns the Versio into a spring tank you play like a dub engineer: throw single snares into it, splash it, drive it, cut its lows like King Tubby's "Big Knob", hold its tail as a bed under the groove, push it into a howl, or feed it from a worn tape echo. The same sound runs as an AU/VST3 plugin, so you can try it in a DAW before flashing the module.
>
> **What's in the download**
>
> - **`resilio-versio-firmware.bin`**: the firmware for the Versio. Install it with Noise Engineering's Firmware Swap web app in Chrome (*Select Custom File*, *Connect*, *Change Firmware*). Use USB power only: never connect USB and Eurorack power at the same time. To go back, pick any Noise Engineering firmware in the same app. Step by step: [the player's guide](https://github.com/jeffebauer/resilio-versio/blob/main/docs/manual.md#installing-noise-engineering-firmware-swap).
> - **`resilio-versio-plugin-macos.zip`**: the plugin for macOS 12 or newer, Apple Silicon or Intel, as VST3 and Audio Unit, with a read-me (install steps), the licence files and a copy of the firmware. It isn't signed by Apple yet, so the read-me shows the one Terminal line that lets macOS open it.
> - **`SHA256SUMS.txt`**: checksums, if you want to check a download.
>
> The same files are also attached under their dated names.
>
> **On the panel**
>
> BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE and DRIVE on the knobs; TANK (1, 2 or ECHO, a tape echo into two springs) and ATTITUDE (CLEAN, TAPE or VALVE) on the switches; THROW / TAP on the button. The Versio's printed labels don't match yet: the [README](https://github.com/jeffebauer/resilio-versio#the-instrument) has the map, and the [player's guide](https://github.com/jeffebauer/resilio-versio/blob/main/docs/manual.md) explains every control.
>
> **Feedback**
>
> Bugs and sound ideas go on [GitHub Issues](https://github.com/jeffebauer/resilio-versio/issues/new/choose). Settings in panel names and a short recording help most.
>
> **Licence**
>
> The code is MIT. The plugin is built with JUCE, used under the GNU AGPLv3, so the plugin binaries are AGPLv3; their source is this repository at this release's tag. The firmware uses libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0). Details: [`NOTICE`](https://github.com/jeffebauer/resilio-versio/blob/main/NOTICE).
>
> Resilio Versio is firmware for the Noise Engineering Versio. Not affiliated with or endorsed by Noise Engineering. Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc.

---

## Note for the three earlier releases

To prepend to the notes of v2026.10.06-a6c70a4, v2026.10.02-cef6a77-candidate-F and v2026.10.01-1d18fce:

> **Pre-public build.** This release was shared privately with friends before Resilio Versio went public, so its notes are written to them, and its download has no licence files. The current release, with install steps and licence notices, is at [Releases → Latest](https://github.com/jeffebauer/resilio-versio/releases/latest). The plugin in this download is built with JUCE under the GNU AGPLv3; its source is this repository at this release's tag. Resilio Versio is not affiliated with or endorsed by Noise Engineering.
