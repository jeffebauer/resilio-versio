# 0040 — The Hold: CLEAN and DRIVEN hold at the top of DECAY, ducked

**Status:** Proposed, 4 Oct 2026 (branch `feat/throw-hold`; owner's call in the dub-lens critique, `docs/research/dub-lens-critique.md` §8: "hold at DECAY's top, CLEAN/DRIVEN, ducked; Howl stays KICKED-only"). Two voicings for the owner to pick by ear: `renders/feat_throw_hold/hold/index.html`. Amends ADR 0001 ("always fades") for CLEAN and DRIVEN above DECAY 0.9. KICKED's Howl (ADR 0002, 0018, 0019) unchanged.

**Context:**
- Dub techno breakdowns and siren beds want a tail that stays: turn DECAY up and the last chord sits under the next 16 or 32 bars. Today DECAY max is a 9 s T60 in every ATTITUDE, and KICKED's Howl (the only way to sustain) is noisy and crashing by design.
- A bed that sits at full level under new playing turns to mud. Dub engineers ride the return down while the band plays; the module should do that itself.
- Two ways to treat new input once the bed holds: keep it out (a frozen bed, new hits dry on top) or let it in quieter (the bed keeps growing with the music). Which one is a musical choice for the owner.

**Decision:**
- **Where:** DECAY 0.9 → 1 (the same range as KICKED's Howl zone), weighted by the CLEAN + DRIVEN Morph weight, so it fades out exactly as the Morph reaches KICKED. Below 0.9 and in KICKED everything is bit for bit as before.
- **T60:** glides in log time (smoothstep weight) from the plain DECAY curve (6.6 s at 0.9, continuous there) to **240 s** at DECAY 1 at the Loop's design points (≈ 33 s at 0.95, 84 s at 0.97). The Spring's Loop-gain cap moves with the zone weight from 0.995 to a **peak per-trip gain of 0.998**: always under 1, so it never self-oscillates (unlike the Howl); the margin covers the peak between the design points (a dense sweep reads ≤ ~0.9993). Heard at DECAY 1: a thrown hit loses ~7 dB over 5–20 s while the damping's highs go, then ~3 dB per 10 s (≈ 190 s). The high path (fast, undispersed echoes) keeps the plain DECAY's T60, so the held wash is the Loop's dispersed sound, not a metallic comb.
- **The bed** (freeze or layer, and the ducking) eases in over the zone's first half and is fully in from DECAY 0.95.
- **Ducking:** a peak follower on the dry input (6 ms attack, 300 ms release); −48 dBFS and below = no dip, −30 dBFS and above = the full **12 dB**, linear in dB between, × the bed weight. Applied to the wet after the limiter, so the bed comes back between phrases at its own level.
- **Voicings** (Renderer `hold_voicing`; firmware and Plugin build the default only):
  - **A "freeze"** (default): the send closes; nothing new gets in and new hits are heard dry over the ducked bed. **An open Throw overrides the freeze** (ADR 0039): the gate is how a chord gets thrown into a frozen bed. Leaving the zone reopens the send.
  - **B "layer":** new input still gets in, 6 dB down, and builds into the ducked bed.
- **No bed keeper.** Voicing B was measured for creep before adding one (the plan: only if > 1 dB per 10 s): a sustained pad for 60 s reads −0.07 to +0.03 dB per 10 s from 20 s on, at −3 and −12 dBFS, CLEAN and DRIVEN (`docs/prototypes/throw-hold/creep.sh`; `test_throw_hold` reports it too). The LoopSat, the Sustain trim and the limiter already hold it. None needed.

**Testable:** `test_throw_hold`: the zone's weight (CLEAN / DRIVEN = zone, KICKED 0, below 0.9 0); T60 rising and continuous (largest step ×1.055 per 0.001 of DECAY); a hit at DECAY 1, every SPRINGS, CLEAN and DRIVEN, loses 0.5–8 dB from 5 s to 20 s, never grows over 40 s, stays under the limiter; ducking ≥ 8 dB under new input, back within 1 dB 1–1.5 s after it stops; freeze leaves the bed untouched by new input (−300 dB) and a throw gets in; layer creep reported. `test_drive`: per-trip Loop gain < 1 at every frequency including CLEAN / DRIVEN DECAY 1; the stability grid at DECAY 1 in the layer voicing never grows (judged from 3 s, after the ducking has let go of the bed); the KICKED → DRIVEN hand-over stays bounded (+1.2 dB at most as the LoopSat lets go, limit +3, then never grows). Other suites' CLEAN / DRIVEN DECAY 1 cells moved to 0.9 (the plain curve's top below the Hold): `test_antires` (evenness, pitch on held tones, tank tails, the ringing metric's control), `test_spring` (T60 at 0.9 within 15 %; stability), `test_tank` (stability; the DECAY sweep goes 0 → 0.9: entering the zone while playing ducks the wet on purpose), `test_m7_tank` (WOBBLE on held tones), `test_springs3` (SPRINGS 3 vs 2 level, width, Ringing tails).

**Consequences:**
- **Open owner question:** flipping ATTITUDE KICKED → CLEAN / DRIVEN at DECAY max now hands the Howl over to a held, ducked bed instead of ADR 0018's ~9 s tail (test_drive reports it; listening page row "howl flip"). Pulling DECAY down still ends it.
- At DECAY max in freeze, turning up from silence gives silence: the bed holds what was in the tank when DECAY passed ~0.95. That is the freeze; the layer voicing or a throw puts sound in.
- The M6 Ringing grid at CLEAN / DRIVEN DECAY 1 is now Hold territory: see the M6 note in `docs/m8-tuning-backlog.md` "Throw and Hold".
- The Plugin's `getTailLengthSeconds()` still says 10 s; a held tail is longer. Left as is (hosts only use it to keep processing after the input stops; Ableton keeps a reverb running anyway). Noted, not changed.
- Costs: see ADR 0039 (the two are one build).
- Measurements and the M6 grid: `docs/m8-tuning-backlog.md` "Throw and Hold".
