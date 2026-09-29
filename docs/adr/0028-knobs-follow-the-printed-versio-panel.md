# 0028 — Knobs follow the owner's printed Versio panel

**Status:** Accepted, 29 Sep 2026. Amends SPEC §3 (knob order).

**Context:** Until Resilio has its own panel, the owner plays it behind the stock Versio panel's printed labels. At the M0 LED check the knobs landed in unexpected places: SPEC §3 listed them as K0–K6 "in panel order", but libDaisy's `KNOB_0..KNOB_6` are not in reading order on the panel. Measured from the M0 LED key, pots P1–P7 (reading order, `docs/panel/`) are libDaisy knobs 0, 4, 2, 1, 5, 3, 6.

**Decision:** Name knobs by panel position (P1–P7), not by libDaisy index. The owner's layout: **P1 MIX, P2 DECAY, P3 TONE, P4 SPLASH, P5 TENSION, P6 WOBBLE, P7 DRIVE**. The release firmware keeps two tables: `kPotKnob` (pot → libDaisy index, a hardware fact) and `kPotParams` (pot → function, this decision), so a future custom panel changes only the second. Each pot's CV input is summed with the pot in hardware, so CV follows its pot. Switches unchanged (top SPRINGS, bottom ATTITUDE). ParamSpec, the Plugin and the Renderer don't change: they name parameters, not pots.

**Consequence:** the M3 profile build ignores knobs, and the m0test build's LED key stays in libDaisy index order (`docs/m0-hardware-check.md` lists which pot lights what).
