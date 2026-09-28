# M6: calibrating the Ringing metric, and what the AntiRes layers do

28 Sep 2026. Code: `host/common/Metrics.cpp` (`ringingGrowth`, `howlStats`), `core/params/AntiRes.h`, `core/dsp/Spring.*`. Tests: `host/tests/test_antires.cpp`, `host/tests/test_metrics.cpp`. Grid: `presets/sweeps/m6_*.json`. Background: SPEC §4.10, §7 M6; ADR 0002, 0010, 0018, 0019, 0021.

## The short version

1. **The old number could not tell a spring from Ringing.** `resonance_peak_db` asks "is there a narrow peak in the tail's spectrum?". Every spring tank is full of narrow peaks: its own modes, the frequencies whose round trip fits a whole number of cycles. So 24 of the 45 real tanks in the IR library "failed" it (median 12.6 dB against a 12 dB limit), and our own renders read 15–24 dB.
2. **The new number asks the question Ringing actually poses: does one narrow frequency keep outliving the frequencies around it?** In a healthy tail every mode dies at about the same speed as its neighbours, however peaky the spectrum is. A Ringing mode dies slower, so it climbs out of its neighbourhood as the tail fades, until the tail is a single tone. The new metric, `ringing_db`, measures that climb in dB. Limit: **15 dB**.
3. **Results.** Real tanks: 38 of the 39 long enough to measure pass (median 6.8 dB, highest 14.7). The one exception, *Short Spring*, has a 387 Hz mode that dies 5× slower than its neighbours: worth a listen, it may really ring. Synthetic Ringing (a feedback loop with a small resonance inside, a sine under a tail, a self-oscillating loop): flagged whenever the ringing mode lasts about 1.7× its neighbours or more. Our Tank, whole M6 grid, both chirp directions: nothing flagged, highest 9.8 dB (13.1 with the chirp flipped).
4. **Howl** now has a concrete test (ADR 0019 made measurable). Before M6 the KICKED Howl was close to a steady tone (it failed). It now rides the Micro-mod hook harder so its pitch wanders, and passes everywhere except 3 dark-TONE cells, which miss the "broadband floor" number by up to 2.2 dB (see §4: the wording should change, not the sound).
5. **Micro-mod floor (layer 2)** is built: each Spring's length drifts by ±0.05 % at about 0.2 Hz, always on, WOBBLE 0 included. It adds **0.1 cent** of pitch movement to a held tone (inaudible). Honest finding: in our Tank it has nothing to fix, because layers 1 and 3 already leave no Ringing to break up, and it does **not** cure Ringing caused by a gain peak (§5.2).
6. **Layer 4 (adaptive suppressor) is not needed and not built** (ADR 0010).

## 1. Why a "peak" test fails on springs

A spring's tail is made of modes: one per frequency where the round trip is a whole number of cycles, spaced about 10–30 Hz apart. An FFT fine enough to see them (5.9 Hz bins here) shows a comb of narrow peaks. The 1/3-octave median sits down in the valleys between them, so the comb's teeth read 10–40 dB "above the median": that is the spring's structure, not Ringing. It also punishes inputs with tonal content: a snare's 185 Hz body puts a peak in every hits render. `resonance_peak_db` is kept in the sidecar (unchanged, for comparison) but no longer judges M6.

## 2. The Ringing metric

**Plain version.** Take the tail after the input has stopped. At each moment, compare every narrow frequency with its neighbourhood (the frequencies within a third of an octave). Follow that "how far does it stick out" number over time. If a frequency sticks out more and more as the tail fades, and keeps climbing into the second half of the tail, and it is not one of the fast-dying top bands, it is Ringing. `ringing_db` is how many dB it climbed. A steady tone that has already left its neighbourhood far behind (a self-oscillating loop) counts too: then `ringing_db` is how far it stands out, capped at 70 ("a bare tone").

**Definition** (`host/common/Metrics.cpp`, `ringingGrowth`). Mono downmix, segment = first event to the next (as T60).

