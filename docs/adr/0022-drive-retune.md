# 0022 — DRIVE retune: obvious from noon, cranked tape/tank at max, level constant

**Status:** Accepted, 28 Sep 2026. Refines ADR 0014 (its intent stands; the M5 build missed it).

**Context:** Owner listening to the M5 build: DRIVE is very subtle even at max. Measured on `02_hits` (peaks −6 dBFS), DRIVE 0 → 1 changes the output by only −27 dB (CLEAN), −13 dB (DRIVEN), −7 dB (KICKED) relative to the signal, and DRIVE 0 → 0.5 by only −33 to −41 dB (barely audible). Causes: gain compensation removes the "pushing" cue, the springs smear input distortion into the tail, and the curve was calibrated for hotter levels than a typical DAW track.

**Decision:**
- **Max DRIVE in KICKED = cranked tape/tank:** thick, compressed, obviously saturated and gritty, still a spring. The reference is the Wellspring with INPUT pushed until the clip light is solid (recipe take C) and the Magneto at REC LVL red (MD3). DRIVEN at max = warm, clearly saturated tape. CLEAN stays mostly clean (transducer tint only).
- **Onset:** clean-ish below ~9 o'clock, **clearly coloured by noon**, driven by ~3 o'clock.
- **Level stays constant** (±2 dB across DRIVE, the M5 criterion unchanged). The character must come from tone, grit and compression, not loudness.
- **Reference input level:** calibrate at peaks of about −6 dBFS (the stimulus level). On the Versio, a 10 Vpp modular signal is about −4 dBFS (inputs clip at ~16 Vpp), so Plugin and module land close.

**Testable (added to M5):** on `02_hits`, the DRIVE 0 vs DRIVE 0.5 difference ≥ −20 dB in DRIVEN and KICKED (clearly audible); DRIVE 0 vs 1 ≥ −6 dB in KICKED; THD rises monotonically; loudness within ±2 dB; aliasing ≤ −60 dB; the owner A/Bs max DRIVE against Wellspring take C / Magneto MD3 when recorded.

**Implementation hints (for the retune):** more pre-gain range or an earlier curve (less than DRIVE^1.8); saturation that survives the tank (e.g. more drive into LoopSat as DRIVE rises, a little post-tank DriveOut saturation that scales with DRIVE); keep gain compensation per ATTITUDE.

**Amended 30 Sep 2026 (ADR 0033, owner):** DRIVE is now the INPUT gain and the level is no longer constant: it follows a quarter of the INPUT gain, **+6 dB at DRIVE 1 (±2 dB)**, the same in every ATTITUDE (judged at SPLASH 0). The DRIVE push on the LoopSat is removed (it squashed the tail every round trip, so DRIVE shortened it: KICKED at 0.6 s after a hit, −19.2 → −22.6 dB from DRIVE 0 to 1); DRIVE drives the input and output stages only. The audibility bars now apply to level-matched nulls. KICKED's pickup push 26 → 28 dB keeps its cranked top (level-matched 0 vs 1: −5.3 dB).
