# Prompt for a cloud session: minisite redesign (revision 2)

Paste everything below the line into a Claude Code cloud session on `jeffebauer/resilio-versio`.

---

Rebuild the Resilio Versio minisite's design to **revision 2**: a hybrid of several references, so it no longer reads as a copy of UDO Audio's DMNO page. Work on a new branch **`feat/site-v2`** created from **`feat/site`** (not from `main`). Open a **draft PR into `feat/site`** when done. Don't merge, and don't touch `main`.

## Read first (in this order)
1. `CLAUDE.md`. In this session only these rules apply:
   - commit with explicit paths (never `git commit -a`);
   - stay out of `core/`, `firmware/`, `plugin/` and `host/`;
   - never commit fonts, reference recordings or `renders/`.
2. **`docs/minisite/design/DESIGN-v2.md`**: the brief for this job. It covers the references and what we take from each, the list of **retired** elements, decisions V1–V11, the revised home-page order and the new tokens. Follow it closely.
3. `docs/minisite/design/DESIGN.md` and `PLAN.md`: revision 1. They still hold where revision 2 doesn't override them, especially accessibility, motion rules, performance budgets, fonts and the release plan.
4. `site/README.md` and the current `site/` code. It's a working Astro site; refactor it rather than starting over.
5. Content: `docs/minisite/content/*.md`, `docs/minisite/assets/` (audio, renders, logo, panel art, screenshots).

## What to build

**Palette and tokens.** Update `docs/minisite/design/tokens.css` to revision 2's tokens:
- white body, cool greys and tiles;
- a near-black dark band for the hero and the sound section;
- Phonic **Bold** for display (`--wt-display: 700`), larger, tighter display sizes, a tile radius;
- remove `--cream` and its uses.

Check every text/background pair for WCAG AA and list the ratios in the PR.

**Fonts.** All 12 Phonic cuts now exist as WOFF2 (outside the repo; **never commit them**):
- `Phonic-Light`, `Phonic-LightItalic`, `Phonic-Regular`, `Phonic-RegularItalic`, `Phonic-Medium`, `Phonic-MediumItalic`, `Phonic-Bold`, `Phonic-BoldItalic`;
- `PhonicMonospaced-Light`, `-Regular`, `-Medium`, `-Bold`.

Then:
- Declare `@font-face` for all 12 (weights 300/400/500/700, italics, mono).
- Extend `site/src/lib/fonts.ts` and `site/scripts/prebuild.mjs` so each file is used when present. Keep the rule that the browser never asks for a missing file.
- Extend the Inter fallback (`@fontsource/inter`) to 300/400/500/700, plus italics where used.
- In this session the Phonic files aren't available, so build and review on Inter. The owner sees Phonic locally.

**Header (V4).** Replace the blurred pill nav with a flat three-column bar on a hairline:
- the logo in column 1 (keep `Logo.astro`, both the full and the short "Resilio" version, the short one on phones);
- small text links across columns 2–3;
- Download at the right, as a small rounded-rectangle button;
- transparent with light text over the dark bands (keep the existing over-dark detection);
- a simple menu on phones.

**Sections (V5) and layout (V6).**
- Remove the numbered left-rail index and the numbered section labels on Home. Each section opens with a big Phonic Bold heading.
- Prose sits in a centred column of about 38–42em. Bento grids, the annotated panel and renders break out wide.
- Doc pages (Manual, Install, Presets, Changelog, FAQ, Credits) keep a plain table of contents, with no numbers, restyled to match.

**Home, in this order** (DESIGN-v2 "Home page, revised order"):
1. **Hero:** keep the current hero frame and image (`/renders/hero-monolith.jpg`) on the dark band. Below it, "Resilio Versio" in Phonic Bold, the tagline in Light, and Listen + Download.
   - Build it ready for the reveal video: if `docs/minisite/assets/renders/hero_reveal.mp4` (+ `.webm`, + `hero_hold_loop.mp4/.webm`, + `hero_still.jpg`) exist, the prebuild copies them. The hero then plays the reveal once, muted, inline, then loops the hold.
   - The still is the poster and is what reduced-motion users see. With no video files, the hero shows the still. The files may not exist yet.
