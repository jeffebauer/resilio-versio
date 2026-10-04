# 0041 — SPRINGS 3 is echo mode: tape echo into the springs

**Status:** Proposed, 4 Oct 2026 (branch `feat/echo-mode`; the owner's design, 4 Oct; listening pages `renders/feat_echo_mode/`; merge after the owner's OK). Supersedes ADR 0037's coupled Springs as what position 3 ships with; the coupled voicings stay as Renderer-only references. Numbers: `core/params/EchoVoicing.h`. Code: `core/dsp/Echo.h`, `core/dsp/EchoClock.h`, `Tank` ("SPRINGS 3 = echo mode"). Tests: `host/tests/test_echo_mode.cpp`.

**Context:** what makes you reach for position 3 stayed open after ADR 0037: the owner heard coupled and plain three Springs as almost the same. The dub-lens critique (`docs/research/dub-lens-critique.md` §3.4, direction D) found that nearly every rig in the lineage put a tape echo in front of the spring (Tubby, Perry's Space Echo, Sherwood, Pole, Echospace; Sylvan Morris tuned a tape loop to each tune's tempo), and that the interaction is the sound: each repeat lands in the springs with its own splash. The prototype (`proto/echo-springs`, `c2fb5b3`) compared series, in-loop and parallel; the owner picked **B, series** (4 Oct), and set the panel:
- In position 3, **DECAY = the echo's feedback**, **TENSION = the echo time**, **the gate = a clock** (one pulse = one beat), with TENSION then picking straight and dotted divisions. Positions 1 and 2 keep the gate's own job.
- The springs behind the echo are **fixed**: today's noon tank with one classic medium tail (~1.5–2 s, like a Space Echo's built-in spring).
- A new time **swoops like tape** (~0.3 s, the repeats bend in pitch).
- In the plugin it follows the DAW's tempo.

**Decision:**
- **Signal path (series):** the input (mono) → the tape → the springs' input, before the Splash. The springs hear the input plus its repeats, so the Clang, Bite, DRIVE and TONE act on every repeat as on a fresh hit. The feedback stays on the tape. The tape's playback head darkens (2-pole low-pass 3.5 kHz) and thins (high-pass 140 Hz) each pass; the record head loses the top (2-pole low-pass 6 kHz) and saturates (`softClip`, drive 1.5), so a build grits up and can't exceed 0.67 on the tape. The record head's low-pass wasn't in the prototype: without it a hot cymbal's highs fold back down as inharmonic tones when the tape saturates (a 0 dBFS 15 kHz tone: −48 dBFS at 3 kHz; with it −113 dBFS); the repeats are darker than 6 kHz anyway. WOBBLE moves the tape too (its own Transport-role generator, ADR 0034) and a slow ~5-cent wander always runs.
- **Echo time, free:** TENSION 0 → 1 = 2 s → 80 ms, log (0.4 s at noon). Turning TENSION up is tighter, as everywhere else.
- **Echo time, clocked:** one gate pulse = a quarter note. TENSION's seven zones, long → short: 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16, with 15 % hysteresis at each border so a resting knob or CV doesn't flicker. A time over 2 s plays at half. The tempo is the median of the last three intervals between rising edges; a reading within 2 % of the tempo in use is ignored; 30–300 bpm; a shorter interval is a bounce (ignored), a longer one starts the count again. The clock is lost after 2.25 beats without a pulse (at most 2.5 s) and the echo glides back to free time. Edges count on their exact samples, so any block size reads the same tempo. The gate feeds the clock in every position (the tempo is already known when SPRINGS reaches 3) and only kicks in positions 1–2: in position 3 the echo is the throw. The button always kicks.
- **Plugin:** the DAW's tempo (`setHostTempo`), one beat = its quarter note, over the gate; no tempo = free time.
- **The swoop:** a time change (TENSION, a new division or tempo, the clock lost) moves the tape speed over ~0.3 s, at most 0.5 samples of delay per sample (the repeats bend within ×0.5–×1.5 in pitch), ramped per sample: never a click.
- **Feedback (DECAY):** CLEAN and DRIVEN 0.95 × DECAY^0.926: DECAY 0 = one repeat, noon 0.5, top 0.95 (a long build that still fades; the heads lose a little each pass). KICKED rises (smoothstep) from DECAY 0.75 to 1.25 at the top; each pass gains from DECAY ~0.87 (measured: 0.873), so the top ~13 % is a runaway, a dub self-oscillation held by the tape's saturation and the output limiter, which dies away when DECAY comes back down. Blended with the ATTITUDE Morph.
- **The springs behind it:** Springs A and B play position 2 (its mix, stages and level) at TENSION noon and the DECAY that gives T60 1.7 s (measured on a click: ~1.6 s); DECAY and TENSION don't touch them (bit for bit up to the first repeat). Level trim ×0.84, so position 3 at DECAY / TENSION noon sits within ±2 dB of position 2 (measured −0.7 … +0.2 dB K-weighted, hits and stabs, every ATTITUDE).
- **Switching:** into position 3 the echo fades in over 80 ms on a **fresh tape** (what was recorded before reads as blank until it is recorded over: wiping 2 s of tape would cost about a whole audio block on the Daisy), while the Springs glide from the knobs' tank to the fixed one. Out of position 3 the echo fades out over 80 ms, its last repeats ringing on in the springs.
- **Spring C** is heard nowhere any more (positions 1 and 2 never had it, position 3 plays A and B), so its audio doesn't run. Its settings still follow on its turn (control rate), so A and B take theirs on the same ticks as before and positions 1 and 2 stay bit for bit while knobs move (skipping its turn too changed them).
- **Memory:** the tape is 2 s + 1,024 floats (97,024 floats, 379 KB at 48 kHz), a second buffer the host hands the Tank (`prepare(fs, block, pool, n, tape, tapeN)`). The firmware puts it in ordinary .bss (AXI SRAM: 418 KB used of 512 KB; DTCM is full with the delay pool); the Renderer and plugin allocate it. A shorter tape caps the longest time; none leaves the echo silent.
- **The coupled Springs** (ADR 0037) stay as a Renderer-only reference: hidden key `echo_mode=0` plays position 3 as before with `springs3_voicing`. The firmware no longer builds the palette (`springs3::kPaletteBuilt` false under `RV_FIXED_VOICINGS`).
- **Renderer:** hidden keys `echo_mode` (1 default) and `host_bpm`; automation JSON `"clocks": [seconds…]` (gate edges) and `"clock_bpm": 120` or `{"bpm", "start", "end"}`.

