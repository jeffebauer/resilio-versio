# Handoff

**Written:** 8 Oct 2026, end of session 11. Start the next session with `/resilio-start`. **Nothing running:** every agent finished and was reviewed. The whole session was **site and README work**: no DSP, firmware or plugin code changed (code last changed at `1e02c96`). The owner shared the project on ModWiggler today.

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1, M4–M7 | Built |
| M2 plugin | Passed. **Installed `1e02c96`** (7 Oct 14:53, AU validated) |
| M3 CPU | Run 22 (`1e02c96`): worst moment (lap wrap) **78.8 %**, 1.2 points under the 80 % ceiling |
| M8 tuning | Nothing new this session |
| M9 polish | **Public and shared.** Release v2026.10.07-0671d22 (Latest). Site live at https://resilio-versio.vercel.app with this session's work (below). Vercel Web Analytics on from 8 Oct |

- **`main` HEAD:** this handoff's commit. `feat/site-refine` is fully merged.
- **Gates (8 Oct, wrap):** full ctest **100 % of 24** (1,695 s), build clean (`ninja: no work to do`), on `main` with code at `1e02c96`.
- **Firmware:** built at wrap, unchanged: release **127,700 B (97 %)**, profile **128,856 B (98 %, ~2.2 KB left)**, m0test 82,320 B (62 %).
- **Plugin in Ableton:** `1e02c96` = the last code commit. **On the Versio:** `1e02c96`.

## What changed this session (all live)

- **README** rewritten as a short summary with the monolith render, linking out to the site.
- **Copy in the owner's voice.** An audit for AI-writing tells (`docs/minisite/copy-audit.md`, sources: Wikipedia's "Signs of AI writing", Pangram), then a full rewrite in first person on every page; facts unchanged (checked file by file). Credits' Licence section and the FAQ's "Is it free?" filled in.
- **Help shape it** (home): community feedback in three steps on a step rail, with Suggest a sound / Report a bug buttons into the GitHub issue forms; the how-to lives in the FAQ; the changelog credits community ideas. The issue chooser's link now points to the site's manual.
- **Dark mode:** follows the system; a System · Light · Dark switch in the footer (`rv-theme` in localStorage, applied before first paint). Tokens in `docs/minisite/design/tokens.css` (`--signal-text`, `--raised`, `--on-render`); DESIGN-v2.md "Dark mode" amendment.
- **Phone layout:** the hero fills the screen in portrait (svh, keeps the h1 in view); the panel map's well fills the column with the panel centred at its old size.
- **Demos round 2** (`docs/minisite/demos-plan.md` status note): ten musical clips written in code from CC0 samples the owner picked on an audition page, each a dry/wet pair. **One floating BLEND fader** (`site/src/components/BlendFader.astro`, after the Intellijel crossfader, dark glass) mixes the playing clip with the module's sqrt BLEND law and shows only while the clips cross the middle of the screen. Audio URLs carry a content fingerprint (`?v=`), so re-renders never play from cache.
- **Knob drawings** (`KnobGlyph.astro`) beside every knob value in demo Settings and on Starting points; clock times dropped from the demo settings.
- **Download block, header, menu:** "Latest release" heading, "You'll need" styling, Firmware Swap linked, spacing fixes.
- **Analytics:** Vercel Web Analytics enabled by the owner; the site's script posts views to a project-specific path (`/cf1b9f2334614c62/view`), not `/_vercel/insights`.

## In flight
Nothing running. Kept on purpose:
- Worktree `site-refine` (branch `feat/site-refine`, merged). Sync with `git merge --ff-only main` before new site work.
- Worktrees `share`, `nifty-shtern-b943cb` (detached, older), and agent worktrees for `proto/splash-round4`, `proto/stereo-in-study` (the study), `proto/diffuse-tank`. Unmerged `proto/*` branches are reference (ADR 0045).
- Merged site branches `feat/site`, `feat/site-v2`, `feat/site-renders` and PR #2: the owner's tidy-up (TASKS §1; this session wasn't allowed to delete branches).
- Local-only: `.claude/launch.json` (configs `renders`, `site-refine` on 4323, `site-dist` on 4324 serving `site/dist`). Never committed.

