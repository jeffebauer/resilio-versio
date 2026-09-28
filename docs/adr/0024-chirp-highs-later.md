# 0024 — Chirp direction: highs arrive later (like real tanks)

**Status:** Accepted, 29 Sep 2026. Corrects SPEC §2.1 (v1.0 said "high frequencies arrive before lows").

**Decision:** The Spring's dispersion makes the **highs arrive later** than the lows, so each echo sweeps up and the sizzle lands last. `kChirpDirection = ChirpDirection::HighsLater` in `core/params/Mappings.h`: allpass coefficient `a` +0.40 → +0.55 across BOING, stages 24 → 64 (52 in 3-Spring mode).

**Why:**
- **Owner, by ear** (chirp A/B pages `renders/chirp_ab2/`): "highs later definitely sounds more like a classic spring reverb."
- **Measured:** every one of the 22 usable real-tank IRs in the IR library chirps this way (ADR 0021, `docs/ir-dispersion-study.md`). The v1.0 wording was written from memory, not measurement.
- **Side benefit:** tail length stays the same at every BOING (T60 0.41 / 1.95 / 9.15 s at DECAY 0 / 0.5 / 1), fixing the M8 backlog item "BOING shortens decay" (the old direction lost up to 12 %).

**Numbers:** chirp 5–37 ms per trip, growing steadily with BOING; covers the IR library except the largest tank (HIC100L, 59 ms); fC 4.4 / 3.6 / 2.9 kHz across DECAY.

**Status of the switch (29 Sep):** flipping it on top of M8 tuning round 1 (which was tuned in the old direction) failed four checks: wet-level spread (7.5 dB at DECAY 0.5, limit 3.5; the excitation trim's band follows the old resonances), aliasing at max DRIVE (−53/−54 dB, limit −60), KICKED DRIVE level spread (2.2 dB, limit 2) and the Micro-mod floor held-tone check (whole Tank 4.9 cents, limit 3). The switch flips in the commit that re-tunes those.

**Consequences:** HighsLater uses a −1 dB wet trim and a 0.5 ms limiter attack (constants). The M6 ringing grid, test_clicks and the full suite pass in this direction. The LowsLater mapping stays in the code behind the switch. The TENSION idea (TASKS, "Later") carries this direction.
