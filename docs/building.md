# Building

## One-time setup (macOS, Apple Silicon)

```bash
brew install cmake ninja
git submodule update --init --recursive
```

Arm compiler: Arm GNU Toolchain 15.3.rel1 (darwin-arm64), unpacked to `~/.local/arm-gnu-toolchain` (no admin password needed). Don't use the Homebrew formula `arm-none-eabi-gcc`: it has no newlib (SPEC §8.1). Add it to your PATH:

```bash
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
```

Xcode is **not** required: AU + VST3 build and pass `auval` with Command Line Tools only (verified 28 Sep 2026).

## Desktop: Core, Renderer, tests, Plugin

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

- Renderer: `build/rv_render in.wav out.wav [--set decay=0.8 --set attitude=VALVE ...]` (switches take their panel labels: `springs=1|2|ECHO`, `attitude=CLEAN|TAPE|VALVE`; the labels from before v1.0.43, `springs=3` and `attitude=DRIVEN|KICKED`, still work, ADR 0044)
- Plugin: development builds do **not** install into `~/Library/Audio/Plug-Ins/` (`RV_INSTALL_PLUGIN` is off), so the plugin in Ableton only changes on purpose. To install a specific version: `tools/install_plugin.sh [commit]` (default: HEAD). It builds that commit in a throwaway worktree, installs AU + VST3, and runs `auval`. The installed version is recorded in `dist/installed_plugin.txt` (never inside the bundles: that breaks their code signature). In Ableton: rescan plug-ins, then find **Resilio → Resilio Versio**.

## Firmware

```bash
make -C libs/libDaisy -j8     # once, or after updating libDaisy
make -C firmware all-variants # builds all three, fails if any is over 128 KB (ADR 0011)
```

`all-variants` prints each binary's size against the 128 KB limit, fails the
build if any variant goes over, and warns (without failing) once a variant
reaches 95%. See "Flash-budget techniques in use" in `firmware/README.md`
for how the three variants stay under the limit (6 Oct 2026: release
126,380 B (96 %, ~4.6 KB left), m0test 82,320 B (63 %), profile 127,520 B
(97 %, ~3.5 KB left), so both warn at 95 % and the flash budget is tight).

Three variants (details: `firmware/README.md`):

| Variant | Build | Output | For |
|---|---|---|---|
| release (default) | `make -C firmware` | `firmware/build/resilio_versio.bin` | The instrument |
| m0test | `make -C firmware MODE=m0test` | `firmware/build/resilio_versio_m0test.bin` | M0 hardware check (saved copy: `dist/resilio_versio_m0_test.bin`) |
| profile | `make -C firmware MODE=profile` | `firmware/build/resilio_versio_profile.bin` | M3 CPU profiling over USB serial |

Flash with NE Firmware Swap → Select Custom File (ADR 0011).

## Test stimulus

```bash
python3 tools/make_stimulus.py
```
