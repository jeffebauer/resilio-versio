# RESILIO VERSIO — Firmware Spec

**Name:** Resilio Versio (Latin *resilio*, "I leap back, rebound"). Firmware target name `resilio_versio`.
**Target:** Noise Engineering Versio platform (Electro-Smith Daisy Seed inside)
**Goal:** Dub-flavoured spring reverb. Priority sound = splashy, drippy tank ring-out on a single snare/rim hit, including "kicked tank" chaos.
**Status:** Spec **v1.0 (frozen)**, 27 Sep 2026. Vocabulary: `CONTEXT.md`. Decisions: `docs/adr/` (0001–0019). Changes after freeze: new ADR + changelog entry. Tuned numbers replace "starting guesses" as milestones confirm them.

### Changelog
- v1.0.8 — §10: stereo-in recorded as an open question for after M3 (incl. plugin-only as a Core mode). Recipe: optional single-tank takes A-L / A-R.
- v1.0.7 — M6: ADR 0023 (Ringing judged by calibrated `ringing_db` < 15 dB, replacing the 12 dB peak test); ADR 0019 floor band 200 Hz–2 kHz; Micro-mod floor set to ±0.05 % of L at ~0.2 Hz.
- v1.0.6 — ADR 0022: DRIVE retune targets after owner listening (obvious from noon, cranked tape/tank at max, level constant); M5 criteria extended.
- v1.0.5 — ADR 0021: Ableton spring IRs as an IR library for range calibration; `tools/ir_analysis.py`.
- v1.0.4 — M4 mono-safe criterion made precise: fold-down energy vs stereo energy (the old wording conflicted with the width criterion).
- v1.0.3 — ADR 0020: Strymon Magneto recorded as a benchmark (not a target) + WOBBLE/DRIVEN calibration source. `08_held_tones` stimulus.
- v1.0.2 — Fact update: Wellspring manual read. It has no spring decay control (fixed T60, one reference point), stereo springs, INPUT = drive. ADR 0009 amended, recipe rewritten, M1/M5 reference checks clarified.
- v1.0.1 — Fact update only: toolchain verified (§8.1), firmware size watch item.
- v1.0 — Frozen. Milestone acceptance criteria from owner interview (§7). ADR 0019 (Howl may lean to pitch). LED_3 Kick flash dropped. §12 complete.
- v0.4 — Grill round 2: ADRs 0006–0018 (min DECAY slap, BOING always spring, WOBBLE zones, Wellspring reference recordings, AntiRes rescoped after Wellspring correction, NE-app flashing, DECAY bend, hold-rattle deferred, DRIVE onset, smoothing tiers, Kick character, TONE range, Howl exit). Tank-level vs Spring-level stages clarified (§4.2). Flashing research (§8). Toolchain facts.
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

Owner's own hardware (Wellspring, Teaching Machines: desktop stereo BBD delay + stereo spring reverb, two tanks each with a pair of 15" springs; the only spring control is SPRINGS dry/wet, with no decay control; INPUT drives it into distortion):
- *Correction (v0.4):* the sine-like **Ringing** the owner hears on the Wellspring comes from the **BBD delay's feedback network**, not the spring. The spring alone has not shown frequency buildup.
- Still relevant: each simulated Spring is itself a delay line with feedback, the same structure that rings in the delay. Digital loops don't have analog noise and drift to break modes up, so the risk is real but unproven for this model. → Designed out by construction and measured automatically (§4.10, ADR 0010).

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
| K0 | **DECAY** | Tail length (feedback gain) **+ coupled tank length** (§4.4) | Core "size" macro. Range ~0.3–0.5 s tight slap → ~8–10 s, always fades (ADR 0001, 0006). KICKED: top ~10% enables Howl (ADR 0002), exits naturally (ADR 0018). Turning it bends the live tail's pitch (ADR 0012) |
| K1 | **TONE** | Bipolar tilt. CCW = dark dub (loop damping LPF down, tilt toward lows); noon = neutral; CW = bright/splashy (HF path up, tilt toward highs) | Hero control (§2.3.3). Tilt applied pre-tank (changes what excites springs) + damping in loop. CCW warm dub dark, CW splashy never harsh (ADR 0017) |
| K2 | **BOING** | Dispersion amount: allpass coefficient `a` + number of active stages | CCW soft/washy but still a spring, CW exaggerated chirp (ADR 0007) |
| K3 | **SPLASH** | Transient sensitivity of nonlinear clatter model (§4.5) | Behaviour scales with ATTITUDE |
| K4 | **DRIVE** | Input gain into drive chain (§4.9); also feeds transient detector | Auto level-compensated. Clean-ish to ~9 o'clock, driven by ~3 o'clock (ADR 0014) |
| K5 | **WOBBLE** | Macro: depth of slow random + LFO modulation of tank delay; rate rises gently with depth | Lower half Drift, top quarter Warble (ADR 0008). Min floor always on (§4.10) |
| K6 | **MIX** | Dry/wet, equal-power | Full CW = 100% wet for send/return |

