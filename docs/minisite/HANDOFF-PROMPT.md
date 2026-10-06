# Handoff prompt (paste into Claude in the website repo)

Copy the whole `docs/minisite/` folder from the Resilio Versio repo into the website repo first (for example to `content-packages/resilio-versio/`), then paste the prompt below, with the path adjusted.

---

I want to add a minisite for **Resilio Versio** to my website: a free dub spring reverb I made, as firmware for the Noise Engineering Versio Eurorack module and as an AU/VST3 plugin for macOS.

Everything you need is in `content-packages/resilio-versio/` (path relative to this repo). Read these first, in order:

1. `README.md`: what's in the package, asset notes, and the owner's open items.
2. `BRIEF.md`: purpose, audience, the seven pages and their jobs, information hierarchy, tone of voice, must-haves, must-nots, accessibility, and how downloads and new releases should work.
3. `content/*.md`: the final copy, one file per page, with front-matter for titles, descriptions and slugs.
4. `assets/`: ten demo clips with `demos.json` (captions, transcripts, settings), the panel diagram SVG, a plugin screenshot (draft).

What to build:

- The pages in `BRIEF.md` §3 (overview as the landing page, manual, install, starting points, changelog, FAQ, credits), in this site's own design system and conventions. Propose the layout and visual direction to me before building it out; the brief deliberately leaves design to this project.
- A demo player driven by `demos.json`: captions, settings, transcripts, keyboard operable, one clip at a time, no autoplay.
- Download buttons and the current version/date from **GitHub's releases API at build time** (`BRIEF.md` §9, option A), with a cached fallback, and a single switch that shows "coming soon" until the repository `jeffebauer/resilio-versio` is public. Link to the stable asset names in §9; don't hard-code a version in the copy.
- The panel SVG inlined (so it can follow light/dark mode via its CSS custom properties).
- SEO from the front-matter; an Open Graph image.

Rules:

- Treat the copy as final for facts: edit for length and layout only. Keep the panel names exactly (BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE; TANK 1 · 2 · ECHO; ATTITUDE CLEAN · TAPE · VALVE; THROW / TAP).
- Never publish the `<!-- OWNER: … -->` comments and never fill them in yourself: list them for me.
- Follow the must-nots in `BRIEF.md` §7: no implied endorsement by Noise Engineering, trademarks named descriptively, no unverified claims, no audio other than the package's demos.
- Don't use the plugin screenshot until I've confirmed it (README, "Before launch").

When you're done, tell me what you built, what's still waiting on me (the OWNER items and the go-public switch), and how a new release will show up on the site.
