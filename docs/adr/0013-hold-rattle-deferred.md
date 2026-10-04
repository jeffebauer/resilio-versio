# 0013 — Hold-for-rattle is not in v1

**Status:** Accepted, 27 Sep 2026. Amended by ADR 0039 (4 Oct 2026): holding the button 1 s leaves throw mode; hold-for-rattle is still deferred.

**Decision:** v1 KICK (button, Gate, MIDI) gives single Kicks on the rising edge only. Holding does nothing more. Sustained rattle while held is a post-v1 idea.

**Why:** Fast tapping already gives repeated Kicks. It keeps the Kick model simple while it's being tuned. Resolves the duplicate entry in SPEC §3 / §10.
