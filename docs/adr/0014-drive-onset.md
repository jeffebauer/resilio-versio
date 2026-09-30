# 0014 — DRIVE: clean-ish until ~9 o'clock, properly driven by ~3 o'clock

**Status:** Accepted, 27 Sep 2026

**Decision:** With a typical modular signal (~10 Vpp), the DRIVE curve is:
- 0 → ~9 o'clock (~0–25%): clean-ish. Only the light transducer colour of the current ATTITUDE.
- ~9 → ~3 o'clock (~25–85%): colour and grit build steadily.
- ~3 o'clock → max: properly driven.

Automatic gain compensation keeps loudness roughly level throughout (SPEC §4.9).

**Why:** Owner preference. Matches "characterful, not a fight". The reverb is fully audible with no drive at all.

**Note:** In CLEAN, "driven" means transducer colour only (no tape, no Loop sat), so the top of the knob is milder than in DRIVEN/KICKED.

**Note (30 Sep 2026): clock positions vs percentages.** The pots turn from 7 o'clock (0) through noon (0.5) to 5 o'clock (1), about 0.1 per hour (the convention in `docs/manual.md`, `docs/presets.md` and the review pages), so 9 o'clock is ~20 % and 3 o'clock ~80 %. The percentages above sit at about **9:30 (25 %)** and **3:30 (85 %)**. The code and tests use the percentages (SPEC §7 M5; `core/params/DriveVoicing.h`: pre-gain covers 17 % of its range at 0.25 and 81 % at 0.85, and 12 % / 75 % at 9 and 3 o'clock). Read the zones as: clean-ish to ~9:30, colour builds to ~3:30, properly driven above. ADR 0022 refines the onset (clearly coloured by noon).
