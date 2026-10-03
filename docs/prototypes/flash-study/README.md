# Flash study (4 Oct 2026)

Branch `proto/flash-study`. The question: how much room can we make in the 128 KB internal flash for the features the owner picked on 4 Oct (TONE after the springs, the tape echo in SPRINGS 3 with clocked controls, the gate THROW, the HOLD at the top of DECAY, the clock logic), and at what risk. The target was **at least ~12 KB free in both release and profile**, with no slower per-sample audio and no change to the sound.

## Result

| Build | Before (`main` 4a619bd) | After (this branch) | Free now |
|---|---|---|---|
| release | 126,280 B (96 %) | **112,628 B (86 %)** | **18,444 B** |
| profile (CPU test) | 130,496 B (99.6 %) | **114,600 B (87 %)** | **16,472 B** |
| m0test | 82,320 B | 82,320 B (unchanged on purpose) | 48,752 B |

Four commits, all in `firmware/` only. None of them touches `core/`: the six Core objects (Tank, Spring, Splash, Drive, Kick, Wobble) compile byte-identical before and after, in both release and profile (checked with an `objdump -d -r` fingerprint of each `.o`). So **no sound change and no per-sample code change, by construction.** For the profile build I also compared the linked disassembly of the audio callback, `Tank::process`, `Spring::process` and libDaisy's audio `InternalCallback`: identical apart from literal-pool addresses.

The profile build is now "release + 2 KB" (it keeps USB serial for the CPU report). Before, it was the build that ran out first.

## Where the flash went (before)

Flash bytes by origin, from the linker map (`.data` initialisers counted, since they are stored in flash too):

| Origin | release | profile |
|---|---|---|
| libDaisy: ST HAL drivers | 36,348 | 35,688 |
| Core: Tank | 27,683 | 28,003 |
| libDaisy: daisy classes | 19,892 | 19,196 |
| Core: Spring | 13,276 | 13,276 |
| firmware `main.o` | 6,635 | 6,817 |
| Core: Splash | 6,180 | 6,180 |
| Core: Drive | 4,404 | 4,404 |
| newlib libm | 2,940 | 2,644 |
| libDaisy: USB device stack | 2,888 | 6,492 (+548 data) |
| Core: Kick / Wobble | 1,998 / 1,812 | same |
| libgcc (double add, pulled by `sinf`/`cosf`) | 888 | 1,428 |
| startup + vectors / crt | 748 / 172 | same |
| newlib libc | 128 (+80 data) | 752 (+80 data) |
| **Total** | **126,280** | **130,496** |

So about 59 KB of release was libDaisy and the ST HAL, and 55 KB our Core. Our own voicing tables are small (all Core `.rodata` together is ~3.4 KB; the largest are `kParams` 360 B, `tankv::kTuning` 288 B, `drive::kVoice` 264 B, `wobble::kVoicings` 208 B). The earlier flash work (UART/SPI/USB-host/printf/exit stubs, `RV_SIZE_OPT`, `RV_NO_UNSWITCH`, fixed voicings) had already taken the easy Core savings.

Largest single functions (release, before): `Tank::process` 8,910 · `Tank::controlTick` 5,608 (already -Os) · `main` 3,724 · `HAL_RCCEx_PeriphCLKConfig` 3,712 · libDaisy `AudioHandle::Impl::InternalCallback` 3,500 · `Spring::process` 3,384 · `Splash::process` 2,416 · `HAL_PCD_IRQHandler` 2,364 · `Spring::coupledReturn` 2,036 · `Tank::updateBaseSettings` 1,966 · `Tank::prepare` 1,800 · `HAL_DMA_Start_IT` 1,736 · `HAL_DMA_IRQHandler` 1,726 · `Spring::prepareTransition` 1,588 · `HAL_DMA_Init` 1,306 · `DriveIn::set` 1,284.

The big finding: **libDaisy set up hardware the Versio firmware never uses.** The QSPI flash chip, the USB device stack in release, and (in the CPU-test build) the knob ADC and the Seed 1.1 codec's I2C. That's where the room came from.

## The levers

Bytes saved per build (release / profile / m0test). "Sound risk" is none wherever the Core objects are byte-identical.

