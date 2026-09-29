# Handoff

**Written:** 29 Sep 2026, end of session 3 (a long hardware night). Start the next one with `/resilio-start`. The owner plans, in the morning: the SPLASH voicings listening page, then M3 run 11 on the Versio.

## State

| Milestone | State |
|---|---|
| M0 toolchains + hardware | **Passed** on the owner's Versio (Daisy Seed 2). Found: the Versio's analog path inverts polarity and is +1.17 dB hot; the release build undoes both (`kOutputTrim` -0.874). Knob order measured: pots P1–P7 = libDaisy knobs 0, 4, 2, 1, 5, 3, 6 |
| M1 one Spring + Renderer | Built. **A/B vs the Wellspring built** (`renders/references/wellspring/ab/index.html`, TASKS 4b): Wellspring tail 3.5 s (combined-click method), matched by DECAY 0.70. Owner listen pending |
| M2 plugin shell | Built. Owner Ableton check pending (TASKS 4) |
| M3 hardware profiling | **In progress, nearly there.** Worst case 83 → ~63 % average (target 65 %), peak 100 → ~67 % (run 9). Runs 10–11 not measured yet. See `firmware/README.md` "M3 results", ADR 0030 |
| M4–M7 | Built |
| M8 tuning | Knob layout (ADR 0028), earlier first echo (ADR 0029) done. **SPLASH round 3** in progress: prototype voicings rendered, owner to pick (TASKS 3d) |
| M9 polish | Not started |

- `main` HEAD: this handoff's commit. Code HEAD before it: `fca8d6a` (ADR 0029) plus docs.
- **Installed plugin: `ae844da`** (29 Sep 19:13, `dist/installed_plugin.txt`). `main` has moved on: ADR 0029's earlier first echo, the Jolt fix and the M3 changes are not in Ableton. Next install after the SPLASH pick.
- **Gates at wrap:** **15/15 suites pass** (ctest log read, not the exit code). M6 grid after ADR 0029: 270 cells, 0 Ringing (worst 14.7, limit 15, the known KICKED/1 Spring/tight/TONE 1/DECAY 0.75 corner), Howl 54/54. Firmware: release 110,008 B (83 %), m0test 85,976 B (65 %), profile 124,996 B (**95 %, warning**: the benchmark code, see "Will bite").
- SPEC v1.0.17. New ADRs 0028 (knobs follow the printed panel), 0029 (earlier first echo), 0030 (fitting the CPU).

## In flight
- **Nothing running.** No worktrees besides `main`. Branch `proto/tension` still superseded (owner hasn't said delete).
- **SPLASH round 3 prototype** lives outside the build: `docs/prototypes/splash-voicings.patch` (apply to a copy of `core/`, build a renderer per `-DRV_SV=0..3`; header has the commands). Renders + listening page: `renders/splash_voicings/index.html` (gitignored; regenerate from the patch if lost). Findings: `docs/m8-tuning-backlog.md` "SPLASH round 3".
- `dist/` holds every M3 profile binary from tonight (`_dtcm`, `_split`, …, `_run10`, `_run11`); only **`resilio_versio_m3_profile_run11.bin`** matters now. Gitignored, local only.
- `renders/` new folders: `references/` (M1 A/B), `m3_stagger_ab/`, `m3_stagger_abc/`, `predelay_ab/`, `splash_check/`, `splash_voicings/`. All listened to except `splash_voicings`.

## Next steps (in order)
1. **Owner's SPLASH picks** (they paste "Copy results for Claude" output into chat). Build the chosen voicing into the Core from the patch: SPLASH = "how hard the hits hit", derived from the input; Kick keeps its crash; Jolt stays. Then ADR 0031, SplashVoicing constants, rework the SPLASH tests (test_splash, test_drive's SPLASH checks, test_tank crash numbers), re-run the full suite and the M6 grid, and check CPU on the next profile run (the envelope + HF split are cheap, but measure). Then ask about DRIVE coupling (TASKS 3d; B and E showed DRIVE alone barely splashes in our model).
2. **M3 run 11** (owner flashes `dist/resilio_versio_m3_profile_run11.bin`, USB only, streams in the Claude Terminal panel; read with `read_terminal` on its tab). Target: worst case (S3 D1.0 TN0.0) `max` ≤ 65 %. If still over: candidates in `firmware/README.md` "M3 results" (more reciprocal/division work, the output stage's 9 %, drive stages). Block 96 only with the owner's OK (ADR 0030). Then trim `m3_bench.cpp` out of the profile build (flash 95 %) and hand the owner the **release** firmware to flash and play (their knob layout, output fix): verify a passthrough take matches a cable (polarity and level) once.
3. **Install the plugin** after SPLASH lands (`tools/install_plugin.sh <commit>`, TASKS "Plugin installed" line, rescan note).
4. **Metrics.cpp T60** reads long on repeated stimuli (segment ends at the next event's -40 dBFS crossing, which includes a spring's build-up): fix, then re-check every gate that pins a T60 (tuning backlog, M1 section).
5. Owner listening backlog (TASKS 4b, 5–10) feeds M8 round 2.

## Waiting on the owner (`docs/TASKS.md`)
SPLASH voicings page (3d) and the DRIVE-coupling decision; M3 run 11 (3c); M2 Ableton check (4); M1 A/B listen (4b); listening tasks 5–10; WOBBLE ceiling (2); delete `proto/tension`?

## Will bite
- **ctest exit codes lie through pipes.** Tonight a piped background run "exited 0" with 2 suites failing. Log to a file and gate on `100% tests passed` (CLAUDE.md Hard rules).
- **Test windows that assume the pickup position:** the Tank-level chirp tests now use `modes::kPickupArrival` (ADR 0029). Any other test that windows "the first echo" by a fixed multiple of L will break the same way if the pickup moves again.
- **Thin margins:** M6 worst cell 14.7 vs 15 (same corner as before). SPLASH round 3 changes what goes into the Loops: re-run the grid.
- **Firmware vs desktop numerics:** firmware uses `-ffp-contract=off`, the desktop fuses; outputs differ ~90 dB down (recirculating tails). Bit-exact checks must compare like with like: build the old and new `core/` with the same flags (tonight: a small harness driving `rv::Tank` with parameter moves and Kicks, float32 output compared byte for byte, once with `-ffp-contract=off`).
- **M7 performance intuition was wrong twice tonight:** reordering the Chirp loop stage-by-stage was bit-exact but slower; measure on the chip (`m3_bench.cpp` shapes print as a BENCH line) before committing to a shape. Profile flash is at 95 %: drop the bench (or the SPLIT/PEAK code) before adding more.
- **Serial on the Versio:** USB CDC `TransmitInternal` drops a second back-to-back send; build one buffer per report. On USB power the knobs read 1000 (no rack rails): knob checks happen on rack power via LEDs. Never USB + rack power together.
- **Owner-facing review pages:** columns per group, colour-coded headers, settings as chips, synced switching, picks + notes + "Copy results" (memory `review-pages-columns`). The older `make_review.py` pages were hard to use; update that tool when next touched.
- **Preview pane can't play local audio** (it snapshots `file://` pages as `data:`); verify file paths from the shell instead.
- **GitHub routing drops** on this network happen; retry push later.

## Where to look
`docs/TASKS.md` (3c, 3d) · `firmware/README.md` "M3 results" · `docs/m8-tuning-backlog.md` "SPLASH round 3" · `docs/prototypes/splash-voicings.patch` · `docs/adr/0028`–`0030` · `core/dsp/Splash.cpp` + `core/params/SplashVoicing.h` (where round 3 lands)