## Session 12 addendum (8 Oct, short site session)
- **Live** (`ae789f2`): an offbeat hat under Echo chord (`build_sources.py` clip05, drawn with the shared random state restored, so the stabs and the other clips' sources are byte-identical; 10_guitar differs from a fresh full build only because it was last built alone). The tile diagrams draw the dry hit in ink and the reverb in orange (decay, throw, echo), and the Howl drawing is mirrored so it starts loud.
- **Tried and reverted:** dry plus wet in the players' waveforms (overlaid, split, solid with the dry cut out, orange and neutral). The owner kept the single wet trace. Don't re-propose without a new idea; the commits are on `feat/site-refine` (`62f1513`..`805828d`) if one is wanted back.
- **The owner signed off the ten demos** and is away for a few days. Modular recordings come early in the week of 12 Oct and may replace some current demos: build them with the same settings and offer an A/B page. The demo-notes steps below are done.

## Next steps
1. **Owner notes on the demos** (TASKS §1c). Re-render a clip: `python3 docs/minisite/tools/demos/v2/render.py renders/demos_v2/src renders/demos_v2/work renders/demos_v2/audio <id>` with the venv Python that has numpy + soundfile (recreate one if the scratchpad is gone: `python3 -m venv … && pip install numpy soundfile`); sources via `build_sources.py <test_audio/demo_sources> renders/demos_v2/src <id>`; samples via `fetch_samples.py`. ⚠ `render.py` with ids rewrites `work/clips.json` with only those ids: merge entries into `docs/minisite/assets/audio/demos.json` by id, don't overwrite it.
2. **Owner's modular stems** (TASKS §1b) replace the FM clav in clips 04 and 05: add a loader in `build_sources.py`, keep the same settings.
3. **Community feedback:** ModWiggler thread and GitHub issues; credit adopted ideas in the changelog (the FAQ promises "I read every issue").
4. **Stereo in** (TASKS §3) when the owner answers: a plugin-only prototype in TANK 2.
5. **Next release** whenever code changes: `tools/make_release.sh --notes <file> --publish`, then `site/src/data/release.json` and a dated changelog entry.

## Waiting on the owner (`docs/TASKS.md`)
- §1 GitHub tidy-up (close PR #2, delete merged site branches, protect `main`).
- §1b modular stems; optional modular siren. §1c demo notes per clip; ModWiggler replies.
- §2 iPhone hero check, “Big Knob” copy read, the five unheard dub/dub techno starting points. §3 stereo-in questions.

## Will bite
- **Everything is public**, and every push to `main` redeploys the site. Never commit fonts, reference recordings, IRs, museum/listing photos, or **samples from record-sampled packs** (memory `demo-sources-licensing`). Demo sources live in `test_audio/demo_sources/` (ignored).
- **Mesurer swallows input events in `astro dev`**: test sliders and players on a build (`npm run build`, then `site-dist`). Navigating to the same URL with a new hash doesn't reload the page.
- **Clocked echo renders** need the 4 s clock lead-in (already in `render.py`, `PREROLL`), or the first repeats glide from the free time into the clock (heard as a warble).
- **Pitch analysis:** narrow-band or autocorrelation trackers made octave errors (the siren's 1000 Hz note read as 500). Check with a full-spectrum peak list.
- **Copy voice:** first person as Jesse, no colon chains or "not X" pivots (memory `site-copy-voice`, `docs/minisite/copy-audit.md`).
- **CPU:** worst moment 78.8 %; any new per-sample DSP needs a chip run before it ships. **Flash:** CPU-test firmware ~98 %.
- **Disk:** ~33 GB free. GitHub pushes sometimes drop on this network: retry.
- **zsh:** don't name a shell variable `path` (it is the command search path).

## Where to look
`docs/TASKS.md` · `docs/minisite/demos-plan.md` · `docs/minisite/tools/demos/v2/render.py` · `docs/minisite/copy-audit.md` · `site/src/scripts/player.ts` + `site/src/components/BlendFader.astro` · `docs/minisite/design/DESIGN-v2.md` (amendments at the end)
