# Reference recording notes

## Wellspring session, 29 Sep 2026

Ableton set: `~/Music/Ableton/Projects/Wellspring reference recordings Project`. 48 kHz / 24-bit.
Stimulus out: interface output 3 → Wellspring LINE L/MONO. Return: Wellspring OUT L/R → interface inputs 11/12 (rear).
Interface input gain unchanged all session (skank peaks about −14 dBTP at the base setting).

- **Take 0 (loopback):** front output 3 → front input 1 (input 1 gain: records 6.0 dB hotter than the stimulus, so its level doesn't represent inputs 11/12; use it for timing only). Timing check: take E2's dry signal on the rear inputs gives −8 samples; the loopback gives −10. The front and rear inputs match within 2 samples.
- **Base INPUT:** set on `04_skank`, just shy of the CLIP light; no other stimulus clips at that setting. Used for A, B, D, E, E2, F, G, G2, A-L, A-R.
- **C:** INPUT raised until the CLIP light flashes on the loudest (−6 dBFS) snare of `02_hits`. Position: **about 1:30–2 o'clock**.
- **E2:** SPRINGS DRY/WET at **noon** (where it sounded best for dub).
- **G, feedback parallel** (`wellspring_G_ringing_parallel.wav`): FEEDBACK + MAGIC 11:30, feedback switch PARALLEL, delay time noon, delay DRY/WET full wet (fully CW), filter HP fully CCW. The feedback switch position had a marked effect on frequency buildup.
- **G2, feedback ping-pong** (`wellspring_G2_ringing_pingpong.wav`): FEEDBACK + MAGIC 11:00, feedback switch PING PONG, delay time noon, delay DRY/WET full wet (fully CW), filter HP fully CCW.

### How the files were made (Claude)
Copied byte-for-byte from the Ableton recordings (`Samples/Recorded/`), renamed per the recipe. One adjustment: in that set the `01_clicks` stimulus clips' start marker sat at 0.005176 s (248 samples) into the file, so the stimulus started late by that much. Takes 0, A, A-L and A-R had 248 samples of silence added at the front, so every take lines up as if its stimulus started at the file's start. Ableton itself places the recordings about 8–10 samples early (it over-compensates latency); `tools/ingest_references.py` handles that.

| Take | File | From |
|---|---|---|
| 0 | wellspring_0_loopback.wav | Loopback 0002 (+248) |
| A | wellspring_A_clicks.wav | Wellspring 0001 (+248) |
| B | wellspring_B_hits.wav | Wellspring 0002 |
| C | wellspring_C_hits_hot.wav | Wellspring 0012 |
| D | wellspring_D_sweep.wav | Wellspring 0003 |
| E | wellspring_E_skank.wav | Wellspring 0004 |
| E2 | wellspring_E2_skank_mix.wav | Wellspring 0005 |
| F | wellspring_F_knocks.wav | Wellspring 0006 |
| G | wellspring_G_ringing_parallel.wav | Wellspring 0008 |
| G2 | wellspring_G2_ringing_pingpong.wav | Wellspring 0009 |
| A-L | wellspring_A-L_clicks_left.wav | Wellspring 0010 (+248) |
| A-R | wellspring_A-R_clicks_right.wav | Wellspring 0011 (+248) |

Not used: Loopback 0001 and Wellspring 0007 (unused passes, no clip in the set).
