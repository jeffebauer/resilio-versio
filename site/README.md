# Resilio Versio minisite

The public website for Resilio Versio, built with [Astro](https://astro.build) as plain static pages. Design decisions: `docs/minisite/design/` (DESIGN-v2 = revision 2, on top of DESIGN and PLAN; tokens). Copy and assets: `docs/minisite/content/` and `docs/minisite/assets/`.

## One source of truth

The site has **no copy of its own text, audio or design tokens**. At build time it reads them from the package next door:

| What | Read from | How |
|---|---|---|
| Page copy (Home, Manual, Install, Presets, Changelog, FAQ, Credits) | `docs/minisite/content/*.md` | Astro content collection (`src/content.config.ts`) |
| Design tokens and base CSS | `docs/minisite/design/tokens.css` | imported unchanged by `src/layouts/Base.astro` |
| Demo clips + captions, settings, transcripts | `docs/minisite/assets/audio/` | `scripts/prebuild.mjs` copies the MP3s to `public/audio/` (git-ignored); `src/lib/demos.ts` reads `demos.json` and computes waveform peaks |
| Panel map (linework, in the manual) | `docs/minisite/assets/panel/resilio-versio-panel.svg` | inlined by `src/components/PanelDiagram.astro` |
| Renders (stills) | `docs/minisite/assets/renders/*.jpg` | imported by `src/lib/renders.ts`; Astro serves AVIF/WebP at several widths |
| Hero film (optional) | `docs/minisite/assets/renders/hero_reveal.{mp4,webm}`, `hero_hold_loop.{mp4,webm}`, `hero_still.jpg` | copied to `public/renders/` by the prebuild; `HeroMedia.astro` plays the reveal once, then loops the hold. Without them the hero shows the still |
| Panel art | `docs/minisite/assets/panel/resilio-versio-panel-art.svg` | copied to `public/panel/` (git-ignored) |

So: **edit the words in `docs/minisite/content/`, never in `site/`.** Markers in the Markdown (`<!-- PANEL -->`, `<!-- DOWNLOADS -->`, `<!-- LATEST -->`) become components; `<!-- OWNER: … -->` notes are stripped and never published.

## Run it locally

Needs Node 22 or newer.

```bash
cd site
npm install
npm run dev        # http://localhost:4321, reloads as you edit
npm run build      # the real thing, into site/dist/
npm run preview    # serve site/dist/ to check a build
npm run check      # type and template check (astro check)
```

`/style` is the style tile (revision 2): type, palette with contrast ratios, header, buttons, tiles, pictograms, tiny diagrams, the annotated panel and the audio tile on one page (not in the sitemap, `noindex`).

If you change a Markdown plugin in `src/plugins/` and the output doesn't change, clear Astro's cache: `rm -rf node_modules/.astro`.

## Fonts (Phonic is licensed: never commit it)

The repository is going public, so the Phonic WOFF2 files must never be in git. `site/public/fonts/` is git-ignored. Before each build, `scripts/prebuild.mjs` fetches them:

- `FONTS_DIR=/path/to/folder` copies every `.woff2` from that folder (good for local work), or
- `FONTS_URL=https://…/` + `FONTS_TOKEN=…` downloads the twelve files below with `Authorization: Bearer <token>` (for Vercel, from private storage), or
- neither: skips. The site still builds and looks right on Inter (and doesn't request the missing files).

The twelve file names (`src/lib/fonts.ts`): `Phonic-Light`, `-LightItalic`, `-Regular`, `-RegularItalic`, `-Medium`, `-MediumItalic`, `-Bold`, `-BoldItalic`, and `PhonicMonospaced-Light`, `-Regular`, `-Medium`, `-Bold`, each `.woff2`.

How they're used: the site writes one `@font-face` per file that is present. Phonic leads the text stack once its four upright weights (Light, Regular, Medium, Bold) are all there; Phonic Mono once its four cuts are. Italics are added when present. Anything missing is simply not declared, so the browser never asks for it, and Inter (300/400/500/700 + italics, from `@fontsource/inter`) takes over.

Locally: `FONTS_DIR=~/Fonts/Phonic npm run dev`.

## Vercel settings

| Setting | Value |
|---|---|
| Framework preset | Astro |
| Root Directory | `site` |
| Include files outside the Root Directory | **on** (the build reads `../docs/minisite`). It sits next to the Root Directory setting; check it's enabled |
| Build command | `npm run build` (default) |
| Output directory | `dist` (default) |
| Install command | `npm install` (default) |
| Node.js version | 22.x or newer |
| Web Analytics | enable in the project's Analytics tab (cookie-free; the script is only added on Vercel builds) |

Environment variables (all optional):

| Name | What |
|---|---|
| `FONTS_URL`, `FONTS_TOKEN` | where to fetch the Phonic files (above) |
| `GITHUB_TOKEN` | read-only token, so the build-time release lookup isn't rate-limited |
| `DOWNLOADS_LIVE=1` | turns the download buttons on. Set it only once the repo is public: GitHub's download links 404 while it's private |
| `SITE_URL` | the public address, once there's a domain (otherwise Vercel's production URL is used) |

## Releases and downloads

`src/lib/release.ts` asks GitHub for the latest release at build time. If that fails (today: the repo is private) or the release lacks the stable asset names, it uses `src/data/release.json`; update that file by hand per release until the API works. Download buttons always link to the stable names:

- `…/releases/latest/download/resilio-versio-firmware.bin`
- `…/releases/latest/download/resilio-versio-plugin-macos.zip`

While `DOWNLOADS_LIVE` is off (`src/data/site.ts`), the buttons show "coming soon".

## Where things are

```
astro.config.mjs        static output, sitemap, Markdown plugins
scripts/prebuild.mjs    copies audio, panel art, renders (+ hero film), fetches fonts
src/content.config.ts   the content collection over docs/minisite/content
src/layouts/            Base (head, header bar, footer), Doc (contents + centred prose)
src/pages/              index (Home), manual, install, presets, changelog, faq, credits, style
src/components/         HeroMedia, AnnotatedPanel, FeatureTiles, AudioAB (audio tile), DemoList,
                        DownloadBlock, Pictogram, Button, Chip, MediaCard, Placeholder, SpecTable,
                        IndexNav (doc contents), PanelDiagram, ScrubSequence, DocContent, Footer, Logo
src/components/icons/   the pictograms (one .astro per glyph, 24-unit grid)
src/components/diagrams/ the tiny diagrams (shapes.ts draws them, Diagram.astro renders)
src/scripts/motion.ts   GSAP reveals and the hero push-in, Home only
src/scripts/player.ts   the Web Audio A/B player
src/lib/                content splitting, demos + peaks, release, contrast, fonts, renders
src/plugins/            remark (slot markers, owner notes) and rehype (headings, tables)
src/styles/site.css     layout and components on top of tokens.css
src/data/               release.json (fallback), site.ts (links, go-public switch), controls.ts (callouts, feature tiles)
public/                 favicon, og/default.png (placeholder share image)
```

## Motion and accessibility rules (from DESIGN.md)

- Content is visible without JavaScript. Home's script adds `html.js-motion` first, and only then hides anything it's about to reveal.
- `prefers-reduced-motion: reduce` turns the choreography off entirely, and the hero shows the still instead of the film.
- Doc pages have no scroll animation.
- The player: real buttons, a keyboard slider for the waveform (arrows, Page Up/Down, Home/End, Space), one clip at a time, never autoplays.
