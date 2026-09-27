# RESILIO VERSIO — Firmware Spec

**Name:** Resilio Versio (Latin *resilio*, "I leap back, rebound"). Firmware target name `resilio_versio`.
**Target:** Noise Engineering Versio platform (Electro-Smith Daisy Seed inside)
**Goal:** Dub-flavoured spring reverb. Priority sound = splashy, drippy tank ring-out on a single snare/rim hit, including "kicked tank" chaos.
**Status:** Spec v0.3, 27 Sep 2026. Grill pass in progress. Vocabulary: `CONTEXT.md`. Decisions: `docs/adr/`. Then milestone acceptance criteria via interview (§12), then hand to Claude Code.

### Changelog
- v0.3 — Grill round 1: ADRs 0001–0005 (DECAY fades, KICKED Howl, switch-change behaviour, plugin = test bench, fixed Kick strength). Added CONTEXT.md glossary.
- v0.2 — Added forum research + design principles (§2), TONE reworked as tilt "hero" control, multi-stage DRIVE voicing (§4.9), anti-resonance / anti-buildup system (§4.10), three-host architecture with shared parameter layer + JUCE plugin (§6), revised milestones (§7).
- v0.1 — Initial spec. Corrected knob count to 7.

---

## 1. Hardware facts (verified 27 Sep 2026)

| Item | Detail | Source |
|---|---|---|
| MCU | STM32H750 Cortex-M7, 480 MHz (boost), 400 MHz without | docs.daisy.audio/hardware/Seed, NIME 2021 Oopsy paper |
| Memory | 64 MB SDRAM, 8 MB QSPI flash | same |
| Knobs | **7** knobs (`KNOB_0`…`KNOB_6`), each paired with a CV jack | libDaisy `src/daisy_versio.h` |
| Switches | 2 × 3-position toggles (`SW_0`, `SW_1`, type `Switch3`) | same |
| Button | 1 momentary (`tap`) | same |
| Gate in | 1 (`gate`), triggers above ~+2 V | same; NE manuals |
| LEDs | 4 × RGB (`LED_0`…`LED_3`) | same |
| Audio | Stereo in / stereo out | NE Desmodus Versio manual |
| CV | 0–5 V; pots act as offsets summed with CV → firmware reads a single 0–1 value per knob | NE Ampla/Electus manuals |

HAL: `daisy::DaisyVersio` in libDaisy. DSP helpers: DaisySP.

---

## 2. Sound target

### 2.1 Core characteristics

1. **Chirp / "boing"** — dispersive low-frequency chirps repeating at the tank round-trip time. High frequencies arrive before lows.
2. **Splash** — dense, bright, noisy wash on hard transients. Real-world cause: springs driven hard, clattering against each other and the housing.
3. **Drip / kick** — the dub move: physically hitting the tank → huge low thump + chaotic crash.
4. **Dark, dampened tail** — dub spring is rarely bright in the tail; HF rolls off fast.
5. **Warm, driven colour** — tape/transducer saturation, not clean digital.

Reference listening (for tuning, not sampling): King Tubby / Lee Perry-era dub mixes, Fender-style amp spring tanks, Roland RE-201 spring section, Basic Channel-style dub techno.

### 2.2 Community research (forums, Sep 2026)

What players value in classic dub springs:
- Descriptors: **"drippy," "liquid," "splashy."** Short tanks prized when drippy.
- Spring ≠ room reverb. It has its own colour; users want that colour, not realism.
- **Transient interaction is the magic** — sudden stabs bring out character in tails. Percussive hits + springs = core use.
- **Banging the tank for "thunder"** at musical moments is the most-cited dub technique ("instant King Tubbyism") → validates KICK as a core control.

