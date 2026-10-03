# 0011 — Flash with NE's Firmware Swap app; firmware fits internal flash

**Status:** Accepted, 27 Sep 2026

**Decision:** Firmware is flashed with Noise Engineering's Firmware Swap web app ("Select Custom File"). The build targets internal flash (≤ 128 KB, standard libDaisy app, no Daisy bootloader / `APP_TYPE`). Every firmware build reports binary size vs. the 128 KB limit.

**Why:** The owner already uses this app routinely for 1st- and 3rd-party firmwares, and it restores stock firmware, so it's a known-safe path. The community firmware index says bootloader-based firmwares don't install through NE's updater.

**Tradeoff:** Code size is capped at 128 KB. That's ample for this DSP. Large data (delay lines) lives in SRAM/SDRAM, not flash. If the cap is ever hit: first `-Os` on non-audio code and trimming libDaisy/DaisySP usage, then (last resort) BOOT_SRAM via the Daisy Web Programmer, reopening this ADR.


**Amendment (4 Oct 2026, owner OK to merge the flash study): stay in internal flash.** The study (`docs/prototypes/flash-study/README.md`) freed 13.7 KB in release (126,280 → 112,628 B) and 15.9 KB in profile (130,496 → 114,600 B) by leaving out hardware set-up the firmware never uses: the Seed's QSPI flash chip (its address window marked no-access in the MPU), the USB device stack in release, and in profile the ADC/controls and the Seed 1.1 codec. The Core's per-sample code is byte-identical (object disassembly hashes). The bootloader route was studied and rejected for now: Firmware Swap can't install it (community firmware index), and the USB/BOOT/RESET install with a 2.5 s window is too much for friends. Revisit only if a feature needs more than ~20 KB more code; then BOOT_SRAM with a custom linker script (libDaisy's puts data and stack in DTCM, which our delay pool fills), not BOOT_QSPI (hot code is larger than the I-cache).
