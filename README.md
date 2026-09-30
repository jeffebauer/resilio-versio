# Resilio Versio

A dub spring reverb firmware for the **Noise Engineering Versio** Eurorack platform (Electro-Smith Daisy Seed inside).

*Resilio*: Latin, "I leap back, rebound".

The goal is the splashy, drippy ring-out of a spring tank on a single snare or rim hit, including the "kicked tank" crash of dub. It's modelled on a physical spring rather than a room reverb, with a wide sweet spot: every knob position should be usable, with no dead zones and no cliff edge into runaway feedback.

> **Status:** in development, not yet released. The spec is frozen at v1.0 (changes via ADRs). M0–M7 are built and **the release firmware runs on the owner's Versio**, sounding the same as the desktop renders (within ~1 dB). CPU is within budget (63 % peak; target ≤ 70 %). Now in the M8 tuning pass, benchmarked against a real Wellspring spring tank: SPLASH is being rebuilt to come from the hits themselves, DRIVE is becoming an INPUT knob, and the tank's echoes are being reshaped toward the Wellspring's smoother, wider sound.

## The instrument

| Control | What it does |
|---|---|
| **P1 MIX** | Dry/wet, equal power. Fully clockwise = 100% wet for send/return |
| **P2 DECAY** | Tail length only (0.4–9 s). Always fades; doesn't change the tank or bend pitch |
| **P3 TONE** | The hero tilt: warm dub dark ↔ splashy bright, never harsh |
| **P4 SPLASH** | How hard hits make the tank clatter and lurch (being rebuilt: the splash will come from the hits themselves, ADR pending) |
| **P5 TENSION** | Which tank is fitted: up = tight (short, quick repeats, small bright chirp), down = loose (long, big darker boing). Always a spring |
| **P6 WOBBLE** | Drift in the lower half, worn-tape warble at the top (a bipolar version is planned: random wow and flutter left of noon, LFO right) |
| **P7 DRIVE** | Transducer and tape colour, level-compensated: changes colour, not volume (becoming the INPUT: how hard the signal hits the tank, a little louder when pushed) |
| **SPRINGS** switch | 1 (sparse, drippy) / 2 (classic) / 3 (dense, lush) |
| **ATTITUDE** switch | CLEAN / DRIVEN (tape dub) / KICKED (hard drive, chaos, may Howl) |
| Button / Gate in | **Kick**: hit the tank |

All seven knobs are CV-controllable (0–5 V). P1–P7 are the pots in reading order on the stock Versio panel (ADR 0028). The four LEDs meter In L, In R, Out L, Out R: green → amber with level, red when the input nears clipping or the output limiter works (ADR 0031). Full panel map: [SPEC.md §3](SPEC.md); player's guide: [docs/manual.md](docs/manual.md).

## How it's built

One DSP core, three hosts. Every host passes the same 0–1 parameter values, so a setting in the plugin sounds identical on the module.

```
                 core/  (DSP + ParamSpec table; no platform code)
                   │
     ┌─────────────┼──────────────────┐
host/render     plugin/            firmware/
offline CLI     JUCE AU + VST3     Versio (libDaisy)
WAV → WAV,      test bench         real knobs, CV,
sweeps,         in Ableton         gate, LEDs
metrics
```

The DSP follows Välimäki, Parker & Abel, *Parametric Spring Reverberation Effect* (JAES, 2010): each simulated spring is a feedback loop around a chain of "stretched" allpass filters, which spread each echo in time by frequency: the highs arrive after the lows, so each echo sweeps up, as in real tanks (ADR 0024). On top of that: 1–3 detuned springs spread across the stereo field (A left, B right, C centre), a drive chain (transducer → tape → loop saturation → pickup), transient-driven Splash and Kick, and a layered defence against single-tone ringing. Details: [SPEC.md §4](SPEC.md).

## Building

Requirements: macOS (Apple Silicon tested), CMake + Ninja, Command Line Tools (full Xcode not needed), and the Arm GNU Toolchain for firmware.

```bash
git clone --recursive https://github.com/jeffebauer/resilio-versio.git
cd resilio-versio
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build          # Core, Renderer, tests, AU + VST3 plugin
ctest --test-dir build       # 16 test suites (log to a file and check the summary line)
make -C libs/libDaisy -j8 && make -C firmware all-variants
```

