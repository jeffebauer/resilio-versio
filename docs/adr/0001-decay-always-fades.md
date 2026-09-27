# 0001 — Max DECAY always fades (no freeze)

**Status:** Accepted, 27 Sep 2026

**Decision:** At DECAY fully CW the tail still decays (target max ~8–10 s). No infinite/freeze mode.

**Why:** Real springs never sustain forever. Freeze drives Loop gain to 1, which fights the AntiRes system and invites runaway Ringing.

**Exception:** Howl in KICKED (ADR 0002) is sustained by saturation, not by unity linear gain.

**Tradeoff:** Loses ambient freeze pads. Acceptable — not a dub spring behaviour.
