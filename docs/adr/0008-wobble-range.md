# 0008 — WOBBLE: Drift in the lower half, worn-tape Warble in the top quarter

**Status:** Accepted, 27 Sep 2026

**Decision:** WOBBLE response curve has three zones:
- **0–50%: Drift.** Subtle, felt more than heard. Tail feels alive; held chords still sound in tune.
- **50–75%: transition.** Pitch movement becoming audible.
- **75–100%: Warble.** Obvious worn-tape warble, clearly out of tune on held chords.

The Micro-mod floor (SPEC §4.10) stays underneath at 0.

**Why:** Owner's musical target. Most of the knob is a colour; the top is an effect.

**Implication:** Strongly non-linear (exponential-ish) depth curve in ParamSpec. The §4.7 "max depth ~0.5–1% of L" guess is probably too small for "clearly out of tune" and must be tuned by ear. Warble pitch deviation in the top quarter is likely tens of cents. Numeric targets come from the Magneto WOW & FLUTTER series (ADR 0020).
