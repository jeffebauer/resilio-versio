# 0002 — KICKED may Howl; CLEAN/DRIVEN never self-oscillate

**Status:** Accepted, 27 Sep 2026

**Decision:** In KICKED, top ~10% of DECAY range lets the Tank tip into self-sustaining feedback (Howl) that the player rides with knobs. CLEAN and DRIVEN never self-oscillate.

**Constraints:** Howl is saturated, noisy, crashing — never a clean sine. AntiRes stays active and still suppresses single-tone Ringing. Output limiter always on.

**Why:** Pushing springs/echoes into feedback is a classic dub performance move. Confining it to one mode + knob zone keeps the rest of the range safe (wide sweet spot principle).

**Tradeoff:** AntiRes must distinguish broadband Howl from narrowband Ringing — test explicitly.
