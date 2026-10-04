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

## Magneto session, 29 Sep 2026

Same Ableton set as the Wellspring session. Rear DIP S2 = ON (Dual Split): L = tape delay, R = spring. 48 kHz / 24-bit, mono.
Path: Ableton → interface ADAT output channel **17** → OPTX2 channel 1 output jack → Magneto; Magneto → OPTX2 channel 1 input jack → interface ADAT input channel **17** → Ableton. One output and one input for every take; repatched at the Magneto between R (spring) and L (tape).

- **Take 0 (ADAT loopback):** OPTX2 ch 1 out → OPTX2 ch 1 in. Measured +84 samples (the TRS path reads −10: the ADAT/OPTX2 round trip is ~94 samples, ~2 ms, longer).
- **Spring takes (MA, MB, ME, MS):** the spring path has no input control in Dual Split; stimulus at 0 dB. No clipping, no audible distortion. **MS** (`03_sweep` through the spring) is an extra take, not in the recipe.
- **Tape takes (MW, MD):** SPEED/PITCH noon. DRY 0, WET 100 %, REPEATS 0, FEEDBACK buttons off, head 1 only. Base REC LVL 8–8:30 (bright green; yellow just above). MW0–MW4: WOW & FLUTTER fully CCW / 9 / 12 / 3 o'clock / fully CW.
- **MD REC LVL:** MD1 green = 9 o'clock, MD2 amber = 12 o'clock, MD3 red = 4 o'clock.

### How the files were made (Claude)
Copied byte-for-byte and renamed; the Eurorack `01_clicks` clip again started 0.005176 s (248 samples) into the file, so takes 0 and MA got 248 samples of silence added at the front.

| Take | File | From |
|---|---|---|
| 0 | magneto_0_adat_loopback.wav | Eurorack 0002 [180455] (+248) |
| MA | magneto_MA_clicks.wav | Eurorack 0001 [180713] (+248) |
| MB | magneto_MB_hits.wav | Eurorack 0002 [181726] |
| MS | magneto_MS_sweep.wav | Eurorack 0003 |
| ME | magneto_ME_skank.wav | Eurorack 0004 |
| MW0–MW4 | magneto_MW0_wow_ccw … MW4_wow_cw | Eurorack 0007–0011 |
| MD1–MD3 | magneto_MD1_reclvl_green … MD3_reclvl_red | Eurorack 0012–0014 |

Not used: Eurorack 0001 [180028], 0005, 0006 (unused passes, no clip in the set).


## Wellspring session 2, 4 Oct 2026

Same Ableton set, patch, "spring only" settings and base INPUT position as session 1 (INPUT unchanged throughout). 48 kHz / 24-bit, stereo. No new loopback take. Recorded on the track "Wellspring Session 2" (`Wellspring Session 2 00xx [2026-10-04 …].wav`).

- **OUTPUT change:** take I's return clipped at the normal OUTPUT (top centre), so the Wellspring's OUTPUT was turned down and **H, I and D2 were all recorded at that lowered OUTPUT** (about 8–9 dB lower: D2's peak is 8.8 dB under session 1's D, the full-OUTPUT H pass 7.8 dB over the lowered H). OUTPUT then went back to top centre for D-L, D-R, J, K, L, M, N.
- **D-L:** dummy plug in the R input (left tank only). **D-R:** stimulus into R only, L empty (right tank only). J, K, L, M, N: default (L in, R unplugged, both tanks).
- **N:** everything patched, nothing playing, OUTPUT top centre.
- **Timing:** the session 2 sweeps sit **34 samples (0.7 ms) later** than session 1's D (cross-correlated in the 3–8 kHz part of the sweep; D2, D-L and D-R all agree within 1 sample). So this session's path latency is about +24 samples, not session 1's −10; the ingest tool aligns every take with session 1's take 0 value, so session 2 takes come out 0.7 ms late. Harmless for levels, spectra and T60; take it into account for onset-timing measurements, or record a loopback take next session.

### How the files were made (Claude)
Copied byte-for-byte from `Samples/Recorded/` (verified with `cmp`) and renamed per the recipe. Every stimulus clip's start marker is at 0 and the recorded clips start at 0, so no silence was added. The clip names in the set mark the lowered-OUTPUT passes: `wellspring_I_sweep_hot (lower output level)`, `wellspring_IB_sweep_quiet (lower output level)` (= H at the lowered OUTPUT) and `wellspring_IC_sweep (lower output level)` (= D2).

| Take | File | From | Peak L / R (dBFS) |
|---|---|---|---|
| H | wellspring_H_sweep_quiet.wav | Wellspring Session 2 0010 (clip "IB", lowered OUTPUT) | −27.6 / −26.9 |
| I | wellspring_I_sweep_hot.wav | Wellspring Session 2 0009 (lowered OUTPUT) | −0.8 / −0.1 |
| D2 | wellspring_D2_sweep_outlow.wav | Wellspring Session 2 0011 (clip "IC", lowered OUTPUT) | −9.6 / −10.4 |
| D-L | wellspring_D-L_sweep_left.wav | Wellspring Session 2 0012 | −1.6 / −36.0 |
| D-R | wellspring_D-R_sweep_right.wav | Wellspring Session 2 0013 | −62.0 / −2.7 |
| J | wellspring_J_tone_bursts.wav | Wellspring Session 2 0015 | −13.6 / −12.7 |
| K | wellspring_K_pink_noise.wav | Wellspring Session 2 0016 | −10.4 / −9.6 |
| L | wellspring_L_held_tones.wav | Wellspring Session 2 0017 | −15.7 / −7.0 |
| M | wellspring_M_pad.wav | Wellspring Session 2 0018 | −13.6 / −13.9 |
| N | wellspring_N_silence.wav | Wellspring Session 2 0019 | −75.5 / −74.8 |
| H (full OUTPUT) | session2_extra/wellspring_H_sweep_quiet_fullout.wav | Wellspring Session 2 0008 (clip "H", before OUTPUT was lowered) | −19.8 / −19.4 |

The full-OUTPUT H sits in `session2_extra/` so the ingest tool (which reads one file per take from the top folder) uses the lowered one.

- **No clipping** in any take used. I is close: 5 samples above −1 dBFS on R, single-sample peaks, no flat tops (the aim was about −6 dBFS).
- **Lengths:** the sweeps are 24.0 s for a 23 s stimulus; J 72.0 s (71.84 s); K 22.0 s for 22 s, so only about 1 s of tail after the noise stops; L 22.0 s (20 s); M 20.0 s (19 s); N 30.0 s (30 s). L's last tail is cut at about −56 dB.
- **Not used:** 0001, 0002, 0004, 0007 (sweeps at other levels/OUTPUT positions, no clip in the set), 0003 and 0005 (take I at the normal OUTPUT: clipped, thousands of samples above −1 dBFS), 0006 (a sweep peaking −1.8 dBFS, no clip), 0014 (a J pass stopped at 16.6 s), 0020 (empty file).
