# Handoff

**Written:** 7 Oct 2026, end of session 10. Start the next session with `/resilio-start`. **Nothing running:** no agents in flight. The session was almost all **minisite** work: merged and live, then ~15 owner review rounds (Mesurer notes), the King Tubby desk renders, and a new “Big Knob” history section. No DSP, firmware or plugin code changed.

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1, M4–M7 | Built |
| M2 plugin | Passed. **Installed `1e02c96`** (7 Oct 14:53, AU validated): the first-chord fix and the plugin panel layout fix |
| M3 CPU | **Run 22** (`1e02c96`, the first-chord fix, 7 Oct): echo mode 69.1 / 76.3 %, switch 76.2 %, worst moment (lap wrap) **78.8 %** (run 21: 78.1). Under the 80 % ceiling, 1.2 points of headroom |
| M8 tuning | First-chord fix merged (SPEC v1.0.45). Nothing new this session |
| M9 polish | **Public (7 Oct):** repo public; first public release **v2026.10.07-0671d22** (Latest, stable asset names, licence files); site live with working downloads (`DOWNLOADS_LIVE` defaults on; `=0` in the environment turns it off). Friends' releases marked pre-public |

- **`main` HEAD:** this handoff's commit. Code (core/firmware/plugin/host) last changed at `1e02c96`.
- **Gates:** full ctest **100 % of 24** at wrap (7 Oct, `main` with code at `1e02c96`; 1,691 s). CPU run 22 on the chip (below).
- **Firmware** (built at wrap, no firmware change): release **127,700 B (97 %)**, profile **128,856 B (98 %, ~2.2 KB left)**, m0test 82,320 B (62 %).
- **Plugin in Ableton:** `1e02c96` = the last code commit. **On the Versio:** `1e02c96` (`dist/resilio_versio_release_1e02c96.bin`): the owner's click check and clock-hold check passed. **Latest release:** v2026.10.07-0671d22 (same code).

## The minisite (how it works now)