| Step | What |
|---|---|
| Spectra | 8192-point Blackman-Harris frames (170 ms at 48 kHz), hop 2048. Power pooled over 3 bins (a tone's main lobe) and averaged over 0.35 s (evens out beating between close modes). dB. |
| Tail start | 4 frames + the smoothing half-width after the frame where the median bin level (100 Hz–10 kHz) peaks: the input has stopped. A median ignores any single tone. |
| Neighbourhood | Per bin and frame: median of the bins within ±1/6 octave (at least ±10 bins), leaving out the bin's own ±4. Prominence = bin level − neighbourhood. |
| Valid frames (per bin) | Neighbourhood within 80 dB of its own start and of the tail's typical start level, at least 15 dB above its own floor (so hum or dither in a recording's noise floor can't "grow"), and the bin not yet 70 dB clear (beyond that the reference is the bin's own window leakage). |
| Growth | Late half of the valid frames only. Robust (Theil-Sen) line through prominence vs time → dB gained over that span. Counted only if: the bin is a narrow peak (louder than the bins ±3 around it), ends ≥ 6 dB clear, **climbs steadily** (the medians of the late half's thirds rise in order, 1 dB slack: a bump is not growth), and **lasts** (the bin itself decays no faster than a T60 of half the tail's own T60). Needs ≥ 0.5 s of late half. |
| Steady tone | A bin ≥ 20 dB clear, within 40 dB of the tail's start level, decaying slower than 3 dB/s (T60 > 20 s) for ≥ 2 s. Score = its median prominence. |
| Result | `ringing_db` = largest score, 100 Hz–10 kHz, capped at 70. **`ringing` = `ringing_db` ≥ 15.** Also `ringing_hz`, `ringing_ratio` (neighbourhood decay rate ÷ the bin's, late half; 99 = not decaying), `ringing_end_db`, `ringing_span_s`. Null when no bin has 0.5 s of late half (files shorter than ~1.5 s). |

Why the late half: real tanks have an early phase where their mode peaks emerge from the initial broadband burst (the valleys between modes drain first), so every peak "grows" 10–20 dB and then decays with the rest. *Berlin Spring* at 451 Hz is the textbook case: +20 dB in the first 1.7 s, then flat. Ringing keeps climbing.

Why "lasts": in our Tank the top bands (5–9 kHz) die within a second at DECAY max, and the hand-over from the fast Loop decay to the slower high path made narrow components there read up to 16 dB of "growth" 60–100 dB down. They can't be heard as Ringing: they are gone while the tail's body still sounds.

**Robust to the input's tone** (criterion c): the metric only looks at how prominence *changes*, never at how big it is. On the M4 hits renders (the ones contaminated by the snare's 185 Hz body) `resonance_peak_db` reads 19.7 dB at DECAY max, `ringing_db` 4.3.

**steady_tone** (SPEC §4.10, unchanged definition) had a bug: it followed only the single most prominent bin per frame, so a steady tone whose harmonics are about as prominent (a saturated, self-oscillating loop) hopped between them and never built a 2 s run. Each bin now keeps its own run. It still flags 0 of the 45 real tanks and 0 of the 270 non-Howl grid cells.

## 3. Evidence

### 3.1 IR library (45 real tanks, ADR 0021)

| | `resonance_peak_db` (old) | `ringing_db` (new) |
|---|---|---|
| Measurable | 45 | 39 (6 files are 1.0–1.4 s long: too short) |
| Median | 12.6 dB | 6.8 dB |
| 90th percentile | ~20 dB | 13.5 dB |
| Max | 39.6 dB | 21.5 dB |
| Over the limit | 24 of 45 (> 12 dB) | **1 of 39** (≥ 15 dB) |

Distribution of `ringing_db`: 0–5 dB: 13, 5–10: 16, 10–12: 4, 12–15: 5, 15–20: 0, ≥ 20: 1. `steady_tone`: 0 of 45.

The highest readings, and why:

