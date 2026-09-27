# 0009 — Record Wellspring reference files from a fixed stimulus set

**Status:** Accepted, 27 Sep 2026. Amended 28 Sep 2026 after reading the Wellspring manual.

**Decision:** The owner records the Wellspring's springs alone ("spring only": delay DRY/WET fully dry, MAGIC zero, SPRINGS fully wet) through a fixed, generated stimulus set: clicks, hits (normal and hot INPUT), sine sweep, chord stabs, optional physical knocks, and optional delay + MAGIC noise bursts that provoke Ringing as a detector test case. Wet L + R are recorded as one stereo take. A loopback take measures interface latency. Recipe: `docs/recording-recipe.md`. Stimulus: `tools/make_stimulus.py` (deterministic).

**Why:** Gives measurable targets (chirp spacing, T60, spectrum, driven colour) and, optionally, a real Ringing example for testing AntiRes's detector. The Renderer uses the same stimulus, so Resilio Versio and the Wellspring can be compared file for file.

**Wellspring facts that shape this (from its manual):**
- Its only spring control is SPRINGS dry/wet. **There is no decay control**, so its T60 is fixed by the tanks. It is one reference point: DECAY is set to match it for comparisons, not the other way round.
- Stereo springs (two tanks, each a pair of 15" springs). Wet is true stereo, so there is no spare channel for dry. Alignment comes from the loopback take.
- The springs are fed after the delay DRY/WET, so "spring only" fully removes the delay, filter and modulation.
- INPUT drives the unit into pleasing distortion: take C is a free driven-spring reference for M5.
- The tanks are shock-mounted against outside vibration, so physical knocks may be weak. The Kick reference is optional.

**Scope:** Reference only. Resilio Versio is not meant to clone the Wellspring. Stimulus WAVs are regenerated, not committed. Reference recordings are committed.
