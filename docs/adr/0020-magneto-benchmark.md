# 0020 — Strymon Magneto as a benchmark reference, not a target

**Status:** Accepted, 28 Sep 2026

**Decision:** The owner also records the Strymon Magneto (digital spring + tape echo, Eurorack) through the same Stimulus set: a short spring set (clicks, hits, skank) in Dual Split Mode (spring only, no tape path), plus wow & flutter and REC LVL series from the tape path. Recipe: `docs/recording-recipe-magneto.md`.

**Roles:**
- **Wellspring = target** for spring character: "is this a spring?" (real physics: chirp spacing, dispersion, tail).
- **Magneto = benchmark** for quality: "does ours hold up next to a respected emulation, and avoid sounding like a digital reverb?" (M8), and a **calibration source**: wow & flutter positions → numeric pitch-wobble targets for Drift / Warble (ADR 0008), REC LVL steps → DRIVEN tape colour reference (M5).
- When the two disagree on spring character, the Wellspring wins.

**Why:** A second, digital reference shows what "good" sounds like for an emulation, and the Magneto's tape section is the dub rig (Space Echo lineage) the spec cites. It costs about 20 minutes of recording and no code: the Renderer, analysis and review page take any reference WAV.

**Tradeoff:** Two references could pull tuning in two directions. The role split above resolves that.
