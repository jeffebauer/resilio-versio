# Handoff

**Written:** 5 Oct 2026, end of session 7 (3–5 Oct). Start the next session with `/resilio-start`. **Three agents were still running when this was written** (see "In flight"): check their worktrees first.

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1, M4–M7 | Built |
| M2 plugin | Built; owner's M2 Ableton check still pending (TASKS §5) |
| M3 CPU | **Run 18** on the chip (bit-identical savings, merged): S1 64.4 % avg / 70.9 % max, S2 66.0 / 72.0, **echo mode 70.2 / 76.2**, SWITCH S3>S1 77.3, lap wrap 79.9. Budget changed this session: **≤ 75 % peak target, 80 % ceiling** with a click check (ADR 0030 amendment). Owner accepted echo mode's 76 % |
| M8 tuning | This session merged: TONE after the springs (ADR 0036 amendment), Throw + Hold (ADR 0039/0040), echo mode as SPRINGS 3 (ADR 0041) + echo tuning (held DECAY top, no KICKED runaway), µ-law on DRIVEN 12-bit / KICKED 10-bit (ADR 0042), knob end stops, flash study, CPU runs 16 and 18. **In flight:** Wellspring round 5 (owner picked B), µ-law moved to wet-only pre-TONE, Kick removal + button throw/tap |
| M9 polish | Manual, starting points and share read-me stale (refresh after the in-flight work). Friends' release notes drafted: `releases/whats-new-since-1-oct.md` (need updating for the Kick removal and µ-law placement) |

- `main` HEAD: this handoff's commit (code last changed at `fa54cb6`, the LED fix).
- **Gates:** last full ctest on `main` at `82e5753` (run 18 merge): **100 % of 25** incl. plugin_host_test. Since then only `fa54cb6` (firmware-only LED fix) + docs. Firmware at `fa54cb6`: **release 127,228 B (97 %), profile 128,744 B (98 %, 2.3 KB left), m0test 82,320 B (62 %)**.
- **Plugin in Ableton:** `62575ff` (main's sound before run 18; run 18 is bit-identical, so the sound is current). AU validated.
- **On the Versio:** release `843c5fc` (the owner's experiment build) or `fa54cb6` if they flashed the LED fix. `dist/resilio_versio_release_fa54cb6.bin` is the current release. No click check yet on a build with everything (TASKS §2).

## In flight (agents started in session 7; they report only to that session)
If an agent's session is gone, its work is on disk in its worktree: check `git -C <worktree> status` and `git log main..<branch>`, then start a fresh agent with the same brief (essentials below) to finish from where it stopped.

1. **Wellspring round 5 fix-up**: branch `fit/round5`, worktree `.claude/worktrees/agent-af17afe929a3bbac6`. Owner picked **B (tank voicing 8) everywhere** (incl. held tone/pad at WOBBLE 0.45, where B's held notes swell most: accepted). Done: merged main (`9ae38c8`). Remaining when last seen: make voicing 8 the default; fix the suites that fail with 8 (KICKED loudness spread vs TONE 3.6 dB > 3 dB bar and across ATTITUDE switches, a SPLASH-vs-DRIVE step in KICKED, the µ-law box's no-new-pitch check in KICKED, the Hold ducking rising into kicks +1.9 dB > +0.5 bar: fix in the SOUND, it's the owner's "no hump" rule; Sustain trim/LED preconditions); flash trim to profile ≤ ~128,000 B (levers in `docs/prototypes/flash-study/README.md`: LED DMA registers ~950 B release only, RV_SIZE_OPT on set-up only, drop voicings 9/10 and Renderer-only tables from firmware); ADR 0038 Round 5 → Accepted, SPEC v1.0.35. Verify: full ctest incl. plugin, sizes, M6 0 flags, voicing 7 bit-for-bit as reference.
2. **µ-law moved to the wet, before TONE: DONE, waiting for the owner's listen + merge.** Branch `fix/mulaw-wet-pretone`, commit `af9a162` (ctest 100 % of 25 incl. plugin; CLEAN and box-off renders 81/81 bit-identical to main; release 127,484 B, profile 128,944 B (+200); box full scale raised to +8 dB (kFullScale 2.5) so the hot pre-limiter wet doesn't clip inside the 24 kHz stage). Page `renders/mulaw_wet/index.html`. Merge it (its ADR 0042 amendment and SPEC v1.0.36 are on the branch), then the round 5 agent/branch merges main again. Original brief: Owner: KICKED aliasing too present on dry + wet; move the box to the wet only, before TONE's return filter, so TONE can thin it; DRIVEN too. Expected: MIX 0 a clean passthrough again in every ATTITUDE (revert ADR 0042's CLEAN-only test adaptations), ADR 0042 amendment, SPEC v1.0.36, page `renders/mulaw_wet/` (A main, B wet pre-TONE). Same gates.
3. **Kick removed; button = manual throw + tap tempo**: branch `feat/button-throw-tap` (from `fedd412`), worktree `.claude/worktrees/agent-ae6e3e351f04c14d1`. Owner decisions: no Kick anywhere (module + plugin, no MIDI Kicks, Clatter removed; the Splash's Jolt/Clang/Bite stay); button in SPRINGS 1–2 = manual throw (held = send open; the first press latches throw mode); in echo mode = tap tempo (shares the gate clock's code); **leave throw mode = double-tap then hold 2 s** (single taps/holds of any length stay throws); gate unchanged (throw in 1–2, clock in 3). No CV-level throw: the Versio's gate jack is a digital pin, no level, no jack detection. Plugin: KICK button → throw/tap; **don't rename or remove any ParamSpec key** (repurpose). New ADR (supersedes 0005/0013/0016, amends 0039/0041), SPEC v1.0.37. Frees ~3–4 KB of flash.
   - **Merge order: round 5 → µ-law move → Kick removal**, one at a time, full ctest after each. For heavy conflicts, have the branch's agent merge main into its branch (memory `owning-agent-merges-main`). Owner OK: round 5 (picked B) and the other two (owner-requested): merge when green; still show the µ-law page.
