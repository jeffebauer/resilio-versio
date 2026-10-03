# Handoff

**Written:** 3 Oct 2026, end of session 6 (2–3 Oct). Start the next session with `/resilio-start`. The owner is doing **CPU run 15 and the Versio click check tomorrow morning** (4 Oct): expect the serial output pasted in.

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1 Spring + Renderer · M4–M7 | Built |
| M2 plugin | Built; panel interface merged. Owner's M2 Ableton check still pending |
| M3 CPU | Run 13 was the last chip run (63.4 % avg / 66.3 % peak, target 70 %). **Run 15 is built** (`dist/resilio_versio_m3_profile_run15.bin`, from `340b542`) and covers everything since run 13: bipolar WOBBLE, sustain trim, Big Knob, SPLASH C, SPRINGS 3 coupled wire gauges, the Wellspring F tank, and the flash savings. Run 14 was skipped |
| M8 tuning | In progress. Merged this session: SPRINGS 3 coupled (ADR 0037), then **Wellspring F** (ADR 0038: tank voicing 7, fitted to the owner's Wellspring) with the owner's Round F2 picks: low cut D (`f_lowcut_voicing` 2, HP 155 Hz, −2 dB shelf at 300 Hz) and SPRINGS 3 = coupled wire gauges (`springs3_voicing` 13). SPEC v1.0.28 |
| M9 polish | LED meters, panel UI done. Manual / starting points / share read-me were refreshed for `a8c64c7` this session, and are **stale again** for the F sound |

- `main` HEAD: this handoff's commit. Code last changed at `340b542` (the F merge); since then docs only.
- **Gates (on `main`, 3 Oct, at `340b542`):** ctest **100 % of 20** (incl. plugin_host_test and test_tank_voicing), read from the log. Firmware: release **126,280 B (96 %)**, profile **130,496 B (99.6 %, 576 B headroom)**, m0test 82,320 B (62 %).
- **Plugin in Ableton:** `340b542`, version 1.3.29 = `main`'s sound. The only Resilio installed ("Resilio Versio F" removed).
- **On the Versio:** still an old release (`1d18fce` or older). Ready: run 15 (above), then `dist/resilio_versio_release_340b542.bin`.
- **Friends' share:** `v2026.10.01-1d18fce` (old sound, with firmware) and the pre-release `v2026.10.02-cef6a77-candidate-F`. The candidate installs as "Resilio Versio F", plugin only, and is F *before* the owner's D + wire-gauge picks. No release with the merged sound yet.

## In flight
Nothing running. No agents, no uncommitted work. Worktrees removed this session: springs3b, wfit4, wf2 (all merged).
- **Kept on purpose:** locked worktrees `agent-a5c8…` (proto/splash-round4) and `agent-adce…` (proto/diffuse-tank); `.claude/worktrees/share` (make_release.sh reuses it); `nifty-shtern-b943cb` (owner archiving it). Unmerged reference branches: `proto/low-tail`, `smooth-arc`, `sweet-tank`, `tension`, `tight-ringing`, `wellspring-fit`, `wobble-hang`, plus the two above. Merged and deletable when convenient: `proto/springs3-palette*`, `proto/wellspring-fit-3/-4`, `proto/wellspring-f2`.
- **Renders whose picks are done** (deletable, ask the owner first): `springs3_palette2`, `wellspring_fit3`, `wellspring_fit4`, `f2_lowend`, `f2_springs3`. Keep `references`, `ir_library`, `sweep_ir`.

## Next steps (in order)
1. **Run 15 results** (owner pastes the serial output). Compare with run 13 per corner (`firmware/README.md` has the table format). Target ≤ 70 % peak. Watch the knob-move peaks: that's the lead for the owner's red input LEDs. Two firmware changes this session could move CPU:
   - `RV_SIZE_OPT` puts `controlTick` at -Os.
   - `RV_NO_UNSWITCH` removes loop unswitching from `Tank::process`.

   Both are in `core/dsp/SizeOpt.h`. If CPU is over, try those first, but flash headroom is only 576 B on profile.
2. **Click check + play on the module** with `dist/resilio_versio_release_340b542.bin` (TASKS §1). The owner will also report red-LED details.
3. **Refresh docs for the F sound:**
   - `docs/manual.md`: TONE noon is now gentler and warmer, left back to dark; SPRINGS 3 = wire gauges; the tank sounds further back.
   - `docs/presets.md`: re-render the six starting points (`presets/starting_points/*.json`) on `02_hits`, `04_skank` and `10_pad_cminor`, and check peaks and tail lengths against the descriptions.
   - `releases/README.md`.

   Then, once run 15 and the click check pass: `tools/make_release.sh --publish` (a normal release from `main`, firmware included).
4. **Open sound questions** (owner's ear first, then a round): what 3 Springs should be (design question in TASKS), and Wellspring round 5 from the owner's notes. For round 5:
   - **Softer transient:** the echo front below 2 kHz. Rise is 1.0–1.3 ms vs the Wellspring's 9.4; onset above 4 kHz already matches.
   - **Tail resonance** sits in a different place.
   - **Centre vs wide** differs: we keep the bass centred below 150 Hz; the Wellspring's L/R correlation is −0.14.

   Session 2 recordings (TASKS §5) would measure the last two.

## Waiting on the owner (`docs/TASKS.md`)
- **Tomorrow:** CPU run 15 (§0), then the release, click check and play (§1), plus red-LED details.
- **When they have a view:**
  - what 3 Springs should be (design questions);
  - three quick checks on the F sound: SPLASH in KICKED with DRIVE down at ~2 o'clock (−2 dB vs before), the level while sweeping TONE on sharp clicks, and TONE fully left;
  - the M2 Ableton check;
  - the session 2 Wellspring recordings.
- Friends' feedback on candidate F, if it comes.

## Will bite
- **Flash:** profile 99.6 % (576 B). Any code added to the firmware path will overflow it. The levers, all firmware-only:
  - `RV_SIZE_OPT` (set-up and per-tick housekeeping only);
  - one-entry voicing tables (`springs3::voicing()`, the fParts table);
  - Renderer-only voicings behind `RV_FIXED_VOICINGS`;
  - `printf`/`putchar`/`exit` stubs in `firmware/no_uart_spi.cpp` (not in m0test).

  The knob-move redesign (`updateBaseSettings`, `updateSpringSettings`, `Spring::prepareTransition`) and every per-sample path must stay -O3. GCC's `optimize` attribute keeps `-ffp-contract=off`: I checked the disassembly, and the only fused multiply-adds are in libm and libDaisy, as on `main` before.
- **Never rename a ParamSpec key.** Ableton saves a device's parameters by the VST3 ID (a hash of the key). BOING → TENSION left dead slots in the owner's set, fixed by editing the .als (backup "Resilio Versio (before TENSION fix).als"). A comment in `core/params/ParamSpec.h` and memory `ableton-saved-param-ids` record this.
- **Plugin installs:** every install gets its own version (1.<commits/100>.<commits%100>). `tools/install_plugin.sh <ref> <label>` installs a candidate next to the main plugin. Always check Ableton is closed first (`pgrep -f 'MacOS/Live'`), then `auval -v aumf RsVs Rslo` (or the candidate's code, e.g. `RsVF`).
- **Thin test margins on the F sound:**
  - The held-pad limiter reads 2.74 dB against a 3.0 bar (the Sustain trim glides down 0.6× as long on the gentler low-cut steps).
  - The test_drive aliasing check now judges what the loud tone adds over the same tone at −40 dB. The shipped default reads −94.5 dBFS against a −94 floor. That check can't see a product sitting exactly on a fixed floor (backlog).
  - test_tank mono notch: −4.3 dB against the −4.5 margin.
  - test_springs3 now runs in three passes, each voicing on the tank it was made on.
- **TONE level on sharp clicks:** the left half is ~+4 dB vs noon and fully right ~−4.5 dB (drum hits stay within ±3). KICKED splash at SPLASH 0.7 / DRIVE 0 is −2 dB vs the old tank. Both are waiting on the owner's ear.
- **Agents in worktrees:** brief them to `cd` into the worktree first. One was denied an edit by the permission classifier ("Modify Shared Resources") when its working directory had drifted to the main checkout. Never route around a denial: ask the owner.
- Worktrees need `test_audio/stimulus` copied in. Their `libs/*` are empty, so build firmware with `LIBDAISY_DIR=… DAISYSP_DIR=…` pointing at the main checkout. There's no numpy on this Mac: write analysis in plain Python or C++.
- GitHub routing drops on this network: retry pushes.

## Where to look
`docs/TASKS.md` · `firmware/README.md` (run table, sizes) · `docs/adr/0038-wellspring-fit-tank-voicings.md` (Decision + Round F2) and `0037` · `docs/m8-tuning-backlog.md` sections "Wellspring F merge" and "F round 2" · `core/dsp/SizeOpt.h` · `docs/manual.md`
