# 0006 — Shortest DECAY is a tight spring slap, never dry

**Status:** Accepted, 27 Sep 2026

**Decision:** DECAY fully CCW gives a T60 of ~0.3–0.5 s: short, pingy, still clearly a Spring (chirps audible). It never approaches a dry signal. With ADR 0001, DECAY range ≈ 0.3–0.5 s → 8–10 s.

**Why:** Wide sweet spot principle (SPEC §2.3.1). Every DECAY position sounds like a spring. MIX already covers "less reverb".

**Implication:** At CCW, Loop delay L ~30 ms, so a 0.4 s T60 means only ~13 round trips. Chirps must still be audible in that time (checked in M1/M4 renders).
