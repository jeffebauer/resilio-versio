# 0035 — The tank tames itself on held sounds (Sustain trim)

**Status:** Accepted (owner, 1 Oct 2026: B, the trim, in every panel of the listening page, plus the limiter hold). Retuned the same day on top of bipolar WOBBLE (ADR 0034), branch `proto/sustain-trim-2`: see "Round 2" below; the owner confirms by ear before it merges. Numbers: `core/params/DriveVoicing.h` "Sustain trim"; measurements: `docs/m8-tuning-backlog.md` "Sustain trim". SPEC changelog v1.0.23 (§4.8 output stage, §4.9 gain staging).

**Context:**
- On the Versio (release `b3e5ac3`, 1 Oct 2026) a low-mid synth pad (C minor, slow swell, energy mostly 90–250 Hz) turned the output LEDs red at mild settings: CLEAN, DRIVE 0, SPLASH 0, DECAY noon, TONE anywhere up to ~3:30, 2 or 3 Springs, TENSION past 3 o'clock. Input LEDs only touched amber. "It definitely sounds overdriven and sometimes a little harsh, as if I have the drive turned way up."
- Why: a held sound keeps adding to what the tank is still ringing with, so the wet ends up louder than the source (desktop, the owner's pad at −6 dBFS peak: wet +1 to +4 dB over the input's peak before the limiter). A tight tank adds a narrow bump at 120–180 Hz. The output limiter (knee 0.82 ≈ −1.7 dBFS) sits only ~4–5 dB above an amber input, and its red LED lights at 0.5 dB of gain reduction (ADR 0031).
- The Excitation trim (M8) fixes *which band* the input sits in, not *how long* it has been held, so it can't help.
- The owner picked "the tank tames itself on held sounds (hits keep their punch; only pads and drones get trimmed)" over "just turn the reverb down".

