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

- Renderer: `build/rv_render in.wav out.wav [--set decay=0.8 ...]`
- Plugin: AU and VST3 are copied to `~/Library/Audio/Plug-Ins/` automatically. In Ableton: rescan plug-ins, then find **Resilio → Resilio Versio**.

## Firmware

```bash
make -C libs/libDaisy -j8     # once, or after updating libDaisy
make -C firmware -j8
make -C firmware size         # fails if over 128 KB (ADR 0011)
```

Output: `firmware/build/resilio_versio.bin`. Flash with NE Firmware Swap → Select Custom File (ADR 0011).

## Test stimulus

```bash
python3 tools/make_stimulus.py
```