2. **Intro** (white): one bold statement plus a short paragraph, then the tank close-up full width.
3. **The sound** (dark): a bold heading, a two-sentence intro, then **bento audio tiles** (V11). Each tile has a bold title, a tiny diagram of what it shows, a compact player (keep `player.ts`'s Web Audio, keyboard support and accessibility) and settings behind a toggle. Tiles in one row must not shift when one's settings open.
4. **The panel** (white): a bold heading, then the **annotated panel** (V7), using the front-on render `/renders/front-on-wide.jpg` (or a tighter crop of it):
   - callout boxes with thin leader lines to each control (name plus one line from `src/data/controls.ts`);
   - the callouts placed by percentage coordinates over the image, so they scale;
   - on phones, a numbered list under the image instead of overlapping callouts;
   - accessible as a list.
   - Get control positions from the panel geometry in `docs/panel/versio_panel_coordinates.csv` / `site/src/components/PanelDiagram.astro`, mapped onto the render's panel area. Adjust by eye against the image.
   - The full controls table moves to the Manual (it's already there).
5. **Features** (white): **bento tiles** (V9), replacing the feature stack.
   - Each tile has one big Phonic Bold statement or number and a **pictogram** (V8) or **tiny diagram** (V10). Examples: "2 springs", "3 attitudes", "7 CV inputs", "0 dead zones", "the throw", "tape echo", "the Big Knob", "wobble".
   - Use mixed tile sizes, rounded corners in `--tile` grey, and content from `src/data/controls.ts` / the overview copy.
6. **Two ways to play** (white): the patched module (`/renders/patched.jpg`) wide, then two tiles, "On the module" and "In your DAW", each with its button.
7. **Download** (light grey band): keep `DownloadBlock` (the firmware button gets the red fill; "coming soon" while `DOWNLOADS_LIVE` is off).
8. **Footer** on a hairline. Keep the disclaimer, licence, credits and Issues link.

**Pictograms (V8):** our own set as inline SVG components in `site/src/components/icons/`:
- bold, solid black glyphs on a shared 24-unit grid, geometric, with consistent stroke and fill weight;
- roughly: springs (a coil), splash (an impact burst), tape echo (two reels), wobble (a pitch wave), throw (an arrow into a tank), attitude/valve (a tube or saturation curve), tone (a filter slope), CV (a jack with a cable), decay (an envelope);
- `currentColor`, with titles for accessibility;
- not copied from Fors or anyone else.

**Tiny diagrams (V10):** small line SVGs (1.5 px strokes, `currentColor`, the signal red for one highlight):
- the spring's decay envelope;
- a throw (the send gate opens on one hit, the tail rings on);
- echo repeats fading;
- the Big Knob filter curve (low cut sweeping);
- WOBBLE's pitch drift.

Keep them simple and true to the manual's descriptions.

**Retire, and make sure nothing UDO-specific is left:**
- the blurred pill nav;
- the numbered rail index;
- the upper-case feature stack with superscripts;
- the right-biased rail layout;
- off-white and cream;
- the all-Light display.

Keep the rounded-rectangle buttons (one shape for all controls).

**Style tile:** update `/style` to show revision 2: the type, palette, header, tiles, pictograms, diagrams, the annotated-panel callout and the audio tile.

## Quality gates (run them; report the numbers in the PR)
- `npm run build` and `npx astro check` clean (no errors or warnings).
- Playwright screenshots of `/`, `/style`, `/manual` at 1440 × 900, 1024 × 700 and 375 × 812, attached to the PR as images (not committed).
  - Check them yourself: no horizontal scroll at 320 and 375; nothing blank without JS; callouts don't overlap on phones; the header is readable over the dark hero and the sound band.
- axe-core: no violations on every page.
- Lighthouse (mobile): Accessibility 100, Performance ≥ 90.
- Keyboard pass: header, menu, audio tiles (play, seek, settings), annotated panel.
- Reduced motion: no scroll animation, and the hero shows the still.

## Don't
- Don't touch anything outside `site/` and `docs/minisite/` (including `docs/minisite/design/tokens.css`), except `.gitignore` if needed.
- Don't add font files, UDO, Teenage Engineering, Fors, Mier or Bull assets, or any third-party icons.
- Don't create Vercel projects, domains, env vars or secrets, and don't make the repo public.
- Don't change the copy's meaning. Shortening lines for tiles is fine; keep the source sentences in `docs/minisite/content/` unchanged unless they're wrong, and say so.

## Report (in the PR description)
- Plain words first, for a designer who's new to web build tooling: what changed and why it no longer reads as UDO.
- Then: screenshots, gate results, contrast ratios, anything in DESIGN-v2 you couldn't follow, and questions for the owner.
- The Vercel preview for `feat/site-v2` builds automatically. Mention its branch URL pattern so the owner can find it.
