---
title: "Credits: how Resilio Versio was made"
description: "Who made Resilio Versio and how: designed by ear, modelled on spring reverb research, fitted to a real tank, built with libDaisy and JUCE. References, acknowledgements and trademarks."
slug: "credits"
order: 7
---

# Credits

## How it was shaped

I'm [Jesse Bauer](https://jessebauer.xyz), a designer and dub enthusiast, and I designed Resilio Versio by ear. I set the musical goals and made every sound decision. Claude (Anthropic's AI) wrote the DSP, the firmware and the tools.

The springs are modelled on physics. They follow Välimäki, Parker and Abel's *Parametric Spring Reverberation Effect* (Journal of the Audio Engineering Society, 2010), where each spring is a feedback loop around a chain of "stretched" allpass filters. Those filters spread each echo in time by frequency, and that's what makes the chirp. On top of the model there are up to two detuned springs spread across the stereo field, a drive chain (input transducer, tape, saturation inside the loop, pickup), splash from the hits themselves, and several layers of protection against single-tone ringing.

I tuned the tank round after round against recordings of my Teaching Machines Wellspring, a desktop BBD delay and stereo spring reverb, matching its softer hit, its warmer main ring and where its width sits. A Strymon Magneto was a second reference. I used the recordings for measurement and listening only, and none of them is in Resilio or on this site.

I heard each change before it shipped. Every candidate sound became a level-matched listening page, A against B in each ATTITUDE, on rim hits, a reggae skank and a held pad, and I picked between them. Over forty of those decisions are written up with their reasons in the source repository.

Sound work happens on the desktop first, in an offline renderer and the plugin. The module is where I measure CPU and judge the final feel. The firmware fits in 128 KB of flash and runs under 80 % of the chip at its busiest moment.

## The demos

Every clip on this site is Resilio itself, rendered offline by its own renderer, with a dry and a wet version so you can blend between them. The sources are:

- Drums from [Big Rusty Drums](https://github.com/sfzinstruments/karoryfer.big-rusty-drums) by Karoryfer and [Virtuosity Drums](https://github.com/sfzinstruments/virtuosity_drums) by Versilian Studios and Karoryfer, both CC0, sequenced in code.
- Wurlitzer EP200 samples by [Greg Sullivan](http://www.sullivang.net), licensed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/).
- A siren synthesised in code.

There are no recordings of other reverbs.

## References

- V. Välimäki, J. Parker and J. S. Abel, "Parametric Spring Reverberation Effect", *Journal of the Audio Engineering Society* 58(7/8), 547–562, 2010.
- J. Parker, "Efficient Dispersion Generation Structures for Spring Reverb Emulation", *EURASIP Journal on Advances in Signal Processing*, 2011.
- J. Parker and S. Bilbao, "Spring Reverberation: A Physical Perspective", DAFx-09, Como, 2009.
- J. S. Abel, D. P. Berners, S. Costello and J. O. Smith, "Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure", AES 121st Convention, 2006.

## Thanks

- Noise Engineering, for the open Versio platform, the Firmware Swap app, and the printable panel template I drew the panel diagram from.
- Electro-Smith, for the Daisy Seed, libDaisy and DaisySP.
- JUCE, which the plugin is built with.
- The engineers whose moves this is built around: King Tubby, Lee "Scratch" Perry, Scientist, Dennis Bovell, Adrian Sherwood, and the dub techno lineage after them.

## Licence

My code, tools and docs are MIT licensed. The plugin is AGPLv3, because it's built on JUCE. The source is on [GitHub](https://github.com/jeffebauer/resilio-versio).

## Trademarks

Versio is a Noise Engineering product, and the Daisy Seed an Electro-Smith one. Wellspring is a product of Teaching Machines, Magneto of Strymon, Space Echo of Roland. Ableton Live, MCI and Audio Units belong to their owners; VST is a trademark of Steinberg Media Technologies GmbH. All product and company names are trademarks or registered trademarks of their respective owners, named here only to describe compatibility and influences. Resilio Versio is an independent project, not affiliated with or endorsed by any of them.
