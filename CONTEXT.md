# RESILIO VERSIO — Context / Glossary

Shared vocabulary for this project. Use these terms exactly in code, comments, and conversation. Spec: `SPEC.md`. Decisions: `docs/adr/`.

| Term | Meaning |
|---|---|
| **Tank** | Whole reverb engine: 1–3 Springs + shared input/output stages. Mirrors a physical spring reverb tank. |
| **Spring** | One simulated spring: low-chirp path + high path (SPEC §4.2). |
| **Loop** | A Spring's feedback path (delay + allpass cascade + filters). Where energy recirculates. |
| **Chirp / Boing** | Dispersive echo where the highs arrive **later** than the lows, so each echo sweeps up (as measured in every real tank, ADR 0024). Signature spring sound. Its size is set by TENSION. "Boing" names the sound only; the BOING knob was replaced by TENSION (ADR 0026). |
| **Tension** | P5 TENSION, "which tank is fitted": echo spacing (Loop delay), Chirp size and brightness together. More tension = tighter: up is tight (short, pingy, small bright Chirp), down is loose (long, boingy, big darker Chirp). Turning it bends the tail's pitch like tightening or slackening a string (ADR 0026). |
| **Decay** | P2 DECAY: tail length (T60) only. Doesn't change the tank or bend pitch (ADR 0026). |
| **Splash** | Bright, noisy wash on hard transients. Produced by Clatter + Jolt. Controlled by SPLASH. |
| **Clatter** | Injected noise bursts simulating springs hitting each other/housing. Part of Splash. |
| **Jolt** | Momentary lurch of Loop parameters on impact. Part of Splash. |
| **Hit** | Detected transient strength (0–1) from input. Drives Clatter + Jolt. |
| **Kick** | Simulated physical strike on Tank (thump + crash). Triggered by button, Gate, or MIDI note. Fixed strength, scaled by Attitude. |
| **Attitude** | SW1 mode: CLEAN / DRIVEN / KICKED. Sets drive stages, Clatter, Jolt, self-oscillation permission. |
| **Drive chain** | Input transducer → tape → Loop saturation → output pickup (SPEC §4.9). |
| **Howl** | Controlled self-sustaining feedback. KICKED only, top ~10% of DECAY. Noisy/crashing, never a pure tone. |
| **Ringing / Buildup** | Unwanted single frequency growing into sine-like tone in tail. Prevented by AntiRes system. (Heard on the Wellspring's BBD delay feedback, not its spring.) |
| **AntiRes** | Layered anti-buildup system (SPEC §4.10). Adaptive suppressor layer is conditional (ADR 0010). |
| **Micro-mod floor** | Always-on tiny Loop delay modulation, active even with WOBBLE at noon (still). Part of AntiRes. |
| **Drift** | WOBBLE left of noon: smooth random pitch movement (wow + flutter) that never repeats. Gentle near noon (held chords in tune, the Springs drift together), clearly out of tune fully left (ADR 0034; was ADR 0008's lower half). |
| **Warble** | WOBBLE right of noon: a sine LFO's periodic pitch wobble, its rate drifting only slightly, up to worn-tape "clearly out of tune" fully right (ADR 0034; was ADR 0008's top quarter). |
| **Wow** | The slow part of Drift: a random pitch sway, ~0.2–1.5 Hz, its speed itself wandering (tape-speed drift). |
| **Flutter** | The fast part of Drift: a smaller, quicker random pitch shimmer, ~5–12 Hz, on top of the wow. |
| **Tank-level stage** | Processing shared by all Springs: DriveIn, Tilt, DriveOut, output limiter. Contrast with Loop contents, which are per Spring. |
| **Stimulus** | Generated, deterministic test input (`tools/make_stimulus.py`): clicks, hits, sweep, skank, noise bursts. |
| **Reference recording** | Wellspring "spring only" (delay DRY/WET dry, MAGIC zero, SPRINGS wet), stereo wet L/R, recorded through the Stimulus. Target for comparison, not for cloning (ADR 0009). |
| **Benchmark recording** | Strymon Magneto (digital spring + tape) recorded through the Stimulus. Quality bar and WOBBLE/DRIVEN calibration, not a target for spring character (ADR 0020). |
| **IR library** | Spring impulse responses from Ableton Live (45 real-tank files), used only to calibrate parameter ranges. Linear snapshots, no transient behaviour (ADR 0021). |
| **Morph** | Switch change applied to live tail without restarting it (Attitude changes). |
| **Core** | Platform-independent DSP + ParamSpec. No libDaisy/JUCE. |
| **Host** | Wrapper feeding audio + params to Core: Renderer, Plugin, Firmware. |
| **Renderer** | Offline CLI host: WAV in → WAV out, sweeps, metrics. |
| **Plugin** | JUCE AU/VST3 host. Test bench, not product (v1). |
| **Firmware** | Versio host. |
| **ParamSpec** | Single table defining every parameter, range, curve, smoothing. |
| **Normalised value** | 0–1 parameter value every Host passes to Core (= Versio knob+CV reading). |
