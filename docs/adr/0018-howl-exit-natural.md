# 0018 — Leaving Howl: dies away naturally

**Status:** Accepted, 27 Sep 2026

**Decision:** When the player pulls DECAY out of the Howl zone, or flips ATTITUDE away from KICKED, the Howl falls back into a normal tail and fades out on its own over ~1–2 s. There's no fast cut.

**Why:** Rideable like backing off feedback on a Space Echo. It's a performance move, not a safety brake. (The output limiter is always on regardless, ADR 0002.)

**Implication:** Loop gain returns below 1 through the normal smoothing (ADR 0015 gliding tier; ATTITUDE Morph, ADR 0003). No special "kill" path. Testable: after exiting the zone, the level drops ≥ 30 dB within ~3 s.
