# 0018 — Leaving Howl: dies away naturally

**Status:** Accepted, 27 Sep 2026

**Decision:** When the player pulls DECAY out of the Howl zone, or flips ATTITUDE away from KICKED, the Howl falls back into a normal tail and fades out on its own over ~1–2 s. There's no fast cut.

**Why:** Rideable like backing off feedback on a Space Echo. It's a performance move, not a safety brake. (The output limiter is always on regardless, ADR 0002.)

**Implication:** Loop gain returns below 1 through the normal smoothing (ADR 0015 gliding tier; ATTITUDE Morph, ADR 0003). No special "kill" path. Testable: after exiting the zone, the level drops ≥ 30 dB within ~3 s.

**Amendment (30 Sep 2026, owner): flipping ATTITUDE at max DECAY calms into the long tail.** Asked whether a flip away from KICKED while Howling at max DECAY should fade within 1–2 s or calm into that DECAY's normal long tail, the owner chose the **long tail**. That is what the Tank already does: measured on `main` (Howl from Kicks, KICKED, DECAY 1, DRIVE 0.8, flip at 6 s), CLEAN falls 2 / 9 / 23 / 35 dB at +1 / 2 / 4 / 6 s (the ~9 s tail of DECAY 1); DRIVEN 13 / 24 / 39 / 51 dB (shorter because DRIVE pushed its loop saturator, which ADR 0033 removes). Pulling DECAY out of the zone still fades fast (28 dB at +1 s, 49 dB at +2 s). So the "~1–2 s" above applies to leaving the Howl by DECAY; leaving it by ATTITUDE at max DECAY hands over to the long tail, with no special fade.
