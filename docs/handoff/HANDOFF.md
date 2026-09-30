# Handoff

**Written:** 30 Sep 2026, end of session 4 (a very long day, 29 Sep overnight → 30 Sep evening). Start the next session with `/resilio-start`. The owner is low on weekly usage: keep agents few and focused, and replies lean.

## State

| Milestone | State |
|---|---|
| M0 hardware | **Passed.** Output polarity/level fix confirmed on hardware (OPTX takes: −0.65 dB vs a straight loop, polarity right) |
| M1 one Spring + Renderer | Built. Superseded as a listening task by the Wellspring work below |
| M2 plugin | Built. Owner's Ableton check waits for the next install |
| M3 CPU | **Done.** Target raised by the owner to **≤ 70 % peak** (ADR 0030 amendment, SPEC v1.0.20). Run 12 (main): 60.7 % avg / 63.3 % peak. Run 13 (SPLASH/DRIVE build, before tuning): 63.4 / 66.3 % |
| M4–M7 | Built |
| M8 tuning | **In progress.** SPLASH/DRIVE build approved (merge pending, see In flight); tank sound being fitted to the owner's Wellspring; bipolar WOBBLE prototyped |
| M9 polish | LED meters with DMA-driven smooth dimming (ADR 0031, merged); manual + preset drafts (`docs/manual.md`, `docs/presets.md`) |

