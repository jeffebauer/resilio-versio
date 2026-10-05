# Firmware — build guide

Plain-language guide to the three firmware builds. Background: `SPEC.md`
§6.4/§7/§8, `docs/adr/0011-flash-via-ne-app-internal-flash.md`.

One `main.cpp` produces three different `.bin` files, picked with `MODE`:

| Variant | Build command | Output | What it's for |
|---|---|---|---|
| **m0test** | `make -C firmware MODE=m0test` | `build/resilio_versio_m0test.bin` | The hardware bring-up check (SPEC §7 M0): dry passthrough, every control shown on the LEDs and over USB serial. Unchanged behaviour from the build you've already been checking against — see `docs/m0-hardware-check.md`. |
| **profile** | `make -C firmware MODE=profile` | `build/resilio_versio_profile.bin` | Hardware CPU profiling (SPEC §7 M3). No knobs, no audio in needed — runs on USB power alone off your desk. Cycles through 24 setting combinations automatically and prints CPU load for each over USB serial. |
| **release** | `make -C firmware` (MODE defaults to `release`) | `build/resilio_versio.bin` | The actual instrument: knobs, switches, tap, gate, CV, all wired to the reverb. The LEDs are level meters (ADR 0031): left pair In L / In R, right pair Out L / Out R, green → amber with level, red on input clip or when the output limiter pulls down. The LEDs dim smoothly: the release firmware drives their PWM itself (TIM5 + DMA, 512 steps at ~1 kHz, see `LedPwm` in `main.cpp`) instead of libDaisy's software PWM, which flickered at dim levels. This is what eventually goes on the module for real use. No serial printing (keeps it small: see the flash budget under "How to build", ADR 0011). |

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
over, with a warning once a variant passes 95%. Right now (approximate, will
shift slightly as DSP work continues):

- release 112,628 B (85%, 18.4 KB free) on `main` `327af86` (4 Oct 2026, flash study: no QSPI chip set-up, no USB device stack in release; `docs/prototypes/flash-study/README.md`). Before: 126,280 B (96%, tank voicing 7, ADR 0038). Binary: `dist/resilio_versio_release_327af86.bin`
- m0test 82,320 B (62%): plain passthrough, no Core linked (identical output to the Tank at MIX 0)
- profile 114,600 B (87%, 16.5 KB free) on `main` `327af86` (flash study: also skips the ADC/controls and the Seed 1.1 codec set-up; audio code byte-identical, so run 15's numbers hold). Binary: `dist/resilio_versio_m3_profile_327af86.bin`. Before: 130,496 B (99.6%) at `340b542`, **run 15 binary `dist/resilio_versio_m3_profile_run15.bin`** (first chip measurement since run 13), release for after it `dist/resilio_versio_release_340b542.bin`

### Knob end stops (release, 4 Oct 2026)

The Versio's pots (summed with their CV in hardware) stop a little short of 0 and 1; M0 only showed "within 2 %". With MIX equal-power, a pot topping out at 0.98 let the dry through at about -30 dB (owner: un-TONE'd dry at MIX fully right on the module, none in the plugin). `firmware/PotEndStops.h` snaps the outer 2.5 % at each end to exactly 0 / 1 and stretches the rest linearly (noon stays noon), for every knob in the release build. Tested in `host/tests/test_pot_endstops.cpp`. +24 B.

### Flash-budget techniques in use (ADR 0011)

All firmware/-only (no changes inside `libs/libDaisy`):

- **`firmware/no_uart_spi.cpp`** stubs out libDaisy's UART and SPI DMA
  bookkeeping (and their IRQ handlers) so the linker never pulls in
  `uart.o`/`spi.o` and the HAL code behind them. Versio never uses raw
  UART/SPI peripherals in any variant; the stubbed functions only reset
  static bookkeeping with no hardware side effects (see the file's header
  comment for the full reasoning, including why the UART half only applies
  outside `m0test`). This is the biggest single win: roughly 15-20 KB off
  every variant.
- **`main.cpp`'s DSP/control-rate split**: `firmware/Makefile` compiles
  `main.cpp` at `-Os` while `core/dsp/*.cpp` (Tank, Spring, Drive, Splash,
  Kick, Wobble) stay at `-O3` in every variant, so release and profile always
  measure the identical, maximally-optimised per-sample DSP code — profile's
  CPU numbers stay valid for release. Only the knob/switch/LED/serial
  "glue" code shrinks.
- **profile's serial logging** talks to USB CDC directly
  (`hw.seed.usb_handle`), formatting the CORNER line with small integer
  helpers in `main.cpp` instead of `hw.seed.PrintLine()`/`snprintf`. Calling
  libDaisy's `StartLog()` pulls in the whole `printf`/`vsnprintf` chain
  unconditionally (it prints its own "Daisy is online" banner internally),
  so profile calls `hw.seed.usb_handle.Init()` directly instead. `m0test`
  still uses the original `PrintLine`-based logging unchanged, since it must
  not change behaviour while the M0 hardware check is in progress.
