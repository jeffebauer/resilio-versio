# 0005 — Kick has fixed strength

**Status:** Accepted, 27 Sep 2026

**Decision:** Kick strength does not follow trigger velocity. Scaled only by ATTITUDE. Plugin ignores MIDI velocity.

**Why:** Versio gate input is digital on/off (libDaisy `DaisyVersio::Gate()` returns bool) — velocity impossible on hardware. Plugin ignores velocity for parity.