| # | Lever | release | profile | m0test | CPU risk | Sound risk | Status |
|---|---|---|---|---|---|---|---|
| 1 | **Skip the Seed's QSPI flash set-up** (`QSPIHandle::Init` stub + MPU guard) | **5,988** | **5,980** | 0 | none (set-up only) | none (verified) | committed `a6a62b4` |
| 2 | **USB device stack out of release** (OTG_HS handler stub, release only) | **7,664** | 0 | 0 | none (never ran) | none (verified) | committed `d9c6029` |
| 3 | **Profile sets up only the Seed and LEDs** (no ADC/switch/gate init) | 0 | **5,076** | 0 | none: audio path disassembles identically | none (verified) | committed `67e4e60` |
| 4 | **Profile skips the Seed 1.1 codec's I2C set-up** | 0 | **4,840** | 0 | none: same | none (verified) | committed `59bcb51` |
| 5 | LED PWM: program the DMA registers directly instead of `HAL_DMA_Start` | ~950 (est.) | 0 | 0 | none (LEDs only) | none | not done: needs an LED check on the module; keep in reserve |
| 6 | `RV_SIZE_OPT` on the knob-move redesign (`updateBaseSettings`, `updateSpringSettings`, `prepareTransition`, `prepareDamping`, `setSettings`, `commitDesign`, `DriveIn::set`, `Tilt::set`) | ~3,990 (9,398 → 5,410) | same | 0 | **real**: these are the knob-move bursts the handoff ties to the red input LEDs | none | not recommended |
| 7 | Whole Core at -Os | ~15,600 | same | 0 | **high**: every per-sample loop slower | none | rejected |
| 8 | Whole Core at -O2 | ≥3,000 | same | 0 | **unknown**: changes every hot loop, would need new chip runs | none | rejected |
| 9 | LTO (`-flto`) | **−3,764 (bigger)** | **−3,824 (bigger)** | +40 | changes hot code (`Tank::process` gets inlined into the callback) | none | rejected (as on 1 Oct). It does keep `-ffp-contract=off`: 50 `vfma`/`vfms` in the image with and without LTO, all in libm/libDaisy |
| 10 | `-fno-math-errno` (inline `vsqrt`) | −1,368 (bigger) | −1,312 (bigger) | 0 | changes hot code | none in theory (IEEE sqrt) | rejected |
| 11 | libm: replace `sinf`/`cosf`/`log10f` with one cheap control-rate approximation | ~1,300–2,200 (`sinf`+`cosf`+tables 1,336, + libgcc 888 if nothing else needs it) | similar | 0 | low (control rate) | **not bit-exact**: coefficients change in the last bits, the desktop build would no longer match | not done |
| 12 | Last voicing-table leftovers (e.g. 4-entry `wobble::kVoicings`) | ~150–500 | same | 0 | none | none | not worth it now |
| 13 | newlib-nano, no exceptions/RTTI/unwind tables, `--gc-sections`, no printf/exit | already in place | | | | | nothing left: `.ARM.extab`/`.exidx` are 0 B, libc is 128 B in release |
| 14 | DaisySP | not linked at all (`DAISYSP_DIR` unset) | | | | | nothing to do |

What each committed lever does, in plain words:

