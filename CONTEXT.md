# RESILIO VERSIO — Context / Glossary

Shared vocabulary for this project. Use these terms exactly in code, comments, and conversation. Spec: `SPEC.md`. Decisions: `docs/adr/`.

| Term | Meaning |
|---|---|
| **Tank** | Whole reverb engine: 1–3 Springs + shared input/output stages. Mirrors a physical spring reverb tank. |
| **Spring** | One simulated spring: low-chirp path + high path (SPEC §4.2). |
| **Loop** | A Spring's feedback path (delay + allpass cascade + filters). Where energy recirculates. |
| **Chirp / Boing** | Dispersive echo where highs arrive before lows. Signature spring sound. Controlled by BOING. |
| **Splash** | Bright, noisy wash on hard transients. Produced by Clatter + Jolt. Controlled by SPLASH. |
| **Clatter** | Injected noise bursts simulating springs hitting each other/housing. Part of Splash. |
| **Jolt** | Momentary lurch of Loop parameters on impact. Part of Splash. |
| **Hit** | Detected transient strength (0–1) from input. Drives Clatter + Jolt. |
| **Kick** | Simulated physical strike on Tank (thump + crash). Triggered by button, Gate, or MIDI note. Fixed strength, scaled by Attitude. |
| **Attitude** | SW1 mode: CLEAN / DRIVEN / KICKED. Sets drive stages, Clatter, Jolt, self-oscillation permission. |
| **Drive chain** | Input transducer → tape → Loop saturation → output pickup (SPEC §4.9). |
| **Howl** | Controlled self-sustaining feedback. KICKED only, top ~10% of DECAY. Noisy/crashing, never a pure tone. |
| **Ringing / Buildup** | Unwanted single frequency growing into sine-like tone in tail. Prevented by AntiRes system. |
| **AntiRes** | Layered anti-buildup system (SPEC §4.10). |
| **Micro-mod floor** | Always-on tiny Loop delay modulation, active even at WOBBLE 0. Part of AntiRes. |
| **Morph** | Switch change applied to live tail without restarting it (Attitude changes). |
| **Core** | Platform-independent DSP + ParamSpec. No libDaisy/JUCE. |
| **Host** | Wrapper feeding audio + params to Core: Renderer, Plugin, Firmware. |
| **Renderer** | Offline CLI host: WAV in → WAV out, sweeps, metrics. |
| **Plugin** | JUCE AU/VST3 host. Test bench, not product (v1). |
| **Firmware** | Versio host. |
| **ParamSpec** | Single table defining every parameter, range, curve, smoothing. |
| **Normalised value** | 0–1 parameter value every Host passes to Core (= Versio knob+CV reading). |
