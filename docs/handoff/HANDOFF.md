# Handoff

**Written:** 6 Oct 2026, end of session 8 (5–6 Oct). Start the next session with `/resilio-start`. **Nothing in flight:** no agents running, every session branch merged and its worktree removed.

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1, M4–M7 | Built |
| M2 plugin | **Passed** (owner, 6 Oct: Ableton check + panel) |
| M3 CPU | **Run 21** on the chip (`bdd3910`): S1 63.1 / 68.8 %, S2 63.9 / 70.4, **echo mode 69.0 / 75.7**, switch S3>S1 76.0, lap wrap 78.1. Budget ≤ 75 % target, 80 % ceiling (ADR 0030); over target only where accepted (echo at DECAY 1, switch moments). Runs 20/21 in `firmware/README.md` |
| M8 tuning | Session 8 merged: tape wear B, springs blend C, gate clock holds, tap LEDs, TENSION note values (summary at the end of `docs/m8-tuning-backlog.md`). Click check passed on `9ba726f` |
| M9 polish | README rewritten for visitors; manual, presets, share read-me checked against the code; **friends' release published** [v2026.10.06-a6c70a4](https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.06-a6c70a4) (private). Left: a printed panel overlay with the new names |

- **`main` HEAD:** this handoff's commit; code last changed at `4c6603f` (comment) / `7821946` (panel names). Pushed to origin.
- **Gates:** full ctest **100 % of 24** at `4c6603f` (since then only docs, TASKS, a preset file rename). Firmware at HEAD: **release 126,380 B (96 %), profile 127,520 B (97 %, 3.5 KB left), m0test 82,320 B (62 %)**.
- **Plugin in Ableton:** `c1d98ac` (= HEAD's code; new panel names). AU validated.
- **On the Versio:** `bdd3910` (springs blend C, purple 4 s; click check passed on `9ba726f`, same sound). Owner to flash the released `a6c70a4` firmware (adds the clock hold) and check it (TASKS §1).
- **Panel names (ADR 0044, v1.0.43):** BLEND (key `mix`), TANK 1 · 2 · ECHO (`springs`), ATTITUDE CLEAN · TAPE · VALVE (`attitude`; code still says Driven/Kicked: mapping in `CONTEXT.md`), button THROW / TAP. **Keys unchanged on purpose** (saved Ableton sets). The Renderer reads old labels too (`paramsjson::switchPosition`).

## In flight
Nothing. Kept on purpose: locked worktrees `agent-a5c8…` (proto/splash-round4), `agent-adce…` (proto/diffuse-tank); `.claude/worktrees/share` (make_release.sh reuses it); `nifty-shtern-b943cb` (owner archiving it). Reference branches unmerged, no worktree: `proto/echo-springs`, `proto/hiss`, `proto/low-tail`, `proto/smooth-arc`, `proto/sweet-tank`, `proto/tension`, `proto/tight-ringing`, `proto/tone-place`, `proto/wellspring-fit`, `proto/wobble-hang`.

## Next steps
1. **Owner's module check** (TASKS §1): flash `dist/release/v2026.10.06-a6c70a4/resilio_versio_firmware_a6c70a4.bin`, clock echo mode by CV from Ableton, stop/start the transport: no swoop. Record the result in TASKS.
2. **Friend feedback** on the release: owner pastes replies; triage into TASKS.
3. **Queued, small:** the first chord after a run of drums is ~2.5 dB hot (the sustain trim's one-tick lag; ADR 0035). Fix in the sound, bit-identical elsewhere.
4. **Panel overlay** (M9): `docs/panel/` has the mapping SVG with the new names (`tools/make_panel_mapping_svg.py`); a printable overlay is the remaining polish item if the owner wants one.
5. **Design questions** still open in TASKS: In R's purpose, the VALVE lurch spreading in stereo, VALVE Howl on a tight tank.

## Waiting on the owner (`docs/TASKS.md`)
- §1 the clock-hold check on the module.
- Design questions; friend feedback.

## Will bite
- **Flash:** profile 97 % (3.5 KB left). Anything new that's compiled in needs a size check; `make -C firmware all-variants` fails over 128 KB. Renderer-only voicings must stay compiled out (`RV_FIXED_VOICINGS`).
- **CPU:** echo mode 75.7 %, the lap wrap 78.1 % against the 80 % ceiling. Desktop estimates read low; only chip runs count.
- **Fresh worktrees** lack `test_audio/stimulus/*` (gitignored) and the JUCE/libDaisy submodule contents: copy stimuli from main, symlink `libs/JUCE` temporarily (restore the empty folder before `git add`), `LIBDAISY_DIR=` main's. Otherwise `test_output_bits` fails falsely (memory `fresh-worktree-stimuli`).
- **Listening pages go in main's `renders/`** by absolute path in every agent brief (memory `listening-pages-in-main-renders`).
- **SPEC version numbers collide** when two branches both add a changelog line: renumber at merge (happened with v1.0.40 → 41).
- **ctest:** ~10 min now; run it alone in the background with a long timeout and read the "tests passed" line from a log.
- **Merges into main** may be blocked by the permission classifier unless the owner's OK is explicit in the conversation: ask first (memory `merge-needs-owner-ok`).
- **`tools/ingest_references.py` selftest overwrites `docs/reference-report.md`** with fake data (existing bug): restore the file after running it.
- **Hardware facts not verifiable in code** (input clip ~16 Vpp, gate ~2 V, In L→R normalling, CV 0–5 V) come from SPEC §1.
- GitHub pushes sometimes drop on this network: retry.

## Where to look
`docs/TASKS.md` · `CONTEXT.md` (new panel names ↔ code names) · `docs/adr/0041-echo-mode.md` (amendments: tape wear, springs blend, clock holds) · `docs/adr/0044-panel-names.md` · `firmware/README.md` (runs 20–21) · `docs/m8-tuning-backlog.md` (session 8 summary at the end)
