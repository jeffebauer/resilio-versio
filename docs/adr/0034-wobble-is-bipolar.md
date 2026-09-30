# 0034 — WOBBLE is bipolar: random wow + flutter left, sine LFO right

**Status:** Proposed (prototype on branch `proto/bipolar-wobble`, 30 Sep 2026; not merged). Supersedes ADR 0008's one-way zones once the owner accepts it by ear.

**Context:** The owner, after the hang page and the SPLASH/DRIVE build: "I quite like the more extreme wobble, but … how random is the wobble? Is it a fixed loop, or a smooth random wave? Sometimes it sounds a bit same-same." And: "differences between wobble at 9 and 12 are very subtle." The owner's idea: make the knob "more in line with other Versio modules by making it bipolar: left of noon introduces smooth random modulation (wow and flutter), and right of noon, increase the strength of a sinusoidal LFO." On the hang page (proto/wobble-hang) they picked **B, "Springs drift together at low WOBBLE"** in every panel.

What the old knob did (ADR 0008): one knob swept depth, rate (0.12 → 1.4 Hz) and the sine's share (30 → 90 %) together. So the top end was 90 % one steady sine (the "same-same"), the random line's rate was tied to the sine's, there was no flutter, and the whole lower half spanned 0–0.75 cents per pass (held tone through the whole Tank at DECAY noon: 0.7 cents at 9 o'clock, 2.8 at noon), which is why 9 o'clock and noon sounded alike.

**Decision:**
- **Noon is still** (only the Micro-mod floor, SPEC §4.10), with a dead zone of ±3 % of travel, because a pot's physical noon doesn't read exactly 0.5.
- **Left of noon: Drift, smooth random wow + flutter**, never repeating. Wow is a random line (Catmull-Rom through seeded random points) whose every segment has its own random length, so its rate wanders (~0.2–1.5 Hz, faster as it grows). Flutter is a smaller, faster random line (~5–12 Hz). Both grow towards fully left, which is roughly as wild as the old top.
- **Right of noon: Warble, a sine LFO** getting stronger (0.6 → 1.4 Hz as it grows) up to the old top end ("clearly out of tune", which the owner keeps). Its rate drifts by ±6 % on a slow random line so it never sounds mechanical; `kLfoRateWander = 0` gives a pure sine.
- **Every step is heard:** depth follows s(a) = (e^{ka} − 1)/(e^k − 1) on each side (k = 1.6 right, 1.0 left) instead of the old k = 5.89, so each 0.1 of travel grows the heard pitch movement by ~1.5–3x and the first step off noon is already a few cents on a held tone.
- **Springs drift together at low amounts** (the hang page's B): Springs B and C take Spring A's movement scaled by their Loop length, blending to their own over 10–45 % of either side's travel. Chords fade evenly; above that the Springs move independently, as before.
- **Kept:** one generator per Spring on the Loop plus the shared Transport on the first echoes (M8), depths specified in cents per pass so WOBBLE sounds the same at every TENSION, the Gliding smoothing tier (ADR 0015), CV adding to the knob (fully left + CV sweeps random → still → LFO). The default moves from 0.2 to 0.45: a touch of shared Drift, as before about 1 cent on a held tone.
- Every number lives in `core/params/WobbleVoicing.h`; `core/dsp/Wobble.*` is the generator.

**Supersedes in ADR 0008:** the three zones along one knob (Drift 0–50 %, transition, Warble 75–100 %). Drift is now the left side and Warble the right side; "held chords stay in tune" now holds at and near noon (the default is < 3 cents), and "clearly out of tune" at both end stops. ADR 0008's Micro-mod floor under everything stays.

**Rejected:**
- *Keep one-way, just reshape the curve:* fixes "9 ≈ noon" but not "same-same" (a sine still dominates the top) and gives no choice between random and periodic.
- *A second control for the rate or the random/sine mix:* no free knob on the Versio; the bipolar layout gives both characters on one knob, like other Versio firmwares.
- *Random side as the old smoothstep random line:* its pitch stops at every random point (a regular "breathing"); Catmull-Rom keeps the slope smooth through them.
- *Flutter on the sine side too:* the owner asked for a clean sine there; flutter is part of the tape-like random side.
- *Independent Springs at every amount:* the hang (ADR 0027 "Open"): one chord note can sing on after the chord at low WOBBLE.

**Consequences:** The random side swings the Loop delay further than the old sine for the same pitch (a slow wow needs more delay swing): up to 217 samples measured, 295 reserved, so the Tank's delay pool grows 28,317 → 29,061 floats at 48 kHz (firmware pool 30,000). CPU: about +0.1 % of the Versio (measurements in `docs/m8-tuning-backlog.md` "Bipolar WOBBLE"). Tests and presets that meant "no WOBBLE" now use 0.5 (noon); ones that meant the old default use 0.45. Held-tone numbers per knob step, the M6 grid at five WOBBLE settings and the sweet-spot check: backlog "Bipolar WOBBLE". The old WOBBLE section of `SplashVoicing.h` is left in place (unused) to keep the merge with the SPLASH/DRIVE branch easy; delete it when merging.
