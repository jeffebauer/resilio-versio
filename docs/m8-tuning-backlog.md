# M8 tuning backlog

Owner listening to the M7 plugin build (commit `81f8124`) in Ableton, 28 Sep 2026, plus measurements. Each item: what the owner heard, why, target, fix direction. The M8 sweet-spot report (`docs/m8-sweetspot.md`) adds numbers per knob.

## 1. BOING changes decay length (closed: highs-later + TENSION, ADR 0024/0026)
- **Heard:** at 0 the reverb rings longer; at 1 there's a slight pitch envelope (the chirp, intended) and it decays faster (not intended).
- **Measured:** T60 at DECAY 0.5: 1.92 s (BOING 0) vs 1.72 s (BOING 1), about −10%; perceived shorter still because smeared echoes are less distinct.
- **Target:** BOING changes the chirp only. T60 within ±5% across BOING at every DECAY.
- **Fix direction:** g design uses the full-band round trip, including the chirp's group delay, per BOING; check the band used for "smallest g". Coordinate with the chirp-direction switch (task 7).

## 2. SPLASH inaudible (0 vs 1)
- **Heard:** no difference.
- **Why:** the owner's rimshot (−9 dBTP) would trigger the hit detector at roughly 3/4 strength, so it's mainly that **the crash is too quiet in the tank**: Clatter enters the quiet high path (DRIVEN −8 dB vs the hit's own 1–6 kHz). CLEAN is designed as a tiny HF lift (a dead zone; owner decision pending, TASKS 8b).
- **Target:** KICKED SPLASH 1 unmistakable on a snare at any sensible level; DRIVEN clearly audible; CLEAN a subtle but audible sparkle; ghost notes still barely trigger.
- **Fix direction:** level-adaptive hit detection (relative to a slow program-level tracker), more Clatter/Jolt level, possibly a Clatter share straight to the wet bus.

## 3. DRIVE subtle, even KICKED at max
- **Heard:** subtle at extreme settings.
- **Why:** calibrated for Eurorack level (10 Vpp ≈ −4 dBFS). DAW tracks are typically 10–15 dB lower, so the plugin gets far less drive than the module will. The springs also smear input distortion.
- **Target:** ADR 0022 as heard in the plugin, not just measured on −6 dBFS test hits.
- **Owner's test material (28 Sep):** a rimshot one-shot, −9 dBTP true peak, −22.9 LUFS integrated. That's only 3–5 dB under the calibration level, so level is a small factor. A short one-shot is the hardest material to hear drive on: the input drive only colours the transient; the tail only carries LoopSat.
- **Decided:** **no plugin-only INPUT control.** Calibrate with a Utility upstream in Ableton, aiming for peaks around **−4 dBFS** (≈ a 10 Vpp modular signal). This keeps Plugin/module parity ([ableton-setup.md](ableton-setup.md)).
- **Fix direction:** more drive that survives into the tail, especially in KICKED (LoopSat push, DriveOut), so it's audible on short hits too. Judge on hits *and* sustained material.

## 4. WOBBLE inaudible, even at 1
- **Heard:** can't hear it.
- **Why:** WOBBLE only modulates the Loop, so pitch movement builds over repeats (≈43 cents on a held note at DECAY noon, ~12 cents per pass). On drums or short DECAY it barely develops. Tape wobble moves the first echo too.
- **Target:** Warble audible on the first echoes at WOBBLE ≥ 0.75, even on drums; Drift still subtle in the lower half (ADR 0008). Depth calibrated to the Magneto WOW & FLUTTER series (ADR 0020).
- **Fix direction:** add wobble to the early part (the pickup taps / first pass), not only the Loop; retune the depth curve.

## 5. SPRINGS: 1 narrow, 3 flams
- **Heard:** 1 Spring fairly mono/centred; 2 spacious (good); 3 as wide, but with a flam: the third spring seems to arrive after the first two.
- **Why:** the M4 stereo fix staggered each Spring's pickup tap, so first echoes arrive at roughly 7 / 19 / 30 ms. A ~20 ms spread between first arrivals is heard as a flam. 1 Spring is deliberately the narrowest mode (a single tank is mono).
- **Target:** no audible flam in any mode (first-arrival spread ≤ ~8 ms), M4 stereo checks still pass (correlation < 0.5, mono-safe); 1 Spring a little wider while mono-safe.
- **Fix direction:** shrink the tap stagger; recover early decorrelation via detuning and short diffusion; revisit 1-Spring side level.

## 6. From the sweet-spot sweeps ([m8-sweetspot.md](m8-sweetspot.md))
- **SPLASH in CLEAN is a dead zone across the whole knob.** SPEC §4.5 designed CLEAN's SPLASH as "mild HF emphasis only", which measures as no change, against §2.3's "every knob usable". **Decided (ADR 0025):** CLEAN gets a real, gentler splash: light Clatter + tiny Jolt. **Done (29 Sep):** CLEAN's crash on a rimshot at −18..−3 dBFS is +4.6..+5.2 dB (DRIVEN +9.3..+10.4, KICKED +13..+15), −6 dBFS snare +2.2 dB; lurch 0.06 % of L (DRIVEN 0.20 %); ghosts −34 dB; nothing at SPLASH 0; sweet-spot sweep finds no dead zone. HF emphasis removed. Listen: `renders/clean_splash/`.
- **DRIVE dead patches:** CLEAN 0–0.4 and 0.5–1, DRIVEN 0–0.3 (confirms item 3).
- **Wet level varies 5–8 dB with material** (outside the Howl zone): broadband and low-heavy material excites the tank more than narrow bright tones. Suggested: a slow (~300 ms) level follower on the tank's excitation band with a gentle inverse trim.
- **BOING** has the smallest step-to-step change of any knob (no dead zone, but a narrow range). Revisit with the chirp-direction switch.
- **Expected, not bugs:** DECAY 0.9 → 1 in KICKED (the Howl zone, ADR 0002); MIX 0 → 0.1 (a measurement artefact).

## Round 1 status (28 Sep 2026, commit `a4fb02e`)
- **Done, awaiting the owner's ears:** 5 (flam: first-arrival spread 23–77 → ~1 ms; 1 Spring correlation 0.43 → 0.28), 4 (WOBBLE on first echoes: ~25 cents on a snare at 1), 2 (SPLASH DRIVEN/KICKED: +6–7 / +10–12 dB crash on a rimshot at −18…−3 dBFS; level-adaptive Hit), 3 (DRIVE: no sweet-spot dead zones), 6-gain (wet spread 2.5–4.4 dB at DECAY ≤ 0.75; still ~8 dB at DECAY 1, held sines between long-loop modes).
- **Merge fixes:** CLEAN pickup outK 4.5 → 4.0; kMorphSeconds 30 → 40 ms.
- **Waiting on owner decisions:** 1 (BOING/decay, chirp direction, TENSION), SPLASH in CLEAN.
- **Watch:** CPU worst case est. 65–66% (target 65%); WOBBLE 1 may be too deep on held notes; held-chord notes can lean L/R.

## HighsLater re-tune (29 Sep 2026, ADR 0024)
`kChirpDirection = HighsLater` on top of round 1. Four checks failed at the flip; what each turned out to be:
- **Wet-level spread (7.5 dB at DECAY 0.5):** not the excitation band. Hits, skank and pink noise sit within 1.2–1.8 dB of each other in HighsLater; the outlier was 08_held_tones, whose four fixed sines landed in dips between the Loop's modes (peaks and dips ~16 Hz apart at 1 kHz). The same notes 12 cents sharp / flat read +1.3 / +8.5 dB instead of −1.7; LowsLater swung as much, it had just landed on peaks at DECAY ≤ 0.5. test_drive now plays the held notes at 12 pitches across a whole tone and uses their power average: spread 2.2–3.2 dB at DECAY ≤ 0.5 (limit 3.5), 1.4–3.3 at 0.75/1 (KICKED DECAY 1 is the Howl).
- **Aliasing at DRIVE 1 (−53 / −54 dB):** real, from the LoopSat. A loud high tone leaks through the Loop's fC low-pass, the pushed LoopSat squares it off, and a high harmonic folds back to 1 kHz, where the Loop rings. The LoopSat now saturates "on flux" like the two transducers: a 2 kHz / 12 dB shelf cuts highs going in and its exact inverse restores them after (small signals unchanged: same Loop gain, T60, round trip; DRIVE nulls on 02_hits unchanged to 0.1 dB). KICKED −64.9 dB, DRIVEN −61.0 (the rest there is the tape pre-emphasis in DriveIn, a 1 dB margin).
- **KICKED DRIVE level on steady noise (2.2 dB):** the flux shelf brought it to 2.0; KICKED wMk 1.3 → 1.6 dB: 1.7 dB (hits: 0.8 dB across DRIVE, unchanged).
- **Micro-mod floor on held tones (whole Tank 4.9 cents):** a measurement artefact. 2 s into an 8 s note at DECAY 1 (T60 9 s) the onset's own modes are only ~13 dB down and beat against the note. Same reading with the floor switched off (3.9) and with SPLASH off; on a long-held chord it dies away with the tail (9 → 0.2 cents over 6 s) with or without the floor. test_antires now measures from max(2 s, ⅔ T60) into each note: p95 ≤ 0.8 cents. Still has teeth: a 10× floor reads 7.6 (fails).
- **Also:** HighsLater wet trim 0.89 → 0.88 (the MIX sweep sat exactly on its ±1.5 dB limit).
- **Rechecked in HighsLater:** full suite passes; first-arrival spread ≤ 5.1 ms (limit 8); 1 Spring correlation 0.28–0.29; SPLASH crash on a rimshot −18…−3 dBFS: DRIVEN +9.3…+10.4 dB, KICKED +13.0…+14.9; no DRIVE or BOING dead zones (sweetspot.py on `renders/m8_hl/`; SPLASH CLEAN is ADR 0025's); M6 grid 270 cells, 0 Ringing, worst `ringing_db` 9.1, no steady_tone; T60 within 2.3 % across BOING at every DECAY (LowsLater: up to 11 %), which closes item 1's decay half.
- **Cost:** the LoopSat shelves add ~10 ns/sample on the desktop (six first-order filters): test_drive's worst-case estimate (KICKED, 3 Springs) goes from ~6,390 to ~6,520 Daisy cycles (64 → 65 %, target 65 %; the upper end of the estimate). Flash: release 103,208 B (+352), profile 113,596 B (+360), of 131,072.
- **LowsLater** still compiles and its 14 core tests pass (not re-tuned).
- **Listening:** `renders/m8_hl/` (BOING, SPLASH, DRIVE sweeps × ATTITUDE; chirp BOING sweep, click + hits).