### Switches

| Switch | Left | Centre | Right |
|---|---|---|---|
| SW0 **SPRINGS** | 1 spring — sparse, most splashy | 2 springs — classic tank | 3 springs — dense, smooth |
| SW1 **ATTITUDE** | CLEAN — linear tank, light transducer colour | DRIVEN — tape saturation, moderate clatter | KICKED — hard drive in loop, full chaos, collisions, Howl allowed |

Switch changes: ATTITUDE Morphs the live tail (all attitude params smoothed); SPRINGS crossfades ~20 ms (ADR 0003).

CV/knob smoothing: snappy (~5 ms) for MIX, DRIVE, SPLASH, TONE; gliding (~50–100 ms) for DECAY, WOBBLE, BOING (ADR 0015).

### Audio I/O

- Hardware normals In L → In R when R is unpatched (NE Versio manuals). Mono-in works with no firmware logic; libDaisy has no jack detection.
- Outputs not normalled.

### Button + Gate

- **Button = KICK.** Injects "tank kick" impulse (§4.6). Momentary. Fixed strength, scaled by ATTITUDE (ADR 0005). Tight thud + big crash (ADR 0016). Hold does nothing extra in v1 (ADR 0013).
- **Gate in = KICK.** Same as button. Digital on/off input — no velocity. Sequencer/envelope can hit tank rhythmically.

### LEDs

- LED_0: input level / drive clip (green → red)
- LED_1: tank energy (brightness = wet RMS)
- LED_2: SPRINGS mode colour
- LED_3: ATTITUDE mode colour (no Kick flash; owner choice, M9 interview)
- Boot pattern: unique colour sequence confirming firmware loaded (NE convention).

---

## 4. DSP architecture

### 4.1 Basis

Parametric model from **Välimäki, Parker & Abel, "Parametric Spring Reverberation Effect," JAES 58(7/8), 2010**, with cost reductions from **Parker, "Efficient Dispersion Generation Structures for Spring Reverb Emulation," EURASIP JASP 2011** (multirate/multiband; reported ~⅓ original cost). Physical background: Parker & Bilbao, "Spring Reverberation: A Physical Perspective," DAFx-09.

Claude Code should read these papers before implementing. Välimäki structure: two parallel paths — low-frequency chirps + faster wideband echoes.

### 4.2 Per-spring structure

**Scope (v0.4):** DriveIn, Tilt and DriveOut are **Tank-level** stages, one copy each, shared by all Springs (input is summed mono first, §4.3). Only the Loop contents (allpass cascade, damping, AntiRes, delay, LoopSat) are per Spring. The diagram shows one Spring with the shared stages drawn around it.

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