| IR | ringing_db | where | reading |
|---|---|---|---|
| hybrid *Short Spring* | **21.5** (flagged) | 387 Hz | One mode decays 5.4× slower than its neighbourhood: by 2 s it stands 60 dB clear. Listen: it probably does ring a little. A real tank can, it just isn't what we want by default. |
| convpack *Moist Diffused Spring Echo 3* R / L | 14.7 / 13.9 | 8.5 / 9.7 kHz | Processed (spring + echo taps): late echo repeats lift single frames. |
| convpack *BRX100 Loud* | 14.0 | 3.4 kHz | A mode 3× slower than its neighbours in a short (2.5 s) file. |
| hybrid *Berlin Spring* | 13.5 | 451 Hz | Early-phase emergence, then decays with the tail (see §2). |
| convpack *Swissecho RevR DelayL 1* | 13.4 | 3.0 kHz | Processed (layered with a delay). |
| convpack *SNRA500 Plucky* | 11.9 | 1.0 kHz | The "plucky" resonance. |

So real, non-ringing tanks reach **up to ~15 dB**; the limit sits just above that.

### 3.2 Synthetic cases

Built by `host/tests/test_metrics.cpp` (a subset, as tests) and a larger scratch set. "Loop" = a feedback delay loop, broadband T60 2 s, a one-pole damping at 6 kHz, tanh (slope 1 at rest), excited by a 50 ms noise burst, with an RBJ peaking resonance inside the loop. "Ratio" = how many times longer the ringing mode lasts than its neighbours, as measured.

| Case | ringing_db | ratio | Verdict |
|---|---|---|---|
| White noise, pinkish noise, decaying white noise (T60 3 s) | 0.0 | – | pass |
| Plain loops (no resonance), L 30/50/80/100 ms | 0.8–1.2 | 1.02–1.03 | pass |
| Dense modal tails (800–3000 modes, ±15 % T60 spread) | 4.4–9.7 | 1.1–1.2 | pass |
| Loop +1 dB, Q 8 at 1.1 kHz, L 80/100 ms | 7.4–9.8 | 1.23–1.33 | pass (mild) |
| Loop +1 dB, Q 20 at 300 Hz / 3 kHz, L 50–100 ms | 8.8–14.4 | 1.28–1.55 | pass (mild) |
| Loop +1 dB, Q 8, L 50 ms / Q 20, L 30–80 ms | 16.7–20.5 | 1.7–2.0 | **flagged** |
| Loop +2 dB, any L, Q 8 or 20, 300 Hz / 1.1 kHz / 3 kHz | 18.4–70 | ≥ 1.84 | **flagged** |
| Loop +3 dB (self-oscillating, steady tone + harmonics) | 26–70 | ≥ 3.5 | **flagged** |
| Long loops (T60 6 s), +0.3 / +0.6 / +1 dB, L 50/100 ms | 8.0 / 16.7–18.3 / 70 | 1.26 / 1.8 / 99 | pass / **flagged** / **flagged** |
| Sine at −20 dB under a decaying noise tail (440 Hz, 1.25 kHz) | 70 | ∞ | **flagged** |
| Sine under a modal tail | 70 | ∞ | **flagged** |
| One of our Tank tails + a slow 1.3 kHz mode (T60 30 s, −30 dB) (test_antires) | 70 | ∞ | **flagged** (bare tail: 7–10) |

The line falls at a mode lasting **about 1.6–1.7× its neighbours**. Milder differences (1.2–1.5×) are what real tanks and dense modal tails do on their own, so the metric can't call them Ringing without failing real springs.

### 3.3 Our renders

