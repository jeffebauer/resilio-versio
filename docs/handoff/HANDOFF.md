# Handoff

**Written:** 29 Sep 2026, end of the first (very long) session; finalised after the highs-later switch-on. Start the next one with `/resilio-start`.

## State

| Milestone | State |
|---|---|
| M0 toolchains | Built. **Owner hardware check pending** (`docs/m0-hardware-check.md`, flash `dist/resilio_versio_m0_test.bin`) |
| M1 one Spring + Renderer | Built. Owner listen pending; Wellspring take A comparison pending (no recordings yet) |
| M2 plugin shell | Built, automated tests pass. **Owner Ableton check pending** (`docs/m2-ableton-check.md`) |
| M3 hardware profiling | Firmware ready (`MODE=profile`). Blocked on M0. Profile serial output is new code: confirm `CORNER` lines are readable |
| M4 multi-spring stereo | Built; reworked in M8 round 1 (no flam) |
| M5 drive + TONE | Built; DRIVE retuned (ADR 0022) and again in round 1 |
| M6 anti-ringing | Built. Calibrated `ringing_db` metric (ADR 0023); adaptive suppressor not needed |
| M7 SPLASH/KICK/WOBBLE | Built; SPLASH and WOBBLE reworked in round 1 |
| M8 tuning | Round 1 merged; chirp switched to highs-later (`8f0a09c`, installed). Next: CLEAN splash, then TENSION |
| M9 polish | Not started (panel template ready: `docs/panel/`) |

- `main` HEAD and installed plugin: **`8f0a09c`** (highs-later chirp + round 1). `cat dist/installed_plugin.txt` to confirm.
- Gates at `8f0a09c`: **15/15 suites pass** (incl. plugin_host_test). Firmware release 103,208 B (78 %), m0test 85,976 B (65 %), profile 113,596 B (86 %). M6 grid (HighsLater): 270 cells, worst `ringing_db` 9.1.

## In flight

- **Nothing running.** The highs-later switch-on finished and is committed (`8f0a09c`) and installed.
- **Branch `proto/tension`** (pushed): the TENSION prototype, kept on purpose for stage 1 of ADR 0026. Not merged.

## Next steps (in order)

1. ~~Highs-later switch-on~~: done (`8f0a09c`, installed).
2. **CLEAN gets a gentler splash** (ADR 0025): light Clatter + tiny Jolt, CLEAN < DRIVEN < KICKED; criteria in the ADR. Files: `core/params/SplashVoicing.h`, `core/dsp/Splash.*`, test_splash / test_m7_tank.
3. **TENSION implementation**, stages 1–5 in ADR 0026 (port `proto/tension`, rename `boing` → `tension`, fix the 5 failing suites incl. two real tuning jobs, re-verify, docs).
4. Then an owner listening pass and M8 round 2 from their notes (`docs/m8-tuning-backlog.md`).

## Waiting on the owner (`docs/TASKS.md`)
Wellspring + Magneto recordings (→ `tools/ingest_references.py`), M0 hardware check, M2 Ableton check, listening tasks 5–9 and 8c (round 1), the ATTITUDE-flip Howl-exit question (task 8), the M7 questions (task 9), the licence (whenever), stereo-in (after M3).

## Will bite
- **CPU:** worst case estimated at ~65–66 % vs a 65 % target (desktop estimate). Confirm in M3 before trimming.
- **Thin margins that tipped before:** the ATTITUDE CLEAN→KICKED click check (now 6.8/10 after Morph 30→40 ms), CLEAN DRIVE mildness (−15.9 vs ≤ −15 dB). Re-check after any Drive change.
- **Combining agents' work breaks margins:** always re-run the full suite on the merge, not only in each branch.
- **Environment:** GitHub's main IPs sometimes time out on this network (push fails; retry later or use a VPN). The safety classifier has had outages (commands blocked; retry). Ableton scanning: a signature-broken bundle gets cached as failed; `touch` the binary so Ableton rescans it. Never write into signed bundles.
- **Test inputs:** `ringing_db` and `steady_tone` don't apply to tonal inputs (held tones); `max_step_db_100ms` doesn't apply to rhythmic ones; the click detector false-fires on impulse inputs. Sweeps use `ignore_flags`.
- **Chirp direction:** `docs/m8-sweetspot.md` and round-1 numbers in the backlog were measured in LowsLater; the backlog's "HighsLater re-tune" section has the current numbers. DRIVEN aliasing margin is only 1 dB (−61 vs −60).
- **LowsLater** still compiles and its core tests pass, but it's no longer tuned. Don't flip back without re-tuning.

## Where to look
`CLAUDE.md` · `docs/TASKS.md` · `docs/m8-tuning-backlog.md` · `docs/adr/0024`–`0026` · `core/params/Mappings.h` · `core/dsp/Tank.cpp`
