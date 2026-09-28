# Firmware — build guide

Plain-language guide to the three firmware builds. Background: `SPEC.md`
§6.4/§7/§8, `docs/adr/0011-flash-via-ne-app-internal-flash.md`.

One `main.cpp` produces three different `.bin` files, picked with `MODE`:

| Variant | Build command | Output | What it's for |
|---|---|---|---|
| **m0test** | `make -C firmware MODE=m0test` | `build/resilio_versio_m0test.bin` | The hardware bring-up check (SPEC §7 M0): dry passthrough, every control shown on the LEDs and over USB serial. Unchanged behaviour from the build you've already been checking against — see `docs/m0-hardware-check.md`. |
| **profile** | `make -C firmware MODE=profile` | `build/resilio_versio_profile.bin` | Hardware CPU profiling (SPEC §7 M3). No knobs, no audio in needed — runs on USB power alone off your desk. Cycles through 24 setting combinations automatically and prints CPU load for each over USB serial. |
| **release** | `make -C firmware` (MODE defaults to `release`) | `build/resilio_versio.bin` | The actual instrument: knobs, switches, tap, gate, CV, all wired to the reverb. This is what eventually goes on the module for real use. No serial printing (keeps it small — see "why release doesn't print" below). |

**Important — this changes what `build/resilio_versio.bin` means.** Before this
change, `firmware/build/resilio_versio.bin` *was* the M0 test firmware (that's
still what `dist/resilio_versio_m0_test.bin` and
`docs/m0-hardware-check.md` refer to). From now on, plain `make -C firmware`
(no `MODE=`) builds the **release** instrument instead, and the M0 test
firmware lives at `build/resilio_versio_m0test.bin`. If you're still in the
middle of the M0 hardware check, keep using the `.bin` you already flashed
(or rebuild it explicitly with `MODE=m0test`) — don't run a plain `make`
expecting to get the test firmware again.

## How to build

```bash
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"   # once per terminal session

# One variant at a time:
make -C firmware MODE=m0test   -j8 && make -C firmware MODE=m0test   size
make -C firmware MODE=profile  -j8 && make -C firmware MODE=profile  size
make -C firmware MODE=release  -j8 && make -C firmware MODE=release  size   # or just: make -C firmware -j8

# All three at once, with a size report for each against the 128 KB limit:
make -C firmware all-variants
```

`size` (or `all-variants`) prints how many of the 128 KB internal-flash
budget each build uses (ADR 0011) and fails the build if any variant goes
over. Right now (approximate, will shift slightly as DSP work continues):

- release ≈ 95 KB (72%)
- m0test ≈ 107 KB (82%)
- profile ≈ 107 KB (82%)

## Which `.bin` to flash, and when

- **Still doing the M0 hardware check?** Flash `build/resilio_versio_m0test.bin`
  via NE Firmware Swap, same as before. Follow `docs/m0-hardware-check.md`.
- **Doing hardware CPU profiling (M3)?** Flash `build/resilio_versio_profile.bin`.
  Run it on USB power alone (no Eurorack cable — see the safety note below);
  read the numbers over serial (below).
- **Just want to hear the reverb / give it to the owner to play?** Flash
  `build/resilio_versio.bin` (the release build). This is the only one
  intended to end up as a "real" release.

Flashing itself is always the same: noiseengineering.us/portal/firmware →
**Select Custom File** → pick the `.bin` → **CONNECT** → **CHANGE FIRMWARE**.

**Never have the Eurorack power cable connected at the same time as the USB
cable.** The module runs on USB power alone for flashing and for serial
monitoring; unplug the rack power first (SPEC §8).

## Reading the profile build's serial output

Flash `resilio_versio_profile.bin`, connect USB only (no rack power needed —
this build ignores knobs and CV and makes its own test signal), then open a
serial monitor:

```bash
screen /dev/tty.usbmodem* 115200
```

(To quit: `Ctrl-A`, then `K`, then `Y`.)

Every ~3 seconds a new line appears, one per "corner" (a combination of
SPRINGS / DECAY / BOING / TONE settings). Example line and what each field
means:

```
CORNER S3 D1.0 B1.0 T1.0 (SPEC worst case)  avg  42.3% max  58.1% min  39.0% | mem 103948 B | prepared yes | block 48 | fs 48000 Hz | ~4230 cyc/sample
```

- `CORNER S3 D1.0 B1.0 T1.0` — 3 Springs, DECAY 1.0 (max), BOING 1.0 (max),
  TONE 1.0 (brightest). `(SPEC worst case)` marks the corner matching the
  budget's named worst case (SPEC §5: 3 springs, max BOING, max DECAY).
- `avg / max / min` — CPU load over that corner's ~3 s window, as a
  percentage of the audio block's time budget. **The number to watch is
  `max`; SPEC §5's target is ≤ 65%.**
- `mem` — bytes the Tank's delay pool + object actually use (should read the
  same ~104 KB on every corner; it doesn't change with settings).
- `prepared yes` / `prepared NO (pool too small -> passthrough)` — whether
  the Tank got its delay memory. If you ever see `NO`, the reverb has
  silently fallen back to a dry passthrough for every corner from then on —
  the CPU numbers in that case are meaningless (measuring passthrough, not
  the reverb) and the fix is in `firmware/main.cpp`, not something to chase
  on the module.
- `block` — the audio block size in use (default 48 samples, SPEC §5).
  Rebuild with `make -C firmware MODE=profile PROFILE_BLOCK=96` (or `128`)
  to compare a bigger block size's CPU cost. Note: every `PROFILE_BLOCK`
  value produces the same output filename (`resilio_versio_profile.bin`) —
  the serial output's own `block` field is what tells you which one you're
  looking at, so check it before comparing two profiling runs.
- `fs` — sample rate (always 48000 Hz on this hardware).
- `~N cyc/sample` — average CPU load turned into "cycles per sample", using
  the SPEC §5 budget (480 MHz ÷ 48 kHz ≈ 10,000 cycles/sample). Handy for
  comparing directly against per-stage cost estimates.

Also on the module while profiling: **LED_0** brightens as the run works
through the 24 corners (dim = early in the table, bright = near the end).
**LED_3 turns red permanently** the first time any corner's `max` goes over
65%, as a check you can see without reading serial line-by-line. Left off,
the run just keeps looping through all 24 corners forever — leave it running
as long as you want more samples, or note the corner that's currently
printing and stop it there.

## Memory: where the reverb's delay memory lives

The reverb (`Tank`) needs about 104 KB of memory for its delay lines at 48 kHz.
The `profile` and `release` builds give it a fixed 120 KB block of normal
internal RAM (AXI SRAM, which has 512 KB total) — plenty of room, and the
fastest RAM available apart from the tiny 128 KB DTCM (already needed for the
stack and other libDaisy internals, too small to also fit the reverb). If a
future milestone ever needs more RAM than SRAM has spare, there's one line in
`firmware/main.cpp` (search for `kTankPool`) that moves just this block out to
the 64 MB external SDRAM instead — everything else stays the same. The
`m0test` build is left exactly as it was and manages its own memory the
original way, since it must not change behaviour while the M0 check is still
in progress.

## Everything the release build ignores (for now)

The release build wires up every control that Core currently listens to.
Two knobs (SPLASH is used lightly, DRIVE and full ATTITUDE behaviour) and
some SPEC-described character are still landing in Core in later milestones
(M5–M7) — turning those knobs already sends the value through, it just
won't audibly do everything the SPEC describes until Core catches up.