**What holds** (`test_echo_mode`, all pass):
- Positions 1 and 2 bit for bit with echo mode off (Spring C running) on hits and stabs at every ATTITUDE, and while DECAY / TENSION / TONE move and the gate clocks; the Renderer's output `cmp`-identical to `main` (`c6df657`) in 104 renders (4 stimuli × SPRINGS 1/2 × every ATTITUDE × DECAY × TENSION, plus automation with kicks).
- Free time 2 s / 0.4 s / 80 ms; on the output the first repeat lands 0.4006 s and 0.0804 s after the hit.
- Clocked divisions at 120 and 30 bpm, hysteresis, ±1 % jitter and a bounce, lost after 2.25 beats, edges exact at block 16 / 48 / 512; the host tempo over the gate.
- Swoop: 63 % of a small move in 0.24 s, never over 0.5 samples per sample; no clicks while TENSION jumps and a clock comes and goes.
- Feedback: one repeat at DECAY 0; CLEAN and DRIVEN fade at DECAY 1; KICKED runs away under 1 (peak 0.72) and falls > 30 dB within 4 s of DECAY coming back to noon.
- Springs fixed (bit for bit across DECAY and TENSION until the repeats), tail ~1.6 s.
- Hot highs into the tape (15 kHz at 0 / −6 dBFS, KICKED DRIVE 1): folds under −90 dBFS (−113 / −131 at 3 kHz).
- Fresh tape (nothing from before plays), level ±2 dB of position 2, switching 1/2 ↔ 3 mid-tail and rapid flipping without clicks, stability at the extremes and over a 72-cell grid (finite, peaks under 1, repeats fade outside KICKED's runaway), block sizes 16 / 48 / 333 / 512 bit for bit.
- The suites that hold the Springs to their bars (`test_tank`, `test_drive`, `test_antires`, `test_sustain_trim`, `test_led_meter`, `test_springs3`) run position 3 as the three-Spring reference (`setEchoMode(false)`); echo mode has its own suite.

**Cost:**
- Flash: release 110,484 B (was 112,628), profile 112,424 B (was 114,600), m0test 82,320 B: dropping the coupled code (~3.8 KB) pays for the echo (~1.7 KB). No fused multiply-adds in our code (only libm's). RAM: AXI SRAM 407 KB (release) / 418 KB (profile) of 512 KB, the tape included.
- CPU, desktop (KICKED, DRIVE 1, DECAY 1, TENSION 0): `test_echo_mode` "cost": SPRINGS 3 echo ~443 ns/sample, SPRINGS 2 ~473 (~557 with Spring C running), the coupled reference ~545. Whole Renderer runs against `main` (skank, best of 5): SPRINGS 1 −8 %, 2 −6 %, 3 −19 %. On the chip: an M3 profile run is due (the CORNER lines now flag SPRINGS 2 and 3 at DECAY 1, TENSION 0 as "(worst?)"; the echo's time shows in the "echo" column, Spring C's old slot).

**Open for the owner (by ear, `renders/feat_echo_mode/`):** noon's 0.4 s as the resting time, the feel of DECAY's range (one repeat → long build), KICKED's runaway point, the swoop's speed, whether one gate pulse = a quarter note is right on the module's sequencers.

## Prototype: diffuse repeats (owner, 4 Oct 2026; open)

**Context:** the owner picked echo mode (B) on every panel, then asked: "It'd be interesting to hear what it sounds like if repeats become slightly diffuse as they repeat so there's a feeling of sound degradation with each repeat." This is not the prototype's C (the springs inside the echo's loop, not picked): the springs stay after the echo.

**Proposal:** a hidden Renderer key `echo_diffuse_voicing` (`EchoVoicing.h` `kDiffuse`; 0 = none, the default, bit for bit). Three short Schroeder allpasses sit on the tape's **feedback** only, so the smear compounds: the first repeat is the input itself (bit for bit as none), the second has been through the diffuser once, and so on. Their lengths drift slowly (±0.15–0.35 ms at 0.31 / 0.47 / 0.73 Hz) so the smear doesn't settle into a metallic comb.

| # | voicing | allpass g, lengths | each repeat's spread on the tape, repeats 1–6 (0.6 s, feedback 0.8) |
|---|---|---|---|
| 0 | none | none | 8.9, 9.2, 9.4, 9.5, 9.4, 8.9 ms (just the head's darkening) |
| 1 | light | 0.35; 2.3 / 4.1 / 6.7 ms | 8.9, 14, 17, 28, 38, 50 ms |
| 2 | medium | 0.5; 3.1 / 6.9 / 10.3 ms | 8.9, 28, 36, 55, 64, 76 ms |
| 3 | heavy | 0.62; 4.3 / 9.1 / 14.7 ms | 8.9, 45, 68, 100, 122, 143 ms |

- **Level per repeat** is unchanged: within 0.19 dB of none on the tape for repeats 1–6, because allpasses add no energy.
- **KICKED trim:** near KICKED's runaway, a smeared repeat's lower peaks saturate less on the tape, so long tails lasted 10–25 % longer. KICKED's feedback is trimmed per voicing (×0.985 / 0.975 / 0.965, with the Morph); KICKED tails now land within about ±15 % of none's.
- **The runaway** is still bounded in every voicing (peak 0.71–0.72) and still dies away: 90–94 dB down 3.5 s after DECAY returns to noon.

**Checks** (`test_echo_mode` "diffuse"):
- The first repeat is bit for bit as none in the Tank.
- M6 Ringing on position 3 (click and burst, every ATTITUDE, DECAY 0.85 and 1, TENSION 0 and 0.5) flags 0 of 20 cells in every voicing, worst ringing_db 10.7 (none 8.4).
- Extremes are finite with peaks under 1.
- A 20 s KICKED rim tail near the runaway (DECAY 0.8–0.85, TENSION 0.25–0.5) sometimes flags at ~6–7 kHz in the diffuse voicings. That bin sits at −145 to −180 dBFS, 90+ dB under the tail: the metric is reading the float noise floor, not an audible tone.

**Cost if it became the default:**
- Firmware: release 111,668 B (+1,000), profile 113,656 B (+1,088), the same for any of 1–3. AXI SRAM +24 KB: the allpass memory is sized for 96 kHz and could be ~9 KB at 48 kHz. Default 0 costs +184 B over the plain echo.
- Desktop: SPRINGS 3 worst case +3 %, about 13 ns/sample.

**Page:** `renders/feat_echo_diffuse/` (`tools/echo_diffuse_page.sh`).

## Level fix: the first repeat steps down too (owner, 4 Oct 2026; default)

**Context:** on the diffuse page the owner heard that "the first repeat is the same amplitude as the hit, which makes it feel like the decay isn't linear". The input went onto the tape at full level, so repeat 1 = hit and only later repeats stepped down by DECAY's feedback.

**Decision:** the input goes onto the tape at the feedback's own gain, so repeat n ≈ hit × gⁿ in CLEAN and DRIVEN. The gain is clamped to `kFirstRepeatMin` 0.316 and `kFirstRepeatMax` 0.95:
- **DECAY 0** stays a single repeat, now about 10 dB down (measured −11.2 dB with the heads' loss).
- **In KICKED** the first repeat is never louder than 0.95; the runaway climbs from the second repeat on, from DECAY ~0.87 as before.

The whole wet's trim `kTrim` went 0.84 → 0.98, so position 3 at DECAY / TENSION noon is again within ±2 dB of position 2. Measured −0.1 … +0.3 dB K-weighted, hits and stabs, every ATTITUDE. The bar still makes sense: it keeps a switch between 2 and 3 from jumping in level, and with this trim the hit itself through the springs is within 0.2 dB of position 2's.

**Checks** (`test_echo_mode` "steps", new): band-limited burst, 2 s echo, CLEAN. Each step is repeat k against repeat k − 1, with repeat 0 the hit:

| DECAY | g | steps (dB) |
|---|---|---|
| 0.3 | −10.1 dB | −11.2, −10.9, −10.8 |
| 0.5 | −6.0 dB | −7.2, −6.8, −6.7 |
| 0.7 | −3.3 dB | −4.5, −4.1, −4.0 |
| 0.9 | −1.3 dB | −2.5, −2.1, −2.0 |
| 0 | | −11.2, then nothing (−65) |

The first step is ~0.4 dB larger than the rest because it is the heads' first darkening. Everything else still passes:
- The runaway threshold is unchanged (DECAY 0.873), with peak 0.72; it falls 95 dB once DECAY returns to noon.
- The 72-cell stability grid passes (worst peak 0.890; least fade 10.4 dB over 30 s).
- ctest passes (100 % of 21), and positions 1–2 still match `main` in all 104 renders.

**Firmware:** release 111,340 B and profile 113,240 B with both prototypes at 0 (+672 B over the diffuse commit: the gain ramp and the shared feedback buffer).

## Prototype: the repeats break up (owner, 4 Oct 2026; open)

**Context:** the owner asked for "more degradation in the repeats … make it sound like it's breaking up: aliasing, bitcrushing, or something more tape-centric". This is not more diffusion.

**Proposal:** a hidden Renderer key `echo_wear_voicing`, default 0 = none. Each voicing is a process inside the tape's feedback (`core/dsp/EchoWear.h`, numbers in `EchoVoicing.h` "Wear"), so it compounds: repeat 1 is untouched, and repeat k has been through it k − 1 times. Every random choice is seeded, so renders are deterministic and identical at block 48 and 333. The firmware builds only the default.
1. **Worn tape** (Space Echo, Black Ark):
   - Each pass gets its own wow (0.6 ms deep, 0.55 / 1.3 Hz) and flutter (7.5 Hz), out of step with the echo, so the pitch wanders more on each repeat.
   - Random oxide dropouts: 2 per second, 3–40 ms long, 4–14 dB deep, raised-cosine. Older repeats carry more of them.
   - A saturation (`softClip(2.5x)/2.5`) that bites as a build grows.
   - The wow line's fixed delay is taken off the tape, so the echo time holds.
2. **Radio band** (dub techno): each pass through a 2-pole high-pass at 250 Hz and low-pass at 900 Hz, with the band's centre at unity. Repeats narrow to telephone / radio while keeping their level in the band.
3. **BBD grit** (Memory Man, the Wellspring's delay):
   - Each pass is sample-and-held at a 9.7 kHz clock between gentle 1-pole 4.5 kHz filters, so it aliases a little more every pass.
   - A 2:1 compander whose expander tracks slightly differently, so it pumps and breathes.
   - A clock whine at 5.2 kHz, −55 dB under the signal, which builds as repeats pass again.
4. **Crushed** (SDE-3000, samplers, Pole's crackle):
   - Each pass is re-sampled at 11.3 kHz with no anti-alias filter.
   - It is re-quantised to 7 bits relative to the signal's level (gain-ranging), so it never leaves a stuck tone.
   - Sparse crackle (5 per second) rides on the signal's level.

**Checks** (`test_echo_mode` "wear"). On the tape (0.6 s, feedback 0.8), level per repeat 1–6 against none:

| Voicing | Level per repeat 1–6 vs none |
|---|---|
| Worn tape | 0, −0.1, −0.2, −0.2, −0.2, −0.2 dB |
| Radio band | 0, −4.6, −5.1, −5.0, −4.9, −4.8 dB (the first pass takes a broadband hit down to the band, then steady) |
| BBD | 0, −2.0, −2.7, −2.7, −2.4, −2.0 dB |
| Crushed | 0, −0.3 … −0.7 dB |

- No repeat is ever louder than none's.
- Echo time: each repeat's centre is within 0.4 ms of none's (radio band 2.8 ms by the 6th repeat, from its filters' own delay).
- In the Tank: no clicks, no NaN, deterministic.
- KICKED DECAY 1 stays bounded (peak 0.72) and falls 57–65 dB within 3 s of DECAY coming back to noon.
- Extremes are finite with peaks under 1.
- M6 Ringing (click and burst, every ATTITUDE × DECAY 0.85/1 × TENSION 0/0.5) flags 0 of 20 cells in every voicing; worst ringing_db is 12.2 (none 8.4).
- The page's 60 renders show no clicks, clipping or NaN.

**Cost if it became the default:**

| Voicing | Release | Profile | AXI SRAM | Desktop, SPRINGS 3 worst case |
|---|---|---|---|---|
| Worn tape | 113,588 B (+2,248) | 115,488 B | +2 KB | +2.8 % |
| Radio band | 112,820 B (+1,480) | 115,472 B | +2 KB | +1.5 % |
| BBD grit | 113,236 B (+1,896) | 115,136 B | +2 KB | +1.9 % |
| Crushed | 112,948 B (+1,608) | 115,136 B | +2 KB | +0.8 % |

**Page:** `renders/feat_echo_wear/` (`tools/echo_wear_page.sh`). Version A there includes the level fix; `renders/feat_echo_diffuse/` was built before it.

## BBD grit is the default wear; strength round (owner, 4 Oct 2026)

**Decision so far:** the owner picked **D, BBD grit, on every panel** of `renders/feat_echo_wear/`, so `kWearDefault = kWearBbd` and the firmware builds it. Its strength stays at today's (`bbd_voicing` 0, A) until the owner picks from `renders/feat_echo_bbd/`. The owner's note: "increase the aliasing of the BBD repeats … the crushed examples all sounded pretty subtle — no audible aliasing".

**Why crushed (and BBD A) sounded subtle:** the feedback reaching them had already passed the tape's playback head (a 2-pole low-pass at 3.5 kHz).
- Almost nothing in it sat above half an 11.3 kHz or 9.7 kHz clock, so there was nothing to fold back.
- The images the hold makes (clock − f, 6–10 kHz) landed above the head, which erased them on the next pass.

A lower clock puts half the clock inside the head's passband. Content between it and 3.5 kHz then folds down to 0.5–3 kHz, and the images fall at 1–5 kHz, under the head, where they are heard and compound. Weaker filters (cutoff a larger share of the clock) let more of both through.

**Two changes to the BBD itself:**
- **The compander never adds level:** the expander gives back at most what the compressor took (gc × ge ≤ 1). It breathes down, never up. Otherwise CLEAN DECAY 1 grew instead of fading once filter make-up was added.
- **Make-up and whine:** the filters' and the hold's loss is made up at 500 Hz, and the whine now sits at the clock's own pitch.

**The four strengths** (`EchoVoicing.h` `kBbd`):

| | Clock | Filters (1-pole) | Character |
|---|---|---|---|
| A | 9.7 kHz | 4.5 kHz | today's, the reference |
| B | 4.8 kHz | 2.9 kHz | aliasing heard from the 2nd–3rd repeat |
| C | 3.8 kHz | 2.6 kHz | gritty, broken by the 3rd–4th repeat |
| D | follows the echo time | 0.6 × clock | B's 4.8 kHz at 0.4 s, × (0.4 / time)^0.35, within 2.5–12 kHz |

D's clock is 2.73 kHz at 2 s and 8.4 kHz at 80 ms. It follows the tape's gliding delay, so a division change swoops the grit too: at 100 bpm, 1/2 → 1/16 moves the clock 3.25 → 3.52 kHz half a second in, then 6.5 kHz.

**How aliasing is measured** (`test_echo_mode` "bbd"): a 1.7 kHz tone burst on the tape (0.6 s echo, feedback 0.8). On each repeat, the inharmonic energy in 0.3–5 kHz, Hann-windowed and skipping the tone's harmonics, relative to the tone. Repeats 2 / 3 / 4:

| | Repeats 2 / 3 / 4 |
|---|---|
| None (the measurement's floor) | −37.7 dB |
| Crushed | −21.8 dB (repeat 3) |
| A | −33.6 / −28.9 / −26.8 dB |
| B | −11.6 / −12.4 / −10.5 dB |
| C | −3.5 / −4.0 / −2.4 dB |
| D | −5.9 / −5.4 / −4.7 dB |

B is 9 dB more obvious than crushed was, C and D 16–18 dB more.

**Level per repeat** (broadband burst, vs no wear). No repeat is ever louder. After the first pass it holds within about 0.6 dB a pass on average; single steps wobble up to 1.7 dB in C with the aliasing and the pumping.

**Stuck tones:** judged with M6's Ringing and steady-tone flags on position 3 (click and burst, every ATTITUDE × DECAY 0.85/1 × TENSION 0/0.5):
- 0 of 20 cells flagged for every strength; worst ringing_db 9.5. The page's runaway rides read 3.6–11.2 dB.
- CLEAN DECAY 1 at a 1.2 s echo still fades (24–28 dB in 24 s).
- An aliasing loop can't hold a tone on its own: the hold is clocked and its images move with the material, and the compander and whine follow the signal's level.

**In the Tank, every strength:**
- Deterministic, and the same at block 48 and 333.
- No clicks or NaN, at TENSION 0.6 and 0.15.
- KICKED DECAY 1 stays bounded (peak 0.72) and falls 58–60 dB within 3 s of DECAY coming back to noon.
- With BBD A as the default, everything else still passes: the level fix's steps (noon −7.2 / −7.3 / −7.0 dB), ±2 dB against SPRINGS 2, and the 72-cell stability grid (least fade 20.9 dB).
- The diffuse round's checks now run with wear off, as that round was built. Light diffuse on top of BBD flagged 1 of 20 cells at 15.1 dB, just over M6's 15; revisit if the owner combines them.

**Cost as the default:**
- Firmware: release 113,492 B for A, B or C (+2,152 over no wear), 114,028 B for D (+536 more). Profile 115,392 B.
- AXI SRAM: 410 KB (release) / 421 KB (profile) of 512 KB.
- Desktop, SPRINGS 3 worst case: about +1.5 % over no wear for every strength (460 vs 453 ns/sample).
- On the chip: the per-sample sine (the whine) and three square roots are untested; the M3 profile run should include it.

**Page:** `renders/feat_echo_bbd/` (`tools/echo_bbd_page.sh`).

## BBD strength: A; bit-depth round (owner, 4 Oct 2026)

**Owner's pick on `renders/feat_echo_bbd/`: A, today, in every panel.** "B, C, and D introduce pitch artefacts … a higher pitched chirp is heard after the 2nd repeat." `kBbdDefault` stays 0. The firmware folds B–D and the time-following clock away (the strength is a constant there), so the release is unchanged by them. B–D stay renderable at no cost.

**Where the chirp came from:** two in-band pitched sources at B–D's low clocks.
- The hold's images (clock − f) of the rim's partials.
- The clock whine, now at the clock's own pitch (4.8 / 3.8 kHz, inside the band).

The new-peak check below reads 36.7 dB and 35.9 dB for B and C, against 11.8 dB for A.

**The whine in A** sits at 9.7 kHz, above the playback head. A probe on the output put the 9.7 kHz bin level with its 9.4 kHz neighbour, about 100 dB under the repeats: inaudible. It stays.

**Bit-depth round:** a hidden key `echo_bits_voicing` (default 0). It runs inside the feedback after BBD A (`core/dsp/EchoBits.h`; numbers in `EchoVoicing.h` "Bits"), so it compounds.

| | Version |
|---|---|
| A | none |
| B | "Sunny Tape", 24 kHz / 12-bit |
| C | "Scorched Cassette", 24 kHz / 8-bit |
| D | 24 kHz / 8-bit µ-law (µ 255) |

- **The rate:** 48 kHz → 24 kHz → 48 kHz through a 63-tap Blackman-windowed half-band low-pass both ways (cut at 12 kHz; half its taps are 0). Nothing folds back and no image stays in the band. Its fixed 62-sample delay comes off the tape and is given to the input, so the echo time holds.
- **The bits:** fixed point against full scale, so each quieter repeat keeps fewer bits. 8 bits run out at about −42 dBFS, 12 bits at about −66 dBFS.
- **Rounding:** to the nearest step while the signal is above 4 steps, so no level is lost. Toward zero below that, so a fading repeat always reaches silence: a loop with gain under 1 can't hold a value up, which rules out a limit cycle (a stuck buzz).
- **Dither:** TPDF, ±1 step, faded out with the signal below 4 steps. Undithered low-bit decays turn into tonal granulation, a pitched buzz on a fading tone. With dither the grain is hiss, and fading it out keeps the hiss from feeding itself round the loop.
- **Why D:** linear 8 bits cut the quiet repeats off early and leave a coarse hiss bed. µ-law keeps C's crunch on the loud repeats, where grit reads as texture, while the quiet ones keep going under a much finer grain (about 13-bit near silence). It's the companding of telephone audio and early digital delays.

**Checks** (`test_echo_mode` "bits"), on the tape (BBD A, a rim-like hit at −6 dBFS, 0.4 s echo, feedback 0.75):
- **Level per repeat** within ±0.1 dB of A for repeats 1–6 in B, C and D: no added energy, no loss.
- **Echo time:** within 0.6 ms of A's.
- **Pitched artefacts:** for each of repeats 2–6, the new narrow peak, meaning a bin standing above its ±⅓-octave median both in the repeat's own spectrum and relative to repeat 1's. Worst readings:

| | Worst new narrow peak |
|---|---|
| A | 11.8 dB (3150 Hz) |
| B | 9.9 dB |
| C | 11.3 dB |
| D | 10.1 dB |
| Rejected BBD B / C | 36.7 / 35.9 dB |

  So none of the bit versions adds a pitch beyond what A already has.
- **Tails:** a rim at CLEAN DECAY 0.85 and KICKED 0.8, 30 s, ends under −100 dBFS and keeps falling, with no M6 steady tone. One 8-bit KICKED tail read ringing_db 16.7 at 7.7 kHz. A probe put that bin level with its neighbours about 90 dB under the tail (the float floor, as the tail drops out in a few seconds), so the tail check reports Ringing without gating on it.
- **In the Tank:** deterministic, the same at block 48 and 333, no clicks or NaN. KICKED DECAY 1 stays bounded (peak 0.72) and falls 58–66 dB within 3 s of backing off.
- **M6** at position 3 (20 cells each): 0 flagged, no steady tone; worst ringing_db 9.4 / 11.6 / 11.2 / 10.2 for A–D.
- **The page's runaway rides:** 8-bit reads 12–13 dB at ~4 kHz, under the 15 dB flag. A probe found no peak there (the 3996 Hz bin is below its 3700 Hz neighbour).

**Seen along the way:** BBD A's compander gates the tail. Below about −56 dBFS the compressor's gain hits its cap and the expander keeps taking level, so quiet tails fall away faster than the feedback alone would make them. This is already in what the owner picked; noted in case long, quiet tails ever sound cut short.

**Cost as the default:**
- Firmware: release 115,068 B for B or C (+1,624 over BBD A's 113,444), 116,276 B for D (+2,832: µ-law's log and exp). Profile 117,000 / 118,232 B. AXI SRAM +2 KB.
- Desktop: SPRINGS 3 worst case about +15 % (≈560 vs 489 ns/sample); the half-band filters are ~34 multiply-adds a sample.
- On the chip: estimated 1.5–3 % of the budget, unmeasured; the M3 profile run should include it.

**Page:** `renders/feat_echo_bits/` (`tools/echo_bits_page.sh`). The judged `renders/feat_echo_bbd/` WAVs were deleted for room; its index remains.
