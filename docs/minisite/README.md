# Resilio Versio minisite package

Everything a website project needs to build the Resilio Versio minisite, without reading the Resilio source repository. Design is the website's job; this package supplies what the site must **say** and **do**, final copy and assets. Checked against the code of release **v2026.10.06-a6c70a4** (6 Oct 2026).

| Path | What it is |
|---|---|
| `HANDOFF-PROMPT.md` | Paste into Claude in the website repo, with this folder copied in |
| `BRIEF.md` | Purpose, audience, pages, hierarchy, tone, must-haves, must-nots, accessibility, downloads and the release update flow |
| `content/*.md` | Final copy, one file per page, with front-matter (`title`, `description`, `slug`, `order`): `overview`, `manual`, `install`, `presets`, `changelog`, `faq`, `credits` |
| `assets/audio/*.mp3` + `demos.json` | Ten demo clips (11–20 s, MP3 160 kbps, loudness-matched ~−20.5 LUFS) with title, caption, transcript, duration and settings in panel names |
| `assets/panel/resilio-versio-panel.svg` | Panel diagram, to scale, panel names only, themeable |
| `assets/screenshots/plugin-panel.png` | The plugin's panel at 2x (506 × 1286), default settings. **Draft: see "Before launch"** |
| `tools/` | How the assets were made: `make_site_panel_svg.py` (run from the Resilio repo root) and `demos/` (stimulus, render and encode scripts, loudness report). Not needed by the website |

Markers in the copy: `<!-- DEMOS -->`, `<!-- DOWNLOADS -->`, `<!-- PANEL -->`, `<!-- SCREENSHOT -->`, `<!-- LATEST -->` say where a component goes. `<!-- OWNER: … -->` marks a question for the owner: never publish it, never invent the answer.

## Assets: usage notes

- **Panel SVG.** `role="img"` with `<title>` and `<desc>` built in. Colours are CSS custom properties with fallbacks, so inline it and set them per theme: `--rv-panel`, `--rv-ink`, `--rv-ink-soft`, `--rv-knob`, `--rv-metal`, `--rv-led`, `--rv-accent` (the button), `--rv-font`. As an `<img>`, use the alt text: *"Resilio Versio panel map: BLEND and DECAY at the top with four level LEDs between them, TONE, SPLASH, TENSION, WOBBLE and DRIVE below, the TANK and ATTITUDE switches, the THROW / TAP button, and twelve jacks."* Geometry is from Noise Engineering's printable Versio panel template; the diagram credits it in small type, keep that.
- **Plugin screenshot** alt text: *"The Resilio Versio plugin: a dark panel laid out like the module, with BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE and DRIVE knobs, TANK and ATTITUDE selectors, the THROW button, a GATE switch and size buttons."*
- **Audio.** No autoplay. One clip at a time. Show caption, duration and settings; offer the transcript (expandable is fine). Suggested order: as numbered. The first three make the best above-the-fold set.

## Before launch (for the owner)

1. **Listen to the ten demos** and approve them (or say which to cut or redo). They were rendered and loudness-checked by machine but not yet heard by you, and their transcripts describe the intended sound.
2. **Plugin screenshot.** In the snapshot the GATE switch overlaps DRIVE's label and the button reads "THR…". Check whether the plugin looks the same in Ableton. If it does, it's a small layout fix in the plugin before a screenshot goes public; if it doesn't, take a screenshot from Ableton instead.
3. **Licence.** None chosen yet; the plugin builds on JUCE (AGPLv3 or JUCE's licences). Needed before the repo and downloads go public (`content/credits.md`).
4. **Your credit** (name or handle, link) in `content/credits.md`.
5. **Feedback channel** (GitHub issues, email, form) in `content/overview.md` and `content/faq.md`.
6. **Which Versio module(s)** it has been played on, for the FAQ.
7. **Stable download names** (BRIEF §9): agree to the release script also uploading `resilio-versio-firmware.bin` and `resilio-versio-plugin-macos.zip` (and optionally `latest.json`), so the site's "latest" links never change.
8. **Go-public switch.** Download links and the source link are live only once the repository is public.

## Sizes

Audio 3.3 MB (10 MP3s), panel SVG 7 KB, screenshot 57 KB; the whole package about 3.5 MB.
