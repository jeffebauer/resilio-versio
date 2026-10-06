# Resilio Versio minisite: design decisions

**Status:** agreed with the owner, 6 Oct 2026 (session 9). Reference: UDO Audio's DMNO page, audited in [AUDIT.md](AUDIT.md). Tokens and base CSS: [tokens.css](tokens.css). How it gets designed and built: [PLAN.md](PLAN.md). The content comes from the package next door (`../content/`, `../assets/`); `../BRIEF.md` still holds for content, audiences and must-nots, but its "built in another repo" handoff is **superseded**: the site lives in this repo (D8).

## The idea in one line

A precise, light, typographic page on a hairline grid, with warm photo-real renders and soft rounded controls set against it: lab notebook meets studio gear. The tank is the hero, because the sound is the product.

## Decisions

| # | Decision | Owner's pick | Why |
|---|---|---|---|
| D1 | **Typeface: Phonic** (Light 300 for nearly all text, Regular 400 for UI, Medium 500 sparingly) + **Phonic Monospaced** for numbers, labels, chips, spec values | Owner licenses Phonic and supplies webfonts | One family in one light weight is the reference's backbone; Phonic gives us that discipline with our own voice instead of Helvetica Now's. The monospaced cut does what UDO's small grey labels do, and suits a technical instrument (version numbers, "01" indexes, parameter values) |
| D2 | **Palette:** paper #f6f5f3, white, mist #ececec, cream #f0e7db, ink #111, greys tuned to pass AA | Light, warm neutrals | Matches the reference's restraint; secondary greys darkened where UDO's fail AA (`--ink-2`, `--ink-2-cream`) |
| D3 | **Accent: LED red / valve orange.** `--signal` #ee5641 (the red on the owner's panel art) for marks, dots, playheads and LED glows; `--signal-ink` #c83d16 for filled buttons (white text 5.1:1) | LED red / valve orange | The module's panel already uses red for its secondary labels (ECHO, TIME, TAP) and its LEDs; one hue, used about once per screen, like UDO's red. A different red from UDO's #f2472b on purpose |
| D4 | **Imagery:** hero = photo-real **spring tank** render; second key image = **3D Versio module with the owner's panel**, three-quarter view biased to the front panel, showing some of the hardware behind it; plus macro crops of both | Both | The tank is what Resilio models and is entirely ours; the module shot shows what you get. Renders scripted in Blender from the real dimensions (PLAN §A3) |
| D5 | **Light only** | Light only | One art-directed palette; renders lit for it |
| D6 | **Structure:** a long story **Home** with a numbered index, plus quieter **doc pages** (Manual, Install, Presets, Changelog, FAQ) in the same grid | Long home + doc pages | The story sells; the docs get read. Doc pages share the hairline grid and index column, without choreography |
| D7 | **Motion: UDO-level scroll choreography on Home**, built so content is never hidden without it (see "Motion rules") | UDO-level | The owner wants the reference's drama; the rules keep it from leaving phones blank (an audit finding) |
| D8 | **Build: Astro in this repo (`site/`), deployed on Vercel** | Astro, Vercel | Static pages from our Markdown, almost no JavaScript by default; docs stay in sync with the code they describe |
| D9 | **Address: a free `*.vercel.app` URL** for now | vercel.app | A domain can come later without redesign (see blind spot 9) |
| D10 | **Audio: custom player** with a **dry ↔ wet A/B** that switches in sync | Custom A/B | Hearing the reverb against the dry hit is the whole pitch; reuses our listening pages' synced switching |
| D11 | **Analytics: Vercel Web Analytics** (cookie-free) | Vercel | Page views and download clicks with no consent banner |

## Rules that make the style ours

**Type.** Everything Light unless it's UI. No bold. Hierarchy by size, grey vs ink, and position. Display sizes tight (line height 1.0, −1.5 % tracking). Reading text 1.35. Paragraphs after the first indent 2em (from the reference) on long pages. Mono for every number a player might compare: versions, dates, parameter values, specs, the section index.

**Grid and linework.** 12 columns (4 on phones), small fluid margins (16–44 px). Columns 1–3 are the **index column**: section labels ("02 The sound"), captions, small notes. Content starts at column 4. Every section opens with a full-bleed 1 px rule; tables are hairline rows; one vertical hairline may join a statement to the thing below it. Lines are structure only: never boxes around text, never decorative.

**Softness.** Rounded only where your hand or eye lands: buttons (10 px), pills (nav, chips, A/B switch), image cards (≈ 0.8vw, clamped). Everything structural stays square. That contrast *is* the look.

**Colour.** The accent appears at most once per viewport as a fill (the download button), plus small signal marks (a dot in a chip, the playhead, an LED in a render). Cream is reserved for the reference material (spec, manual), as UDO does for its tech section. Dark (`--panel`) cards echo the module's black panel: use them for the panel and the audio demos.

**Imagery.** Renders lit like a product shot: big soft key light top-left, warm-grey ground, shallow depth on macros. Never a busy background. Crops alternate: whole object → angled macro → detail. Captions sit above images in the index style ("Fig. 02 — The tank, rear transducer").

