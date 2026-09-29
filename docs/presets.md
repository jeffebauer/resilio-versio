# Starting points

*Draft for M9.* Six classic dub settings to start from, then move by ear. Each is also a preset file in `presets/starting_points/` that the Renderer and tests can load:

```bash
build/rv_render test_audio/stimulus/02_hits.wav out.wav --preset presets/starting_points/tubby_snare_splash.json
```

Clock positions are approximate (fully left 7 o'clock = 0, noon = 0.5, fully right 5 o'clock = 1, so each hour is about 0.1); the numbers are exact. Switches: SPRINGS 1 / 2 / 3 = left / centre / right; ATTITUDE CLEAN / DRIVEN / KICKED = left / centre / right.

**SPLASH values are provisional.** SPLASH is being redesigned (round 3: how hard the hits hit, from the input). Re-set SPLASH by ear once it lands; nothing below depends on its exact sound.

| Preset | SPRINGS | ATTITUDE | MIX | DECAY | TONE | TENSION | SPLASH* | DRIVE | WOBBLE |
|---|---|---|---|---|---|---|---|---|---|
| Tubby snare splash | 2 | DRIVEN | 0.45 (11:30) | 0.6 (1) | 0.65 (1:30) | 0.35 (10:30) | 0.7 (2) | 0.5 (12) | 0.2 (9) |
| Skank chord wash | 3 | CLEAN | 0.35 (10:30) | 0.7 (2) | 0.4 (11) | 0.5 (12) | 0.2 (9) | 0.25 (9:30) | 0.35 (10:30) |
| Tight slap | 1 | CLEAN | 0.35 (10:30) | 0.05 (7:30) | 0.6 (1) | 0.9 (4) | 0.3 (10) | 0.2 (9) | 0.1 (8) |
| Drowned Howl | 2 | KICKED | 0.8 (3) | 0.95 (4:30) | 0.35 (10:30) | 0.3 (10) | 0.5 (12) | 0.6 (1) | 0.5 (12) |
| Kicked tank drop | 1 | KICKED | 0.6 (1) | 0.7 (2) | 0.55 (12:30) | 0.2 (9) | 0.6 (1) | 0.55 (12:30) | 0.2 (9) |
| Mix-bus spring | 3 | CLEAN | 0.15 (8:30) | 0.35 (10:30) | 0.45 (11:30) | 0.6 (1) | 0.1 (8) | 0.1 (8) | 0.15 (8:30) |

\* provisional.

## Tubby snare splash — `tubby_snare_splash.json`
**Sounds like:** the snare thrown into the spring: a bright crack, a loose boing, a ~2.5 s dripping tail with tape grit.
**Play into it:** snare or rimshot, one hit per bar on the backbeat. Best on a send: MIX fully right on a return, then ride the send.
**Move next:** TENSION further left for a bigger, slower boing; TONE right for more sizzle; DRIVE up for dirtier drips. Flip ATTITUDE to KICKED mid-tail for a one-off crash.

## Skank chord wash — `skank_wash.json`
**Sounds like:** a smooth, dense halo behind offbeat chords, warm and slightly moving, never smearing the groove.
**Play into it:** guitar or organ skank, short stabs on the offbeat.
**Move next:** DECAY down if the chords start to blur into each other; WOBBLE up past noon for a seasick, worn-tape feel; SPRINGS to 2 for a more classic, splashier tank.

## Tight slap — `tight_slap.json`
**Sounds like:** a short, pingy spring slap (~0.5 s): the "amp in a small room" spring, quick repeats, still clearly springy.
**Play into it:** melodica, vocals, rimshots, anything busy that a long tail would swamp.
**Move next:** TENSION a little left if it's too metallic; MIX up for an obvious slapback; TONE left to sit it behind the source.

## Drowned Howl — `drowned_howl.json`
**Sounds like:** the tank tipping into its own feedback: a rough, moving, siren-like roar that keeps going after the input stops. It never becomes a clean tone.
**Play into it:** one hit or a dub-siren blip, then stop and ride the knobs.
**Move next:** ride TONE and TENSION to steer the pitch and colour of the Howl; WOBBLE for more seasickness. Pull DECAY below ~4 o'clock to let it fall back into a normal tail (it fades in a second or two). Watch the output LEDs: red here means the limiter is holding it.

## Kicked tank drop — `kicked_tank_drop.json`
**Sounds like:** the whole tank dropped or kicked: a tight thud, then a big metallic crash that boings out for ~3.5 s. One loose Spring keeps it sparse and splashy.
**Play into it:** press the button (or patch a gate) on the drop, the one before the bass comes back in. Works with no input at all.
**Move next:** a gate sequence for rhythmic kicks; DECAY past 4 o'clock and the crash can build into Howl; ATTITUDE to DRIVEN or CLEAN for a softer knock.

## Mix-bus spring — `mix_bus_spring.json`
**Sounds like:** a light, short, dense spring you feel more than hear, gluing a whole mix together.
**Play into it:** a submix or a full stereo mix.
**Move next:** MIX a touch up or down until you only notice it when it's off; TONE left if the highs get busy; keep DRIVE and SPLASH low so peaks stay clean.
