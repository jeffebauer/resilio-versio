# Handoff

**Written:** 29 Sep 2026, end of session 2. Start the next one with `/resilio-start`. The owner plans to test the plugin, then start the **Versio flashing tasks** (M0 hardware check, TASKS task 3).

## State

| Milestone | State |
|---|---|
| M0 toolchains | Built. **Owner hardware check next** (`docs/m0-hardware-check.md`, flash `dist/resilio_versio_m0_test.bin`; its LED_0 blue = K2, now TENSION) |
| M1 one Spring + Renderer | Built. **Wellspring recorded and analysed** (T60 ~5 s, repeat ~83 ms, highs 5.7 ms later, fC ~4.5 kHz). Next: DECAY matched to the Wellspring, M1 A/B page |
| M2 plugin shell | Built. **Owner Ableton check pending** (`docs/m2-ableton-check.md`, updated for TENSION) |
| M3 hardware profiling | Firmware ready (`MODE=profile`); blocked on M0. Corner lines now read `S3 D1.0 TN0.0 TO1.0` (TENSION 0 = loosest = worst case) |
| M4–M7 | Built |
| M8 tuning | TENSION (ADR 0026 + amendment: up = tighter), CLEAN splash (0025), SPLASH round 2 "heavier clang", TONE bright-side low cut, per-Spring damping/decay spread (0027): all **installed**. Owner listening (TASKS 8–10) feeds round 2 |
| M9 polish | Not started |

- `main` HEAD: see `git log -1` (this handoff's commit). Last code commit **`ae844da`** = **installed plugin** (`dist/installed_plugin.txt`, 29 Sep 19:13). Everything after it is docs.
- Gates at `ae844da` (re-run at wrap): **15/15 suites pass**. Firmware release 105,808 B (80 %), m0test 85,976 B (65 %), profile 116,196 B (88 %). M6 grid: 270 ringing cells 0 Ringing (worst 14.6), Howl 54/54 `howl_ok`.
- SPEC v1.0.14. New ADR 0027. ADR 0009 amended (recordings stay out of git), ADR 0026 amended (TENSION direction).
- Reference recordings: `test_audio/reference/` holds 12 Wellspring + 13 Magneto WAVs **on the owner's Mac only** (gitignored). Provenance and settings: `test_audio/reference/NOTES.md`. Results: `docs/reference-report.md`.

## In flight
- **Nothing running.** No worktrees besides `main`.
- Branch **`proto/tension`**: fully superseded by `main`. Safe to delete once the owner agrees.
- `.idea/` in the repo root is the owner's IDE folder; leave it untracked.

## Next steps (in order)
1. **Support the M0 hardware check** (owner's next task, `docs/m0-hardware-check.md`). Help read serial output; never have USB and rack power connected at once. After M0 passes: M3 profiling (`firmware/README.md`), confirming the `CORNER …` lines are readable and the CPU worst case (desktop estimate ~65–70 %, target 65 %).
2. **M1 A/B:** make `tools/ingest_references.py` measure take A's T60 by combining the six click tails (each single click only reaches ~40 dB above the −94 dBFS floor, so the per-click Schroeder fit fails). Then match DECAY to the Wellspring (B/C read 5.0–5.2 s) and build the A/B review page against `wellspring_A`/`B`/`E` (the ingest's matched-render path does this once A's T60 exists).
3. **Owner's listening answers** (TASKS 5–10, 8b, 8c) → **M8 round 2**. Known candidates in `docs/m8-tuning-backlog.md` "Session 2 close": the WOBBLE ceiling vs the Magneto (owner decision), sharing WOBBLE's slow Drift across Springs (skank partial at WOBBLE 0.2), and re-running `tools/sweetspot.py` (its report predates TENSION, SPLASH round 2 and the TONE low cut).
4. **Use the Magneto data:** MW (wow 0 / 0.6 / 3.5 / 8.4 / 7.7 cents at CCW / 9 / 12 / 3 / CW) for WOBBLE's constants once the owner answers the ceiling question; MD1–3 (crest 27 → 21 dB, green → red) as the DRIVEN tape reference (ADR 0020).

## Waiting on the owner (`docs/TASKS.md`)
M0 hardware check (task 3), M2 Ableton check (task 4), listening tasks 5–10 on the new build (8b SPLASH C, 10 TENSION/DECAY and the ringing corners), the WOBBLE-ceiling decision (task 2), the ATTITUDE-flip Howl-exit question (task 8), the M7 questions (task 9), whether to delete `proto/tension`. Stereo-in stays "after M3".

## Will bite
- **Thin margins:** M6 worst cell 14.6 dB vs 15 (KICKED, 1 Spring, tightest TENSION, TONE 1, DECAY 0.75, noise bursts; SPLASH's clang in the Loop nudges it). test_tank stab mono notch −4.4 vs −4.5 (after `kSide2` 0.43 → 0.40). CLEAN DRIVE mildness −15.5..−16.6 vs ≤ −15. Re-run the full suite and the M6 grid after any change to Springs, SPLASH, DRIVE or TONE.
- **TENSION direction:** 1 = tight, 0 = loose (flipped mid-session). Older renders (`renders/tension/`) and backlog notes written before the flip use 0 = tight; they say so.
- **Worst case is TENSION 0** (loosest: most stages, longest L). Tests that pin "worst" use `tension = 0`.
- **Git:** `git apply --3way` stages what it applies; a docs commit swept in staged core files once this session (caught before pushing). Keep the main checkout on `main`: the owner reads docs from it (CLAUDE.md Git rule).
- **Ableton sessions:** unwarped clips store start markers in **seconds**; the `01_clicks` clips had a stray 0.005 s start (compensated on copy). Recordings land ~10 samples early (TRS); ADAT/OPTX2 is ~94 samples later than TRS (own loopback per path). A loaded plugin keeps the old build after an install: restart Live, rescan, fresh instance.
- **Disk:** the Mac ran out of space once (renders). `renders/` was cleaned 31 → ~3 GB; keep big sweeps in scratch and delete their WAVs after reading the sidecars.
- **Environment:** GitHub routing drops on this network (retry push later). Never run `auval -a` or touch CoreAudio (CLAUDE.md).

## Where to look
`docs/TASKS.md` · `docs/m0-hardware-check.md` · `firmware/README.md` · `docs/m8-tuning-backlog.md` ("Session 2 close") · `docs/reference-report.md` + `test_audio/reference/NOTES.md` · `docs/adr/0026`, `0027`
