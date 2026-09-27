# 0009 — Record Wellspring reference files from a fixed stimulus set

**Status:** Accepted, 27 Sep 2026

**Decision:** Owner records the Wellspring's spring section (100% wet, BBD delay section off) through a fixed, generated stimulus set: clicks, hits, sine sweep, chord stabs, physical Kicks, and (optionally, delay on) noise bursts that provoke delay-feedback Ringing as a detector test case. Dry and wet are recorded on one stereo take. Recipe: `docs/recording-recipe.md`. Stimulus: `tools/make_stimulus.py` (deterministic).

**Why:** Gives measurable targets (chirp spacing, T60, spectrum) and, optionally, a real Ringing example for testing AntiRes's detector. The Renderer uses the same stimulus, so Resilio Versio and the Wellspring can be compared file for file.

**Scope:** Reference only. Resilio Versio is not meant to clone the Wellspring. Stimulus WAVs are regenerated, not committed. Reference recordings are committed.
