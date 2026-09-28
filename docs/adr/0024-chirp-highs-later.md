# 0024 — Chirp direction: highs arrive later (like real tanks)

**Status:** Accepted, 29 Sep 2026. Corrects SPEC §2.1 (v1.0 said "high frequencies arrive before lows").

**Decision:** The Spring's dispersion makes the **highs arrive later** than the lows, so each echo sweeps up and the sizzle lands last. `kChirpDirection = ChirpDirection::HighsLater` in `core/params/Mappings.h`: allpass coefficient `a` +0.40 → +0.55 across BOING, stages 24 → 64 (52 in 3-Spring mode).

**Why:**
- **Owner, by ear** (chirp A/B pages `renders/chirp_ab2/`): "highs later definitely sounds more like a classic spring reverb."
- **Measured:** every one of the 22 usable real-tank IRs in the IR library chirps this way (ADR 0021, `docs/ir-dispersion-study.md`). The v1.0 wording was written from memory, not measurement.
- **Side benefit:** tail length stays the same at every BOING (T60 0.41 / 1.95 / 9.15 s at DECAY 0 / 0.5 / 1), fixing the M8 backlog item "BOING shortens decay" (the old direction lost up to 12 %).

**Numbers:** chirp 5–37 ms per trip, growing steadily with BOING; covers the IR library except the largest tank (HIC100L, 59 ms); fC 4.4 / 3.6 / 2.9 kHz across DECAY.

**Status of the switch (29 Sep):** on: `HighsLater`. Flipping it on top of M8 tuning round 1 failed four checks; re-tuned (details in `docs/m8-tuning-backlog.md` "HighsLater re-tune"): aliasing at max DRIVE was real (the LoopSat now saturates on flux, a 2 kHz / 12 dB shelf pair: −65 / −61 dB, limit −60); KICKED DRIVE level on noise (KICKED wet makeup 1.3 → 1.6 dB: 1.7 dB, limit 2); the wet-level spread and the whole-Tank held-tone pitch were measurement artefacts (four fixed sines landing in the Loop's mode dips; the onset's own modes beating 2 s into a DECAY-1 note, same with the floor off), now measured over 12 pitches and after the note has built up. Wet trim −1 → −1.1 dB. Full suite passes; LowsLater still builds and passes, untuned.

**Consequences:** HighsLater uses a −1 dB wet trim and a 0.5 ms limiter attack (constants). The M6 ringing grid, test_clicks and the full suite pass in this direction. The LowsLater mapping stays in the code behind the switch. The TENSION idea (TASKS, "Later") carries this direction.
