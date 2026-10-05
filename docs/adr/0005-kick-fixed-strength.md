# 0005 — Kick has fixed strength

**Status:** Superseded by ADR 0043 (5 Oct 2026: the Kick was removed). Accepted, 27 Sep 2026

**Decision:** Kick strength does not follow trigger velocity. Scaled only by ATTITUDE. Plugin ignores MIDI velocity.

**Why:** Versio gate input is digital on/off (libDaisy `DaisyVersio::Gate()` returns bool) — velocity impossible on hardware. Plugin ignores velocity for parity.
