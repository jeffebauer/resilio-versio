# 0023 — Ringing is judged by `ringing_db`, calibrated on real tanks

**Status:** Accepted, 28 Sep 2026. Replaces the SPEC §4.10 "12 dB narrowband peak" test.

**Context:** The original test (a narrow peak more than 12 dB above the 1/3-octave-smoothed median) "failed" 24 of 45 real, non-ringing spring tanks in the IR library (ADR 0021). A spring's own dense modes read as peaks. It also flagged our renders because of the synthetic snare's pitched body.

**Decision:** Ringing = one narrow frequency that keeps climbing out of its 1/3-octave neighbourhood through the late tail, measured as `ringing_db` (definition and evidence: `docs/m6-metric-calibration.md`). **Flag at ≥ 15 dB.** `steady_tone` (a tone standing ≥ 20 dB clear for > 2 s above −30 dBFS) stays as a second check. `resonance_peak_db` remains in sidecars for information only.

**Evidence:** passes 38/39 measurable real-tank IRs (median 6.8 dB, max 14.7; the one flag, *Short Spring*, has a mode decaying 5.4× slower than its neighbours). Flags synthetic resonant loops (+2/+3 dB peaks at L 30–100 ms, 300 Hz–3 kHz), a sine 20 dB down in a tail, and self-oscillation. Unaffected by the input's own pitch (hits renders: 19.7 dB on the old metric → 4.3).

**Pending:** the Wellspring's real delay-Ringing take (recipe take G) as a "must flag" case, once it's recorded.