Intellijel Springray / Springray² (real-tank Eurorack module) — community feedback used as design contrast:
- Often needs driving very hot (Drive ≥ 3 o'clock) before the spring is audible.
- Common complaint: reverb feels "not there," then tips into uncontrollable feedback → **narrow usable range**.
- Most-praised feature: **tilt / parametric EQ** — strongly affects how present the reverb sits.
- Voltage control of parameters valued over passive modules (e.g. Doepfer A-199).

Owner's own hardware tank (Wellspring) issue:
- **Single frequency builds up into a sine-like ringing tone** in the tail, distracting from spring character. Owner currently spends the onboard filter taming it. → Must be designed out (§4.10).

### 2.3 Design principles (derived)

1. **Wide sweet spot.** Every knob position should sound usable. No dead zone, no cliff edge into runaway feedback.
2. **Characterful drive, not a fight.** Drive adds colour and splash with automatic level compensation; never needed just to make the reverb audible.
3. **TONE is a hero control.** Powerful tilt, not a subtle damping filter.
4. **No ringing single tones.** Tail stays spring-textured at all settings, without spending TONE to fix it.
5. **Transients are the instrument.** Hits should visibly change behaviour (splash, jolt, kick).
6. **Everything CV-able** (hardware gives this for free on all 7 knobs).

---

## 3. Panel map

### Knobs (all CV-able, 0–5 V + pot offset)

| # | Name | Function | Notes |
|---|---|---|---|
| K0 | **DECAY** | Tail length (feedback gain) **+ coupled tank length** (§4.4) | Core "size" macro. Always fades, max ~8–10 s (ADR 0001). KICKED: top ~10% enables Howl (ADR 0002) |
| K1 | **TONE** | Bipolar tilt. CCW = dark dub (loop damping LPF down, tilt toward lows); noon = neutral; CW = bright/splashy (HF path up, tilt toward highs) | Hero control (§2.3.3). Tilt applied pre-tank (changes what excites springs) + damping in loop |
| K2 | **BOING** | Dispersion amount: allpass coefficient `a` + number of active stages | CCW smeared/diffuse, CW exaggerated chirp |
| K3 | **SPLASH** | Transient sensitivity of nonlinear clatter model (§4.5) | Behaviour scales with ATTITUDE |
| K4 | **DRIVE** | Input gain into drive chain (§4.9); also feeds transient detector | Auto level-compensated |
| K5 | **WOBBLE** | Macro: depth of slow random + LFO modulation of tank delay; rate rises gently with depth | Min floor always on (§4.10) |
| K6 | **MIX** | Dry/wet, equal-power | Full CW = 100% wet for send/return |

### Switches

| Switch | Left | Centre | Right |
|---|---|---|---|
| SW0 **SPRINGS** | 1 spring — sparse, most splashy | 2 springs — classic tank | 3 springs — dense, smooth |
| SW1 **ATTITUDE** | CLEAN — linear tank, light transducer colour | DRIVEN — tape saturation, moderate clatter | KICKED — hard drive in loop, full chaos, collisions, Howl allowed |

Switch changes: ATTITUDE Morphs the live tail (all attitude params smoothed); SPRINGS crossfades ~20 ms (ADR 0003).

### Button + Gate

- **Button = KICK.** Injects "tank kick" impulse (§4.6). Momentary. Fixed strength, scaled by ATTITUDE (ADR 0005). Hold = sustained rattle (stretch goal).
- **Gate in = KICK.** Same as button. Digital on/off input — no velocity. Sequencer/envelope can hit tank rhythmically.

### LEDs (proposal)

- LED_0: input level / drive clip (green → red)
- LED_1: tank energy (brightness = wet RMS)
- LED_2: SPRINGS mode colour
- LED_3: ATTITUDE mode colour; flashes white on KICK
- Boot pattern: unique colour sequence confirming firmware loaded (NE convention).

---

## 4. DSP architecture

### 4.1 Basis

Parametric model from **Välimäki, Parker & Abel, "Parametric Spring Reverberation Effect," JAES 58(7/8), 2010**, with cost reductions from **Parker, "Efficient Dispersion Generation Structures for Spring Reverb Emulation," EURASIP JASP 2011** (multirate/multiband; reported ~⅓ original cost). Physical background: Parker & Bilbao, "Spring Reverberation: A Physical Perspective," DAFx-09.

Claude Code should read these papers before implementing. Välimäki structure: two parallel paths — low-frequency chirps + faster wideband echoes.

### 4.2 Per-spring structure

```
                 ┌──────────────── LOW-CHIRP PATH (C_lf) ────────────────────────────────┐
in ─ DriveIn ─ Tilt ┤ + ─ DCblock ─ SpectralDelay(stretched AP × M) ─ LPF(tone) ─ AntiRes ─ Delay(L+mod) ─┐
                 │ ▲                                                                                 │
                 │ └──────── g_lf(decay) × LoopSat(attitude) ◄───────────────────────────────────────┘
                 │
                 └──────────────── HIGH PATH (C_hf) ─────────────────────────────┐
                   + ─ SpectralDelay(short AP chain) ─ HPF ─ Delay(L_hf+mod) ─ ┘ (feedback g_hf)

out_spring = DriveOut( C_lf + hf_level(tone) × C_hf )
```

- **Spectral delay filter:** cascade of M interpolated stretched allpass sections. Each = Schroeder-style allpass with embedded delay K−1 samples + first-order fractional-delay allpass. K sets chirp spacing; `a` sets chirp steepness.
- **DC blocker** in low-chirp loop (paper uses ~40 Hz).
- **DriveIn / DriveOut / LoopSat** = drive chain (§4.9). **AntiRes** = resonance suppressor (§4.10).

### 4.3 Multiple springs (SW0)

- 1/2/3 instances of §4.2 in parallel, **detuned** L, K, `a` per spring (±3–8%, tune by ear). Detuning = beating + density, and helps prevent shared resonances (§4.10).
- Stereo: Spring A → L, B → R, C centre with small cross-feed. 1-spring mode: decorrelate R with short allpass diffuser.
- Input summed to mono before tank (real tanks are mono). Dry path stays stereo.
- ~20 ms crossfade on spring-count change.

### 4.4 DECAY ↔ tank-size coupling (agreed decision)

| DECAY | Loop delay L | Stretch K | Feedback g | Effect |
|---|---|---|---|---|
| CCW | short (~30 ms) | small | low | short pingy tank, sparse chirps |
| CW | long (~80–100 ms) | larger | high (clamped <1) | long tank, clustered dense ring |

Values = **starting guesses, not from literature.** Exponential curves on L and T60. Fractional-delay interpolation + slew limiting on L.

### 4.5 SPLASH / ATTITUDE nonlinear model

1. **Transient detector:** fast env (~1 ms attack) − slow env (~50 ms) on driven input → `hit` 0–1. SPLASH sets sensitivity.
2. **Clatter injection:** on `hit`, bandpassed noise bursts (1–6 kHz, ~5–30 ms decay) into HIGH path, ∝ hit × SPLASH, few-ms timing jitter.
3. **Coefficient jolt:** momentary modulation of `a` and L ∝ hit (decays ~50–200 ms) → chirp smear / pitch lurch.
4. **Loop saturation:** per §4.9.

| Mode | Loop sat | Clatter | Jolt |
|---|---|---|---|
| CLEAN | off | off | off (SPLASH = mild HF emphasis only) |
| DRIVEN | gentle tape | moderate | small |
| KICKED | hard, asymmetric | full | large + energy-dependent rattle |

### 4.6 KICK (button + gate)

Inject into tank input (post-drive):
- Low thump: decaying sine ~40–80 Hz, ~20–40 ms
- Broadband noise burst ~10 ms
- Forces maximal SPLASH jolt
Level scales with ATTITUDE. Debounce button; rising-edge on gate.

### 4.7 WOBBLE

Slow modulation of L per spring, independent phases: sine LFO (0.1–2 Hz, scaled by knob) + smoothed random. Max depth ~0.5–1% of L. **Minimum floor always active** even at knob = 0 (§4.10).

### 4.8 Output stage

- Wet: gentle high-shelf cut + limiter.
- MIX: equal-power.
- Denormal protection (FTZ; tiny noise if needed).

### 4.9 DRIVE voicing — "characterful, not a fight"

Physical rationale: a real driven tank colours sound in three places — the **input transducer** (electromagnetic driver coil), the **springs/loop**, and the **output pickup transducer**. Dub rigs then often add **tape** (Space Echo / Echoplex lineage). Model the chain, not a distortion pedal in front of a reverb.

| Stage | Where | Character | Model (starting point) |
|---|---|---|---|
| **Input transducer** | DriveIn, pre-tank | Mid-forward, slightly gritty, soft magnetic saturation; LF + HF loss | Band-limit (HPF ~80–150 Hz, LPF ~5 kHz, tunable) → soft asymmetric saturator (e.g. biased tanh) |
| **Tape** | DriveIn, after transducer | Rounded peaks, gentle compression, HF smear increasing with drive | Pre-emphasis → soft saturator → de-emphasis; drive-dependent LPF; optional light hysteresis approximation |
| **Loop saturation** | Inside feedback | Keeps feedback bounded; adds thickness as tail builds | tanh-style; asymmetric in KICKED (adds even harmonics, "valve-ish") |
| **Output pickup** | DriveOut | Subtle second transducer colour | Light soft-clip + band-limit |

ATTITUDE sets which stages engage and how hard:

| ATTITUDE | Transducers | Tape | Loop sat |
|---|---|---|---|
| CLEAN | light | off | off |
| DRIVEN | medium | **on** (core dub colour) | gentle, symmetric |
| KICKED | hard | on, hot | hard, asymmetric |

Requirements:
- **Automatic gain compensation** on DRIVE: perceived loudness roughly stable across knob range; DRIVE changes colour, not volume. (Directly addresses Springray "must drive hot to hear it" complaint.)
- Reverb clearly audible at DRIVE = 0 with typical Eurorack levels.
- **Oversample nonlinear stages ×2 (min)** to limit aliasing. Include in CPU budget (§5).
- Optional research reference for tape modelling (verify before use): J. Chowdhury, "Real-time Physical Modelling for Analog Tape Machines," DAFx-19. Full hysteresis model likely too heavy for Daisy; use simplified version.

### 4.10 Anti-resonance / anti-buildup system

**Problem:** feedback loop with slightly excess gain at one frequency → that mode reinforces every pass → sine-like ringing tone dominates tail (observed on owner's Wellspring). Must be prevented **without using TONE**.

Layered defence, in priority order:

1. **Even loop gain by design.** Loop gain per spring kept below target at *all* frequencies, not just on average. Tone/damping filters designed so no band peaks above others. Unit test measures loop magnitude response across the band.
2. **Always-on micro-modulation.** Tank delay L modulated continuously by slow smoothed random at a small floor depth (starting ~0.05–0.1% of L), even with WOBBLE at 0. Resonant frequencies keep moving → no mode can lock in. Depth below audible pitch wobble (confirm by ear).
3. **Spring detuning** (§4.3): springs don't share exact modes → no common reinforcement.
4. **Adaptive resonance suppressor (AntiRes block).** Safety net if a mode still pokes out:
   - Detector at control rate (not per sample): e.g. small FFT on wet tail in main loop, or bank of bandpass energy trackers. Flags narrowband peak exceeding broadband level by threshold.
   - Response: dynamic peaking-cut biquad **inside the loop** at detected frequency; depth ramps in (up to ~−6 to −12 dB), releases when peak subsides. Max 2–3 simultaneous notches.
   - Must be inaudible on normal material — only acts on runaway modes.
5. **Loop saturation** (§4.9) bounds energy as last resort.

Measurable criterion (starting thresholds — tune/confirm in interview):
- Impulse + noise-burst input, DECAY max, all SPRINGS × ATTITUDE combos, WOBBLE 0. KICKED Howl zone excluded from peak test but must still show no sustained pure sinusoid (ADR 0002).
- In tail from 1 s onward: no narrowband peak > **12 dB** above median of 1/3-octave-smoothed spectrum.
- No sustained sinusoid (> 2 s) above −30 dBFS.
- Renderer (§6) reports this metric automatically.

---

## 5. Performance budget

- 48 kHz, block 48 initial. 480 MHz ÷ 48 kHz ≈ **10,000 cycles/sample**.
- Target **≤ 65% CPU** worst case (3 springs, KICKED, max BOING, max DRIVE).
- Main costs: allpass cascades, oversampled nonlinear stages. Mitigations:
  1. Delay lines + filter state in internal SRAM, not SDRAM.
  2. Decimated low-chirp path (×2/×4) per Parker 2011.
  3. Fewer stages per spring in 3-spring mode.
  4. Oversampling only on nonlinear blocks; cheap polyphase halfband filters.
  5. AntiRes detector at control rate in main loop, not audio callback.
  6. `-O3`, float only, CMSIS-DSP where useful.
- libDaisy `CpuLoadMeter` + serial logging during dev.
- **Unverified:** achievable M and oversampling factor. Decide after hardware profiling milestone.

---

## 6. Architecture: one DSP core, three hosts

Same DSP code, three wrappers. Hosts differ only in where audio + parameters come from.

```
                 ┌─────────────────────────────┐
                 │ core/                       │
                 │  params/ ParamSpec table    │  ← single source of truth
                 │  dsp/    Spring, Tank, ...  │  ← no platform deps
                 └──────┬──────────┬───────────┘
                        │          │           │
        ┌───────────────┘          │           └───────────────┐
  host/render (CLI)        plugin/ (JUCE AU+VST3)        firmware/ (Versio)
  WAV in → WAV out         live in Ableton               real knobs, CV, gate
  sweeps, overnight        automation, A/B               final feel + CPU
```

### 6.1 Shared parameter layer (`core/params/`)

- One table defines every parameter: id, display name, normalised range 0–1, mapping curve to internal units, default, smoothing time.
- **Every host passes normalised 0–1 values** (same as Versio knob + CV reading). Mapping curves live only here → a setting in the plugin sounds identical on the module.
- Switches = 3-state enums. KICK = trigger event.
- Presets/test settings stored as JSON of normalised values → portable across all hosts.

### 6.2 Host A — offline renderer (`host/render`)

- CLI: input WAV + JSON params → output WAV.
- **Automation:** parameter breakpoints over time (e.g. SPLASH ramp, KICK events at timestamps).
- **Sweep mode:** grid over params (e.g. DECAY × ATTITUDE × SPRINGS) → batch of WAVs + manifest (JSON/CSV) naming each file's settings. For overnight runs.
- **Metrics per render:** peak, RMS, estimated T60, resonance peak ratio (§4.10), NaN/Inf count, clip count.
- Deterministic: all randomness seeded → same input + params = bit-identical output.
- Test suite (`host/tests`): impulse chirp check, stability at max settings, NaN/denormal, loop magnitude response, resonance criterion.

### 6.3 Host B — JUCE plugin (`plugin/`)

- Formats: **Audio Unit + VST3** (Mac, Ableton).
- Parameters generated from ParamSpec table: 7 knobs as float params, 2 switches as 3-choice params, KICK as button param.
- **KICK also triggered by MIDI note** (any note, velocity ignored — ADR 0005) → sequence kicks from Ableton clips, mirrors Gate in.
- Test bench only in v1: generic parameter UI, no custom graphics (ADR 0004).
- Sample rate: core must be sample-rate-aware. Reference/validation rate = 48 kHz (matches Daisy). Tuning decisions checked at 48 kHz.
- Used for: live sound design, automation, A/B against other spring plugins/hardware.
- JUCE via CMake. **Check current JUCE licence terms at build time (unverified).**

### 6.4 Host C — Versio firmware (`firmware/`)

- libDaisy `DaisyVersio`; reads knobs/CV (0–1), switches, button, gate → ParamSpec → core.
- LEDs, boot pattern, CPU meter.
- Standard libDaisy Makefile.

### 6.5 Repo layout

```
resilio-versio/
  CMakeLists.txt            # desktop: core + render + tests + plugin
  core/
    params/ParamSpec.h      # parameter table + mapping curves
    dsp/
      StretchedAllpass.h
      SpectralDelay.h
      Spring.h/.cpp
      SpringTank.h/.cpp
      Splash.h/.cpp
      Kick.h
      Wobble.h
      Drive.h/.cpp          # transducer, tape, loop sat, output pickup (§4.9)
      AntiRes.h/.cpp        # resonance detector + dynamic notches (§4.10)
      Oversampler.h
  host/
    render/main.cpp
    tests/
  plugin/                   # JUCE AU/VST3
  firmware/
    Makefile
    main.cpp
    Controls.h/.cpp
    Leds.h/.cpp
  test_audio/               # impulse, snare, rimshot, dub skank, chord stab
  presets/                  # JSON param sets
```

**Rule:** `core/` has zero platform includes (no libDaisy, no JUCE).

---

## 7. Build milestones

Acceptance criteria below are **drafts**. Final criteria to be written via interview (§12).

| # | Milestone | Done when (draft) |
|---|---|---|
| 0 | Toolchains | Desktop CMake builds empty core + renderer; Daisy toolchain flashes Versio passthrough; knobs/switches/button/gate print over serial |
| 1 | Core: single spring, CLEAN + renderer | Impulse render shows repeating dispersive chirps; stable at max DECAY; deterministic output; tests pass |
| 2 | JUCE plugin shell | Plugin loads in Ableton (AU + VST3); all params visible + automatable; MIDI note triggers KICK |
| 3 | Hardware profiling | 1 spring on Versio; CPU logged; decide M, decimation, oversampling |
| 4 | Multi-spring, stereo, DECAY coupling | SW0 click-free; DECAY sweeps short/pingy → long/dense |
| 5 | Drive chain + TONE tilt | All ATTITUDE drive stages; level-compensated DRIVE; reverb audible at DRIVE 0 |
| 6 | Anti-resonance system | §4.10 criterion passes across full sweep grid |
| 7 | SPLASH + KICK + WOBBLE + MIX | Snare in KICKED crashes; gate/MIDI kicks work; all 7 knobs respond to CV 0–5 V on hardware |
| 8 | Tuning pass | A/B vs reference dub recordings + owner's hardware tank; final ranges/curves |
| 9 | Polish | LEDs, boot pattern, panel overlay, README/manual |

---

## 8. Flashing / distribution — open questions (research at M0)

- Custom libDaisy firmware via NE firmware updater, or only Daisy web programmer / DFU? Community index notes some Versio firmwares need a different bootloader (flash.daisy.audio). **Unverified — confirm at M0.**
- Internal flash (128 KB) vs app size — bootloader build (`APP_TYPE`) needed? **Unverified.**
- NE offers blank panel + DXF overlay templates (World of Versio). Overlay at M9.

---

## 9. Risks

| Risk | Mitigation |
|---|---|
| CPU too high (cascades + oversampling) | Multirate lf path; fewer stages in 3-spring mode; oversample only nonlinear blocks |
| Instability at max DECAY + KICKED | Loop sat, g clamp, limiter, NaN guard resets tank |
| Single-tone buildup | §4.10 layered defence + automated metric |
| Drive = volume jump, not colour | Gain compensation; test loudness across DRIVE sweep |
| Narrow sweet spot (Springray complaint) | Perceptual curves; sweep renders reviewed for dead zones/cliffs |
| Zipper noise | Smoothing on all params; slew-limit L |
| Sounds "digital reverb" | Chirp correctness first (M1); tune vs references |
| Clatter = added noise | Feed clatter through tank HF path |
| Plugin ≠ hardware sound | Shared ParamSpec; validate at 48 kHz; compare renders vs hardware recordings |
| Aliasing from saturation | Oversampling; test with high-freq sine sweeps |

---

## 10. Open questions (for grill pass)

- Exact TONE tilt curve and pivot frequency?
- AntiRes: FFT-based vs filter-bank detector — CPU/latency tradeoff on Daisy?
- Hold-for-rattle on button: in v1 or later?

---

## 11. References

1. V. Välimäki, J. Parker, J. S. Abel — "Parametric Spring Reverberation Effect," *JAES* 58(7/8), 547–562, 2010.
2. J. Parker — "Efficient Dispersion Generation Structures for Spring Reverb Emulation," *EURASIP J. Adv. Signal Process.*, 2011.
3. J. Parker, S. Bilbao — "Spring Reverberation: A Physical Perspective," DAFx-09, Como.
4. J. S. Abel, D. P. Berners, S. Costello, J. O. Smith — "Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure," AES 121st Conv., 2006.
5. "Automated Calibration of a Parametric Spring Reverb Model," DAFx-11 — https://www.dafx.de/paper-archive/2011/Papers/39_e.pdf
6. libDaisy Versio header — https://github.com/electro-smith/libDaisy/blob/master/src/daisy_versio.h
7. NE: Create your own Versio firmware — https://noiseengineering.us/blogs/loquelic-literitas-the-blog/create-your-own-firmware-on-a-versio-module/
8. Versio firmware index — https://github.com/Maxhodges/noise-engineering-firmware-index
9. Forum research (dub spring character, Springray feedback): Gearspace dub/spring threads; ModWiggler "Which spring reverb should I get?", "Intellijel Springray 2?" threads.
10. (Verify before use) J. Chowdhury — "Real-time Physical Modelling for Analog Tape Machines," DAFx-19.

---

## 12. Next steps before Claude Code handoff

1. Run gap-review ("Grill Me With Docs" skill) over this spec; patch gaps.
2. Milestone-criteria interview: plain-language musical questions → technical acceptance criteria per milestone.
3. Freeze spec v1.0 → hand to Claude Code, starting M0.