- **Live** from `main` on Vercel project `resilio-versio` (team `jeffebauers-projects`, Root Directory `site`). Every push to `main` deploys production; branch pushes make protected previews (`resilio-versio-git-<branch>-jeffebauers-projects.vercel.app`).
- **Working copy:** worktree `.claude/worktrees/site-refine` (branch `feat/site-refine`, fully merged into `main`). Dev server config `site-refine` in `.claude/launch.json` (port 4323; the file is local-only on purpose, never committed). Sync it with `git merge --ff-only main` before new work.
- **Owner's review loop:** Mesurer (mesurer.dev) notes pasted into chat; fix on `feat/site-refine`, verify in the browser at 1448 × 1030, push; merge to `main` only on "merge it" (memory `site-review-mesurer`). Mesurer runs in `astro dev` only (an integration in `site/astro.config.mjs` injects `src/scripts/mesurer-dev.ts`); built pages contain no Mesurer or React (checked).
- **Fonts:** MD UI (text) + MD IO (mono), variable WOFF2s, licensed, **never in git**. Vercel fetches them at build from a private Blob store (env `FONTS_URL` = `https://<store>.private.blob.vercel-storage.com/fonts/`; the prebuild falls back to the build's `VERCEL_OIDC_TOKEN`, newer stores inject no read-write token). Local copies: the worktree's ignored `site/public/fonts/`; originals in the owner's Dropbox (`…/Mass-Driver/MD_IO-V2-FutureFonts/Fonts/Variable/`).
- **Design decisions this session:** `docs/minisite/design/DESIGN-v2.md` → "Amendments, session 10" (warm-neutral greys, pill buttons, casing rule, “Big Knob” in quotes, doc column 42rem, hero film mechanics…).
- **Link preview:** `site/public/og/resilio-hero.jpg` (the hero still).
- **“Big Knob” section** (home, after The sound): copy in `docs/minisite/content/overview.md` (facts only from `docs/research/big-knob.md`), images `docs/minisite/assets/renders/bigknob-area.jpg`, `bigknob-topdown.jpg`, `desk-front.jpg`.
- **Desk renders:** scenes merged in `site/renders/mci_desk.py` + `render_bigknob.py` (Blender 5.2.2: `/Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup --python site/renders/render_bigknob.py`). Seven stills in `renders/minisite_final/bigknob/` (git-ignored). The MoPOP and Altec reference photos were only ever in a session scratchpad: **never commit them** (museum / listing photos).

## In flight
Nothing running. Kept on purpose:
- Worktree `site-refine` (above). `share`, `nifty-shtern-b943cb` (detached, older), and the agent worktrees `agent-a5c8…` (`proto/splash-round4`), `agent-aa1f…` (`proto/stereo-in-study`, the study doc), `agent-adce…` (`proto/diffuse-tank`).
- Merged branches still present locally/remote: `feat/site`, `feat/site-v2`, `feat/site-renders`, `feat/site-refine`, `worktree-agent-af8f70117b76a1b09` (all in `main`). **PR #2** (`feat/site-v2` → `feat/site`) is still open on GitHub though its work is in `main` (owner task to close).
- Unmerged reference branches: `proto/*` (kept public-safe per ADR 0045).

## Next steps
1. **Owner's open site checks** (TASKS §2): the hero film on iPhone (white edges, the reveal→loop handover, resume after scrolling away: none of these could be watched here), the “Big Knob” copy, the demo clips.
2. **Next release** with the first-chord fix (CPU timed: run 22): `tools/make_release.sh`, then the owner's click check and the clock-hold check (TASKS §1).
3. **Done this session:** gone public (release, visibility, site downloads). Next releases: `tools/make_release.sh --notes <file> --publish`, then update `site/src/data/release.json` (fallback) and add a dated entry to `docs/minisite/content/changelog.md`; the site reads the latest release from GitHub at build, so a redeploy (any push to `main`) picks it up.
4. **Stereo in:** owner's answers (TASKS §3), then a plugin-only prototype in TANK 2.
5. Housekeeping when convenient: delete the merged site branches (local + remote) once PR #2 is closed.

## Waiting on the owner (`docs/TASKS.md`)
- §2 iPhone hero check, “Big Knob” copy read, demo clips, close PR #2. §3 stereo-in questions.

## Will bite
- **Everything is public now:** the repo (history included), issues, and the site; every push to `main` deploys the live site. Never commit fonts, reference recordings or IRs, museum/listing photos. Site work goes on `feat/site-refine` and merges only on "merge it".
- **The browser pane:** when hidden, video won't autoplay (the page falls back to the still) and screenshots fail; time also stops. Ask the owner to show it (Cmd+Shift+B) to verify video. Chrome also pauses muted autoplay offscreen (handled: the hero resumes via an IntersectionObserver).
- **Astro dev:** new files in `docs/minisite/assets/renders/` need a dev-server restart (eager image glob); the first request after a restart shows images still converting. `demos.json` is cached in memory: restart after editing it. Scoped `<style>` doesn't reach `set:html` content: use `:global(...)`.
- **Fonts never enter git** (`site/public/fonts/` is ignored); check `git status` before site commits.
- **The 200 vs 250 Hz step:** an original 9069-B photo reads 200 where our research (from clones/press) says 250 (`docs/research/big-knob.md`); the site avoids listing steps.
- **Flash:** CPU-test firmware at 98 % (~2.2 KB). **CPU:** worst moment 78.8 % (run 22, the lap wrap): only 1.2 points under the 80 % ceiling, so any new per-sample DSP needs a chip run before it ships.
- **Disk:** ~20 GB free (96 % full). Blender renders and render grids eat it; delete judged pages.
- GitHub pushes sometimes drop on this network: retry.

## Where to look
`docs/TASKS.md` · `docs/minisite/design/DESIGN-v2.md` (session-10 amendments at the end) · `site/README.md` (fonts, Mesurer, Vercel) · `docs/research/big-knob.md` · `site/src/pages/index.astro` + `site/src/styles/site.css`