- **`RV_SIZE_OPT` (`core/dsp/SizeOpt.h`)**: set-up and per-tick
  housekeeping in the Core is built for size (-Os) in the firmware only:
  every `prepare` / `reset`, the pool layout, the tank voicing's set-up, the
  per-tick parts of Splash, Wobble and Kick (`Splash::set` / `controlTick` /
  `fire`, `Wobble::setAmount` / `tick`, the Morph blends), the Tank's
  `controlTick`. Every per-sample path and the knob-move coefficient redesign
  (`Tank::updateBaseSettings` / `updateSpringSettings`,
  `Spring::prepareTransition` / `prepareDamping` / `setSettings` /
  `commitDesign`, `DriveIn::set`, `Tilt::set`) stay -O3: the knob-move CPU
  bursts are the lead for the red input LEDs.
- **`RV_NO_UNSWITCH` on `Tank::process`**: -O3 without loop unswitching,
  which copied the whole input loop (Tilt, low cut, coil, Clang) once more
  for one loop-invariant branch (the Clang's ceiling): 1.5 KB for a
  predictable branch per sample.
- **`printf` / `putchar` / `exit` stubs** in `no_uart_spi.cpp` (release and
  profile): libDaisy's USB device core logs through `printf` (its
  `USBD_DEBUG_LEVEL` is 3) and crt0 calls `exit`, which together pulled
  newlib's stdio, `malloc` and the FILE machinery in. No variant retargets
  stdout, so that output went nowhere; `main()` never returns.
- The firmware Tank links no `malloc` / `free` (it gets its pool from
  `main.cpp`), `exp(log)` instead of `pow` in the Core (`powf` is ~1.1 KB),
  and profile's report divides in float, not 64-bit integers (~0.85 KB).
- Link-time optimisation (`-flto`) was evaluated and **not adopted**: on
  this small a set of translation units it made both release and profile a
  few hundred bytes *larger*, not smaller (LTO's own bookkeeping outweighed
  any extra inlining), with no speed benefit to justify the added build-time
  cost and risk. `firmware/Makefile` does not enable it.

## Which `.bin` to flash, and when

- **Still doing the M0 hardware check?** Flash `dist/resilio_versio_m0_test.bin` (the saved copy; a fresh `build/resilio_versio_m0test.bin` behaves the same)
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
SPRINGS / DECAY / TENSION / TONE settings). Example line and what each field
means:

```
CORNER S3 D1.0 TN0.0 TO1.0 (SPEC worst case)  avg  42.3% max  58.1% min  39.0% | mem 103948 B | prepared yes | block 48 | fs 48000 Hz | ~4230 cyc/sample
```