See §6 for the full M6 grid. Every non-Howl cell passes, highest 9.8 dB (negative `a`) and 13.1 dB (flipped `a`); median about 4 dB for noise bursts and 0 for single clicks (a click's tail has no narrow peak that qualifies at all). test_antires' own Tank tails (click + noise burst, every ATTITUDE × SPRINGS, DECAY max, BOING 1): highest 5.6 dB (7.7 flipped).

### 3.4 The Wellspring's delay-Ringing take (take G)

**Pending.** `test_audio/reference/` is empty. When `wellspring_G_*.wav` arrives: `build/rv_render --analyze test_audio/reference/wellspring_G_<desc>.wav` must print `ringing=…(RINGING)` or `steady=true`. It should: a BBD delay ringing tone is either a slow mode (growth) or a sustained tone (the steady-tone clause), both of which the synthetic cases cover.

## 4. The Howl criterion (ADR 0019, KICKED Howl zone only)

In the Howl zone (KICKED, DECAY ≥ 0.9) the Tank is *meant* to sustain and may lean to a pitch, so the Ringing test doesn't apply there (the Howl sweeps list `ringing` and `steady_tone` in `ignore_flags`). Instead, over 1 s after the first event to the end of the file:

- **`howl_floor_db`** (broadband floor): 1/3-octave band energies 200 Hz–5 kHz of the average spectrum (16384-point Hann); median band minus strongest band. Pass ≥ **−25 dB** (a bare sine reads about −100).
- **`howl_move_pct` / `howl_move_db`** (movement): the strongest peak (100 Hz–8 kHz) every 0.25 s, frequency by parabolic interpolation. In every 2 s window where that peak is audible (> −30 dBFS) it must move ≥ **0.5 %** in frequency or ≥ **3 dB** in level. The sidecar reports the *steadiest* window.
- **`howl_ok`** = both.

Kicked Howl vs Ringing, in one line: Ringing is a steady, pure tone; a Howl is allowed a pitch as long as it has a noisy floor under it and never holds still for 2 s.

**Before M6** the Howl failed: a self-oscillating Loop settles on one resonance and stays there. The M6 grid measured 0.04–0.1 % median movement with noise-burst input (8 of 27 cells passed). The Micro-mod floor alone (0.05 %) doesn't change that (5 of 27). **Now** the Howl rides the same modulation hook harder, scaled by the Howl amount (`core/params/AntiRes.h`): a slow sine of ±0.6 % of L (0.35 Hz for Spring A, ×1.27 and ×0.83 for B and C, so they beat) plus ±0.3 % extra random drift. That is roughly ±5–10 cents of siren-like wander. Grid result, negative `a`: noise bursts 27/27 pass, single click 24/27. The 3 failures are KICKED DECAY 1 **TONE 0 BOING 0** (1, 2 and 3 Springs): movement passes, the floor reads −26.5 to −27.2 dB. With flipped `a`, 18/27 click cells pass; the 9 failures are all TONE 0, floor −28.8 to −30.9. At TONE 0 the Loop's damping sits at 1.6 kHz, so the bands above it are simply darker: the median band drops, although the Howl is plainly not a bare sine. **Proposed wording change** (lead): measure the floor over 200 Hz–2 kHz (the band TONE never darkens much), or loosen it to −32 dB. **Listen:** the Howl movement is an audible design change, owner to judge at the next listening pass (depths in `AntiRes.h`).

## 5. The AntiRes layers

### 5.1 Layer 1: even Loop gain (`test_antires` "evenness")

Per Spring, small signal, at every ATTITUDE × TONE {0, ½, 1} × BOING {0, ½, 1} × DECAY {0, ½, ¾, 0.89, 1} corner outside the Howl zone (378 Spring cells):

| Check | Negative `a` (current) | Flipped `a` (+0.35…+0.70) |
|---|---|---|
| Per-trip gain g·\|H(f)\| max, 30 Hz–24 kHz | 0.934 | 0.938 |
| Largest per-trip bump over its own 1/3-octave median | 0.006 dB | 0.006 dB |
| Longest T60(f) ÷ design T60 | 1.021 | 1.011 (was **1.19** before the fix below) |
| Largest T60 bump over its 1/3-octave median, DECAY ≥ 0.5 | ×1.010 | ×1.145 |
| Same, DECAY < 0.5 (reported only) | ×1.003 | ×1.48 (DECAY 0, BOING 1) |

The per-trip response is a product of monotonic filters (DC blocker, Butterworth at fC, one-pole damping), so it has one broad hump and no narrow peak anywhere, including near fC and the damping corner. What matters for Ringing, though, is the decay *per second*: per-trip loss divided by round-trip time. With negative `a` the round trip is longest at the lows, inside the design band, so no band outlasts the design. **With positive `a` (highs later, as real tanks) the round trip peaks at fC**, outside the old design band (70 Hz–1.2 kHz), and the band just under fC rang up to 1.19× longer than DECAY asks. **Fix (sign-independent):** the g design also checks 0.6, 0.75, 0.85, 0.92 and 1.0 × fC (`Spring.cpp` `kDesignFcRatios`). With negative `a` those points never set g (0 of 378 cells), so today's output is bit-identical. With positive `a` they bind in 9 cells. What remains with positive `a` is a *relative* bump: the Chirp's top edge outlasting its neighbours by up to 1.15× at DECAY 0.5 and 1.48× at DECAY 0 / BOING 1 (a 0.6 s "ping" in a 0.4 s slap). That is a "ping", not Ringing, and the metric agrees, but it is worth an ear if the owner flips the sign.

### 5.2 Layer 2: the Micro-mod floor

**Design** (`core/params/AntiRes.h`, `Spring::advanceModulation`). Per Spring: a new uniform random target every 0.6 s, smoothed by two one-pole low-passes with that time constant (a glide, never a step), scaled so its peaks are ±1, times the depth, **0.05 % of L**. Measured over 120 s: peak 0.049 % of L, ~0.23 Hz, largest change 1.5·10⁻⁸ of L per sample, correlation between Springs −0.14 / −0.03 / −0.09 (independent). Seeded per Spring, restarted by `Tank::reset()`; block sizes 1/7/48/512 are bit-identical, and Plugin == Renderer still holds. The Loop's reads (feedback and pickup tap) use L·(1 + m); the high path is left unmodulated, because at 8–9 kHz the linear interpolator's loss depends on the fractional delay (0 to −1.2 dB per trip), so a drifting fraction would make the HF decay rate wobble. The Loop is dark above fC, where that loss stays under 0.3 dB.

**Hook for WOBBLE (M7, ADR 0008):** `SpringSettings::modDepth` is the total random depth (the Tank sets it to the floor; WOBBLE adds on top), `lfoDepth` / `lfoHz` the sine part (the Howl uses it now, WOBBLE's LFO later).

**Inaudible on held chords** (`test_antires` "pitch", `08_held_tones.wav`, WOBBLE 0, wet only). Pitch of each partial by complex demodulation (200 ms windows every 20 ms). Estimator floor on the dry input: 0.03 cents.

| | p95 pitch deviation |
|---|---|
| One Spring, floor off → on, DECAY 0.5 | 0.04 → 0.15 cents |
| One Spring, floor off → on, DECAY 1 | 1.9–2.8 → 2.0–2.5 cents (no change within noise) |
| **Added by the floor, worst case** | **0.11 cents** (limit 1) |
| Whole Tank, DECAY 0.5, any SPRINGS / ATTITUDE | 0.1–0.2 cents |
| Whole Tank, DECAY 1 | 1.8–2.5 cents (limit 3) |

The DECAY 1 figure is there with or without the floor: a held tone through a 9 s tail isn't a pure tone yet after a few seconds, because the onset's free-ringing modes beat against it, and a phase-based estimate reads that as a couple of cents of wobble. **Threshold justification:** the smallest pitch change listeners notice on a sustained tone is about 5 cents, and slow vibrato is detected at about 3–5 cents. The floor's own contribution (0.1 cent) is 50× below that. The whole-Tank limit of 3 cents is below the ~5 cent threshold. ADR 0008's "Drift" (WOBBLE's lower half, "felt more than heard") must sit well above this floor, so the floor leaves it all the room.

**What it does, honestly.**
- *Gain-peak Ringing:* no effect. The same synthetic loops with the floor at 0, 0.05 % and 0.5 % read within ±1 dB (e.g. +2 dB Q 20 at L 100 ms: 23.0 / 23.0 / 23.5 dB). A resonance in the Loop's filters boosts whichever mode sits under it; moving the modes by 0.05 % (≈1 Hz at 1 kHz) doesn't move them out from under a 50 Hz-wide peak. Layer 1 is the defence against that, and it holds.
- *Mode lock-in:* this is what it is for (coincident modes, self-oscillation). In our Tank today nothing locks in outside the Howl: the Loop is linear and smooth, and the Springs are detuned. The grid's `ringing_db` median is unchanged with the floor on; per-cell values move ±5 dB either way (the metric's cell-to-cell noise at these low levels).
- *Comb peakiness:* `resonance_peak_db` medians drop 0.4–1.1 dB with the floor on (the modes smear slightly).
- *Howl:* the floor alone is far too small to make the Howl move (above). The Howl zone uses the same hook at ~12–20× the depth.

So the floor is cheap insurance, kept because SPEC and ADR 0010 ask for it, and the carrier for WOBBLE and the Howl movement. It is not what keeps today's Tank clean: layers 1 and 3 are.

### 5.3 Layer 3: Spring detuning (M4)

Effective. The Springs don't share a Loop, so shared modes could only add, never build up. The grid shows no penalty for more Springs: noise-burst medians are about the same for 1, 2 and 3 Springs, and the highest cells are spread across modes.

### 5.4 Layer 5: LoopSat (M5)

Unchanged. Slope ≤ 1, so it can only lower Loop gain. In the Howl zone it is what holds the level (peak 0.72–0.82, below the 0.89 limiter, test_drive).

### 5.5 Layer 4: adaptive suppressor

**Not built.** No grid cell fails after layers 1–3 (+5), in either chirp direction (ADR 0010). The CPU and flash stay free for M7.

## 6. M6 grid

`presets/sweeps/m6_{click,bursts}_{ringing_d075,ringing_d1,howl}.json`: `07_click_single.wav` (+8 s tail) and `06_noise_bursts.wav` (+2 s; the metric uses the first 50 ms burst's tail), SPRINGS × BOING {0, ½, 1} × TONE {0, ½, 1}, WOBBLE 0, DRIVE ½, MIX 1. `ringing_d075`: all three ATTITUDEs at DECAY 0.75. `ringing_d1`: CLEAN and DRIVEN at DECAY 1. `howl`: KICKED at DECAY 1 (the Howl zone, judged by §4). 162 cells per input, 324 renders. Render: `build/rv_render --sweep presets/sweeps/m6_<…>.json --out-dir renders/m6_<…>`.

| Sweep (cells) | Negative `a` (committed) | Flipped `a` (+0.35…+0.70, scratch build) |
|---|---|---|
| click, DECAY 0.75, all ATTITUDE (81) | max 5.8, median 0 | max 0.0 |
| click, DECAY 1, CLEAN/DRIVEN (54) | max 0.0 | max 0.0 |
| bursts, DECAY 0.75, all ATTITUDE (81) | max 9.8 (627 Hz), median 3.8 | max 13.1 (1.5 kHz), median 4.1 |
| bursts, DECAY 1, CLEAN/DRIVEN (54) | max 5.5, median 3.6 | max 6.3, median 4.1 |
| **Ringing, all 270** | **0 flagged**, `steady_tone` 0 | **0 flagged**, `steady_tone` 0 |
| click Howl, KICKED DECAY 1 (27) | 24 / 27 `howl_ok` | 18 / 27 |
| bursts Howl, KICKED DECAY 1 (27) | 27 / 27 | 27 / 27 |

No NaN/Inf or clipping in any render. The flipped build also fails, as expected, only the tests that check the chirp *direction* itself (test_spring's Chirp and Mappings checks, test_drive's TONE-0 chirp check); every AntiRes, stability, Howl and determinism check passes.

## 7. Leaving the Howl

- **Pulling DECAY** from 1 to 0.75 (test_antires, KICKED, DRIVE ½, click): the level drops 43.7–44.2 dB within 3 s for 1, 2 and 3 Springs (44.6–45.7 with the chirp flipped) (ADR 0018 asks ≥ 30). test_drive's M5 check (DRIVE 0.8, snare, to 0.7): 45 dB.
- **Flipping ATTITUDE** KICKED → DRIVEN or → CLEAN at DECAY 1 (open owner decision, not changed): the Howl stops at once, and what's left then fades at DECAY 1's normal ~9 s T60: 18–19 dB down after 3 s, 37 after 6 s, 55–56 after 9 s. That is a smooth fall, not the ~1–2 s ADR 0018 describes and not ≥ 30 dB within 3 s. If the owner wants the ADR 0018 behaviour for this exit too, the ATTITUDE Morph would need to shorten T60 briefly, or cap DECAY outside KICKED.

## 8. Limits, and SPEC wording to change

- The metric needs about 1.5 s of tail (0.5 s of late half): 6 of the IR files are too short. It is blind below DECAY ~0.2 for the same reason; the M6 grid (DECAY ¾ and 1) is well inside its range.
- It can't call a mode "Ringing" until it lasts about 1.6–1.7× its neighbours: real tanks do up to ~1.5× on their own.
- Per-cell values wobble by several dB. Our Tank sits around 4 dB (highest 9.8, or 13.1 flipped), real tanks up to 14.7, flagged synthetic cases from 16.7. If a future change pushes Tank cells toward 15, look at the flagged frequency and listen before concluding.

**Proposed SPEC §4.10 / §7 M6 wording** (lead to apply):
- Replace "no narrowband peak > 12 dB above median of 1/3-octave-smoothed spectrum" with: **"`ringing_db` < 15 dB: no narrow peak keeps climbing more than 15 dB out of its 1/3-octave neighbourhood over the late half of the tail, and no steady tone stands ≥ 20 dB clear for > 2 s (docs/m6-metric-calibration.md)."** Keep "no sustained sinusoid (> 2 s) above −30 dBFS" (`steady_tone`) as is.
- The IR library is the "must pass" reference (≥ 38 of 39 measurable real tanks), the synthetic Ringing loops and take G the "must flag" ones.
- ADR 0019's floor: measure 200 Hz–2 kHz, or −32 dB, so dark TONE settings aren't penalised (§4).
- "Micro-mod floor starting ~0.05–0.1 % of L": now 0.05 % at ~0.2 Hz; the listening check stays.

## 9. Cost

- **CPU** (same method as M1/M4/M5: desktop ns/sample × 15–25 × 0.48): the modulation costs ~8 ns/sample for 3 Springs, **~60–95 Daisy cycles/sample (≈ 1 % of the 10k budget)**. Whole Tank, worst case (KICKED, 3 Springs, max DECAY/BOING/TONE/DRIVE): 480–483 → 491–496 ns/sample, i.e. about 5.8k → 5.9k cycles at the pessimistic end (59 %, target ≤ 65 %). The delay reads were already fractional, so there is no new interpolation cost.
- **Flash:** +808 B per variant: release 108,940 B, m0test 120,916 B, profile 120,188 B (was 108,132 / 120,108 / 119,380; limit 131,072).
- **RAM:** a few floats per Spring (Spring object still < 2 kB); no pool change.

## 10. Reproduce

```
cmake --build build && ctest --test-dir build --output-on-failure
build/rv_test_antires            # layers, pitch, Howl, exits, CPU
build/rv_render --analyze renders/ir_library/<ir>.wav   # ringing= in the summary line
for f in presets/sweeps/m6_*.json; do build/rv_render --sweep $f --out-dir renders/$(basename $f .json); done
```

Sidecar keys added (existing keys and format unchanged): `ringing_db`, `ringing_hz`, `ringing_ratio`, `ringing_end_db`, `ringing_span_s`, `ringing`, `howl_floor_db`, `howl_move_pct`, `howl_move_db`, `howl_ok`. The review page (tools/review, not changed here) doesn't flag them yet.
