# 0015 — Parameter smoothing in two tiers: snappy vs gliding

**Status:** Accepted, 27 Sep 2026

**Decision:** ParamSpec smoothing times (starting values):
- **Snappy (~5 ms):** MIX, DRIVE, SPLASH, TONE. Envelope-driven dub throws land tightly.
- **Gliding (~50–100 ms):** DECAY (including L slew, ADR 0012), WOBBLE, BOING.
- Switches: ATTITUDE Morph and SPRINGS crossfade per ADR 0003.

**Why:** Snappy tier = level and colour, safe to move fast. Gliding tier = parameters that move delay lengths or allpass coefficients, where fast changes cause pitch lurches or zipper noise.

**Testable:** An envelope on MIX CV (5 ms attack) produces a throw with no audible lag. A full-range DECAY CV step produces a bend, not a click (Renderer click detector).
