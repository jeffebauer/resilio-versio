---
title: "Credits: how Resilio Versio was made"
description: "Who made Resilio Versio and how: designed by ear, modelled on spring reverb research, fitted to a real tank, built with libDaisy and JUCE. References, acknowledgements and trademarks."
slug: "credits"
order: 7
---

# Credits

## How it was shaped

Resilio Versio was designed by ear. <!-- OWNER: your name or handle, and a link, as you'd like it credited --> a designer and dub enthusiast, brought the musical goals and made every sonic decision. Claude (Anthropic's AI) was the engineering partner: it wrote the DSP, the firmware and the tools, and turned each technical question into a musical one.

- **Modelled on physics.** The springs follow Välimäki, Parker and Abel's *Parametric Spring Reverberation Effect* (Journal of the Audio Engineering Society, 2010): each spring is a feedback loop around a chain of "stretched" allpass filters that spread each echo in time by frequency, which is what makes the chirp. On top: up to two detuned springs spread across the stereo field, a drive chain (input transducer, tape, saturation inside the loop, pickup), splash from the hits themselves, and a layered defence against single-tone ringing.
- **Fitted to a real tank.** The tank was tuned, round after round, against recordings of the owner's Teaching Machines Wellspring (a desktop BBD delay and stereo spring reverb): its softer hit, its warmer main ring, where its width sits. A Strymon Magneto was a second reference. The recordings were used for measurement and listening only; none of them is in Resilio or on this site.
- **Every change heard before it shipped.** Each candidate sound became a level-matched listening page (A against B, in every ATTITUDE, on rim hits, a reggae skank and a held pad), and the owner picked. Over forty decisions are recorded, with their reasons, in the source repository.
- **Desktop first, hardware for the feel.** Sound work happens in an offline renderer and the plugin; the module is where CPU is measured and the final feel is judged. The firmware fits in 128 KB of flash and runs under 80 % of the chip at its busiest moment.

## The demos

Every clip on this site is Resilio itself, rendered offline by the project's renderer from synthetic test signals the project generated (a snare, a rim, an offbeat chord skank, a pad). No recordings of other instruments or other reverbs.

## References

- V. Välimäki, J. Parker and J. S. Abel, "Parametric Spring Reverberation Effect", *Journal of the Audio Engineering Society* 58(7/8), 547–562, 2010.
- J. Parker, "Efficient Dispersion Generation Structures for Spring Reverb Emulation", *EURASIP Journal on Advances in Signal Processing*, 2011.
- J. Parker and S. Bilbao, "Spring Reverberation: A Physical Perspective", DAFx-09, Como, 2009.
- J. S. Abel, D. P. Berners, S. Costello and J. O. Smith, "Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure", AES 121st Convention, 2006.

## Thanks

- **Noise Engineering** for the open Versio platform, the Firmware Swap app, and the printable panel template the panel diagram is drawn from.
- **Electro-Smith** for the Daisy Seed, libDaisy and DaisySP.
- **JUCE**, which the plugin is built with.
- The engineers whose moves this is built around: King Tubby, Lee "Scratch" Perry, Scientist, Dennis Bovell, Adrian Sherwood, and the dub techno lineage after them.

## Licence

<!-- OWNER: no licence has been chosen yet. The plugin is built on JUCE (AGPLv3 or JUCE's commercial licences), so publishing plugin builds brings those terms in; the firmware and the DSP core don't use JUCE. Decide before the repository and downloads go public, then replace this comment with one line, e.g. "Resilio Versio is free software under <licence>. Source: github.com/jeffebauer/resilio-versio." -->

## Trademarks

Versio is a Noise Engineering product, and the Daisy Seed an Electro-Smith one. Wellspring is a product of Teaching Machines, Magneto of Strymon, Space Echo of Roland. Ableton Live, MCI and Audio Units belong to their owners; VST is a trademark of Steinberg Media Technologies GmbH. All product and company names are trademarks or registered trademarks of their respective owners, named here only to describe compatibility and influences. Resilio Versio is an independent project, not affiliated with or endorsed by any of them.
