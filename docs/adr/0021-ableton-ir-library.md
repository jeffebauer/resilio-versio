# 0021 — Ableton spring impulse responses as an IR library for range calibration

**Status:** Accepted, 28 Sep 2026

**Decision:** Use the spring impulse responses bundled with the owner's Ableton Live 12 Suite (Hybrid Reverb "Springs" and the Convolution Reverb pack's "09 Springs"): 45 files after removing synthetic ("Fake…", "Synth…") and duplicate ones. They form a third reference type, the **IR library**, used only to check that DECAY, BOING and TONE ranges cover real tanks.

**Roles (with ADR 0009, 0020):** Wellspring = target for spring character. Magneto = quality benchmark. **IR library = range calibration** (the spread of decay, brightness, chirp spacing across many real tanks). None of them is cloned.

**Limits:**
- Impulse responses are linear snapshots: no Splash, Clatter, drive, Kick or wobble. They say nothing about transient behaviour.
- Some are processed (tank + amp cab, tape delay, "Swissecho RevR DelayL" layered with delay), and are tagged as such in analysis.
- They start with a broadband pulse, so first-arrival dispersion can't be measured from them. Chirp-ridge analysis in later echoes is needed (SPEC ref 5, "Automated Calibration of a Parametric Spring Reverb Model"). That's planned for M8.
- Licence: Ableton content, used for private analysis only. Never committed. Converted copies live in `renders/ir_library/` (gitignored), regenerable from the Ableton install.

**First findings (28 Sep 2026, `tools/ir_analysis.py` + `rv_render --analyze`):**
- T60 of real tanks: mostly ~1.1–7 s (outliers to ~23 s). Our DECAY range (0.4–9 s) covers it.
- Brightness (3–6 kHz vs 0.7–1.4 kHz): real tanks ≈ −10 to +11 dB; ours −1 to −7 dB at TONE noon. Darker than average, as intended for dub; check TONE's bright end reaches the brighter tanks (M5).