**A cousin, not a clone.** No UDO images, copy, wordmark, icons or layouts lifted one to one; our hero is a tank, not a product on a gradient sky; our type is Phonic; our accent is a different red used differently; our structure follows our content (sound → panel → install). Reference screenshots stay out of the repo.

## Motion rules (choreography without the blank screen)

1. **Visible by default.** Elements are opted into animation by a script that runs first (`html.js-motion`); without JS, with slow JS, or with "reduce motion" on, everything is simply there.
2. **Animate transform and opacity only**, 60 fps, nothing that shifts layout.
3. **Choreography on Home only**: hero tank slow push-in and light sweep; the intro paragraph reveals line by line; the module turns a few degrees as you scroll past (an image sequence or a short video scrubbed by scroll, not a WebGL scene); the feature stack pins and reveals line by line, each line's superscript note fading after it; the panel diagram draws its linework; demos fade in as cards.
4. **Doc pages: no scroll animation**, only the pill nav and hover states.
5. **Reduced motion:** cross-fades only, and the scrubbed module shows a single still.
6. Library: GSAP ScrollTrigger (free since 2024, ~45 KB) loaded only on Home, or CSS scroll-driven animations where supported with the static fallback.

## Blind spots, with suggestions

| # | Blind spot | Why it matters | Suggestion |
|---|---|---|---|
| 1 | **Paid fonts in a public repo** | The repo is going public (ADR 0045). Committing Phonic's webfont files would redistribute them, which webfont licences forbid | Keep `site/public/fonts/` git-ignored; store the WOFF2 files privately (a private Vercel Blob or a private repo) and pull them at build time with a token in Vercel's env. Read the Phonic web licence for page-view tiers and self-hosting terms |
| 2 | **Noise Engineering's hardware in our hero** | The 3D module is NE's product design; the reference renders work because UDO owns the product | Keep the disclaimer near the module image; no NE logo on the render; the tank as the hero (already decided) keeps NE out of the first impression. Consider a courtesy note to NE before launch |
| 3 | **Accessibility of the "light" look** | Light 300 at small sizes on cream, grey-on-paper, and red buttons are where the reference fails AA | Tokens already fix contrast (AUDIT §3); keep body text ≥ 17 px; never Light below 15 px (use Regular); visible focus rings; A/B player fully keyboard- and screen-reader-operable; captions/transcripts for demos |
| 4 | **Scroll choreography vs speed and phones** | UDO's page is ~15,000 px tall with heavy images; on phones content stayed blank until scripts ran | Motion rules above; a performance budget (PLAN §B8): LCP < 2.5 s on 4G, Home ≤ 1.5 MB before scroll; AVIF/WebP at several widths; scrubbed sequences ≤ 60 frames and lazy |
| 5 | **The plugin is unsigned on macOS** | Public visitors will meet Gatekeeper's "can't be opened" warning; many will stop there | Short term: a clear, friendly install step with a screenshot. Medium term: an Apple Developer account ($99/yr) to sign and notarise, which removes the step entirely. Say "Mac only" plainly; Windows and Linux users will ask |
| 6 | **Downloads before the repo is public** | `releases/latest/download/...` links 404 while the repo is private | Launch order in PLAN §B10: flip the repo → publish the first public release with the stable names → deploy the site |
| 7 | **The site going stale after a release** | Versions, changelog and download sizes are on the site | Build-time fetch from GitHub's API (with a token, to avoid rate limits) + a Vercel deploy hook that `make_release.sh` calls after `--publish` |
| 8 | **Docs drifting from the code** | The content package is a copy; the manual changes with the firmware | Point Astro's content collections at the source docs (`docs/manual.md`, `docs/presets.md`) or regenerate the package on release; one source of truth |
| 9 | **The vercel.app name is temporary but sticky** | Links shared on forums and in the release notes live forever | Decide a domain before the first public post, or keep links pointing at the GitHub repo/README, which can redirect later |
| 10 | **Nothing to do after reading** | Visitors with no Versio can't try it on hardware | Lead with the plugin download for desktop users and the demos; for module owners, link NE's firmware-swap instructions and the exact file |
| 11 | **Where the community is** | A minisite alone gets little traffic | Plan announcement posts (ModWiggler, r/modular, NE's Discord or forum) with the hero render and a demo; give a press-kit section (renders, short description) |
| 12 | **Legal pages** | AGPL source link, trademarks and a privacy note (analytics) need a home | A small footer: licence links (source at the release tag), the NE disclaimer and trademark lines, "cookie-free analytics, no personal data" |
| 13 | **Demos not yet heard** | The ten clips were checked by numbers only | Owner listens and approves before any design depends on them (dry/wet pairs need re-rendering anyway, PLAN §B6) |
| 14 | **Open Graph / sharing image** | Links on social and forums show whatever the crawler finds | A dedicated 1200 × 630 render crop with the wordmark, per page title |
