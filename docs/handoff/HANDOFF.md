# Handoff

**Written:** 29 Sep 2026, end of the first (very long) session. Start the next one with `/resilio-start`.

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
| M8 tuning | Round 1 merged + installed (`a4fb02e`). Round 2 = the three decisions below |
| M9 polish | Not started (panel template ready: `docs/panel/`) |

- `main` HEAD: see `git log`. Installed plugin: `cat dist/installed_plugin.txt` (was `a4fb02e`, round 1, LowsLater chirp).
- Gates at `a4fb02e`: **15/15 suites pass** (incl. plugin_host_test). Firmware release 102,856 B (78 %), m0test 85,976 B (65 %), profile 113,236 B (86 %).

## In flight

- **Highs-later switch-on + retune (agent started 29 Sep, may still be running).** Brief: set `kChirpDirection = HighsLater` (`core/params/Mappings.h`), fix the four checks a straight flip failed (test_drive wet-level spread, aliasing at max DRIVE, KICKED DRIVE level spread; test_antires Micro-mod whole-Tank reading, possibly a measurement artefact), re-verify round 1 in HighsLater, render `renders/m8_hl/`. **If the working tree has uncommitted changes in `core/`, `host/tests/`, `tools/ir_dispersion.py` or `docs/m8-tuning-backlog.md` / ADR 0024, that's this agent's unreviewed work.** Review the diff, run the full suite + M6 grid, then commit (explicit paths) and install.
- **Branch `proto/tension`** (pushed): the TENSION prototype. Kept on purpose for stage 1 of ADR 0026. Not merged.

## Next steps (in order)

1. **Finish the highs-later switch-on** (above), commit, `tools/install_plugin.sh <commit>`, update TASKS "Plugin installed" + a rescan note.
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
- **Chirp direction:** anything measured or tuned before the switch-on (round 1 numbers, sweet-spot report) was in LowsLater.

## Where to look
`CLAUDE.md` · `docs/TASKS.md` · `docs/m8-tuning-backlog.md` · `docs/adr/0024`–`0026` · `core/params/Mappings.h` · `core/dsp/Tank.cpp`
