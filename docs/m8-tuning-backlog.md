# M8 tuning backlog

Owner listening to the M7 plugin build (commit `81f8124`) in Ableton, 28 Sep 2026, plus measurements. Each item: what the owner heard, why, target, fix direction. The M8 sweet-spot report (`docs/m8-sweetspot.md`) adds numbers per knob.

## 1. BOING changes decay length
- **Heard:** at 0 the reverb rings longer; at 1 there's a slight pitch envelope (the chirp, intended) and it decays faster (not intended).
- **Measured:** T60 at DECAY 0.5: 1.92 s (BOING 0) vs 1.72 s (BOING 1), about −10%; perceived shorter still because smeared echoes are less distinct.
- **Target:** BOING changes the chirp only. T60 within ±5% across BOING at every DECAY.
- **Fix direction:** g design uses the full-band round trip, including the chirp's group delay, per BOING; check the band used for "smallest g". Coordinate with the chirp-direction switch (task 7).

## 2. SPLASH inaudible (0 vs 1)
- **Heard:** no difference.
- **Why:** hit detector calibrated for hits peaking ~−6 dBFS (DAW tracks often sit 10–15 dB lower); CLEAN is designed as a tiny HF lift; Clatter enters the quiet high path (DRIVEN −8 dB vs the hit's own 1–6 kHz).
- **Target:** KICKED SPLASH 1 unmistakable on a snare at any sensible level; DRIVEN clearly audible; CLEAN a subtle but audible sparkle; ghost notes still barely trigger.
- **Fix direction:** level-adaptive hit detection (relative to a slow program-level tracker), more Clatter/Jolt level, possibly a Clatter share straight to the wet bus.

## 3. DRIVE subtle, even KICKED at max
- **Heard:** subtle at extreme settings.
- **Why:** calibrated for Eurorack level (10 Vpp ≈ −4 dBFS). DAW tracks are typically 10–15 dB lower, so the plugin gets far less drive than the module will. The springs also smear input distortion.
- **Target:** ADR 0022 as heard in the plugin, not just measured on −6 dBFS test hits.
- **Fix direction:** (a) **plugin-only INPUT level** control (test-bench calibration so a DAW track can be driven like a modular signal; parity preserved once levels match); (b) more drive that survives the tank in KICKED (LoopSat push, DriveOut).

## 4. WOBBLE inaudible, even at 1
- **Heard:** can't hear it.
- **Why:** WOBBLE only modulates the Loop, so pitch movement builds over repeats (≈43 cents on a held note at DECAY noon, ~12 cents per pass). On drums or short DECAY it barely develops. Tape wobble moves the first echo too.
- **Target:** Warble audible on the first echoes at WOBBLE ≥ 0.75, even on drums; Drift still subtle in the lower half (ADR 0008). Depth calibrated to the Magneto WOW & FLUTTER series (ADR 0020).
- **Fix direction:** add wobble to the early part (the pickup taps / first pass), not only the Loop; retune the depth curve.

## 5. SPRINGS: 1 narrow, 3 flams
- **Heard:** 1 Spring fairly mono/centred; 2 spacious (good); 3 as wide, but with a flam: the third spring seems to arrive after the first two.
- **Why:** the M4 stereo fix staggered each Spring's pickup tap, so first echoes arrive at roughly 7 / 19 / 30 ms. A ~20 ms spread between first arrivals is heard as a flam. 1 Spring is deliberately the narrowest mode (a single tank is mono).
- **Target:** no audible flam in any mode (first-arrival spread ≤ ~8 ms), M4 stereo checks still pass (correlation < 0.5, mono-safe); 1 Spring a little wider while mono-safe.
- **Fix direction:** shrink the tap stagger; recover early decorrelation via detuning and short diffusion; revisit 1-Spring side level.
