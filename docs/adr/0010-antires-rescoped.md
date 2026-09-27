# 0010 — AntiRes: design-in defences always; adaptive suppressor only if measured need

**Status:** Accepted, 27 Sep 2026

**Context:** SPEC v0.3 justified AntiRes with Ringing heard on the owner's Wellspring. Correction: that Ringing comes from the Wellspring's **BBD delay** feedback, not its spring. The spring alone hasn't shown buildup.

**Decision:** The musical goal is unchanged: no single-tone Ringing in the tail, ever, without spending TONE (principle §2.3.4). Implementation is rescoped:
- **Always built:** even Loop gain by design (1), Micro-mod floor (2), Spring detuning (3), Loop saturation (5), and the automated resonance metric in the Renderer.
- **Conditional:** the adaptive suppressor (4: detector + dynamic notches) is built only if the metric fails at M6 after layers 1–3 are tuned.

**Why:** Each simulated Spring is a feedback delay loop, the same structure that rings in the delay, and digital loops lack analog noise/drift. So the risk is real, but no longer evidenced. Layer 4 is the most complex, CPU-costly, and risky piece (it must tell Howl from Ringing, ADR 0002). Build it only if it's needed.

**Consequence:** Frees CPU and M6 effort. The Micro-mod floor, detuning, and the metric carry more weight and must be tested properly. The optional Wellspring delay-Ringing recording (ADR 0009, take G) becomes the "known bad" case for validating the metric.