1. **QSPI.** `DaisySeed::Init` always wakes up the Seed's 8 MB external flash chip: pins, clock, a reset, and a write to the chip's status register, then maps it into memory. We never use that chip (code is in internal flash, the delay pool in DTCM, no presets). A same-name `QSPIHandle::Init` in `firmware/no_uart_spi.cpp` replaces libDaisy's (the existing stubs work the same way). Because the chip's memory window is then dead, the stub also marks it "no access" in the MPU, so the processor can never read there speculatively (an STM32H7 hazard ST's app notes AN4838/AN4861 describe; it costs ~40 bytes). Release and profile; m0test keeps the real one.
2. **USB in release.** The release firmware never starts USB (no serial, and installs go through the chip's own DFU loader). libDaisy's USB interrupt handler still named the USB device driver, so the whole stack was linked. A zeroed handle and an empty handler (the same trick as the USB-host stub already in that file) leave it out. Profile keeps USB for its report.
3. **Profile init.** The CPU test ignores knobs, switches, button and gate, and never started the ADC, but `DaisyVersio::Init` set them all up. It now calls the Seed's own `Init` (identical clocks, caches, SDRAM, codec, audio) and starts the four LEDs itself.
4. **Profile codec.** On a Seed rev 1.1 the audio set-up talks to a WM8731 codec over I2C. The CPU test only measures the audio callback, which runs on the chip's own audio clock with or without a codec, and nobody listens to its output. So profile leaves the I2C and codec drivers out. **Release keeps them**: a friend's Versio may have a Seed 1.1.

**Recommended set: 1–4** (all committed). Keep 5 in reserve (≈1 KB, release only, needs a look at the LEDs). Don't do 6–11 unless a later CPU run shows clear margin, and then only 6, measured on the chip.

### Merge notes

- All four commits are safe to merge: firmware-only, Core untouched. ctest on this branch (`build-proto`, plugin off): **100 % tests passed out of 19**, read from the log.
- Each needs one **on-module check** before a release: the release build boots (boot sweep), audio passes, LEDs meter, knobs work; the profile build prints its CORNER lines over USB. Nothing in them can change the sound, but they change how the board is set up at power-on, which only the module can confirm. If the release boots and plays, levers 1 and 2 are proven; if the profile prints, 1, 3 and 4 are.
- Re-flash afterwards with NE Firmware Swap as usual: the install path is unchanged (internal flash, no bootloader).
- `firmware/README.md`'s size list and the HANDOFF "Will bite → Flash" note will need the new numbers when this merges (not edited here).

### What the new features would cost (rough, for the decision records)

- TONE after the springs: ~3 KB (owner's prototype estimate).
- Tape echo in SPRINGS 3 with clocked controls: ~2–2.5 KB. If position 3 stops being the coupled springs, the coupled code goes: `Spring::coupledReturn` 2,036 + `Tank::processCoupled` 824 + `Spring::coupledFinish` 568 + `Tank::coupleMatrix` 220 + tables 184 = **~3.8 KB** of named functions, plus whatever is inlined into `Tank::process`/`controlTick`. That roughly pays for the echo.
- THROW, HOLD, clock/division logic: small (hundreds of bytes each).
- Echo memory: 2 s of mono float at 48 kHz is 384 KB, which doesn't fit DTCM but does fit the 480 KB AXI SRAM (only ~31 KB used today) or the 64 MB SDRAM. Delay memory costs RAM, not flash.

Total ≈ 6–8 KB of new code, against 18 KB (release) and 16 KB (profile) free. Comfortable, with lever 5 and maybe 6 as reserves.

## Part 2: the larger-firmware route (Daisy bootloader)

Marked **[D]** documented (with link), **[A]** anecdotal/forum, **[I]** inferred by me.

**Can a bootloader build go through NE's Firmware Swap?** No. The community firmware index says some Versio firmwares "require a different bootloader" and must be installed with the Daisy tools because they "will not install through Noise Engineering's firmware updater" (Cithara Versio is the named example) [D, [index](https://github.com/Maxhodges/noise-engineering-firmware-index)]. The Daisy bootloader lives in internal flash and the app goes to the QSPI chip at 0x90040000 [D, [DaisyBootloader README](https://github.com/electro-smith/DaisyBootloader), [libDaisy bootloader guide](https://github.com/electro-smith/libDaisy/blob/master/doc/md/_a7_Getting-Started-Daisy-Bootloader.md)]; Firmware Swap writes one `.bin` to internal flash [I: that is what our builds are and they install fine]. So third-party bootloader firmwares exist for the Versio (Cithara), but none installs through NE's app.

**Install path.** Module out of the rack, rack power off, USB into the Seed on the back. (1) Put the Seed in the chip's ROM DFU mode: hold BOOT, tap RESET, release [D, [Daisy forum](https://forum.electro-smith.com/t/welcome-to-daisy-get-started-here/15/25)]. The Seed's BOOT and RESET buttons sit beside its USB socket [A, same thread]; on the Versio the Seed faces the back, so they should be reachable with the module out (SPEC §8 still lists this as unverified, so check by eye first) [I]. (2) Flash the Daisy bootloader to internal flash with the Daisy Web Programmer ([flash.daisy.audio](https://flash.daisy.audio/)) or `make program-boot`. (3) Within the bootloader's 2.5 s start-up window (the Seed's LED pulses; pressing BOOT keeps it waiting) send the app with `make program-dfu` / the web programmer [D, libDaisy guide]. Later updates need only step 3. Friends would have to do all three steps, and use a different tool than the one they know.

**Getting back to stock NE firmware.** Very likely yes, through Firmware Swap. The ROM DFU loader is burned into the chip and can't be overwritten [A/D, [Daisy forum](https://forum.electro-smith.com/t/welcome-to-daisy-get-started-here/15/25)]. NE's own note says that after any third-party firmware you return with a "button combo swap" [D, [NE: Firmware swapping is faster than ever](https://noiseengineering.us/blogs/loquelic-literitas-the-blog/hot-swap/)]. NE firmware is written to internal flash, the same place the Daisy bootloader lives, so installing it replaces the bootloader [I]. Nobody documents this exact round trip on a Versio [unverified]. Our app image would stay in QSPI at 0x90040000 (harmless) unless NE's firmware rewrites that area. Whether NE firmwares use the QSPI chip for their own data isn't documented anywhere I found [unknown].

**Brick risk.** Low. The ROM DFU is always there behind BOOT+RESET [A/D]. The real risks are practical: the buttons are on a board behind the panel, the 2.5 s window is fiddly (forum users report needing several tries) [A, [forum](https://forum.electro-smith.com/t/lets-please-discuss-the-bootloader-source-code/5530)], and there is no NE-supported path.

**BOOT_SRAM (code runs from internal RAM).** Apps up to 480 KB, copied from QSPI into AXI SRAM at boot, at speed "comparable to the internal flash" [D, libDaisy guide]. **It collides with our memory plan.** libDaisy's `STM32H750IB_sram.lds` puts all data, `.bss` and the stack in DTCM (128 KB) [D, [linker script](https://github.com/electro-smith/libDaisy/blob/master/core/STM32H750IB_sram.lds)], but our delay pool already fills 120,000 B of DTCM (ADR 0030: moving it there was worth ~1 CPU point), plus ~18.5 KB of `.bss` and the stack. That needs a custom linker script (e.g. code in the first ~300 KB of AXI SRAM, `.bss`/stack in the rest, pool kept in DTCM). Doable, but new work, and SRAM2/AXI space then has to be shared with the future echo buffer. CPU: code would run from AXI SRAM through the same 16 KB I-cache as today; misses are cheaper than flash wait states, so it's probably the same or slightly faster [I; no Daisy measurement found].

**BOOT_QSPI (code runs straight from the QSPI chip).** Up to ~7.75 MB, but "performance becomes more cache-dependent" / "a fair bit slower" [D, DaisyBootloader README and libDaisy guide]. Our hot code (`Tank::process` 8.9 KB, `Spring::process` 3.4 KB, `Splash::process` 2.4 KB, coupled path, Drive, the callback) is about 20 KB, more than the 16 KB I-cache, so every audio block would miss into QSPI. That's a real CPU risk at 63–66 % peak. A way out would be to copy the hot functions into the unused 64 KB ITCM at boot [I], but that's more custom linker work. No published per-sample measurements found.

## Recommendation

**Stay in internal flash.** The four committed levers made 18.4 KB free in release and 16.5 KB in profile without touching the sound or any per-sample code. That fits the picked features (about 6–8 KB of new code, part of it paid back if SPRINGS 3's coupled code goes), with ~1 KB (LED DMA) and ~4 KB (knob-move code at -Os, CPU-gated) left in reserve. The owner keeps the one-click Firmware Swap install, the known restore path, and a share that friends can install the way they install everything else.

Go to the bootloader only if a later feature (say a looper or a second big mode) needs more than ~20 KB more code. If that day comes: BOOT_SRAM with a custom linker script (pool stays in DTCM), not BOOT_QSPI. Expect a two-tool install for friends, and test the round trip back to stock NE firmware on the owner's module first.

## How to reproduce

```bash
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
make -C firmware all-variants LIBDAISY_DIR=<main checkout>/libs/libDaisy   # worktrees have empty libs/
```

Size map: the linker map (`firmware/build/obj-<mode>-48/*.map`) grouped by input object/archive member, and `arm-none-eabi-nm --size-sort -S -C`. Per-sample identity: an `objdump -d -r` hash of each Core `.o`, and a function-by-function disassembly compare of the linked ELFs with addresses normalised. The analysis scripts were scratch tools and aren't committed.
