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