- **Kept on purpose:** locked worktrees `agent-a5c8…` (proto/splash-round4), `agent-adce…` (proto/diffuse-tank); `.claude/worktrees/share` (make_release.sh reuses it); `nifty-shtern-b943cb` (owner archiving it). Reference branches (unmerged, no worktree): `proto/tone-place`, `proto/echo-springs`, `proto/hiss` (renamed from `worktree-agent-a294…`), `proto/low-tail`, `smooth-arc`, `sweet-tank`, `tension`, `tight-ringing`, `wellspring-fit`, `wobble-hang`. Merged branches still exist as refs (feat/*, perf/*, tune/*, fix/pot-endstops, proto/flash-study): deletable.

## Next steps (in order)
1. **Collect the three agents' results**, verify each, merge in the order above, full ctest + firmware sizes after each. Flash must stay ≤ ~128 KB profile with margin.
2. **Owner listens** to `renders/mulaw_wet/`. Then **install the plugin** (owner closes Ableton; `tools/install_plugin.sh <HEAD>`; `auval -v aumf RsVs Rslo` only with Live closed) and **build a release + run 19 profile** (`make -C firmware all-variants`, copy `firmware/build/resilio_versio.bin` and `..._profile.bin` to `dist/`). Run 19 on the chip and the **click check** (TASKS §2: include flipping SPRINGS 3 ↔ 1/2 and the new button).
3. **Update the friends' release notes** `releases/whats-new-since-1-oct.md`: the Kick removal (it still describes the Kick and "hold KICK 1 s"), the µ-law placement (wet only, before TONE; dry clean), the button (throw/tap, double-tap-hold exit), round 5. After the owner has tested module + plugin: refresh `docs/manual.md`, `docs/presets.md`, `releases/README.md`, then `tools/make_release.sh --notes releases/whats-new-since-1-oct.md --publish`.
4. Carry the owner's open questions (TASKS §4 / Design questions): echo mode feel (0.4 s resting time, DECAY range, the ~10 s settle of the held top, swoop speed), the CLEAN/DRIVEN echo top leaning on the limiter with DRIVE > ¾, what In R is for, the KICKED lurch, the tight-tank Howl.

## Waiting on the owner (`docs/TASKS.md`)
- The click check on a build with everything (§2), after the merges.
- The µ-law wet page (§3d), echo-mode feel (§4), the M2 Ableton check and panel look (§5), design questions.
- Testing the new build on module + plugin before the friends' release.

## Will bite
- **Flash:** profile 98 % (2.3 KB) at `fa54cb6`. Round 5 adds ~2 KB; the Kick removal frees ~3–4 KB. Run `make -C firmware all-variants` after each merge (it fails over 128 KB). Never put the knob-move redesign or per-sample paths under -Os.
- **CPU:** echo mode 76.2 % peak, the SPRINGS switch 77–80 % (run 18). The µ-law box costs ~6–7 points on the chip (desktop estimates ran ~2× low: trust only chip runs). Next levers (owner: "not yet"): a fast approximation of the µ-law's logf (not bit-exact), block 96.
- **Disk:** ~24 GB free; agents filled it once (memory `agent-disk-budget`). Give every agent a ~2 GB budget.
- **ctest in the background:** chained runs hit the 30-min background limit; run ctest alone with a 3,600,000 ms timeout and read the log's "tests passed" line.
- **zsh doesn't split `$args`:** in shell loops use `${=args}` (a bit-exactness comparison once silently compared missing files).
- **Hardware facts learned:** the pots stop short (MIX read ~0.98 → dry leak; fixed by `firmware/PotEndStops.h`); a pot reads ~0.47 at physical noon; the gate jack is digital only; knob + CV sum in hardware (0–5 V on top of the pot, clamped).
- **Red input LEDs** on hot material are real input peaks (a 0 dBFS shaker loop): not a bug.
- **Plugin installs:** Ableton closed first (`pgrep -f 'MacOS/Live'`), then `tools/install_plugin.sh`, then `auval -v aumf RsVs Rslo`. Never `auval -a`.
- GitHub pushes sometimes drop on this network: retry.

## Where to look
`docs/TASKS.md` · `docs/research/dub-lens-critique.md` (§8: every owner decision this session) · `firmware/README.md` (runs 15–18) · ADRs 0039–0042 (+ amendments to 0030, 0036, 0038, 0041, 0042) · `docs/m8-tuning-backlog.md` ("Wellspring session 2: findings for round 5", "Wellspring round 5") · `releases/whats-new-since-1-oct.md`
