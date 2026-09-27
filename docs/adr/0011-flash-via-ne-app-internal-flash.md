# 0011 — Flash with NE's Firmware Swap app; firmware fits internal flash

**Status:** Accepted, 27 Sep 2026

**Decision:** Firmware is flashed with Noise Engineering's Firmware Swap web app ("Select Custom File"). The build targets internal flash (≤ 128 KB, standard libDaisy app, no Daisy bootloader / `APP_TYPE`). Every firmware build reports binary size vs. the 128 KB limit.

**Why:** The owner already uses this app routinely for 1st- and 3rd-party firmwares, and it restores stock firmware, so it's a known-safe path. The community firmware index says bootloader-based firmwares don't install through NE's updater.

**Tradeoff:** Code size is capped at 128 KB. That's ample for this DSP. Large data (delay lines) lives in SRAM/SDRAM, not flash. If the cap is ever hit: first `-Os` on non-audio code and trimming libDaisy/DaisySP usage, then (last resort) BOOT_SRAM via the Daisy Web Programmer, reopening this ADR.