- `CORNER S3 D1.0 TN0.0 TO1.0` — 3 Springs, DECAY 1.0 (max), TENSION 0.0 (loosest tank),
  TONE 1.0 (brightest). `(SPEC worst case)` marked the corner matching the
  budget's named worst case (SPEC §5: 3 springs, loosest TENSION, max DECAY).
  Since echo mode (ADR 0041) no position runs 3 Springs: the candidates are
  marked `(worst?)` (S2 and S3 at DECAY 1.0, TENSION 0.0; S3 is 2 Springs at
  the fixed tank plus the tape echo), and the `SPLIT` line's `echo` column
  (Spring C's old slot) is the tape's share.
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
the 64 MB external SDRAM instead — everything else stays the same.
Echo mode's tape (SPRINGS 3, ADR 0041) is a second block, 2 s at 48 kHz plus a
margin (379 KB, `kEchoTape`), in ordinary AXI SRAM (.bss, zeroed at start-up):
about 407 KB (release) / 418 KB (profile) of the 512 KB is then in use. The
`m0test` build is left exactly as it was and manages its own memory the
original way, since it must not change behaviour while the M0 check is still
in progress.

## What the release build wires up

Every control on the panel reaches the Tank; nothing is ignored. Per audio
block (1 ms at 48 frames), `main.cpp`'s release section reads:

- **P1–P7 + their CV** → MIX, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE
  (ADR 0028; `kPotKnob` is the pot → libDaisy index table, `kPotParams` the
  pot → function table).
- **SW0 → SPRINGS, SW1 → ATTITUDE** (CLEAN / DRIVEN / KICKED).
- **Button → Kick**, on the rising edge, at the start of the block. **Held 1 s** (`TimeHeldMs()`, ThrowHold.h `kThrowExitHoldSeconds`): throw mode off, once per press (`Tank::exitThrowMode()`); if it was on, all four LEDs show white for 150 ms in the main loop (ADR 0039). **Gate → THROW** in positions 1-2 (ADR 0039): every change goes to `Tank::gate()` at the start of the block; the send is open while the gate is high, from its first rising edge (unpatched it reads low, so nothing changes). Every rising edge also goes to `Tank::clock()` (echo mode's clock, ADR 0041); in position 3 that is the gate's only job (the Tank keeps the role, ThrowHold.h `gateRole`).
- **Output trim**: undoes the Versio's polarity flip and +1.2 dB (M0), so
  MIX 0 sounds like a patch cable.
- **LEDs**: level meters (ADR 0031, `LedMeter.h`).

DRIVE, SPLASH and full ATTITUDE behaviour are all in Core now (M5–M7), so
each knob does on the module what it does in the Plugin (same Core, same
ParamSpec). Tuning continues in M8 (`docs/m8-tuning-backlog.md`; SPLASH is
being reworked). Not in the release build: USB serial (flash budget, see
"How to build") and MIDI (the Plugin's
MIDI-note Kick has no Versio equivalent; since ADR 0039 the gate is the throw, and the button the only Kick).

## M3 results

**Run 1, 29 Sep 2026** (`dist/resilio_versio_m3_profile.bin`, pool in AXI SRAM, block 48, KICKED, DRIVE max). **Over budget.**

| Corner | avg | max |
|---|---|---|
| S1 loose (TN0) | 71.5–71.6 % | 85–88 % |
| S2 loose | 81.0–81.3 % | 98.3–98.4 % |
| S3 loose (SPEC worst case, D1.0 TN0.0) | 82.3–82.6 % | 99.9–100 % |
| S2/S3 tight (TN1) | 61.3–61.8 % | 78.7–78.9 % steady; 99–100.8 % on the corner right after a TENSION change |

Readings: TENSION loose → tight is ~20 points (the loose tank runs up to 64 Chirp stages per Spring per sample). 2 → 3 Springs costs ~1 point because the Tank always runs all three Springs and mutes the unused ones. The short 100 % peaks follow a setting change. Desktop estimate was 65–70 %.

**Run 2** (`dist/resilio_versio_m3_profile_dtcm.bin`): the delay pool moved to DTCM, which was completely unused. Same DSP, same sound. **Only ~1 point better** (S1 loose 70.5 %, S2 loose 80.1 %): memory wasn't the bottleneck. Even the tight tank sits at ~60 %, so most of the cost isn't the Chirp stages.

**Run 3** (`dist/resilio_versio_m3_profile_split.bin`): adds a `SPLIT` line under each corner, the time per Tank section as % of the budget (`ctl` control tick, `drvIn` input drive + excitation followers, `splash` Kick + Splash, `tilt` TONE tilt + transport, `sprA/B/C` each Spring incl. its input prep, `out` mix, decorrelator, pickups, shelf, limiter, MIX). Hooks: `core/dsp/ProfileHook.h`, compiled in only with `RV_PROFILE_HOOKS` (profile builds); release is unchanged.

Run 3 results (29 Sep 2026), % of the budget:

| Corner | ctl | drvIn | splash | tilt | sprA | sprB | sprC | out | total |
|---|---|---|---|---|---|---|---|---|---|
| S3 loose (worst case) | 2.0 | 7.4 | 2.4 | 0.8 | 19.6 | 19.4 | 19.4 | 10.0 | 81.4 |
| S2 loose | 2.0 | 7.4 | 2.4 | 0.8 | 22.4 | 22.2 | 12.7 | 10.0 | 80.2 |
| S1 tight | 1.9 | 7.4 | 2.3 | 0.8 | 12.8 | 12.6 | 12.7 | 9.7 | 60.7 |

- The Springs are 58 of the worst case's 81 points. An unused Spring still runs (at ~12.7, like a tight one).
- A Chirp section costs ~24 cycles: the per-sample chain waits on each multiply-add (the loop itself is 17 instructions).
- `ctl` rises to 5.3–5.8 on the corner after a TENSION change (coefficient redesign).
- `max` sits ~17 points above `avg` even on steady corners: something outside the Tank interrupts the audio callback now and then (suspect: USB serial, profile-only).

**Run 4** (stage-by-stage order over 32-sample runs, bit-exact on the desktop): **slower**, S2 loose 80.2 → 87.5 %, tight 61 → 65 %. Its finer split (all three Springs together, worst case): Chirp stages ~42 %, Loop reads + LoopSat + DC block ~17 %, high path 3.4 %, fC low-pass + damping 0.7 %, Tank-side prep 1.1 %. A Chirp section costs ~28 cycles in either order. Reverted. Reading: the M7 issues in order, and each multiply-add in the section waits on the one before it, whatever the loop order.

**Run 5** (`dist/resilio_versio_m3_profile_bench.bin`): the run 3 build (sample-by-sample Springs, per-Spring SPLIT) plus `firmware/m3_bench.cpp`, micro-benchmarks run once at boot and printed as a `BENCH` line after the first corner of each pass: clock, I/D-cache state, one multiply-add's latency and throughput, and the Chirp section's cycles in three loop shapes with identical arithmetic: `fused` (today), `split` (every section's D{v} first, then the x chain), `split3` (split with the three Springs' x chains interleaved).

Run 5 results: `BENCH clock 480 MHz icache on dcache on | fma latency 9.0 throughput 8.3 | section cycles: fused 22.3 split 31.0 split3 23.1`. Clock and caches are right. **Independent multiply-adds cost as much as dependent ones (~8–9 cycles):** GCC's `vfma` accumulates into its destination, so it emits `vmov / vfma / vmov` for every `a*b + c` and serialises everything through one temporary. Reordering can't fix that, which is why runs 4 and 5's loop shapes didn't help.

**Run 6** (`dist/resilio_versio_m3_profile_nofma.bin`): firmware built with `-ffp-contract=off` (plain `vmul` + `vadd`, three-operand, pipelined). Release and profile. Output vs the fused build (desktop, 12 s Tank run with moves and Kicks): the difference stays 84–95 dB below the signal (recirculating tails carry the last-bit rounding); inaudible, no drift.

Run 6 results (29 Sep 2026): **~5 points better everywhere, still over.** BENCH: mul-add latency 6.0, independent throughput 2.0 (was 8.3); Chirp section still ~21 cycles (fused), split3 19.5.

| Corner | avg | max | Springs (each) | out | drvIn | splash | ctl |
|---|---|---|---|---|---|---|---|
| S3 loose (worst case) | 75.7–76.0 % | 92.5 % | ~18.0 | 9.4 | 6.7 | 2.5 | 2.0 (5.4 after a change) |
| S2 loose | 74.9 % | 91.4 % | 20.5 / 20.5 / 11.9 | 9.4 | 6.7 | 2.5 | 2.0 |
| any tight | 57.0–57.4 % | 73.6 % | ~11.9 | 9.3 | 6.7 | 2.5 | 1.9 |

Next candidates (no sound change unless noted): (1) spread the Springs' coefficient redesign over several control ticks (the ~16-point `max` bursts follow setting changes, i.e. any knob move); (2) the Chirp section's scheduling (~21 cycles vs a ~8–10 floor; test loop shapes on-chip with m3_bench); (3) LoopSat: two soft clips with divisions per sample per Spring at 2× (`sIn` ~5.6 % per Spring in run 4); (4) skip muted Springs at SPRINGS 1/2 (worst case unaffected; a newly switched-in Spring would start empty: owner question); (5) block 96 instead of 48 (halves per-block overhead and the burst's share; +1 ms latency: owner question, SPEC §5).

Owner (29 Sep): unused Springs keep running (seamless SPRINGS switching); block 96 only if the sound-neutral fixes fall short.

**Run 7** (`dist/resilio_versio_m3_profile_peak.bin`): adds a `PEAK` line (the worst single block per section, % of one block's budget: what lifts `max` above `avg`) and two more Chirp section shapes to BENCH: `pipe` (the next section's D{v} computed while this section's x chain waits; the fused section is a 7-step chain at ~3 cycles a step = 21) and `fused3` (the three Springs' sections side by side).

Run 7 results: `PEAK ctl` is 16.5–17.7 % of a block on every corner (avg ~2 %): the Springs' coefficient redesign, triggered by the Jolt on each test click (in use: on hits and knob moves). Every other section peaks at its average. BENCH: `pipe` **14.4** cycles per section (fused 21.3, split3 19.1, fused3 19.1).

**Run 8** (`dist/resilio_versio_m3_profile_pipe.bin`), both bit-exact with run 7 (float32, desktop, with and without FMA contraction): the `pipe` section loop in `Spring::processLow`; and the Loop gain design caches cos(w) and the LoopSat latency at its eight fixed design frequencies (`Spring::prepare`) and shares cos(w) between round trip and magnitude at the fC points. Flash: release 108,752 B (82 %), profile 123,732 B (94 %, the benchmark code).

Run 8 results (29 Sep 2026): **worst case average under target.**

| Corner | avg | max | Springs (each) | PEAK ctl |
|---|---|---|---|---|
| S3 loose (worst case) | **62.9 %** | 74.7–75.6 % | ~13.6 | 13.1–13.2 |
| S2 loose | 62.2 % | 74.3 % | 15.3 / 15.2 / 10.0 | 13.3 |
| S1 loose | 57.0 % | 68.0 % | 15.4 / 9.9 / 10.0 | 14.3 |
| any tight | 51.6–52.0 % | 63.7 % | ~10.1 | 13.1–13.2 |

`max` is now `avg` + the control burst (~12 points): every other section peaks at its average. Next: stagger the three Springs' coefficient redesign over consecutive control ticks (not bit-exact: B and C take a change up to 2 ticks, ~1.3 ms, later); LoopSat divisions for margin; block 96 held in reserve.

**Run 9** (`dist/resilio_versio_m3_profile_stagger.bin`, owner's choice over block 96): one Spring takes new settings per control tick (`Tank::controlTick`), so B and C follow a change up to two ticks (1.3 ms) after A. Not bit-exact: level and per-octave spectrum match within 0.05 dB (desktop renders, hits/skank/moves); KICKED waveforms differ 21–27 dB below the signal (the Jolt reaches B and C a little later), CLEAN 81 dB. A/B for the owner: `renders/m3_stagger_ab/`. Results: `PEAK ctl` 13.2 → 6.0; S1 loose 56.6 % / 61.7 %, S2 loose 62.1 % / 67.2 % (avg / max); tight 51.4 % / 56.5 %.

Owner listen (29 Sep): a bit more undulation on KICKED hits with the run 9 stagger, clearest with built-up feedback. Cause: the Splash Jolt moves each Spring's allpass coefficient every tick, and staggering delayed it for B and C. Fix: every Spring takes the Jolt's coefficient each tick (`Spring::setAllpassCoeff`); only the full redesign (Loop gain and filters) takes turns. Static-settings renders now match the pre-stagger build 112–118 dB down (the run 9 version: 23–27 dB). Owner: A/B/C at moderate DRIVE (`renders/m3_stagger_abc/`) hard to tell apart.

Also: the hot-path saturators (DriveIn, DriveOut, LoopSat) multiply by 1/k worked out at control rate instead of dividing by k per sample (14 divides per sample fewer); output difference 95–113 dB down.

**Run 10** (`dist/resilio_versio_m3_profile_run10.bin`): both of the above. Release 109,944 B (83 %), profile 124,932 B (95 %: the benchmark code; trim before more profile features).


**Run 11** (`dist/resilio_versio_m3_profile_run11.bin`, 30 Sep, owner's Versio, USB): run 10 + the earlier first echo (ADR 0029). Worst case S3 D1.0 TN0.0 TO0.5 **avg 62.1 % / max 67.8 %** (target max ≤ 65 %). Highest max anywhere: **68.6 %** (S3, tight tank, TO 0.5); the first corner's 68.3 % on the first lap is a start-up reading (60.6 % on lap 2). Averages: 3 Springs 62.1 % loose, 50.3 % tight; 2 Springs 61.6 / 50.3; 1 Spring 55.9 / 50.0. Where the max comes from: the per-Spring redesign burst (tight tank `sprA` PEAK 14.8 vs 10.4 average, one Spring per tick after run 9's stagger), the control tick (`ctl` 6.1 vs 1.5–2.6) and the output stage (`out` 10.3 vs 8.6 on some corners), landing in the same 1 ms block. Steady cost is well under target; the ~3-point overshoot is those occasional blocks. Next candidates, cheapest first: split each Spring's redesign over two ticks (~2 points expected); move the tick's remaining work off the tick that also redesigns; block 96 (owner OK, +1 ms latency, ADR 0030).

**Run 12** (`dist/resilio_versio_m3_profile_run12.bin`, owner's choice: keep trimming, no sound change). Where the peaks came from: every corner change moves knobs, and the settings keep gliding for up to a second (TENSION's smoothing tail), so the first second of a corner redesigns a Spring on every tick; at block 48 every other block holds two ticks. The tight corners' `sprA` PEAK is the tank still loose at the start of the corner (the stages glide down over ~0.3 s), not a redesign. Changes, all in Core:
- **The redesign in parts** (`Spring::setSettings`): everything in the Loop gain design that only depends on fC or the damping cutoff is kept per design point, so the Jolt, DECAY and the L and stage glides only redo 13 exp and a few divisions (was: 3 filter designs, ~23 cos, 13 exp, ~100 divisions). A TENSION move takes two ticks: the new filters (K, fC low-pass, high-path HPF, the fC-dependent points) are worked out on the first while the old ones keep playing, and installed together with g on the second; the audio never runs a half-updated Spring. The Jolt's allpass coefficient still reaches every Spring on the same tick.
- **One heavy step per block** (`Tank::controlTick`): a Spring whose fC or damping moved waits out every third tick (grid ticks 3n + 1, the second tick of each two-tick block at 48 samples; on the fixed tick grid, so every block size renders the same).
- **The tick's own work**: the Springs' settings (4 exp, the pickup alignment's cos, ...) are worked out only when DECAY, TENSION, TONE, DRIVE, the Morph or SPRINGS moved, and then only for the Spring taking its turn; the Morph blends only when the Morph moves; the MIX gains (two square roots per sample) only while MIX moves.
- Profile build: the BENCH line keeps the clock, caches and multiply-add timing; the Chirp section shapes (runs 5-8) are gone.

Sound: static settings are bit-identical to run 11 (desktop, with and without FMA contraction: the six starting-point presets on hits and skank, alone and with Kicks, plus KICKED/Howl/CLEAN/DRIVEN harness renders with hits; DECAY, DRIVE, WOBBLE, SPLASH, SPRINGS and ATTITUDE moves too). Only TENSION and TONE moves change timing: a Spring takes a new TENSION up to ~6 ms later (was up to 1.3 ms). Moving renders (TENSION, DECAY, TONE automation + Kicks, starting points): per-octave levels within 0.03-0.23 dB over the file (0.46 dB on the Howl preset), 200 ms windows within 2 dB, the same size as run 9's stagger measured on the same renders (0.02-0.58 dB, 1.7 dB); TONE-only moves 70 dB down. Desktop hint: control tick peak -32 %, average -36 %.

Flash: release 115,424 B (88 %, +4.6 KB: the new code, and the Tank object is copied from flash at boot), m0test 85,976 B (unchanged), profile 127,044 B (96.9 %). Expected on the chip: the corner-start blocks lose most of the control tick's burst (~2-3 points), so the worst case max should land around 64-66 %; averages move a little (~0.3 point from the MIX square roots). If it still misses: block 96 (ADR 0030), or give the Tank object zero-initialised storage in `main.cpp` to win back ~7 KB of flash per variant.

**Run 12 on the chip** (30 Sep, owner's Versio, USB): **target met.** Worst case S3 D1.0 TN0.0 TO0.5 **avg 60.7 % / max 63.3 %** (run 11: 62.1 / 67.8); highest max anywhere **64.0 %** (S3 and S2, tight tank, TO 0.5; run 11: 68.6). `PEAK ctl` 6.1 → 2.6–3.2; `out` average 8.6 → 7.7 (the MIX square roots). Every corner's max ≤ 65 %. Headroom against the target: ~1 point on peaks, ~4 points on averages.

**Run 13 on the chip** (30 Sep, owner's Versio; the SPLASH/DRIVE build before the owner's tuning, `dist/resilio_versio_m3_profile_run13.bin`): worst case S3 D1.0 TN0.0 TO0.5 **avg 63.4 % / max 66.3 %**; highest max anywhere 66.5 % (S3 D0 TN1 TO0.5). `splash` 2.5 → 4.8 %, `drvIn` 6.0 → 5.4 %. All under the 70 % target (ADR 0030 amendment); ~3.5 points of peak headroom left. The tuning after it (gentler Bite, milder DRIVEN, KICKED Clang) moved the desktop worst case from +2.5 % to ~+3 % vs main; D's Clang blend and the DRIVE stretch add no work.

**Run 15 on the chip** (4 Oct, owner's Versio, `dist/resilio_versio_m3_profile_run15.bin` from `340b542`: everything since run 13, incl. the Wellspring F tank, ADR 0038, and SPRINGS 3 coupled wire gauges, ADR 0037). **Over budget.** Worst case S3 D1.0 TN0.0 TO0.5 **avg 82.1 % / max 86.3 %**; highest max anywhere **87.6 %** (S3 D0/D1 TN1 TO0.5, the corner's glide from loose to tight). S2 worst avg 76.1 % / max 80.7 %; S1 worst avg 74.1 % / max 79.9 % (86.1 % on the first corner of lap 1, start-up). Tight tanks: S1/S2 ~65.5 % avg / ~70.5 % max, S3 ~70.5 / 75.8. Where it went (SPLIT, % of budget, loose tank): `tilt` **16.0** (was ~0.8: it now carries the shared Sweep, ~40–64 highs-later sections; 10.8 at a tight tank), each Spring 12.5–12.8 (sprA) / 10.5 (idle), `out` 9.4 at S1/S2 and **~51** at S3 (the coupled Loops are processed inside `out`), `splash` 5.4, `drvIn` 4.9, `ctl` 3.2–4.6 (PEAK 7.9). The 576 B-headroom profile (runs 13 → 15) didn't change what it measures. Next: sound-neutral savings in the Sweep, diffusers and transducers (pipelining as in ADR 0030), echo mode (removes the coupled S3), then block 96 if needed.

**Run 16 (prepared)** (branch `perf/run16`, not yet on the chip; same corner list as run 15, so the SPLIT compares line for line). Sound-neutral CPU savings only: every desktop render identical bit for bit with main (58 renders: hits, skank, clicks; every ATTITUDE, SPRINGS 1/2/3, TENSION 0/0.5/1; KICKED DRIVE 1 DECAY 1; a moves + Kicks render with odd block sizes; with and without FMA contraction), firmware selects by construction (same IEEE compare). Without the chip, the cycles come from `tools/m7_issue_model.py` (an in-order issue model over the disassembly; it reads main's Chirp loop at 14 cycles per section, run 8's measurement, and run 15's `tilt` and Spring shares within ~0.5 points). What changed, cycles/sample at a loose tank (TENSION 0: Sweep 50 sections, active Loops 39, idle 24):
- **Sweep** (`Sweep.h`, `StretchedChain.h`): sections pipelined as the Chirp (ADR 0030), coefficients in registers (they were reloaded after every store), rings section-interleaved in the pool (DTCM) with the Thiran state as one more row, a run of samples per call; 24 → 10.5 cycles per section: **~670 cycles, ~6.7 points of `tilt`** (16.0 → ~9.3; tight tank ~3.2).
- **Chirp sections** (`Spring.cpp` `chirpSections`, shared `dsp/StretchedChain.h`): the same interleaved rings (no per-section address arithmetic), two sections per pass with D{v} two ahead: 14 → 10.5 cycles per section, ~130 cycles per active Spring, ~77 per idle one: **~2.8 points at S1, ~3.4 at S2** (~2.3 at a tight tank).
- **Loop diffusers** (`Spring::processLow`): the three written out, their delayed samples read before the chain: **~59 cycles per Spring, ~1.8 points** (idle Springs too).
- **Branch-free clips and envelopes** (`dsp/Select.h`; `softClipSel` / `asymClipSel` in the LoopSat and DriveIn, the Splash's detector and envelope followers, the Clang ceiling): GCC's branches on the signal's sign or an envelope's attack/release became VSEL selects: LoopSat −12 per Spring, DriveIn −61, Splash −17, Clang −3: **~1.2 points** plus mispredicted branches the model can't count (the profile's noise makes the Splash's coin flips). DriveOut keeps the branches: there the selects measured slower.
- Tried and reverted (the model said slower): the high path run beside the LoopSat; both DriveOut pickups side by side (register spills).

**Expected run 16:** S2 worst ~63 % avg / **~68 % max** (run 15: 76.1 / 80.7), S1 ~62 / **~67** (74.1 / 79.9); tight tanks ~57 / ~62. SPLIT, loose: `tilt` ~9.3, sprA ~10.7, idle Springs ~9.0, `drvIn` ~4.3, `splash` ~5.2, `out` and `ctl` as run 15. S3 is left to echo mode (the coupled Loops in `out` are unchanged). Flash: release 113,412 B, profile 115,328 B (main 112,628 / 114,600). The estimates are a model's: run 16 on the chip is the check.

**Run 16 on the chip** (4 Oct, `dist/resilio_versio_m3_profile_run16.bin` from branch `perf/run16`, bit-identical to `main`: pipelined Sweep and Chirp sections, written-out Loop diffusers, branch-free selects). Worst cases: S1 avg 66.0 % / max 70–74 % (74.1 on the first corner after BENCH); **S2 avg 67.7 % / max 72.8 %**; S3 (still coupled) avg 72.1 % / max **77.8 %** (highest anywhere, the TN1 TO0.5 corner's loose → tight glide). Against run 15: S2 −8.4 points average / −7.9 peak, S3 −10.0 / −9.8. SPLIT at a loose tank: `tilt` 16.0 → 10.6, active Spring 12.7 → 11.9, idle Spring 10.5 → 9.9, `out` (S1/S2) unchanged ~9.3, coupled S3 `out` 51 → 46.5. The in-order issue model predicted S2 63 / 68; the chip is ~4–5 points higher (branch mispredictions and memory stalls the model doesn't count). Budget (ADR 0030 amendment, 4 Oct): ≤ 75 % peak target, 80 % ceiling: S1/S2 within target, S3 under the ceiling (echo mode replaces the coupled S3).

**Run 17 on the chip** (5 Oct, `dist/resilio_versio_m3_profile_run17.bin` from `843c5fc`: run 16 + TONE after the springs, echo mode, Throw + Hold, µ-law box). S1 worst avg 67.3 % / max 71.9 %; S2 avg 68.9 / max 74.6; **S3 = echo mode avg 73.3 / max 79.1** (echo section 6.0 %, Springs A/B 11.6 / 11.1, no Spring C; flat across DECAY/TENSION/TONE corners). **The first S1 corner after the S3 corners peaks at 82.1–82.4 % every lap**: the switch from echo mode back to the Springs (echo PEAK 6.1 and the Springs both running in that window, `ctl` PEAK 9.4, `out` PEAK 20.4). SPLIT changes vs run 16: `out` 9.3 → **18.4** (µ-law box ~8–9 points, its estimate was 3–4; TONE's return filter is skipped at noon), `drvIn` 4.7 → 6.0 (throw send + hold duck key), `ctl` +0.7. Budget: S1/S2 within the 75 % target, echo mode under the 80 % ceiling, the switch transient over it. Next: sound-neutral savings in the µ-law box and spreading the echo → Springs switch work.

**Run 18 (prepared)** (branch `perf/run18`, not yet on the chip; run 17's corner list plus a SPRINGS switch pair at the end). Sound-neutral savings only: 63 desktop float32 renders identical bit for bit with `main` (hits, skank, clicks; every ATTITUDE, SPRINGS 1/2/3, TENSION 0/0.5/1; moves with automation, Kicks, gates and clocks at blocks 48/37/7 incl. ATTITUDE flips; SPRINGS 3 ↔ 1/2 mid-tail; the Hold zone), with and without FMA contraction; the firmware by construction (same expressions, same libm calls). Cycles from `tools/m7_issue_model.py` along the actual path (it read run 16 4–5 points low: compare, don't add up):
- **µ-law box, the expansion from a table** (`OutputBits.h`): `(exp(|v| ln 256 / q) − 1) / 255` depends only on the code, so `prepare()` works every code out once with the same expression (12 + 8 bit: 2,176 floats, 8.7 KB of SRAM; whole bit depths asserted, a 10-bit KICKED just sizes it): no `expf` and no divide per half-rate sample and channel, ~0.5 points.
- **µ-law box, steady pair loops**: with no ATTITUDE fade under way, whole half-rate steps for both channels side by side (the four all-pass chains overlap; state read whole into locals), no per-sample glide or drain bookkeeping; the quantiser's signal coin flips (envelope attack/release, the signs) are VSEL selects. KICKED steady, one pair both channels: main 694 → 454 cycles (both steps: ~1.2 points of `out`). **CLEAN steady: only the down filters run (kept warm), 208 → 56 cycles per pair: ~0.3 points left** (it can't be 0 and stay bit-exact on a flip to DRIVEN/KICKED). Fades, the 2 ms drain and odd half steps keep the per-sample loop. Desktop, box alone: KICKED 20.1 → 14.1 ns/sample, CLEAN 7.8 → 1.4.
- **Spring C keeps its redesign turn without the maths** (`Spring::setSettingsUnheard`): with echo mode built Spring C is never run, so its L glide never landed and it redid `commitDesign` (~1,700 cycles) on every third tick even at rest, plus its heavy steps on every move and switch. Same answers to the Tank's turn logic, so A and B take their settings on the same ticks. ~0.2 points on average, up to ~0.4 on the blocks it landed on; one in three heavy steps gone from the switch. Desktop: the switch block's `ctl` 2.2 → 1.6 µs.
- The switch itself: the echo already stops the moment both ends of its fade are 0; the Springs' heavy redesign steps are already one per block (run 12), and spreading them further would move A/B's settings to other ticks (not bit-exact). What the window adds over steady echo mode is the echo fading out (its full cost for 80 ms), the Springs gliding from the echo's fixed tank to the knobs (a heavy step every block), the SPRINGS mix fade (a square root and a divide per sample) and, at the lap's wrap, TONE/DECAY/TENSION moving too.
- **New corners `SWITCH S3>S1` and `SWITCH S1>S3`** after the grid (D1 TN1 TO1, the last grid corner's knobs, so nothing else moves): SPRINGS flips half way through; their max / PEAK is the switch alone. The lap's wrap (S3 → first S1 corner, run 17's 82 %) is unchanged.

**Expected run 18** (if the box's chip cost scales like run 17's 8–9 points against the model's 3.5, the box saves ~2.5–3 points; if not, ~1.2): S1 max ~69–70.5 %, S2 ~72–73.5, S3 (echo mode) ~76–78, the S3 → S1 switch ~79–81. Still over the 75 % target in echo mode and at the switch. Left: the box's `logf` (double precision in newlib, ~55 cycles per half-rate step and channel; a bit-exact copy needs `vfma.f64`), interleaving the two channels' quantisers (−0.2 points by the model, +1 KB of flash: not done), the per-tick control maths of a switch, DriveOut. Flash: release 123,932 → 125,964 B (96 %), profile 125,664 → 127,696 B (97 %; +288 B of it the switch corners). No new `vfma`.

**Run 18 on the chip** (5 Oct, `dist/resilio_versio_m3_profile_run18.bin` from branch `perf/run18`, bit-identical to `main` at 843c5fc: the µ-law box's expansion from a table and steady pair loops, Spring C's redesign turn without the maths in echo mode; two SWITCH corners added). S1 worst avg 64.4 % / max 70.9; S2 66.0 / 72.0; **echo mode 70.2 / 76.2**; SWITCH S3>S1 max 77.3, S1>S3 74.6; the lap wrap (S3 → S1 with DECAY, TENSION, TONE jumping too) 79.7–79.9. `out` 18.4 → 16.3. Owner (5 Oct): merge it; echo mode's 76 % over the 75 % target is accepted under the 80 % ceiling, the click check decides.
