# Handoff

**Written:** 2 Oct 2026, end of session 5 (1–2 Oct). Start the next session with `/resilio-start`. The owner's weekly usage reset during this session: local agents are fine again (cloud sessions launched by the owner at claude.ai/code also work: see memory `remote-agent-ran-locally`).

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1 Spring + Renderer · M4–M7 | Built |
| M2 plugin | Built; panel interface (knobs, toggles, KICK, LED meters) merged. Owner's M2 Ableton check still pending |
| M3 CPU | Run 13 was the last chip run (63.4 % avg / 66.3 % peak, target 70 %). **Run 14 is ready and not yet run**: everything since run 13 (bipolar WOBBLE, sustain trim + limiter hold, Big Knob, SPLASH C, engine built at boot, USB-host stub) is unmeasured on the chip |
| M8 tuning | In progress. Merged this session: SPLASH/DRIVE (ADR 0032/0033), bipolar WOBBLE voicing D (0034), sustain trim gentle + limiter hold (0035), Big Knob TONE v5 (0036), SPLASH stronger C (0032 amendment), `rv_render --set` labels fix. Prototypes waiting on the owner: SPRINGS 3 round 2 (0037 Proposed), Wellspring fit round 4 (0038 Proposed) |
| M9 polish | LED meters done (module and plugin); share releases via `tools/make_release.sh`; manual/presets need refreshing for the new sound |

- `main` HEAD: this handoff's commit (code last changed at `a8c64c7`, the SPLASH C merge; since then tools and docs only). SPEC v1.0.25.
- **Gates (wrap run on `main`, 2 Oct):** ctest **100 % of 18** (incl. plugin_host_test), read from the log. Firmware: release 120,504 B (91 %), m0test 82,320 B (62 %), profile 128,556 B (98 %, warning; ~2.5 KB left).
- **Plugin in Ableton:** `a8c64c7` (installed 2 Oct 12:32: Big Knob v5 + SPLASH C + everything before). = `main`'s sound.
- **On the Versio:** probably `1d18fce` or older. Ready: `dist/resilio_versio_m3_profile_run14.bin` (run 14, rebuilt with SPLASH C) and `dist/resilio_versio_release_a8c64c7.bin` (same sound as the plugin). Both are the first hardware runs of the Tank built at boot (placement new, `firmware/main.cpp`) and the USB-host stub (`firmware/no_uart_spi.cpp`): if the module misbehaves on boot or USB, suspect those first.
- **Friends' share:** GitHub Release `v2026.10.01-1d18fce` (private repo) is the older sound; `tools/make_release.sh --publish` makes a new one (universal plugin + firmware + read-me from `releases/README.md`).

