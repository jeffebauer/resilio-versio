# Starting points

*Draft for M9.* Six classic dub settings to start from, then move by ear. Each is also a preset file in `presets/starting_points/` that the Renderer and tests can load:

```bash
build/rv_render test_audio/stimulus/02_hits.wav out.wav --preset presets/starting_points/tubby_snare_splash.json
```

Clock positions are approximate (fully left 7 o'clock = 0, noon = 0.5, fully right 5 o'clock = 1, so each hour is about 0.1); the numbers are exact. Switches: TANK 1 / 2 / ECHO = left / centre / right; ATTITUDE CLEAN / TAPE / VALVE = left / centre / right (names since v1.0.43: SPRINGS 1 / 2 / 3, CLEAN / DRIVEN / KICKED before; the preset files read either).

WOBBLE is bipolar: noon is still, left drifts like tape, right is a vibrato. Every preset sits just left of noon for a little tape drift. TONE right of noon is the Big Knob low cut, so the presets that sit there (Tubby snare, tight slap) are slightly thinned. Rechecked on the sound of 6 Oct 2026 (the wet alone, on the test hits, skank and a held pad): skank and pads stay clear of the output limiter in every preset; loud drum hits reach it in Tubby snare splash, Drowned Howl and Valve tank drop (the wet's peaks touch the limiter's ~−1.7 dBFS knee), so a red output light on the hardest hits is normal there.

| Preset | TANK | ATTITUDE | BLEND | DECAY | TONE | TENSION | SPLASH | DRIVE | WOBBLE |
|---|---|---|---|---|---|---|---|---|---|
| Tubby snare splash | 2 | TAPE | 0.45 (11:30) | 0.6 (1) | 0.65 (1:30) | 0.35 (10:30) | 0.7 (2) | 0.5 (12) | 0.45 (11:30) |
| Skank chord wash | 2 | CLEAN | 0.35 (10:30) | 0.7 (2) | 0.4 (11) | 0.5 (12) | 0.2 (9) | 0.25 (9:30) | 0.43 (11:15) |
| Tight slap | 1 | CLEAN | 0.35 (10:30) | 0.05 (7:30) | 0.6 (1) | 0.9 (4) | 0.3 (10) | 0.2 (9) | 0.45 (11:30) |
| Drowned Howl | 2 | VALVE | 0.8 (3) | 0.95 (4:30) | 0.35 (10:30) | 0.3 (10) | 0.5 (12) | 0.6 (1) | 0.41 (11) |
| Valve tank drop | 1 | VALVE | 0.6 (1) | 0.7 (2) | 0.55 (12:30) | 0.2 (9) | 0.6 (1) | 0.55 (12:30) | 0.45 (11:30) |
| Mix-bus spring | 2 | CLEAN | 0.15 (8:30) | 0.35 (10:30) | 0.45 (11:30) | 0.6 (1) | 0.1 (8) | 0.1 (8) | 0.45 (11:30) |

*Skank chord wash and Mix-bus spring were voiced on the old third position, three Springs, which became echo mode (ADR 0041). Since 6 Oct 2026 they sit on TANK 2, the closest spring sound (in ECHO they would have played a tape echo, not a dense halo); every other setting is unchanged.*

## Tubby snare splash — `tubby_snare_splash.json`
**Sounds like:** the snare thrown into the spring: a bright crack, a loose boing, a ~2.5 s dripping tail with tape grit.
**Play into it:** snare or rimshot, one hit per bar on the backbeat. Best on a send: BLEND fully right on a return, then ride the send.
**Move next:** TENSION further left for a bigger, slower boing; TONE further right to thin it out like Tubby's Big Knob (the snare gets a nasal ring); SPLASH past 3 o'clock for a much bigger clang; DRIVE up for dirtier drips. Flip ATTITUDE to VALVE mid-tail for a one-off crash.

## Skank chord wash — `skank_wash.json`
**Sounds like:** a smooth, dense halo behind offbeat chords, warm and slightly moving, never smearing the groove.
**Play into it:** guitar or organ skank, short stabs on the offbeat.
**Move next:** DECAY down if the chords start to blur into each other; WOBBLE further left for a seasick, worn-tape drift (or right of noon for a steady vibrato); TANK to 1 for a sparser, splashier tank; or TANK to ECHO with TENSION on a dotted 1/8 to skank into the tape echo.

## Tight slap — `tight_slap.json`
**Sounds like:** a short, pingy spring slap (~0.5 s): the "amp in a small room" spring, quick repeats, still clearly springy.
**Play into it:** melodica, vocals, rimshots, anything busy that a long tail would swamp.
**Move next:** TENSION a little left if it's too metallic; BLEND up for an obvious slapback; TONE left to sit it behind the source.

## Drowned Howl — `drowned_howl.json`
**Sounds like:** the tank tipping into its own feedback: a rough, moving, siren-like roar that keeps going after the input stops. It never becomes a clean tone.
**Play into it:** one hit or a dub-siren blip, then stop and ride the knobs.
**Move next:** ride TONE and TENSION to steer the pitch and colour of the Howl; WOBBLE further left for more seasickness. Pull DECAY below ~4 o'clock to let it fall back into a normal tail (it fades in a second or two). Watch the output LEDs: red here means the limiter is holding it.

## Valve tank drop — `valve_tank_drop.json`
*Was "Kicked tank drop" until 6 Oct 2026 (ATTITUDE's KICKED is now VALVE, ADR 0044).*
**Sounds like:** a hard hit lands like the whole tank was kicked: a big metallic crash that boings out for ~3.5 s. One loose Spring keeps it sparse and splashy. (It was voiced around the Kick, removed in ADR 0043: it now needs a hit fed in.)
**Play into it:** throw the snare or rim on the drop, the one before the bass comes back in: hold the button (or a gate) for that one hit.
**Move next:** a gate sequence for rhythmic throws; DECAY past 4 o'clock and the crash can build into Howl; ATTITUDE to TAPE or CLEAN for a softer knock.

## Mix-bus spring — `mix_bus_spring.json`
**Sounds like:** a light, short, dense spring you feel more than hear, gluing a whole mix together.
**Play into it:** a submix or a full stereo mix.
**Move next:** BLEND a touch up or down until you only notice it when it's off; TONE left if the highs get busy; keep DRIVE and SPLASH low so peaks stay clean. Pads in the mix are looked after: the tank turns down held sounds before they reach the limiter.
