# 0019 — Howl may lean toward a pitch, but stays rough and moving

**Status:** Accepted, 27 Sep 2026. Refines ADR 0002 (doesn't reopen it).

**Decision:** In the KICKED Howl zone the Tank may settle toward a pitch, like a dub siren or amp feedback. But it is always rough, noisy and moving, never a clean, steady sine.

**Why:** Owner wants a rideable pitched roar. A strictly pitchless Howl would be less musical.

**Testable (Howl zone only, replaces the Ringing test there):**
- Broadband floor: the spectrum 1/3-octave median in the 200 Hz–5 kHz band stays within 25 dB of the strongest peak (i.e. noise and harmonics present, not a bare sine).
- Movement: the strongest peak's frequency or level varies over any 2 s window (not a steady tone). Frequency deviation ≥ 0.5% or level deviation ≥ 3 dB.

Thresholds are starting values, confirmed by ear at M6.
