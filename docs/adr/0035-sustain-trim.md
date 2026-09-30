# 0035 — The tank tames itself on held sounds (Sustain trim)

**Status:** Proposed, 1 Oct 2026 (prototype on branch `proto/sustain-trim`, waiting for the owner's listen). Numbers: `core/params/DriveVoicing.h` "Sustain trim"; measurements: `docs/m8-tuning-backlog.md` "Sustain trim". On acceptance: SPEC changelog line (§4.8 output stage, §4.9 gain staging).

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
6. **The limiter stays as it is** (the prototype doesn't change it). Its harshness on sustained lows is not the soft clip but its gain moving within each low cycle; a 30 ms peak hold would clean that up by 15–20 dB (see Consequences). A separate decision.

**Testable:** `test_sustain_trim` (the pad, drone and organ at −6 dBFS peak, owner's settings, 27 SPRINGS × TONE × TENSION cells: limiter < 0.5 dB, i.e. no red LED; the drone's worst cell without the trim pulls ≥ 0.5 dB; hits and stabs never trimmed, every ATTITUDE and SPRINGS; lets go before the next hit; no pumping on a steady drone; the Howl as loud with a pad fed in). `test_drive` wet-vs-material spread at DECAY ≤ 0.5 still ≤ 3.5 dB (reads 3.0; `main` 2.1) and DRIVE's level curve unchanged. `test_m7_tank` WOBBLE pitch checks run with the trim off (see Consequences).

**Consequences to watch (owner, by ear):**
- Pads and drones at DAW-hot levels come back quieter in the wet than before, by as much as they used to overflow (loudest part of the hold, MIX 1: the synthetic pad at TENSION 0.8, 2 Springs ~1 dB, at TENSION 1 ~2.5 dB; the drone 4–5.5 dB; a pad that never reached the limiter is untouched). At MIX 1 that's what you hear; at MIX noon the dry covers most of it.
- The trim follows a pad's own slow beating by 2–4 dB (it sits still on a steady drone). Heard as pumping? The listening page has the pad and the drone.
- The first 0.3 s of a held sound passes like a hit (the organ's attack is the worst case: −1.9 dBFS, just under the knee).
- At DECAY max, held sounds are trimmed more (the tank builds more). The tail after the sound stops is as long as before, starting from the trimmed level.
- WOBBLE's Drift check (1 kHz held tone at −12 dBFS, DECAY 1, WOBBLE 0.5) reads 5.5–6 cents with the trim on (limit 5). Not the trim moving: the same check reads 5.5 on `main` with the tone 6 dB quieter (the Loop's quiet-tail fade). test_m7_tank now measures WOBBLE with the trim off and prints the trim-on value as INFO.
- The limiter's gain ripple on sustained lows (drone, `main`: −30 dB of intermodulation, up to 0.6 dB per cycle) is what sounded "driven"; the soft clip adds nothing measurable (−150 dB). With the trim the limiter no longer engages at the owner's settings; a peak hold would help wherever it still does (DRIVE up, hotter input, the Howl).

**Why:** the owner's pick: hits keep their punch, and held sounds stop overflowing the tank. Trimming the input rather than the wet keeps every tail and the Howl exactly as they were.
