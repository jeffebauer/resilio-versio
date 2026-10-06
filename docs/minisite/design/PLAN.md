# Resilio Versio minisite: design plan and build plan

Decisions: [DESIGN.md](DESIGN.md). Tokens: [tokens.css](tokens.css). Content: `../content/`. Each step names what the owner reviews before the next one starts.

# A. Design plan

## A1. Style tile (first review)
One page showing the system before any layout: Phonic at every size in the scale, the mono labels, the palette with contrast ratios, hairline rules on paper / cream / ink, buttons, pills, chips, the nav pill over an image, an image card with caption, a spec table row, the A/B player. Built as a real HTML page from `tokens.css` (not a mock), so it doubles as the component test page.
**Owner reviews:** type sizes and weights in Phonic (the scale was tuned on UDO's Helvetica Now; Phonic's widths will differ), the two accent tones, how soft the rounding feels.

## A2. Page wireframes (second review)
Low-fidelity layouts on the 12-column grid for Home (desktop + phone) and one doc page (Manual), with placeholder grey boxes for renders. Content is the real text from `../content/`.
**Owner reviews:** the order of the story, what the index column carries, where the one accent goes on each screen.

## A3. Renders (runs alongside A1–A2)
Blender 4.x (not installed yet: free from blender.org), scenes scripted in Python so they're repeatable and versioned in `site/renders/`:

