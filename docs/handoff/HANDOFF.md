# Handoff

**Written:** 7 Oct 2026, end of session 9 (6–7 Oct). Start the next session with `/resilio-start`. **Nothing running:** no agents in flight. Most of this session was the **minisite** (a public site for Resilio, in `site/`, on branches, not on `main`).

## State

| Milestone | State |
|---|---|
| M0 hardware | Passed |
| M1, M4–M7 | Built |
| M2 plugin | Passed. Installed `c1d98ac`; **not** yet the first-chord fix or the new plugin panel layout |
| M3 CPU | Run 21 on the chip (`bdd3910`). **The first-chord fix (merged since) hasn't been timed on the chip**: run a CPU test before the next release |
| M8 tuning | First chord after drums fixed and merged (`1e02c96`, SPEC v1.0.45, ADR 0035 amendment; owner picked B everywhere). VALVE lurch spread listened: no audible difference, today's kept (backlog "Session 9") |
| M9 polish | Going-public prep merged (ADR 0045, SPEC v1.0.44): licences (MIT code, AGPLv3 plugin binaries), NOTICE, README credit/disclaimer, issue forms, stable release asset names, plugin panel layout fix. **Repo still private**. Minisite on branches (below) |

- **`main` HEAD:** this handoff's commit (code last changed at `1e02c96`; since then `.gitignore` and the panel art only).
- **Gates:** full ctest **100 % of 24** at `1e02c96` (no code changes since). Firmware at `1e02c96`: **release 127,700 B (97 %), profile 128,856 B (98 %, ~2.2 KB left), m0test 82,320 B (62 %)**.
- **Plugin in Ableton:** `c1d98ac`. On the Versio: `bdd3910`. Release `a6c70a4` built and published privately (owner's clock-hold check pending, TASKS §1).

## The minisite (where it lives)

- **Branches:** `feat/site` (first pass, PR #1 → `main`, draft) ← `feat/site-v2` (**revision 2**, PR #2 → `feat/site`, draft, HEAD `5f58135`) ← render scripts on `feat/site-renders` (Blender, `site/renders/*.py`, pushed, not merged).
- **Design docs:** `docs/minisite/design/DESIGN.md` (rev 1), **`DESIGN-v2.md`** (rev 2 + the owner's amendments at the end), `AUDIT.md` (UDO audit), `PLAN.md`, `tokens.css`. Prompts used for cloud sessions: `docs/minisite/CLOUD-BUILD-PROMPT.md`, `CLOUD-BUILD-PROMPT-v2.md`.
- **Vercel:** project `resilio-versio` (team `jeffebauers-projects`), Root Directory `site`, files outside root included, Analytics on, previews protected. Production = `main` (no `site/` there yet, so production builds fail harmlessly and nothing is public). Branch preview: `https://resilio-versio-git-feat-site-v2-jeffebauers-projects.vercel.app`. Shareable no-login links via the Vercel MCP `get_access_to_vercel_url` (23 h) or the dashboard Share button (no expiry).
- **Fonts:** Phonic is licensed, **never committed**. All 12 cuts as WOFF2 in `dist/webfonts/phonic/` (git-ignored) and in the review worktree's `site/public/fonts/`. Vercel builds fall back to Inter (`@fontsource/inter`) until the files are uploaded to private storage with `FONTS_URL` + `FONTS_TOKEN` (prebuild fetches them).
- **Renders:** finals in `renders/minisite_final/` (git-ignored): stills, `hero_reveal.*`, `hero_hold_loop.*`, `hero_still.png`. Web copies the site uses are committed in `docs/minisite/assets/renders/` (on the site branches).
- **Local review:** worktree `.claude/worktrees/site-v2-review` (branch `feat/site-v2`), dev server config `site-v2` in `.claude/launch.json` (port 4322; also `site-review` → `.claude/worktrees/site-review`, port 4321). `.claude/launch.json` is modified locally on purpose (dev-server configs pointing at worktrees); not committed.

## In flight
Nothing running. Kept on purpose:
- Worktrees: `site-review` (`feat/site`), `site-v2-review` (`feat/site-v2`, has the Phonic files in its ignored `public/fonts`), `agent-ae4c72fa…` (`feat/site-renders`, the Blender scripts), `agent-aa1f0b12…` (`proto/stereo-in-study`, the study doc), plus the old `agent-a5c8…`, `agent-adce…`, `share`, `nifty-shtern-b943cb`.
- Unmerged reference branches: `proto/valve-lurch-stereo` (judged, kept for reference) and the older `proto/*` (kept public-safe per ADR 0045).

## Next steps
1. **Owner reviews revision 2** (TASKS §2) on localhost:4322 or the preview; apply changes on `feat/site-v2`; when OK'd, merge PR #2 into `feat/site`, then merge `feat/site-renders` into `feat/site` (scripts only).
2. **Fonts to Vercel:** walk the owner through private storage (Vercel Blob, private) for the 12 WOFF2 files; set `FONTS_URL`/`FONTS_TOKEN` in the project env (Preview + Production).
3. **Demo clips:** the ten "The sound" clips have never been heard by anyone; owner listens and approves (TASKS §2). Dry/wet pairs would enable the A/B switch (PLAN §B6).
4. **First-chord fix to the owner:** build the CPU-test firmware (`make -C firmware all-variants`) for a chip run; install the plugin with `tools/install_plugin.sh <commit>` when Ableton is closed (brings the new panel layout too); update TASKS' "Plugin installed" line.
5. **Going public** (owner's "flip it" only): flip visibility → first public release with stable asset names → set `DOWNLOADS_LIVE=1` on Vercel → merge the site to `main` (production). Draft public notes: `docs/release-notes-public.md`.
6. Stereo in: owner's answers (TASKS §3), then a plugin-only prototype in TANK 2 (study's recommendation).

## Waiting on the owner (`docs/TASKS.md`)
- §1 clock-hold check on the module. §2 revision-2 review, demo clips, fonts upload. §3 stereo-in questions. §4 "flip it".

## Will bite
- **Fonts must never enter git** (public repo soon). `site/public/fonts/` is ignored in `site/.gitignore`; check `git status` before every commit in a site worktree.
- **Flash:** CPU-test firmware at 98 % (~2.2 KB left). Anything compiled in needs a size check.
- **CPU:** echo mode 75.7 %, lap wrap 78.1 % (run 21), before the first-chord fix. Re-time on the chip.
- **The browser pane:** hidden pane = videos don't advance and `await` on `play()` hangs (use synchronous checks). Desktop-width screenshots come out tiny; send 375-px or 1024-px shots to the owner (memory `owner-reviews-on-phone`).
- **Astro dev** converts images on first request: the first screenshot after a restart can show empty frames.
- **Vercel preview URL** for any branch: `resilio-versio-git-<branch>-jeffebauers-projects.vercel.app`. Previews need a Vercel login; production on `main` is public once the site merges.
- **Blender** (5.2.2, `/Applications/Blender.app`): renders ~13–20 s/frame at 1080p; the hero took ~66 min. Print sharpness at grazing angles needs linear texture filtering + 1.0 px filter + guided denoise (in `site/renders/common.py`).
- **References:** take principles, never signature layouts (memory `references-principles-not-layouts`).
- GitHub pushes sometimes drop on this network: retry.

## Where to look
`docs/TASKS.md` · `docs/minisite/design/DESIGN-v2.md` · PR #2 description (`gh pr view 2`) · `site/README.md` · `docs/adr/0045-going-public.md` · `docs/research/stereo-input-study.md` (branch `proto/stereo-in-study`)
