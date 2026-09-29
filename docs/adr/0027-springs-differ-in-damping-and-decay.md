# 0027 — The Springs also differ in damping and tail length

**Status:** Accepted, 29 Sep 2026. Amends SPEC §4.3 (detune) and AntiRes layer 3 (§4.10).

**Context:** After TENSION (ADR 0026) the owner heard "some resonances creeping in, even at mid-level decay settings" in Ableton. A scan over DECAY 0.2–0.75 × TENSION × SPRINGS × ATTITUDE (972 cells, clicks, hits, bursts) found 42 two- and three-Spring cells at `ringing_db` ≥ 15, worst 23.2 dB, and none on one Spring alone (bare Springs ≤ 4 dB). The cause was the interaction between Springs: two Springs with slightly different round trips line their modes up every 1/(RT_B − RT_A) Hz (about every 330 Hz on the tight tank). Near each line-up, a mode of A and a mode of B sit a fraction of a Hz apart and die at the same rate, and when the pair drifts into phase late in the tail it swells into one singing note. Tight tanks show it most (modes ~30 Hz apart, few neighbours to hide among); BOING could never reach them with a 1–4 s tail. Any L/fC/a detune ratio produces such line-ups, so the ratios aren't the fault.

**Decision:** Each Spring is also a step darker and shorter than the one before, so the two modes of a pair die at different rates and can't sustain each other: damping cutoff × 1.00 / 0.85 / 0.72 and T60 × 1.000 / 0.930 / 0.865 for Springs A / B / C (`SpringModes.h` `kDetune`). Spring A is unchanged, so 1-Spring mode is bit-identical. Control rate only (two multiplies per tick).

**Results:** 2/3-Spring cells ≥ 15 dB: 42 → 0 (worst 14.7); M6 grid 270 cells, 0 Ringing (worst 10.3); Howl cells all pass ADR 0019. Where both builds reach the same L and T60, the last BOING build had 22 of 144 cells ≥ 12 dB; TENSION after this change has 0 of 72. 2-Spring stereo spread `kSide2` 0.43 → 0.40 to keep the mono-notch margin once SPLASH's clang goes through the Loops (stabs −4.4 dB, limit −4.5; L/R correlation 0.39, limit 0.47).

**Rejected:** a darker-but-longer Spring (worse); a doubled spread (0 cells, but a 3-Spring chord tail thinned into Spring A alone, one partial singing at +9 dB); a Micro-mod floor shared by all Springs (no better, and it breaks the Springs' independent drift).

**Open:** on `04_skank` at WOBBLE 0.2, one partial just above the tight tank's first line-up (~363 Hz) still hangs up to +14 dB after note-off; at WOBBLE 0 it's gone. WOBBLE's independent per-Spring Drift is the remaining source. Candidate: share the slow Drift across Springs at low WOBBLE (backlog).

**Thin margin (merge with SPLASH round 2):** with SPLASH's clang in the Loops, the M6 grid's worst cell is KICKED, 1 Spring, tightest TENSION, TONE 1, DECAY 0.75, noise bursts: `ringing_db` 14.6 at 3.9 kHz (limit 15; the next worst is 10.0). The same corner read 15.4 before the short-tank Micro-mod scaling. Owner listening task.
