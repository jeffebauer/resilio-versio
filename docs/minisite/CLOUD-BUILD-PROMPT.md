# Prompt for a cloud session: build the Resilio Versio minisite (first pass)

Paste everything below the line into a Claude Code cloud session on `jeffebauer/resilio-versio`.

---

Build the first pass of the **Resilio Versio minisite** in this repo, in a new `site/` folder, on a branch `feat/site`. Open a **draft PR** when done. Don't merge.

## Read first (in this order)
1. `CLAUDE.md`: the project rules. In this session, only these apply: commit with explicit paths (never `git commit -a`), stay out of `core/`, `firmware/`, `plugin/` and `host/`, and never commit reference recordings or `renders/`.
2. `docs/minisite/design/DESIGN.md`: the agreed decisions (D1–D11), the style rules, the motion rules and the blind spots.
3. `docs/minisite/design/PLAN.md`: §B1 project shape, §B2 Home section by section, §B3 doc pages, §B5–B9.
4. `docs/minisite/design/tokens.css`: the tokens and base components. Use them as they are; if you must change a value, say why in the PR.
5. `docs/minisite/design/AUDIT.md`: the style reference, UDO Audio's DMNO page. It's a cousin, not a clone: copy no UDO images, text, logo or layout.
6. `docs/minisite/BRIEF.md`: audiences, tone, must-haves and must-nots. Ignore its "another repo" handoff; the site lives here now.
7. Content: `docs/minisite/content/*.md` (page copy with front-matter), `docs/minisite/assets/audio/*.mp3` + `demos.json`, `docs/minisite/assets/panel/resilio-versio-panel.svg` (the linework diagram) and `resilio-versio-panel-art.svg` (the owner's panel art).

## Build (this pass)
- **Astro** (latest), static output, ready for Vercel with **Root Directory = `site`**. No UI framework; plain Astro components plus a little TypeScript. Add `site/README.md` with the dev, build and Vercel settings.
- **The style tile** at `/style` (PLAN §A1): every type size, the mono labels, the palette with contrast ratios, rules on paper/cream/ink, buttons, pills, chips, the nav pill over an image, an image card, a spec row and the A/B player. It's the owner's first review, so make it beautiful and complete.
- **Layouts and components** from PLAN §B1: `Base`, `Doc`, `Rule`, `SectionHead`, `IndexNav`, `Button`, `Chip`, `MediaCard`, `SpecTable`, `FeatureStack`, `PanelDiagram` (inline the linework SVG, 1 px strokes, able to draw itself in), `AudioAB`, `DownloadBlock`, `ScrubSequence` (still-image fallback for now).
- **Doc pages** (PLAN §B3): Manual, Install, Presets, Changelog, FAQ, from content collections built on `docs/minisite/content/`. Use a symlink or a copy step; either way, don't fork the text. The manual gets an anchor per control.
- **Home** (PLAN §B2, sections 00–08) with the real copy. Render slots use placeholders: a neutral `--mist` card at the final aspect ratio, labelled in mono (e.g. "R1 Tank hero, 16:9, render pending"). The owner's panel art may appear flat in "The panel" section.
- **Motion** (DESIGN "Motion rules", PLAN §B7): GSAP ScrollTrigger on Home only. Content is visible without JS, and `prefers-reduced-motion` turns it off. Don't hide content until a script runs.
- **AudioAB** (PLAN §B6): Web Audio, one clip at a time, an SVG waveform with peaks computed at build, a signal-red playhead, keyboard and screen-reader support. Dry/wet pairs don't exist yet: build the A/B switch so it appears only when a `dry` file is listed in `demos.json`, and use the existing wet MP3s for now.
- **Releases** (PLAN §B5): `src/lib/release.ts` reads GitHub's latest release at build time (optional `GITHUB_TOKEN`). The repo is still private, so fall back to `src/data/release.json` (create it from the latest release notes in `docs/minisite/content/changelog.md`). Download links use the stable names `releases/latest/download/resilio-versio-plugin-macos.zip` and `…/resilio-versio-firmware.bin`.
- **Footer**: the Noise Engineering disclaimer and trademark lines (copy them from `README.md`), licence links, GitHub Issues, credit "Jesse Bauer → https://jessebauer.xyz", and "cookie-free analytics". Add `@vercel/analytics` for Astro.
- **SEO**: a title and description per page, OG tags (a placeholder OG image is fine) and a sitemap.

## Fonts (important)
The site uses **Phonic**, a licensed font. The owner will supply the files; **they must never be committed** (the repo is going public). Add `site/public/fonts/` to `.gitignore` and add a `prebuild` script that copies WOFF2 files from `$FONTS_DIR` when it's set, or downloads them from `$FONTS_URL` with `$FONTS_TOKEN` when those are set, and otherwise skips. With no fonts present, the site must still build and look right on the fallback stack in `tokens.css`.

## Quality gates (run them, report the numbers in the PR)
- `npm run build` and `npx astro check` clean.
- Playwright screenshots of `/`, `/style` and `/manual` at 1440 × 900 and 375 × 812, attached to the PR (not committed). Check them yourself: no horizontal scroll at 375, nothing blank on phones, rules aligned to the grid.
- axe-core: no violations on every page. Lighthouse (mobile): Accessibility 100, Performance ≥ 90 with placeholders.
- Keyboard pass on the nav and the player.

## Don't
- Don't touch anything outside `site/`, `.gitignore` and (if needed) `docs/minisite/` (only to fix a content error, and say so).
- Don't add the font files, any UDO assets, or audio other than what's in `docs/minisite/assets/audio/`.
- Don't create Vercel projects, domains or secrets, and don't make the repo public; the owner does those.

## Report (in the PR description)
What's built, screenshots, gate results, anything in DESIGN or PLAN you couldn't follow and why, and questions for the owner. Write the summary for a designer who's new to web build tooling: plain words first, file paths after.
