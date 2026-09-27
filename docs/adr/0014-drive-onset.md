# 0014 — DRIVE: clean-ish until ~9 o'clock, properly driven by ~3 o'clock

**Status:** Accepted, 27 Sep 2026

**Decision:** With a typical modular signal (~10 Vpp), the DRIVE curve is:
- 0 → ~9 o'clock (~0–25%): clean-ish. Only the light transducer colour of the current ATTITUDE.
- ~9 → ~3 o'clock (~25–85%): colour and grit build steadily.
- ~3 o'clock → max: properly driven.

Automatic gain compensation keeps loudness roughly level throughout (SPEC §4.9).

**Why:** Owner preference. Matches "characterful, not a fight". The reverb is fully audible with no drive at all.

**Note:** In CLEAN, "driven" means transducer colour only (no tape, no Loop sat), so the top of the knob is milder than in DRIVEN/KICKED.