Full setup, including the Arm toolchain and the firmware variants: [docs/building.md](docs/building.md). Flashing uses Noise Engineering's Firmware Swap web app → *Select Custom File* ([ADR 0011](docs/adr/0011-flash-via-ne-app-internal-flash.md)). The firmware must fit the 128 KB internal flash.

## Repository layout

| Path | Contents |
|---|---|
| `core/` | Platform-independent DSP (`dsp/`) and the parameter table + mappings (`params/`) |
| `host/render/` | `rv_render`: offline renderer with presets, automation, sweep grids, analysis |
| `host/common/` | Desktop helpers: WAV, JSON, FFT, metrics, spectrogram sidecars |
| `host/tests/` | Core and host test suites (ctest) |
| `plugin/` | JUCE AU/VST3 test-bench plugin + a host test that loads it like a DAW |
| `firmware/` | Versio firmware: `release`, `m0test`, `profile` variants ([README](firmware/README.md)) |
| `tools/` | Stimulus generator, listening-page generator (`tools/review/`), reference-recording ingest, IR and sweet-spot analysis |
| `presets/` | Parameter presets and sweep definitions (JSON) |
| `docs/` | Decisions (ADRs), milestone contracts, recording recipes, checklists, tuning backlog, prototypes (`docs/prototypes/`) |
| `libs/` | Submodules: libDaisy, DaisySP, JUCE |

## Documentation

- **[SPEC.md](SPEC.md)**: the full specification: hardware facts, sound target, DSP design, architecture, milestones with acceptance criteria.
- **[CONTEXT.md](CONTEXT.md)**: the glossary. The code, docs and conversations all use these terms.
- **[docs/adr/](docs/adr/)**: architecture decision records, one per decision (0001–0031).
- **[docs/TASKS.md](docs/TASKS.md)**: the owner's running to-do list (listening pages, hardware checks, design questions).
- **[docs/m8-tuning-backlog.md](docs/m8-tuning-backlog.md)**: the tuning findings, measurements and decisions, newest at the end.
- **[docs/dub-spring-reference.md](docs/dub-spring-reference.md)**: how dub engineers used spring reverb, and what that means for Resilio.
- **[docs/manual.md](docs/manual.md)** and **[docs/presets.md](docs/presets.md)**: the player's guide and dub starting points (drafts).
- Reference recordings: [Wellspring recipe](docs/recording-recipe.md), [Magneto recipe](docs/recording-recipe-magneto.md), [Ableton setup](docs/ableton-setup.md).
- Checks: [M0 hardware](docs/m0-hardware-check.md), [M2 Ableton](docs/m2-ableton-check.md). Hardware recordings and how they compare: `test_audio/hardware/NOTES.md`.

## Milestones

| # | Milestone | State |
|---|---|---|
| 0 | Toolchains + hardware check | Passed on the owner's Versio |
| 1 | One spring (CLEAN) + Renderer | Built; Wellspring A/B listening check pending |
| 2 | JUCE plugin shell | Built + automated tests pass; Ableton check pending |
| 3 | Hardware profiling | Done: worst case 61 % average, 63 % peak (target ≤ 70 %, ADR 0030) |
| 4 | Multi-spring, stereo, tank coupling | Built |
| 5 | Drive chain + TONE tilt | Built |
| 6 | Anti-resonance | Built |
| 7 | SPLASH, KICK, WOBBLE, MIX | Built |
| 8 | Tuning pass | In progress. Done: TENSION, knob layout, earlier first echo, output polarity/level fix. Now: SPLASH from the hit + DRIVE as INPUT (building), the tank's echo shape vs the Wellspring (prototyping), bipolar WOBBLE (planned) |
| 9 | Polish (LEDs, panel overlay, manual, release) | LED meters built and smooth (DMA-driven dimming, ADR 0031); manual and preset notes drafted; panel overlay and release to come |

Acceptance criteria per milestone: [SPEC.md §7](SPEC.md).

## Licence

No licence has been chosen yet. Note that the plugin builds on [JUCE](https://juce.com), which is dual-licensed (AGPLv3 or JUCE's commercial licences), so distributing plugin builds brings those terms into play. The firmware and Core don't depend on JUCE.

## Acknowledgements

- Spring reverb modelling: V. Välimäki, J. Parker, J. S. Abel; J. Parker; S. Bilbao (full references in [SPEC.md §11](SPEC.md)).
- [Electro-Smith](https://electro-smith.com) for libDaisy and DaisySP, and [Noise Engineering](https://noiseengineering.us) for the open Versio platform.