| Shot | Use | Spec |
|---|---|---|
| **R1 Tank hero** | Home hero, OG image | Open spring tank (Accutronics-style long tank: two/three springs, transducers, steel chassis, RCA jacks), off-white or bare steel, three-quarter top view, big soft key top-left, warm-grey ground, 1 strong light streak along the springs. 2880 × 1620 + a 1200 × 630 crop |
| **R2 Tank macros** | Section bands | Spring coils catching light; the transducer end. Shallow depth of field |
| **R3 Module 3/4** | "The module" section | The Versio (10 HP, 128.5 mm, NE's published panel dimensions from `docs/panel/versio_panel_template.svg`) with the **owner's panel** as the faceplate texture; three-quarter view biased to the front, showing the PCB, pots' bodies and power header behind the panel; knobs and jacks modelled to the hole sizes; LEDs lit in `--signal` |
| **R4 Module turn** | Scroll-scrubbed sequence | R3's camera orbiting ~20°, 48 frames, AVIF |
| **R5 Panel macro** | Manual header | Knobs + the TANK/ATTITUDE toggles, shallow depth |

Inputs from the owner: the panel art, now in `docs/minisite/assets/panel/resilio-versio-panel-art.svg` (190.87 × 485.67 pt = 50.5 × 128.5 mm, text outlined; it also draws the knobs and jacks, which the 3D scene replaces with models, so the texture uses only the flat print layers), exported from Figma (file `RtGyUlerVn3LjGYbCOLj0D`, frame `75:1292`, `resilio-versio_panel`, black panel, white labels, red secondary labels) exported as **SVG** with guide layers hidden. Claude reads the layer positions (pots, jacks, LEDs, switches, button, mounting holes) through the Figma connection to place the 3D parts.
**Owner reviews:** clay renders (no materials) for composition first, then lit renders.

## A4. High-fidelity pages (third review)
Home and Manual built in code from A1–A3 (no separate Figma comps unless the owner wants them), shown on a Vercel preview URL at desktop and phone widths, with the choreography on.
**Owner reviews:** feel of the motion, the renders in place, the A/B player with real audio.

# B. Build plan

## B1. Project shape
```
site/                      Astro project (Vercel root directory)
  astro.config.mjs         static output, sitemap, Vercel adapter (static)
  src/styles/tokens.css    from docs/minisite/design/tokens.css
  src/layouts/Base.astro   head, OG tags, header (logo + nav pill + Download), footer
  src/layouts/Doc.astro    index column + prose column, section rules, no choreography
  src/pages/index.astro    Home (story)
  src/pages/manual.astro, install.astro, presets.astro, changelog.astro, faq.astro
  src/content/             content collections (B4)
  src/components/          Rule, SectionHead, IndexNav, Button, Chip, MediaCard, SpecTable,
                           FeatureStack, PanelDiagram, AudioAB, DownloadBlock, ScrubSequence
  src/scripts/motion.ts    GSAP ScrollTrigger choreography, Home only
  public/fonts/            git-ignored; fetched at build (B9)
  public/audio/, public/renders/
  renders/                 Blender scene scripts (A3), not deployed
```

## B2. Home, section by section (how the style is applied)

| # | Section | Layout | Elements | Motion |
|---|---|---|---|---|
| 00 | **Header** | Wordmark left, nav pill centre (Sound, Manual, Install, Presets, Changelog), dark pill "Download" right | `.nav-pill`, `.btn-dark` | Pill blurs over the hero |
| 01 | **Hero** | Full-bleed R1 tank render; below it, one row: "Resilio Versio" (display) + "Dub spring reverb for the Noise Engineering Versio." (display, `.quiet`) left; `Download` primary button right | `.display`, `.btn-primary` (the screen's one accent) | Slow push-in on the tank, light sweep along a spring |
| 02 | **Introduction** | Index column: the numbered index (01 Introduction … 06 Download) in mono numbers; main column: the lede paragraph | `.lede`, `.num` | Lines reveal in sequence |
| 03 | **The sound** | Dark (`.on-ink`) band; demo cards in a 2- or 3-up grid, each an A/B player with a caption chip of its settings in panel names | `AudioAB`, `.chip` with a signal dot | Cards fade up |
| 04 | **The panel** | R3 module render left (scrubbed R4 turn), the panel diagram in linework right; every control listed with a hairline row: name + one line of what it does | `ScrubSequence`, `PanelDiagram` (SVG, 1 px strokes), `.spec` rows | Module turns as you scroll; diagram lines draw in |
| 05 | **Features** | Centred feature stack, one line each with a small mono note: TWO SPRINGS (note: "TANK 1 · 2"), TAPE ECHO ("TAP TEMPO · CLOCK"), THREE ATTITUDES ("CLEAN · TAPE · VALVE"), THROW AND HOLD, SPLASH, WOBBLE, … | `.feature-stack` with mono superscripts | Pinned; lines reveal one by one |
| 06 | **Two ways to play** | Two columns divided by a vertical hairline: "On the module" (firmware, link to Install) / "In your DAW" (AU/VST3 plugin, macOS) | `.v-rule`, `.btn-ghost` | — |
| 07 | **Download** | Cream (`.on-cream`) band: version and date (mono), the two downloads as primary buttons with file sizes, checksums link, "what's new" (latest changelog entry), licence line | `DownloadBlock`, `.spec` | — |
| 08 | **Footer** | Hairline-ruled: links to docs, GitHub, Issues; credit "Jesse Bauer → jessebauer.xyz"; NE disclaimer and trademark lines; analytics note | `.small`, `.quiet` | — |

## B3. Doc pages
`Doc.astro`: sticky index column (that page's headings, numbered in mono) + prose column (max 38em) on cream for Manual and Presets, paper for Install, Changelog, FAQ. Tables are `.spec`. Each control in the Manual gets an anchor so the Home panel rows can link straight to it. Install has two tabs styled as a pill switch: Module / Plugin. Changelog is a list of releases: version (mono) + date + notes, newest first.

## B4. Content, one source of truth
Astro content collections read Markdown with front-matter. Phase 1: copy `docs/minisite/content/*.md`. Phase 2 (before launch): point the Manual and Presets collections at `docs/manual.md` and `docs/presets.md` directly (with a small remark plugin for any repo-only bits), so a firmware change and its doc change ship together.

## B5. Releases and downloads
At build time, `src/lib/release.ts` fetches `GET /repos/jeffebauer/resilio-versio/releases/latest` (token `GITHUB_TOKEN` in Vercel env, to avoid the 60/hour limit) for the version, date, notes and asset sizes; links use the stable names (`releases/latest/download/resilio-versio-plugin-macos.zip`, `…/resilio-versio-firmware.bin`). Fallback when the fetch fails: the last known values committed in `site/src/data/release.json`. `tools/make_release.sh --publish` calls a Vercel **deploy hook** (URL in a local env var, never committed) so the site rebuilds with each release.

## B6. The A/B player
- Needs **dry and wet versions of each demo, sample-aligned**: re-render the ten demos as pairs (`*_dry.m4a`, `*_wet.m4a`) from the same stimulus with BLEND 0 and the demo's BLEND; loudness-match the pair so the switch judges sound, not level (stating so in the caption).
- Web Audio: decode both on first play, run them through two gain nodes from one start time, and crossfade 30 ms on toggle. One clip plays at a time.
- Waveform: precomputed peak arrays at build (a tiny Node script), drawn as a 1 px SVG polyline; the playhead in `--signal`.
- Accessibility: real `<button>`s with `aria-pressed`, space/enter, arrow keys to seek, a text description per clip.
- Formats: AAC `.m4a` 192 kbps (plays everywhere incl. iOS); lazy-loaded on first interaction; iOS needs the first play to start from a tap.

## B7. Choreography
`motion.ts` loads on Home only, after `astro:page-load`, adds `html.js-motion`, then registers ScrollTrigger timelines per section (B2). Rules from DESIGN "Motion rules". `ScrubSequence` draws AVIF frames to a `<canvas>` (preloaded progressively) with a single still as `<img>` fallback and for reduced motion.

## B8. Performance and quality gates (in CI on Vercel previews)
- Lighthouse (mobile): Performance ≥ 90, Accessibility 100, Best Practices ≥ 95, SEO 100.
- LCP < 2.5 s on throttled 4G; CLS < 0.05; Home transfer ≤ 1.5 MB before scrolling (hero AVIF ≤ 250 KB at phone width).
- Images via `astro:assets` at 640/1024/1440/2048/2880 widths, AVIF + WebP.
- Fonts: 4 WOFF2 files, subset to Latin, Light and Mono preloaded.
- axe-core run on every page; keyboard pass on the player and nav.

## B9. Fonts in a public repo
`.gitignore`: `site/public/fonts/`. The WOFF2 files live in private storage (Vercel Blob, private) and a `prebuild` script downloads them with `FONTS_TOKEN`. Local development: copy them in by hand. Check the Phonic web licence for page-view tiers and self-hosting.

## B10. Launch order
1. Owner approves the demos (listen) and the renders.
2. Build on Vercel previews (password-protected deploys optional).
3. **Flip the repo public** (owner's "flip it", ADR 0045).
4. Publish the first public release with the stable asset names (`make_release.sh --publish`), which triggers the deploy hook.
5. Check every download link and checksum on the live site; run the B8 gates.
6. Share: forum posts with R1 and one demo; link from jessebauer.xyz.

## B11. Order of work (milestones)
| Step | Work | Owner's part |
|---|---|---|
| S1 | Merge the content package + this design folder; install Blender | OK the merge; install Blender; send the Phonic WOFF2 files and licence terms |
| S2 | Style tile (A1) on a Vercel preview | Review type, colour, softness |
| S3 | Clay renders R1, R3 (A3) | Pick angles |
| S4 | Wireframes (A2) | Review the story order |
| S5 | Astro scaffold, tokens, components, doc pages (B1, B3, B4) | — |
| S6 | Home with renders, dry/wet demos, player, choreography (B2, B6, B7) | Review on preview, listen to the demos |
| S7 | Release feed, deploy hook, gates (B5, B8) | — |
| S8 | Launch (B10) | "Flip it" |
