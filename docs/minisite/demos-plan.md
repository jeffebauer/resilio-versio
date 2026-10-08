# Demos, round 2: a musical set with a BLEND slider

8 Oct 2026. Decided with the owner: replace the ten control demos (made from the test stimuli, DRIVE mostly at noon or above) with eight musical clips that show the range of the sound, sweet by default, with a dry/wet slider on each.

## Decisions

- **Sources (mixed):** the owner records modular parts in a dub techno style. Everything else comes from licence-free (CC0) samples or is synthesised in code. No commercial recordings: the site and repo are public.
- **Sweet by default:** most clips have DRIVE at or below 10 o'clock and are in CLEAN or TAPE. One clip is the rough end (VALVE, howl) and is labelled as such.
- **BLEND slider:** each clip has a dry and a wet file rendered from the same source. Dry is BLEND fully left (a clean passthrough, so it's time-aligned with the wet). Wet is BLEND fully right. The page blends them live with the module's equal-power BLEND curve. The slider starts at the clip's own BLEND setting. Clips don't automate BLEND, and throws use the THROW button.
- **Levels:** both files of a clip get the same gain, set so the clip at its default BLEND sits at the set's loudness target. The slider then behaves like the knob.
- **The old ten are retired.** The manual explains the controls in words.

## The eight clips

| # | Clip | Source | Direction |
|---|---|---|---|
| 1 | Tubby throw and sweep | CC0 drums, sequenced in code (one-drop, ~75 bpm) | TANK 2, TAPE, DRIVE ~9:30. Throws on the snare, then the “Big Knob” ridden |
| 2 | Skank into echo | CC0 organ or guitar, or synthesised organ | TANK ECHO clocked, dotted 1/8, CLEAN or TAPE, DRIVE low |
| 3 | Melodica slap | CC0 melodica, or a synthesised reed lead | TANK 1, CLEAN, short DECAY, bright TENSION |
| 4 | Chord stab into the bed | **Owner's modular**: stabs + kick | TANK 2, CLEAN, DECAY top (the Hold), ducking under the kick |
| 5 | Echo chord | **Owner's modular**: sparse stabs | TANK ECHO clocked, TAPE, TONE ridden slowly |
| 6 | Rhodes in the springs | CC0 electric piano | TANK 2, CLEAN, long DECAY, WOBBLE a touch left |
| 7 | Modular plucks | **Owner's modular**: pluck sequence | TANK 1, TAPE, DRIVE low, soft and wide |
| 8 | The rough end | Synthesised dub siren | VALVE, DRIVE high, the howl, then DECAY pulled back |

## Recording spec for the owner's modular parts

- **Format:** WAV, 48 kHz, 24-bit, mono or stereo. Dry: no reverb or delay. Filter and VCA movement are part of the sound and welcome.
- **Level:** peaks around −6 dBFS. No limiting or master processing.
- **Tempo and key:** 120 bpm (or your choice; tell me the tempo). Any minor key; tell me which.
- **Length:** exactly the bars listed, starting on the downbeat. I add the silence for the tails when rendering.
- **Parts** (one WAV each):
  1. `stabs.wav`: 8 bars of dub techno chord stabs, short (an eighth note or less), one or two per bar on the offbeats.
  2. `kick.wav`: 8 bars of four-on-the-floor kick at the same tempo, to play under the stabs. Resilio's held bed ducks to it.
  3. `stabs_sparse.wav`: 8 bars, one stab every two bars, for the echo chord.
  4. `plucks.wav`: 8 bars of a sequenced pluck or blip line, short envelopes, with some rests.
  5. Optional: `bass.wav`, 8 bars of sub bass, if you want it in the mix.
- **Where:** put them in `test_audio/demo_sources/` in the repo (WAVs never enter git), or in an Ableton project folder and tell me the path.

## Build steps

1. The slider player (site): dry and wet in sync, the equal-power BLEND curve, the slider starting at the clip's BLEND, keyboard and screen-reader friendly. Works with one pair until all eight exist.
2. Sources: CC0 samples found and licence-checked (credited on the Credits page), the synthesised parts written, the owner's stems added.
3. Render script: `docs/minisite/tools/demos/` gets a v2 that renders each clip's dry and wet pair, matches loudness and encodes MP3s, plus the settings in `demos.json`.
4. A listening page for the owner (phone-sized) before anything goes on the site.
