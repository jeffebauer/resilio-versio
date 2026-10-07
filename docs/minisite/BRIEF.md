# Resilio Versio minisite: brief

This brief says what the site must **say** and **do**. Layout and visual design belong to the website project. Final copy is in `content/`, assets in `assets/`. Nothing here needs the Resilio Versio source repository.

## 1. Purpose

Resilio Versio is a free dub spring reverb: alternative firmware for the Noise Engineering Versio Eurorack module, plus an AU/VST3 plugin for macOS that runs the same sound engine. The minisite is its public home:

1. **Make someone hear it** within ten seconds of landing (a demo player near the top).
2. **Explain what it does** in a dub player's language, not a DSP engineer's.
3. **Get it onto their module or into their DAW** safely (downloads, a careful flashing guide, the way back to stock firmware).
4. **Be the manual** people keep open next to the rack.

## 2. Audience

| Who | What they know | What they need first |
|---|---|---|
| **Versio owners** (Eurorack players, any Versio module) | Eurorack, patching, maybe NE's Firmware Swap. Probably not this project | What it sounds like, that it's safe to try and easy to undo, the panel map (their printed labels won't match) |
| **Plugin users without a Versio** (producers on macOS) | DAWs, sends and returns, dub moves | Sound, the plugin download, the macOS install steps (it isn't notarised) |
| **Dub and dub-techno heads** | King Tubby, Lee Perry, the throw, Basic Channel | That the moves they know are built in: the throw, the Big Knob, tape echo into springs, the held bed |
| **Builders and the curious** | Code, DSP | A link to the source on GitHub (once it's public) |

Assume a reader who has never heard of the project. Never assume they read another page first.

## 3. Pages

| Page | File | Its job |
|---|---|---|
| **Overview** (the landing page) | `content/overview.md` | Hook, demos, what it is, the dub moves, two download buttons, status. Answers "what is this and should I care?" in one scroll |
| **Manual** | `content/manual.md` | Every control, the echo mode, the throw, LEDs, CV, a quick start, plugin-only extras. The page people return to |
| **Install** | `content/install.md` | Flashing via Noise Engineering's Firmware Swap, going back to stock, installing the plugin on macOS including Gatekeeper, system requirements, troubleshooting |
| **Starting points** (presets) | `content/presets.md` | Six dub settings as tables a player can dial in by hand |
| **Changelog** | `content/changelog.md` | What changed in each release, newest first; the current version and date at the top |
| **FAQ** | `content/faq.md` | The questions people will ask, short answers |
| **Credits** | `content/credits.md` | References, acknowledgements, trademarks, licence status, contact |

A single page with anchored sections is fine if the design wants it; keep the seven sections and their order of importance.

## 4. Information hierarchy

Landing page, top to bottom:

1. **Name + one line**: "Resilio Versio: a dub spring reverb for the Noise Engineering Versio" (and "also as an AU/VST3 plugin for macOS").
2. **Listen**: the demo player (all ten clips available; the first two or three visible without scrolling on desktop).
3. **Get it**: two buttons side by side, *Firmware for Versio (.bin)* and *Plugin for macOS (AU/VST3)*, each with the version, date and size, plus a "how to install" link.
4. **What it sounds like** (four short points).
5. **The dub moves** (throw, Big Knob, tape echo into springs, the held bed, the howl), each a sentence, linking into the manual.
6. **The panel** diagram (`assets/panel/resilio-versio-panel.svg`) with a link to the manual.
7. **Status and not-affiliated note**, links to changelog, FAQ, credits, source.

Everywhere: the **current version and its date** must be visible near the download buttons and at the top of the changelog.

## 5. Tone of voice

Write like the project's own manual: **plain, musical, dub-literate.**

- Describe sound and gesture, not algorithms: "the tail rings on", "a bright clang", "thin and telephone-like", not "allpass cascade" or "T60". Numbers only where a player uses them (seconds, clock positions, dB of gain, note values).
- Short sentences. Active voice. Second person ("hold the button on the snare you want drenched").
- Knob positions as clock positions (7 o'clock fully left, noon, 5 o'clock fully right).
- **Panel names in capitals, exactly**: BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE; the TANK switch with positions 1 · 2 · ECHO (write "TANK 2", "TANK ECHO"); ATTITUDE with CLEAN · TAPE · VALVE; the THROW / TAP button. Never use the old names (MIX, SPRINGS, DRIVEN, KICKED, KICK) except in the changelog's history, where the copy already explains them.
- Name dub figures with respect and accuracy: King Tubby, Lee "Scratch" Perry, Scientist, Dennis Bovell, Adrian Sherwood, Basic Channel. No slang pastiche, no "vibes".
- No hype words: not "revolutionary", "ultimate", "studio-grade", "analog-perfect".

## 6. Must-haves

- **Download buttons** for the firmware (`.bin`) and the plugin (`.zip`), each with version, date and file size, and the link scheme in §9.
- **System requirements**, next to the buttons and on the install page:
  - Firmware: a Noise Engineering Versio module; a computer with Chrome and a micro-USB data cable.
  - Plugin: macOS 12 or newer, Apple Silicon or Intel; a DAW that loads AU or VST3. No Windows or Linux build.
- **Version and date** of the current release (from the release data, §9; never hard-coded copy).
- **The panel diagram** (SVG, themeable: see "Assets: usage notes" in the package `README.md`) on the overview and the manual.
- **The safety line** wherever flashing is mentioned: *Never connect USB and Eurorack power at the same time.* Make it visually prominent on the install page.
- **The way back**: going back to Noise Engineering's firmware is one step, stated on the overview and the install page.
- **Demo player** with captions, settings and a transcript per clip (`assets/audio/demos.json`).
- **Not-affiliated note** on every page footer: "Resilio Versio is an independent project. It is not affiliated with or endorsed by Noise Engineering."
- **A link to the source** on GitHub (`https://github.com/jeffebauer/resilio-versio`) once the repository is public.
- **Status**: "Pre-release: played on real hardware; the sound may still change between versions."
- SEO: each page's `title` and `description` from its front-matter; an Open Graph image (the panel SVG rasterised, or a design of the site's own).

## 7. Must-nots

- **No implied endorsement by Noise Engineering.** Say "for the Noise Engineering Versio", "runs on the Versio". Never "official", "Noise Engineering's Resilio", or the NE logo. Don't reuse NE's product photography. The panel diagram's geometry comes from NE's public panel template and is credited.
- **Trademarks named properly and only descriptively**: Noise Engineering, Versio; Electro-Smith Daisy Seed; Teaching Machines Wellspring; Strymon Magneto; Roland Space Echo (RE-201); Mutable Instruments Beads; Ableton Live; Logic Pro; Audio Units (Apple); VST is a trademark of Steinberg Media Technologies GmbH. The credits page carries the trademark line.
- **No reference recordings of commercial units** (the Wellspring, the Magneto, any IR library) on the site, in demos or for download. The project used them for tuning only. The demos in `assets/audio/` are renders of Resilio itself on synthetic test signals the project generated.
- **No unverified claims.** In particular, don't claim: identical sound to any hardware unit, "sounds exactly like a real spring", support on Windows/Linux, notarised/signed by Apple, tested in any DAW other than Ableton Live, a licence that hasn't been chosen (see §11), or compatibility with a specific Versio module the owner hasn't confirmed. When unsure, cut the claim.
- **No auto-playing audio.** No audio that starts on hover.
- Don't present the plugin as a product with support; it's the project's desktop test bench, free, as-is.

## 8. Accessibility

- **Audio**: every clip has a visible caption (what you hear, in one line), the settings used, and a short **transcript** (a text description of the sound, in `demos.json`). Players are keyboard operable, have visible focus, a text label (not icon-only) and a pause control; only one clip plays at a time. Show the duration.
- **Images**: the panel SVG carries `role="img"` with a `<title>` and a `<desc>` describing the layout; when inlined, keep them, or give the `<img>` the alt text in the package README. The plugin screenshot needs alt text (README).
- **Clock positions** are visual; where the copy says "2 o'clock", the presets page also gives the exact 0–1 value (the tables already do).
- Colour: don't encode meaning in colour alone (e.g. the LED colours in the manual are also named in words).
- Code blocks (the Terminal commands on the install page) must be selectable and have a copy button with an accessible label.
- Respect `prefers-reduced-motion` for any waveform animation; `prefers-color-scheme` for the panel SVG (it uses CSS custom properties, see README).

## 9. Downloads and the release update flow

**Today** (repository private; links go live once it's public): each release is a GitHub Release on `jeffebauer/resilio-versio`, tagged `vYYYY.MM.DD-<commit>` (e.g. `v2026.10.06-a6c70a4`), with two assets whose names carry the commit:

| Asset | Today's name | Size (current) |
|---|---|---|
| Plugin (+ firmware + read-me), zip | `ResilioVersio_v2026.10.06-a6c70a4.zip` | 7.8 MB |
| Firmware | `resilio_versio_firmware_a6c70a4.bin` | 126 KB |

Pre-releases (sound candidates for A/B, plugin only, e.g. `v2026.10.02-cef6a77-candidate-F`) are marked as pre-release on GitHub and must **not** be offered as the main download.

**Proposed stable asset names** (a small change to the release script, not made yet; the owner decides): each release also uploads the same files under fixed names, so the site can link to "latest" without knowing the version:

- `resilio-versio-firmware.bin`
- `resilio-versio-plugin-macos.zip`
- `latest.json` (option B below)

Links then become, and stay, valid for every future release (GitHub's `releases/latest` skips pre-releases and drafts):

- `https://github.com/jeffebauer/resilio-versio/releases/latest/download/resilio-versio-firmware.bin`
- `https://github.com/jeffebauer/resilio-versio/releases/latest/download/resilio-versio-plugin-macos.zip`
- Release page: `https://github.com/jeffebauer/resilio-versio/releases/latest`

**Live once the repo is public.** Until then the buttons must fall back to a "coming soon" state; build the site so this is a single switch.

**How the site learns of a new version.** Two ways; the site needs version, date, notes and asset URLs/sizes:

- **A. GitHub's releases API (recommended).** `GET https://api.github.com/repos/jeffebauer/resilio-versio/releases/latest` returns `tag_name`, `published_at`, `body` (the notes, plain text), `html_url` and `assets[]` (`name`, `size`, `browser_download_url`). Fetch it **at build time** (static generation), not from every visitor's browser: unauthenticated calls are limited to 60 an hour per IP. Rebuild on release: either a GitHub Action in the Resilio repo on `release: published` that calls the website host's deploy hook (one secret, the hook URL), or a daily scheduled rebuild. Keep the last good response cached so a GitHub outage doesn't break the build. Nothing needs to change in the release script except (optionally) the stable names above.
- **B. A `latest.json` the release script publishes.** The script would add a small file to each release, for example:
  ```json
  { "version": "v2026.10.06-a6c70a4", "date": "2026-10-06", "plugin_version": "1.5.33",
    "firmware": { "name": "resilio-versio-firmware.bin", "bytes": 126380 },
    "plugin": { "name": "resilio-versio-plugin-macos.zip", "bytes": 7807928, "macos": "12.0", "arch": ["arm64", "x86_64"] },
    "notes_url": "https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.06-a6c70a4" }
  ```
  served at `…/releases/latest/download/latest.json`. Simpler to parse and shaped exactly for the site, but it's one more thing for the release script to keep right, and GitHub's download redirects aren't meant for browser `fetch` (CORS), so it too should be read at build time.

**Recommendation: A**, read at build time, with the stable asset names for the buttons. The API already carries everything (including sizes and notes), it needs no new file to maintain, and it can't drift from what was actually published. Use B only if the site's host can't make an HTTPS call during the build.

The changelog page: build it from the releases API (`GET /repos/jeffebauer/resilio-versio/releases`, skipping pre-releases unless shown as "candidates"), or from `content/changelog.md`, which is written for the site and should be updated by hand per release if the API route isn't used. The release notes on GitHub are the project's plain-text read-me (install steps, controls, "what's new"); show only their "What's new" part, or link to them.

## 10. Content rules for the website's Claude

- The copy in `content/` is final and checked against the firmware's code (October 2026, release v2026.10.06-a6c70a4). Edit for layout and length, not for facts. If a fact must change, ask the owner.
- Front-matter (`title`, `description`, `slug`, `order`) is for SEO and navigation.
- `<!-- OWNER: … -->` comments mark the few places that need the owner's input before launch; don't publish them, and don't invent the answer.

## 11. Open items for the owner (before launch)

Listed in the package `README.md` under "Before launch".