**Decision:**
1. **Only held sounds.** The input counts as held when its fast level (20 ms) stays within 6 dB of its slow level (0.3 s) for 0.3 s. Hits, rimshots and skank stabs fall away faster and are never touched: 02_hits, 04_skank and the Kick render bit for bit as before.
2. **The tank reads its own build-up.** While a sound is held, the Tank measures its build-up gain for that sound: the wet's peak level, where the limiter reads it, over what went into the springs (lagged the way the tank fills). That gain depends on the sound and the settings (TENSION's bump, SPRINGS, TONE, DECAY), not on the trim, so the trim that keeps the wet's peaks at **−7 dBFS** (5.3 dB under the limiter's knee) is read straight off it. No feedback loop, so it can't hunt or pump.
3. **DRIVE rides on top.** The level it reads is the springs' own (DRIVE's heard gain divided out, ADR 0033), so the target holds at DRIVE 0 and DRIVE still adds its few deliberate dB. At DRIVE above ~0.6 on hot held material the limiter can engage again, as ADR 0033 already says.
4. **It trims the springs' input, never the wet,** and only the part that would push past −7 dBFS: a held sound that stays under it is untouched. A ringing tail is never changed, so DECAY's tail length is the same and the Howl (which feeds itself) stays as loud.
5. **Smooth, and it lets go.** Down over 0.1 s (at long DECAYs no faster than half the tank's fill time), up over 2 s, and still while it is within 1 dB of where it should be. When the sound stops being held it lets go over 50 ms, so the next hit arrives at full strength. At most 12 dB.
6. **The limiter holds 30 ms** (owner, after the listen: limiting on a bass pad "sounded driven"). Its harshness on sustained lows was not the soft clip but its gain moving within each low cycle; its envelope now holds 30 ms (longer than a 40 Hz cycle), refreshed by any peak within 0.5 dB of it, then releases as before (`Tank.h` kLimitHoldS, kLimitHoldRefresh). Where it still engages (DRIVE up, hotter input, the Howl) it no longer rides the bass cycles.

**Testable:** `test_sustain_trim` (the pad, drone and organ at −6 dBFS peak, owner's settings, 27 SPRINGS × TONE × TENSION cells: limiter < 0.5 dB, i.e. no red LED; the drone's worst cell without the trim pulls ≥ 0.5 dB; hits and stabs never trimmed, every ATTITUDE and SPRINGS; lets go before the next hit; no pumping on a steady drone; the Howl as loud with a pad fed in). `test_drive` wet-vs-material spread at DECAY ≤ 0.5 still ≤ 3.5 dB (reads 3.0; `main` 2.1) and DRIVE's level curve unchanged. `test_m7_tank` WOBBLE pitch checks run with the trim off (see Consequences).

**Consequences to watch (owner, by ear):**
- Pads and drones at DAW-hot levels come back quieter in the wet than before, by as much as they used to overflow (loudest part of the hold, MIX 1: the synthetic pad at TENSION 0.8, 2 Springs ~1 dB, at TENSION 1 ~2.5 dB; the drone 4–5.5 dB; a pad that never reached the limiter is untouched). At MIX 1 that's what you hear; at MIX noon the dry covers most of it.
- The trim follows a pad's own slow beating by 2–4 dB (it sits still on a steady drone). Heard as pumping? The listening page has the pad and the drone.
- The first 0.3 s of a held sound passes like a hit (the organ's attack is the worst case: −1.9 dBFS, just under the knee).
- At DECAY max, held sounds are trimmed more (the tank builds more). The tail after the sound stops is as long as before, starting from the trimmed level.
- WOBBLE's Drift check (1 kHz held tone at −12 dBFS, DECAY 1, WOBBLE 0.5) reads 5.5–6 cents with the trim on (limit 5). Not the trim moving: the same check reads 5.5 on `main` with the tone 6 dB quieter (the Loop's quiet-tail fade). test_m7_tank now measures WOBBLE with the trim off and prints the trim-on value as INFO. (Round 2: `main`'s bipolar WOBBLE check replaced that one; it too measures with the trim off.)
- The limiter's gain ripple on sustained lows (drone, `main`: −30 dB of intermodulation, up to 0.6 dB per cycle) is what sounded "driven"; the soft clip adds nothing measurable (−150 dB). With the trim the limiter no longer engages at the owner's settings; a peak hold would help wherever it still does (DRIVE up, hotter input, the Howl).

## Round 2 (1 Oct 2026, on bipolar WOBBLE; branch `proto/sustain-trim-2`)

Merged with `main`'s bipolar WOBBLE (voicing D), round 1 failed 2 checks: the held organ's attack reached the limiter (1.49 dB; its first 0.3 s was let through like a hit, and WOBBLE's new movement at the default moved it ~1 dB), and the drone's settled trim moved 2.87 dB (limit 2). Run across WOBBLE 0 / 0.25 / default / 0.75 / 1 it was worse left of noon: up to 4.8 dB of limiting and 7.8 dB of trim movement. The cause: WOBBLE's Drift moves a held note on and off the tank's resonances, so the springs themselves swell and dip (a pure drone fully left: ~10 dB), and round 1 chased each swell. What changed (decisions 1, 2, 5 amended):

- **Held is recognised sooner when the level stops falling.** Besides round 1's rule, a sound whose level has stayed within 3 dB of its own peak for 80 ms counts as held. A snare, a rimshot or a skank chord has already fallen away by then (still exactly 0 dB of trim, every ATTITUDE and SPRINGS; 02_hits and 04_skank render bit for bit as `main`). **Trade-off:** a flat stab longer than ~80 ms (an organ bubble, a held synth stab) is now treated as held for the rest of its length, and the tank's share of it can be eased down if it would overflow. A drum or a plucked chord is never affected.
- **While a held sound arrives (its first 0.5 s), the trim is quick and careful:** it aims at −7 dBFS peaks and moves down over 30 ms, so the attack's first peaks are caught. Once settled it allows 2 dB more (−5 dBFS peaks, 3.3 dB under the knee), so held sounds keep about round 1's level.
- **The trim remembers the loudest swell.** It reads the tank's build-up as a high-water mark: the highest met while the sound is held, kept 3 s, then let down at 1 dB/s. So it answers the loudest swell once and sits still through the rest, instead of following each one down and back up (heard as pumping, or as the pad swelling). Settled, it moves only as far as needed: within ±1 dB it stays put; past that it moves to the edge of that band, not all the way to the target.
- **The build-up is read in step on both sides** (the wet's peak envelope over the input's envelope, both released alike, the input lagged a quarter of the tank's fill time): round 1 read it high while a sound was still arriving and too slowly on a sudden swell.

Measured (`test_sustain_trim`, synthetic pad / drone / organ at −6 dBFS peak, the owner's settings, 27 SPRINGS × TONE × TENSION cells per WOBBLE):

| WOBBLE | worst limiter pull, pad / drone / organ (round 1) | drone's settled trim movement (round 1) | pad's largest trim rise in the hold (round 1) |
|---|---|---|---|
| 0 (fully left) | 1.21 / 1.26 / 0.56 dB (1.90 / 2.40 / 1.69) | 1.46 dB (7.80) | 2.43 dB (9.02) |
| 0.25 | 0.93 / 0.43 / 1.44 (1.46 / 1.64 / 4.83) | 4.95 (6.69) | 3.01 (5.62) |
| 0.45 (default) | **0 / 0 / 0** (0.02 / 0 / 1.49) | **1.23** (2.87) | 1.28 (4.49) |
| 0.75 | 0 / 0 / 0 (0 / 0 / 0.07) | 0.16 (0) | 1.03 (3.04) |
| 1 (fully right) | 0 / 0 / 0 (0 / 0 / 0) | 0 (0) | 1.56 (2.92) |

`test_sustain_trim` checks the limit (< 0.5 dB) and the movement (≤ 2 dB) at the default and right of noon. **Left of noon it is printed, not checked:** there the springs' own swells are unforeseeable, so a later, louder swell than any met so far can still touch the limiter for a moment (worst 1.4 dB: the organ's attack at WOBBLE 0.25, 3 Springs, TENSION 1) and costs one more step down of the trim (worst 4.95 dB, the drone at 0.25: one step, not back and forth). Every left-side limiter number is lower than round 1's.

**Consequences to watch (round 2):**
- Held sounds sit a little lower than round 1 where the tank swells most (the owner's settings, renders of the stimulus WAVs, loudest 200 ms at MIX 1: drone within ±0.8 dB of round 1; pad 0.5–2.5 dB lower, most at TENSION 1 with 2 Springs; organ 0.5–3.1 dB lower, mostly because round 1's loudest moment was its untrimmed attack). The limiter is no longer reached in any of these renders (highest output peak −1.9 dBFS vs the knee's −1.7: the organ, 3 Springs, TENSION 1).
- After a big swell the trim holds for 3 s before easing back, so a held sound that gets quieter on its own (or a legato chord change to a quieter chord) stays trimmed that long, then comes up at 1 dB/s.
- `test_clicks`' held-chord limiter scan and `test_led_meter`'s "limiter pulls, LEDs red" case now run with the trim off: they need a held chord to reach the limiter, and the retuned trim keeps it under (0 of 31 TENSION settings reached it). What they test (clicks, the LEDs) is the limiter's, not the trim's.

**Why:** the owner's pick: hits keep their punch, and held sounds stop overflowing the tank. Trimming the input rather than the wet keeps every tail and the Howl exactly as they were.
