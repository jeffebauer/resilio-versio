# 0025 — CLEAN gets a real, gentler splash

**Status:** Accepted, 29 Sep 2026. Changes SPEC §4.5 (CLEAN was "SPLASH = mild HF emphasis only").

**Context:** The M8 sweet-spot sweeps found SPLASH does nothing across its whole range in CLEAN, the only fully dead knob range (docs/m8-sweetspot.md), against SPEC §2.3 "every knob position usable".

**Decision:** In CLEAN, SPLASH gives a real but gentle splash on hard hits: a **light, bright shimmer of Clatter plus a very small Jolt** (a polite tank getting nudged). The three ATTITUDEs step up: **CLEAN gentle, DRIVEN clear, KICKED unmistakable.**

**Testable (M8):** on a rimshot/snare from −18 to −3 dBFS peak, SPLASH 0 vs 1 crash (1–6 kHz energy in the first 150 ms): CLEAN ≥ +1.5 dB (audible), below DRIVEN's at the same hit. The Jolt pitch lurch stays small, e.g. ≤ a third of DRIVEN's. Ghost notes in a groove still barely trigger (same −15 dB rule). CLEAN uses the same level-adaptive hit detection as DRIVEN/KICKED. The sweet-spot tool finds no SPLASH dead zone in CLEAN.

**Why:** Owner decision after the dead-zone finding; keeps the wide-sweet-spot principle while CLEAN stays the polite, hi-fi voice.