Slow modulation of L per spring, independent phases: sine LFO (0.1–2 Hz, scaled by knob) + smoothed random. Max depth ~0.5–1% of L (starting guess, likely too small for ADR 0008's top-quarter Warble; tune by ear). **Minimum floor always active** even at knob = 0 (§4.10).

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

**Problem:** feedback loop with slightly excess gain at one frequency → that mode reinforces every pass → sine-like ringing tone dominates tail. Heard on the owner's Wellspring **delay** feedback (not its spring, §2.2), and structurally possible in any digital feedback loop. Must be prevented **without using TONE**.

**Scope (ADR 0010):** layers 1–3 and 5 are built by default (cheap, by design). Layer 4 (adaptive suppressor) is built **only if** the automated metric below fails at M6 after layers 1–3 are tuned.

Layered defence, in priority order:

1. **Even loop gain by design.** Loop gain per spring kept below target at *all* frequencies, not just on average. Tone/damping filters designed so no band peaks above others. Unit test measures loop magnitude response across the band.
2. **Always-on micro-modulation.** Tank delay L modulated continuously by slow smoothed random at a small floor depth (set at M6: ±0.05 % of L at ~0.2 Hz, ≤ 0.11 cents on held tones), even with WOBBLE at 0. Resonant frequencies keep moving → no mode can lock in. Depth below audible pitch wobble (confirm by ear).
3. **Spring detuning** (§4.3): springs don't share exact modes → no common reinforcement.
4. **Adaptive resonance suppressor (AntiRes block).** *Conditional (ADR 0010).* Safety net if a mode still pokes out:
   - Detector at control rate (not per sample): e.g. small FFT on wet tail in main loop, or bank of bandpass energy trackers. Flags narrowband peak exceeding broadband level by threshold.
   - Response: dynamic peaking-cut biquad **inside the loop** at detected frequency; depth ramps in (up to ~−6 to −12 dB), releases when peak subsides. Max 2–3 simultaneous notches.
   - Must be inaudible on normal material — only acts on runaway modes.
5. **Loop saturation** (§4.9) bounds energy as last resort.

Measurable criterion (starting thresholds — tune/confirm in interview):
- Impulse + noise-burst input, DECAY max, all SPRINGS × ATTITUDE combos, WOBBLE 0. KICKED Howl zone excluded from peak test but must still show no sustained pure sinusoid (ADR 0002).
- In tail from 1 s onward: ~~no narrowband peak > 12 dB above median of 1/3-octave-smoothed spectrum~~ **`ringing_db` < 15 dB** (ADR 0023; the old test failed real tanks).
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

Criteria come from the owner interview (27 Sep 2026). **[A]** = automated (Renderer/tests, must pass in CI-style runs). **[L]** = owner listening check. **[H]** = on hardware. Numbers marked *(start)* are starting thresholds, confirmed by ear at that milestone. Any change is noted in the changelog.

Shared definitions:
- **Stimulus set** = `tools/make_stimulus.py` output. **Reference set** = Wellspring recordings (ADR 0009).
- **Review page** = locally generated HTML per render batch: player + spectrogram + settings + metrics per file, next to the matching Reference file. WAVs also written to a folder for Ableton.
- **Click detector** = Renderer metric flagging sample-to-sample discontinuities above the signal's local high-frequency level *(start: 20 dB above)*.

### M0 — Toolchains
- [A] `cmake` builds empty Core + Renderer + test runner on macOS arm64. The Renderer copies a WAV through unchanged (bit-identical).
- [A] The Plugin target builds AU + VST3 (installing Xcode if Command Line Tools aren't enough). `auval` passes for the AU.
- [A] Firmware builds with `gcc-arm-embedded`. Binary ≤ 128 KB, reported by the build (ADR 0011).
- [H] Test firmware flashed via NE Firmware Swap (custom file). Boot LED pattern shows.
- [H] Passthrough indistinguishable from a patch cable: level within 0.5 dB per channel, no audible added hum/hiss with the gain up, both channels, mono-in on L comes out of both sides.
- [H] Every control verified two ways: LEDs react to each knob, switch, button and gate, and serial prints exact values. Knob+CV reads ≈0 at 0 V / knob CCW and ≈1 at 5 V or knob CW (±0.02). Switches report 3 states. Button and gate edges print once per press (no bounce).

### M1 — Core: one Spring, CLEAN + Renderer
- [A] Click render shows repeating dispersive chirps: spectrogram has descending chirps at a regular repeat time matching the configured L *(±5%)*.
- [A] Stable at every DECAY × BOING corner (grid incl. extremes): no NaN/Inf, no growth, tail decays at max DECAY (ADR 0001). T60 at min DECAY 0.3–0.5 s (ADR 0006), at max 8–10 s.
- [A] BOING 0 still shows a chirp (ADR 0007).
- [A] Deterministic: same input + params → bit-identical output.
- [A] Metrics reported per render: peak, RMS, T60, resonance ratio, NaN/Inf count, clip count, click-detector hits.
- [A] Review page generated for the M1 grid, next to the Wellspring Reference set.
- [L] Owner A/B vs Wellspring clicks (take A), with DECAY set so T60 matches the Wellspring's measured T60 (the Wellspring has no decay control): "same family" (repeating boings, highs before lows, dark tail). Thin/sparse is acceptable at this stage.

### M2 — JUCE Plugin shell
- [A] AU + VST3 load in Ableton. All ParamSpec params visible and automatable. Names/ranges generated from ParamSpec, no hand-written list.
- [A] Kick from any MIDI note on a routed MIDI track, velocity ignored, **sample-accurate** (offline test: note at sample N → Kick onset at N, with reported latency).
- [A] Plugin latency reported to the host. The dry path stays aligned with other tracks in Ableton (null test vs a duplicate track at MIX 0).
- [A] Plugin render == Renderer output for the same stimulus/params at 48 kHz (bit-identical, or within float tolerance −120 dBFS).
- [A] Works at 44.1/48/96 kHz without crashing or detuning (T60 and chirp timing within 5% across rates).

### M3 — Hardware profiling
- [H] One Spring running on the Versio. CPU load logged over serial (average and peak) for each setting corner.
- [H] Decisions recorded as an ADR: M stages, decimation factor, oversampling factor, and the headroom plan to hit ≤ 65% worst case (§5).
- [H] CPU tradeoff order if short: simplify 3-spring mode first (fewer stages per Spring), keeping 1- and 2-spring modes at full detail.
- [H/L] Module vs Plugin: the same stimulus recorded from the module vs rendered on the Mac. The owner can't reliably pick which is which in a blind A/B (ABX: ≤ 12/16 correct). T60 within 5%, 1/3-octave spectrum within 1.5 dB (ignoring converter noise floor).

### M4 — Multi-spring, stereo, DECAY coupling
- [A] SPRINGS switch changes are click-free (click detector) in every combination, mid-tail.
- [A] SPRINGS levels matched: loudness of 1/2/3 within ±1.5 dB for the same input.
- [L] 1 → 2 → 3 sounds sparse/drippy → classic → dense/lush, clearly different in a blind test.
- [A] Stereo: clearly wide (inter-channel correlation of the wet tail < 0.5 *(start)*). Mono-safe: mono fold-down (L+R) energy no more than 1.5 dB below the stereo (L²+R²) energy, i.e. no phase cancellation (definition: `docs/m4-contracts.md`), no comb-filter notches > 6 dB in the 200 Hz–5 kHz band.
- [A] DECAY sweep min→max over 4 s on a held tail: no click-detector hits, smooth pitch bend (ADR 0012), no loudness jump > 3 dB in any 100 ms step.
- [L] DECAY sweep audibly goes short/pingy → long/dense.

### M5 — Drive chain + TONE tilt
- [L] ATTITUDE at DRIVE noon on a snare: CLEAN hi-fi, DRIVEN warm tape dub, KICKED gritty/trashed. Owner picks all three correctly in a blind test.
- [A] ATTITUDE loudness within ±2 dB of each other at the same settings.
- [A] DRIVE audibility (ADR 0022): on `02_hits`, DRIVE 0 vs 0.5 difference ≥ −20 dB in DRIVEN/KICKED; DRIVE 0 vs 1 ≥ −6 dB in KICKED.
- [A] DRIVE sweep 0→max: loudness within ±2 dB (LUFS-style short-term). Clean-ish below ~25%, colour builds to ~85% (ADR 0014), measured as THD rising monotonically.
- [L] DRIVEN at high DRIVE vs Wellspring hot-INPUT hits (take C): comparable warmth/grit character (reference, not a clone).
- [A] Reverb clearly audible at DRIVE 0 with a 10 Vpp-equivalent input (wet within 6 dB of dry at MIX noon).
- [A] Aliasing: a 5–15 kHz sine sweep at max DRIVE/KICKED shows alias products ≤ −60 dB relative to the fundamental.
- [A/L] TONE: chirp still visible and audible at full CCW (ADR 0017). Full CW is splashy, not harsh (owner check on hats/cymbals; energy above 10 kHz capped *(start: ≤ +6 dB vs noon)*). TONE sweep loudness within ±3 dB.

### M6 — Anti-resonance
- [A] §4.10 criterion across the full sweep grid (DECAY max, all SPRINGS × ATTITUDE, WOBBLE 0): `ringing_db` < 15 dB (ADR 0023) and no steady tone > 2 s above −30 dBFS.
- [A] The metric **catches** the owner's Wellspring delay-Ringing recording (take G) if available, or a synthetic ringing loop. Proves the test isn't toothless.
- [A] Howl zone (KICKED, top ~10% DECAY): ADR 0019 criteria (rough, moving, may lean to a pitch, never a steady sine). Exiting the zone drops ≥ 30 dB within ~3 s (ADR 0018).
- [L] Micro-mod floor inaudible: with WOBBLE 0 the owner hears no pitch movement on a held chord stab.
- [A] If any grid cell fails after layers 1–3 are tuned → build layer 4 (ADR 0010) and re-run.

### M7 — SPLASH + KICK + WOBBLE + MIX
- [L] SPLASH max, KICKED, hard snare → big bright crash + pitch lurch that settles into the tail within ~1 s. Ghost notes (−18 dBFS hit in the stimulus) barely trigger it. The −6 dBFS hit clearly does.
- [A] Hit detector monotonic: hit value rises with input level. The −18 dB hit gives < 25% of the −6 dB hit's Clatter energy.
- [L] SPLASH 0 in DRIVEN still gives a faint natural splash on hard hits (not zero).
- [L] Kick = tight thud + big crash (ADR 0016). [A] Energy < 100 Hz down ≥ 20 dB within 300 ms.
- [H] Gate Kicks: every gate at up to 12/s (16ths at 180 bpm) gives exactly one Kick, onset within 1 ms of the gate edge. No double triggers.
- [L] WOBBLE: lower half Drift (held chords in tune), top quarter Warble (clearly out of tune) (ADR 0008).
- [A] WOBBLE pitch deviation (cents, on `08_held_tones`) sits in the range measured from the Magneto's WOW & FLUTTER series (takes MW0–MW4): Drift ≈ the 9 o'clock–noon takes, Warble ≈ the 3 o'clock–fully CW takes.
- [A] MIX: CCW = dry only (null vs input), CW = wet only (no dry leakage > −80 dB), noon = equal-power blend. Sweep loudness within ±1.5 dB.
- [A/H] Envelope on MIX CV (5 ms attack): throw lands with no audible lag (smoothing ≤ 5 ms, ADR 0015).
- [H] All 7 knobs respond to CV 0–5 V over their full range.

### M8 — Tuning pass
All four must hold:
- [L] Sweet-spot sweep: owner reviews a grid of renders stepping every knob. No dead zones, no cliffs, every position usable.
- [L] Dub record A/B: on the owner's own material it sits alongside King Tubby / Basic Channel references without sounding like a "digital reverb".
- [L] Wellspring A/B: same family as the spring Reference set, with less Ringing and more splash.
- [A] IR library (ADR 0021): DECAY, BOING and TONE ranges cover the spread of real-tank T60, chirp spacing/dispersion (ridge method) and brightness.
- [L] Magneto A/B (ADR 0020): holds up next to the Magneto's spring on the same stimulus; the owner would reach for Resilio Versio for dub.
- [H/L] Live session on the module (patching, throws, Kicks): nothing surprises in a bad way.
- Final ranges/curves written back into ParamSpec, and SPEC starting guesses replaced with the tuned values.

### M9 — Polish
- [H] LEDs: LED_0 input level/clip (green → red), LED_1 Tank energy (glows with tail and Howl), LED_2 SPRINGS colour, LED_3 ATTITUDE colour. Boot pattern.
- One-page manual: panel map, controls, flashing via NE Firmware Swap, recovery to NE firmware.
- Panel overlay from NE's blank-panel/DXF template, labelled with Resilio Versio controls.
- Tagged release with `.bin` installable via NE Firmware Swap.
- Preset notes: a few documented starting points for classic dub sounds (as JSON presets + prose).

---

## 8. Flashing / distribution

Research 27 Sep 2026 (sources in §11). Status per item.

- **Connection (confirmed, NE Desmodus Versio manual):** power off, take the module out, **unplug the Eurorack power cable**, plug micro-USB into the Daisy Seed on the back of the module. The module runs on USB power alone. Never have rack power and USB connected at once.
- **Primary path: NE Firmware Swap web app with a custom .bin (verified by owner, who has flashed 1st- and 3rd-party firmwares with it many times):** noiseengineering.us/portal/firmware → "Select Custom File" → CONNECT → CHANGE FIRMWARE. The same app restores stock NE firmware, so recovery is known (ADR 0011).
- **Fallback: Daisy Web Programmer / dfu-util (generic Daisy method, not needed unless NE's app fails):** hold BOOT, tap RESET, release BOOT → STM32 system DFU (in ROM, can't be overwritten → very low brick risk). **Unverified for Versio:** whether the Seed's BOOT/RESET buttons are reachable when mounted, and whether NE's app enters DFU for you. → Check physically at M0 before any flash.
- **App size / bootloader:** internal flash is 128 KB. If the app exceeds it, build with the Daisy bootloader (`APP_TYPE = BOOT_SRAM`, up to 480 KB, runs from SRAM; RAM data then goes in DTCM, only 128 KB → delay lines must be placed explicitly). Install the bootloader with `make program-boot`, then flash apps with `make program-dfu` during the bootloader's LED-pulsing grace period. This is likely why some community Versio firmwares "need a different bootloader" (inferred, not stated by the index). The firmware index says bootloader-based firmwares "will not install through Noise Engineering's firmware updater". **Decision (ADR 0011):** plain internal-flash build (≤ 128 KB) so the NE app works. Binary size is checked on every firmware build.
- **Unverified:** whether restoring NE firmware through NE's app also removes the Daisy bootloader.
- **Flash procedure for M0:** first flash = passthrough + control-print test firmware via NE's app (custom file). Owner flashes; Claude produces the .bin.
- NE offers blank panel + DXF overlay templates (World of Versio). Overlay at M9.

### 8.1 Toolchain facts (27 Sep 2026)

- ARM compiler: `brew install --cask gcc-arm-embedded` (official Arm, native arm64). **Not** the Homebrew formula `arm-none-eabi-gcc` (no newlib → `nosys.specs` error). Daisy Toolchain installer is stale (2022, Intel-era).
- `brew install dfu-util cmake ninja`.
- libDaisy `DaisyVersio` confirmed on master. Quirks: knobs pre-inverted (`flip=true`); `ProcessAllControls()` only processes knobs, so `tap.Debounce()` must be called separately; `Gate()` already inverted; LEDs RGB order, inverted; default block 48, 48 kHz / 24-bit.
- Knob + CV sum clips at 0/1 in the analog stage (ADC rails), not in software. CV range 0–5 V (NE manual). Audio inputs clip ~16 Vpp.
- JUCE: now JUCE 9. Free for this use (Starter tier ≤ $20k/yr revenue, or AGPLv3). **Verified 28 Sep 2026:** AU + VST3 build with Command Line Tools only (CMake + Ninja, JUCE 9.0.2) and the AU passes `auval`. No Xcode needed.
- ARM toolchain in use: Arm GNU Toolchain 15.3.rel1 tarball in `~/.local/arm-gnu-toolchain` (sha256 verified). M0 test firmware = 94 KB of 128 KB. **Watch item for M3:** ~35 KB left for DSP code. Mitigations: drop USB logging in release builds, `-Os` on non-audio code.

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

- ~~Exact TONE tilt curve and pivot frequency?~~ Character decided (ADR 0017); numbers tuned at M5/M8.
- ~~AntiRes detector type?~~ Only needed if layer 4 is built (ADR 0010); decide then.
- ~~Hold-for-rattle?~~ Not v1 (ADR 0013).
- **Stereo in (open, explore after M3).** Should the Tank keep left/right placement from a stereo input instead of summing to mono (§4.3)? Options: per-Spring L/R blend (cheap, one drive stage); full dual input (two DriveIn/Tilt stages, ~+300–700 Daisy cycles/sample); or plugin-only if the Versio lacks CPU headroom. If plugin-only, implement it as a **Core mode** that the firmware doesn't enable, so both hosts still share one Core and mono settings stay identical across hosts (the ParamSpec parity principle, §6.1). A new ADR is needed before building. Data: Wellspring takes A-L / A-R (recipe).
- No other open design questions remain. Tuning numbers marked "starting guess" are confirmed by measurement or ear at their milestone.

---

## 11. References

1. V. Välimäki, J. Parker, J. S. Abel — "Parametric Spring Reverberation Effect," *JAES* 58(7/8), 547–562, 2010.
2. J. Parker — "Efficient Dispersion Generation Structures for Spring Reverb Emulation," *EURASIP J. Adv. Signal Process.*, 2011.
3. J. Parker, S. Bilbao — "Spring Reverberation: A Physical Perspective," DAFx-09, Como.
4. J. S. Abel, D. P. Berners, S. Costello, J. O. Smith — "Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure," AES 121st Conv., 2006.
5. "Automated Calibration of a Parametric Spring Reverb Model," DAFx-11 — https://www.dafx.de/paper-archive/2011/Papers/39_e.pdf
6. libDaisy Versio header — https://github.com/electro-smith/libDaisy/blob/master/src/daisy_versio.h
7. NE: Create your own Versio firmware. NE Firmware Swap: https://noiseengineering.us/portal/firmware/. NE Desmodus Versio manual: https://noiseengineering.us/manuals/desmodus-versio/. Daisy bootloader: libDaisy `doc/md/_a7_Getting-Started-Daisy-Bootloader.md`, https://github.com/electro-smith/DaisyBootloader.
   NE blog — https://noiseengineering.us/blogs/loquelic-literitas-the-blog/create-your-own-firmware-on-a-versio-module/
8. Versio firmware index — https://github.com/Maxhodges/noise-engineering-firmware-index
9. Forum research (dub spring character, Springray feedback): Gearspace dub/spring threads; ModWiggler "Which spring reverb should I get?", "Intellijel Springray 2?" threads.
10. (Verify before use) J. Chowdhury — "Real-time Physical Modelling for Analog Tape Machines," DAFx-19.

---

## 12. Pre-build steps (complete)

1. ~~Gap review~~: grill rounds 1–2, ADRs 0001–0019.
2. ~~Milestone-criteria interview~~: §7.
3. ~~Freeze spec v1.0~~: 27 Sep 2026. Building from M0.
