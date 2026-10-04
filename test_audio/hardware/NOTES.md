# Hardware recordings (Resilio on the Versio)

30 Sep 2026, release firmware `e2ff5a7` (run 12 CPU trim, LED meters, before the LED flicker fix). Ableton set `~/Music/Ableton/Projects/Wellspring reference recordings Project`, stimulus out and return through the OPTX2 (ADAT), Ableton clip gain 0 dB. Copied byte for byte by Claude from `Samples/Recorded/` (clip name → file via the set's XML). Unused files there: `Resilio Versio e2ff5a7 0001` and `0007` (not referenced by any clip), `0015` (empty).

| Take | Settings | From |
|---|---|---|
| h0_loopback_{01_clicks,09_pink_noise} | OPTX out → OPTX in, no Versio | 0002, 0003 |
| h1_dry_{01_clicks,09_pink_noise} | Versio, MIX fully left | 0004, 0005 |
| h2_clean_{01_clicks,02_hits,04_skank} | MIX fully right; DECAY, TONE, TENSION noon; SPLASH, DRIVE, WOBBLE fully left; SPRINGS centre; ATTITUDE left (CLEAN) | 0006, 0010, 0008 |
| h3_kicked_{01_clicks,02_hits,04_skank} | as H2, ATTITUDE right (KICKED) | 0009, 0011, 0012 |
| h4_splash_{02_hits,04_skank} | as H2, SPLASH fully right | 0013, 0014 |

Timing: the OPTX loop is 69–70 samples late vs the stimulus; with the Versio in the loop, 199–201 (the Versio adds 131 samples, 2.7 ms: two 48-sample blocks plus converters).

## 4 Oct 2026, release firmware `327af86` (DSP of main `340b542`, no knob end stops)

Same set, track "Resilio Versio 340b542", OPTX2 out/return, clip gain 0 dB. Copied byte for byte by Claude (clip name → file via the set's XML). Unused: `0001` (9.9 s, not referenced by any clip; peaks −5 dBFS, no glitches), `0005` (0 bytes, empty).

| Take | Clip name | Settings | From |
|---|---|---|---|
| h5_clean_decay0_01_clicks | 01_clicks_decay_ccw | CLEAN, SPRINGS 2, MIX fully right, TONE/TENSION noon, WOBBLE noon (still), SPLASH/DRIVE fully left, **DECAY fully left** | 0002 |
| h5_clean_decay1_01_clicks | 01_clicks_decay_cw | as above, **DECAY fully right** | 0003 |
| h6_shaker_return_1 | Hi Seed Shaker 100bpm Off Accents_Red Input LED flashes | DRIVEN, DRIVE ~9–12 o'clock, owner moving DRIVE/SPLASH/MIX/TONE; source `User Library/Samples/ELPHNT/Shake/Loops/Hi Seed Shaker 100bpm Off Accents.wav` (44.1 kHz mono, 9.6 s, peaks 0.0 dBFS), warped (Beats) 100 → 120 bpm, looped, gain 0 dB | 0004 |

Timing: the h5 returns line up with desktop renders of `01_clicks.wav` at **+250 samples** (correlation 0.80 at DECAY 0); the −30 dB dry leak starts ~+239. Analysis (per-octave T60, pot end readings): `docs/m8-tuning-backlog.md` once the coordinator writes it up; short version: DECAY fully left reads 0.00 (±0.01), fully right ~0.99.