- `main` HEAD: this handoff's commit (code last changed at `bc90d47`: profile LED threshold 70 %). Pushed to GitHub today (was 57 commits behind).
- **Gates on `main`:** 16/16 suites pass (ctest log read, after the afternoon merges; later commits are docs only). Firmware: release 118,760 B (90 %), m0test 85,976 B (65 %), profile 127,044 B (96 %, warning).
- **On the Versio:** release `dist/resilio_versio_release_e618e12.bin` (= main's sound; LED meters, run 12 trim). The owner last flashed run 13 (profile) and may still have it on.
- **Plugin in Ableton: `ae844da`** (29 Sep). Way behind: the next install should be the merged SPLASH/DRIVE build.
- SPEC v1.0.20 on main. ADRs on main to 0031 (+ amendments to 0018, 0030). On branches: 0032, 0033 (SPLASH/DRIVE build), 0034 (bipolar WOBBLE).

## In flight (read before touching anything)
1. **SPLASH/DRIVE build: approved by the owner, merge pending.** Branch `worktree-agent-a96f54d1b700d5ca6` (worktree `.claude/worktrees/agent-a96f54d1b700d5ca6`). Commits to 3c65405 (version D, approved in every panel), plus possibly one more: an agent was **stretching the DRIVE grit** (owner: "drive80 still feels a bit hot … 100% would yield distortion that wouldn't be particularly useful") so that today's 0.8 grit arrives at ~1.0, without changing the INPUT gain range, the +6 dB "partly louder" curve, Bite/Clang, or the CLEAN < DRIVEN < KICKED spread. It was told to render spot-check files into `renders/splash_drive_build2/drive_top/` and update `dist/resilio_versio_m3_profile_run13.bin`. **Check:** `git -C .claude/worktrees/agent-a96f54d1b700d5ca6 log --oneline -3` and its `git status`; if the stretch commit is there, re-run the full ctest in its `build-agent` (read the summary line), `make -C firmware all-variants`, and listen to or measure the drive_top files against D. If it isn't there (the agent died with the session), do the stretch yourself (lower each ATTITUDE's top colour pre-gain to its old DRIVE-0.8 value; `core/params/DriveVoicing.h`), same checks. **Then (owner already approved D):** cherry-pick onto main (conflicts likely in SPEC changelog, backlog, firmware/README; keep both sides), full gates, update firmware/README "M3 results" with run 13, the README's SPLASH/DRIVE rows, build the release to `dist/resilio_versio_release_<hash>.bin`, and install the plugin with `tools/install_plugin.sh <commit>` (Ableton closed; then TASKS' "Plugin installed" line).
2. **Bipolar WOBBLE prototype**: branch `proto/bipolar-wobble`, worktree `.claude/worktrees/agent-a8fb67c34b36df755`. Launched as a "remote" agent but it **ran locally** (see memory `remote-agent-ran-locally`). At wrap: code committed (7c6be39), docs uncommitted (CONTEXT, SPEC, manual, ADR 0034, docs/prototypes/bipolar-wobble/). It was to push the branch and commit a render script (no WAVs). **Check** the worktree's status and commits; finish or commit the docs; build its `rv_render` and run the render script, then build the page with `tools/review/make_review.py` into `renders/proto_bipolar_wobble/`. Its tests need re-checking on the Mac (plugin_host_test). WOBBLE constants were meant to live in a new header, so the merge with the SPLASH/DRIVE build stays easy.
3. **Prototype branches kept on purpose** (listening history; code to reuse): `proto/wellspring-fit` (worktree kept: the next round builds on it), `proto/smooth-arc`, `proto/sweet-tank`, `proto/diffuse-tank`, `proto/splash-round4`, `proto/tight-ringing` (bend fade, now inside the SPLASH/DRIVE build), `proto/wobble-hang` (pushed; its "drift together" is folded into bipolar WOBBLE), `proto/low-tail` (retired idea), `proto/tension` (old; owner hasn't said delete). Two worktrees (splash-round4, diffuse-tank) are locked by the app; leave them. `claude/nifty-shtern-b943cb` belongs to a separate session fixing `rv_render --set` switch labels.
4. **Renders (gitignored, local):** listening pages under `renders/` for every round today (`splash_round4`, `splash_drive_build`, `splash_drive_build2`, `proto_*`, `eq_preview`), plus `test_audio/hardware/` (the owner's OPTX takes H0–H4, notes in `NOTES.md`, tracked).

## Next steps (in order)
1. **Finish and merge SPLASH/DRIVE** (In flight 1). Then the release firmware, a click check on the Versio (TASKS §3), the plugin install, and the owner's M2 Ableton check.
2. **Next Wellspring fit round** (local only: the recordings never leave the Mac; one focused agent). Build on the merged SPLASH/DRIVE code plus `proto/wellspring-fit`'s **B** (the Sweep: `core/dsp/Sweep.h`, `core/params/WellspringFit.h`; the owner picked B, and C's 2.5 kHz tone dip is dropped). Targets for the owner's "resonant quality … in a different register": **thin arcs** with near-silence between (less smear and fewer extra pickups now that the Sweep carries the dispersion; guard the wobble and 250–500 Hz width), and the Wellspring's **highs-only arcs at half the echo period** (2–5 kHz every ~35 ms vs ours 68 ms: a faster high path). Then tail peakiness and the low-mid T60 (~0.9 s short). Compare at **SPLASH 0** and at the default (memory `compare-tank-at-splash-0`). Fix the two tight-end Howl cells the fit left. Method, targets and scripts: `proto/wellspring-fit:docs/prototypes/wellspring-fit/` and its backlog section "Wellspring fit".
3. **Bipolar WOBBLE page** for the owner (In flight 2), then merge with ADR 0034 after their pick.
4. Fold the chosen tank changes into the Core with ADRs, re-tuned tests (many tests encode today's sound: see each prototype's gate list), a CPU run (~3.5 points left under 70 % after run 13), then an install.

## Waiting on the owner (`docs/TASKS.md`)
- Nothing blocking right now: the SPLASH/DRIVE build is approved; they're waiting on Claude for the merge, release and plugin.
- Next listens: the Wellspring-fit round 2 page, the bipolar WOBBLE page.
- Hardware: the click check on the next release; optional DECAY fully-left/right OPTX takes (the tails measured ~10 % shorter on hardware, probably DECAY's physical noon).
- Open design questions list in TASKS (SPLASH scaling the Kick's crash, KICKED stereo lurch, Howl on a tight tank, TONE's right side / Big Knob, bright vs dark material).

## Will bite
- **Merging needs the owner's yes** (auto-mode blocked an unasked merge; memory `merge-needs-owner-ok`). D is approved; the DRIVE stretch was requested by the owner, so merging D + stretch is covered.
- **`rv_render --set attitude=KICKED` renders CLEAN**, and `--set springs=2` gives 3 Springs (it parses switch labels as numbers). Use `--preset` JSON until the other session's fix lands. A T60 CLEAN-vs-DRIVEN check was wrong because of it.
- **Profile flash at ~97 %** (128,2xx B on the SPLASH/DRIVE branch): trim the BENCH line / profile-only code before adding more. Release ~120 KB (91 %).
- **CPU headroom ~3.5 points** after run 13 against the 70 % target; the Sweep (fit C) was estimated cheaper than smooth-arc D in the worst case, and bipolar WOBBLE ~+0.2. Each landing needs a profile run.
- **Thin margins on the SPLASH/DRIVE build:** 3 `steady_tone` flags (CLEAN, DECAY 1, TENSION 1, TONE 1: the same tail as main, pushed over the metric's −30 dBFS by DRIVE's added level); 16/16 otherwise.
- **The first-hit fix** starts the excitation trim at −6 dB; the first chord after a run of drums is still ~2.5 dB hot (one control-tick lag).
- **Agents writing outside their worktree:** an agent found another branch's backlog text in its worktree once. Brief agents to check `pwd` before writing, and diff their branches for stray files.
- **Disk:** an agent's scratch grids once filled the disk (56 GB). Brief a ~10 GB cap.
- **Concurrent ctest runs** make timing-based suites fail spuriously (test_antires, test_drive, test_metrics). Re-run alone before believing a failure.
- **Stale reference renders:** `renders/references/wellspring/ab/` was regenerated tonight by the ingest; don't trust older copies of match renders.
- **GitHub routing drops** on this network happen; retry pushes.

## Where to look
`docs/TASKS.md` (the owner's list, reorganised today) · `docs/m8-tuning-backlog.md` from "Sonic signature vs real springs" to the end (the whole Wellspring story, SPLASH rounds 3–4, DRIVE decisions, run 13) · the SPLASH/DRIVE branch's ADRs 0032/0033 and its backlog section "SPLASH/DRIVE build" · `proto/wellspring-fit:docs/m8-tuning-backlog.md` "Wellspring fit" · `docs/dub-spring-reference.md` §8 · `firmware/README.md` "M3 results"
