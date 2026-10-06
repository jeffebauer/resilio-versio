# Resilio Versio minisite

The public website for Resilio Versio, built with [Astro](https://astro.build) as plain static pages. Design decisions: `docs/minisite/design/` (DESIGN, PLAN, tokens). Copy and assets: `docs/minisite/content/` and `docs/minisite/assets/`.

## One source of truth

The site has **no copy of its own text, audio or design tokens**. At build time it reads them from the package next door:

| What | Read from | How |
|---|---|---|
| Page copy (Home, Manual, Install, Presets, Changelog, FAQ, Credits) | `docs/minisite/content/*.md` | Astro content collection (`src/content.config.ts`) |
| Design tokens and base CSS | `docs/minisite/design/tokens.css` | imported unchanged by `src/layouts/Base.astro` |
| Demo clips + captions, settings, transcripts | `docs/minisite/assets/audio/` | `scripts/prebuild.mjs` copies the MP3s to `public/audio/` (git-ignored); `src/lib/demos.ts` reads `demos.json` and computes waveform peaks |
| Panel map (linework) | `docs/minisite/assets/panel/resilio-versio-panel.svg` | inlined by `src/components/PanelDiagram.astro` |
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

`/style` is the style tile: every type size, colour, rule, button, pill, card and the player on one page (not in the sitemap, `noindex`).

If you change a Markdown plugin in `src/plugins/` and the output doesn't change, clear Astro's cache: `rm -rf node_modules/.astro`.

## Fonts (Phonic is licensed: never commit it)

The repository is going public, so the Phonic WOFF2 files must never be in git. `site/public/fonts/` is git-ignored. Before each build, `scripts/prebuild.mjs` fetches them:

- `FONTS_DIR=/path/to/folder` copies every `.woff2` from that folder (good for local work), or
- `FONTS_URL=https://…/` + `FONTS_TOKEN=…` downloads the four files below with `Authorization: Bearer <token>` (for Vercel, from private storage), or
- neither: skips. The site still builds and looks right on the fallback stack in `tokens.css` (and doesn't request the missing files).

File names the CSS expects: `Phonic-Light.woff2`, `Phonic-Regular.woff2`, `Phonic-Medium.woff2`, `PhonicMonospaced-Regular.woff2`.

Locally: `FONTS_DIR=~/Fonts/Phonic npm run dev`.

While the files are absent, the build prints four Vite warnings ("/fonts/Phonic-….woff2 didn't resolve at build time"). They are expected and go away once the fonts are in.

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
scripts/prebuild.mjs    copies audio + panel art, fetches fonts
src/content.config.ts   the content collection over docs/minisite/content
src/layouts/            Base (head, header, footer), Doc (index column + prose)
src/pages/              index (Home), manual, install, presets, changelog, faq, credits, style
src/components/         Rule, SectionHead, IndexNav, Button, Chip, MediaCard, Placeholder, SpecTable,
                        FeatureStack, PanelDiagram, AudioAB, DemoList, DownloadBlock, ScrubSequence,
                        DocContent, Footer
src/scripts/motion.ts   GSAP ScrollTrigger choreography, Home only
src/scripts/player.ts   the Web Audio A/B player
src/lib/                content splitting, demos + peaks, release, contrast, fonts
src/plugins/            remark (slot markers, owner notes) and rehype (headings, tables)
src/styles/site.css     layout and components on top of tokens.css
src/data/               release.json (fallback), site.ts (links, go-public switch), controls.ts
public/                 favicon, og/default.png (placeholder share image)
```

## Motion and accessibility rules (from DESIGN.md)

- Content is visible without JavaScript. Home's script adds `html.js-motion` first, and only then hides anything it's about to reveal.
- `prefers-reduced-motion: reduce` turns the choreography off entirely.
- Doc pages have no scroll animation.
- The player: real buttons, a keyboard slider for the waveform (arrows, Page Up/Down, Home/End, Space), one clip at a time, never autoplays.
