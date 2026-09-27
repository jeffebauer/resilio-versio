# 0012 — Turning DECAY bends the live tail's pitch

**Status:** Accepted, 27 Sep 2026

**Decision:** Because DECAY also sets Loop delay L (SPEC §4.4), turning DECAY during a tail bends its pitch smoothly, like changing tape speed. There's no two-tank crossfade. L is slew-limited so the bend is always smooth, never a zip or a click.

**Why:** A playable dub move, and the cheapest option (one tank).

**Implication:** L's slew limit is a tuning parameter (see ADR 0015). Bend depth depends on how far L moves (short → long tank ≈ 30 → 100 ms).
