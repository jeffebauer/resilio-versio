# 0003 — ATTITUDE morphs live tail; SPRINGS crossfades

**Status:** Accepted, 27 Sep 2026

**Decision:** Changing ATTITUDE applies to the live tail (Morph) — flipping to KICKED mid-tail makes the existing tail go chaotic. Changing SPRINGS uses a short (~20 ms) crossfade from old to new structure.

**Why:** ATTITUDE flip is a performance gesture. SPRINGS changes Loop structure; morphing it risks clicks/discontinuities.

**Implication:** Attitude-dependent parameters (drive stages, Loop sat) must be smoothed, not stepped.