## In flight (read before touching anything)
1. **SPRINGS 3 round 2** (`proto/springs3-palette-2`, pushed; worktree `.claude/worktrees/springs3b`). Six position-3 voicings at today's repeat timing (`springs3_voicing` 5–10: pan brighter, pan chirp, mixed gauges, coupled, diffuse, cross-fed wide). Page `renders/springs3_palette2/index.html` (labels verified). ctest 18/18 at default. **Waiting for the owner's pick.** On a pick: make it the default (`core/params/Springs3Voicing.h`), merge `main` in (Big Knob / SPLASH / tank voicings touch Tank.cpp), full gates, flash (round 1 plumbing adds ~0.4 KB), CPU run if 8 or 10 (new per-sample path). Round 1 (`proto/springs3-palette`) stays as reference.
2. **Wellspring fit round 4** (`proto/wellspring-fit-4`, pushed; worktree `.claude/worktrees/wfit4`; builds on `proto/wellspring-fit-3`). `tank_voicing` 0–7: 1 Sweep, 2 stereo together, 3 faster diffusion, 4 gentler (round 3), 5 = 3 + transducers (fitted to the sweep IR), 6 = 5 + wide again, 7 = 6 + gentler low cut with makeup. Page `renders/wellspring_fit4/compare/index.html` (A = the Wellspring). Default 0: ctest 18/18, renders bit-identical to `main`. **Waiting for the owner's pick.** If 5–7 is picked: TONE needs re-mapping (its dark half becomes much less dark), ~5 suites need re-tuning (listed in the backlog section "Wellspring fit round 4"), the profile firmware overflows by 1.3–3.1 KB (trim first), CPU run. Open after round 4: echo rise too sharp (1 ms vs 9.4: the Chirp's echo front), first arc ~2× at noon (Sweep fitted at TENSION 0.875), 3rd harmonic −24 vs −36 dB (pickups), lowest octave 3.3 vs 4.7 s (stretching it breaks ADR 0001's DECAY range: ask the owner what DECAY should set).
3. **Kept on purpose:** `proto/wellspring-fit-3` (round 3, base of round 4), `proto/wellspring-fit` (round 1 Sweep), `proto/springs3-palette` (round 1), older prototypes (`proto/tension`, `low-tail`, `smooth-arc`, `sweet-tank`, `diffuse-tank`, `splash-round4`, `tight-ringing`, `wobble-hang`). Locked worktrees `agent-a5c8…` (splash-round4) and `agent-adce…` (diffuse-tank): leave. `nifty-shtern-b943cb`: the owner is archiving it in the app (its only content is a build folder; its commit is in `main`).
4. **Renders kept** (owner purged ~19 GB of decided pages on 2 Oct): `renders/references` (aligned recordings), `ir_library`, `springs3_palette2`, `wellspring_fit3/compare`, `wellspring_fit4`. Disk was full (3 GB); now ~23 GB free. Delete pages once the owner has picked from them.

## Next steps (in order)
1. **Owner listens** (TASKS §3b, §3c): SPRINGS 3 round 2 and Wellspring round 4. Merge each pick as above.
2. **CPU run 14** on the module (TASKS §0), then the release `a8c64c7` + click check; the owner's red-input-LED report (flashes red while moving knobs, intermittent) is waiting on run 14's knob-move peaks: if run 14 shows overruns, that's the lead.
3. **Wellspring session 2** (TASKS §5, `docs/recording-recipe.md` §5b): when the owner records takes H–N, run `python3 tools/ingest_references.py test_audio/reference/` (mapping already added), then `tools/sweep_ir.py` on H / D / I (level series: input-stage level dependence, even-order colour), D-L / D-R (stereo matrix), and the octave bursts J (per-band darkening). Feeds Wellspring round 5.
4. Refresh `docs/manual.md`, `docs/presets.md` and the share read-me for the new sound; republish the share release when the owner wants.

## Waiting on the owner (`docs/TASKS.md`)
- Listens: SPRINGS 3 round 2, Wellspring round 4.
- Hardware: run 14, then the new release + click check; note red-LED details (flicker vs held, click, which knob).
- Plugin: look at the panel interface; the M2 Ableton check.
- Session 2 recordings.
- Design questions list (Kick with SPLASH 0, KICKED stereo lurch, Howl on a tight tank).

## Will bite
- **Verify hidden-voicing labels before handing a page over** (memory `verify-voicing-labels`): a clamp once made "v4" really v3. And sanity-check fit measures against the owner's ear: the settings search matched the wrong echo band for two rounds (fixed in `tools/wellspring_settings_fit.py`: main repeats 200–1000 Hz + onset brightness).
- **Flash:** release 91 %, profile 98 % (~2.5 KB). Any merged sound change can overflow the profile build; `RV_FIXED_VOICINGS` compiles Renderer-only voicings out (TONE, WOBBLE, sustain, SPLASH, SPRINGS 3, tank) — keep that pattern for every new voicing.
- **Tests judged at SPLASH 0 / pinned to voicing 0** since SPLASH C (ADR 0032 amendment, said plainly there): `test_splash` detector checks, `test_tank` level match, `test_drive` wet − dry spread.
- **Big Knob top is 800 Hz** because KICKED / 3 Springs / TENSION 0 rings at 3.1 kHz above ~850 Hz (the LoopSat lets a Loop mode ring when the lows are cut). A tank fix there would allow the researched ~1.2 kHz.
- **Review pages:** use the picking-page format (memory `review-pages-columns`): reference as its own version, one panel per sound, names not params, level-matched. The in-app browser can't open file:// pages: use the `renders` launch config (`.claude/launch.json`, port 8765).
- **Worktrees** need `libs/JUCE` symlinked to build the plugin and lack `test_audio/stimulus` (copy it in) — tests fail with "wav not found" otherwise. Removing a worktree with symlinked libs: guard paths (`${p:?}`) or the safety check blocks it.
- **CPU:** ~3.5 points under 70 % at run 13, with five changes since unmeasured. Coupled/cross-fed SPRINGS 3 and the tank voicings add per-sample work.
- GitHub routing drops on this network: retry pushes.

## Where to look
`docs/TASKS.md` · `docs/m8-tuning-backlog.md` (sections from "SPLASH stronger" to the end, and on the round 4 branch "Wellspring fit round 4") · ADRs 0032–0038 · `tools/wellspring_settings_fit.py`, `tools/wellspring_character.py`, `tools/sweep_ir.py` · `docs/recording-recipe.md` §5b · `firmware/README.md` (sizes, run 14)
