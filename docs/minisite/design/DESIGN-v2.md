# Resilio Versio minisite: design revision 2 (a hybrid, not a copy)

**Status:** agreed with the owner, 7 Oct 2026. Supersedes the parts of [DESIGN.md](DESIGN.md) listed under "Retire". Reason: the first build followed UDO Audio's DMNO page too literally in several signature elements; the owner doesn't want to be seen copying it. The site keeps its content, renders, Phonic and the signal red, and takes **principles** (never layouts) from a wider set of references.

## References and what we take from each

| Reference | What it does well | What we take |
|---|---|---|
| Teenage Engineering EP-133 | The product annotated with callout boxes joined by thin leader lines; huge bold headline; one hot accent | **The annotated panel**: the front-on render with callouts explaining each control |
| Teenage Engineering OP-XY, TP-7 | Cinematic dark mood, very little text, confident one-liners | **The dark hero** (the reveal film) and a dark sound section |
| General Type Studio, Mier | Heavy display weights at huge sizes, white page, hairline rows, tiny labels, a strict three-column header | **Heavy display + light text**, the **white body**, the **three-column header bar** |
| Tight Type, Bull 5 | A grid of rounded grey tiles, each holding one big typographic moment | **Bento tiles** for features and for the sound demos |
| Fors, Pivot and Tela | Narrow centred reading column; a bold black pictogram per feature; tiny explanatory diagrams | **Feature pictograms**, **tiny diagrams**, the **centred column** |
| Intellijel Jellymix | Plain, honest spec list | The manual and specs stay plain and complete |

Nothing is lifted one-to-one from any of them: no copied layouts, glyphs, images, copy or type.

## Retire (too close to UDO)

1. The centred blurred pill nav with a dark pill button.
2. The numbered left-rail index ("01 Introduction …") and numbered section labels.
3. The centred upper-case feature stack with superscript notes.
4. Content pushed right of a wide left rail.
5. The warm off-white page and cream section, and the all-Light type with full-bleed hairlines as the main structure.

## Decisions

| # | Decision | Owner's pick |
|---|---|---|
| V1 | **Direction: a mix**: the annotated product (TE), pictograms and tiny diagrams (Fors), bento tiles (Bull 5), heavy display type and white body (Mier), a cinematic dark hero (TE) | Mix |
| V2 | **Palette: dark hero, light body.** The hero film and the sound section are near-black; everything else is **white** (Mier), not warm off-white or cream. Ink #0a0a0a; secondary greys cool and neutral; the signal red #ee5641 stays for LEDs, marks and the one primary action; tiles in a cool light grey. Reading pages (manual, install) stay on white | Dark hero, light body (Mier) |
| V3 | **Type: heavy display + light text.** Headlines, section titles and big numbers in **Phonic Bold** (tight, large); body in Phonic Light/Regular; Phonic Monospaced for small labels and values. Strong contrast replaces the all-Light look. (Phonic Bold needs a fifth web-font file; the fallback is Inter Bold) | Heavy display + light text |
| V4 | **Header: a flat three-column bar** on a hairline: logo in column 1, small text links spanning columns 2–3, Download at the right as a small rounded-rectangle button. No pills, no blur. Over the dark hero the bar is transparent with light text | Mier-style bar |
| V5 | **Sections: big bold headings, no numbers.** Each section opens with a Phonic Bold headline; no left-rail index. Doc pages keep a plain table of contents | Big bold headings |
| V6 | **Layout: a centred reading column + full-width moments.** Prose sits in a centred column (about 38–42em); bento grids, the annotated panel and renders break out wide | Centred column |
| V7 | **Annotated panel:** the front-on render with callout boxes and thin leader lines to each control (name + one line), replacing the controls table as the main explainer on the home page; the table moves to the manual | Annotated panel |
| V8 | **Feature pictograms:** a bold, solid black glyph for each feature (springs, splash, tape echo, wobble, throw, valve/attitude, tone, CV), drawn in our own geometric style on a shared grid | Pictograms |
| V9 | **Bento tiles** replace the feature stack: rounded tiles in a grid, each with one big statement or number in Phonic Bold plus a pictogram or tiny diagram ("2 springs", "3 attitudes", "7 CV inputs", "0 dead zones", "the throw", "tape echo") | Bento tiles |
| V10 | **Tiny diagrams:** small line diagrams that explain the sound: the spring's decay envelope, a throw (send opens on one hit, tail rings on), echo repeats fading, the Big Knob filter curve, WOBBLE's pitch drift | Tiny diagrams |
| V11 | **Sound demos as bento audio tiles:** each demo a tile with a bold title, a tiny diagram of what it shows, a compact player; settings behind a toggle | Bento audio tiles |

Kept from revision 1: the content and its sources, the renders and the hero film, rounded-rectangle buttons (one shape for all controls), the signal red, accessibility and motion rules (content visible without JS, reduced motion respected), the performance budget, the release and fonts plans.

## Home page, revised order

1. **Header** (V4), transparent over the hero.
2. **Hero** (dark): the reveal film in its rounded frame; below it, on the dark band, "Resilio Versio" in Phonic Bold, the tagline in Light, Listen + Download.
3. **Intro** (white): one bold statement + a short paragraph in the centred column; the tank close-up full width.
4. **The sound** (dark): bold heading, two-sentence intro, then the bento audio tiles (V11).
5. **The panel** (white): bold heading, the annotated front-on panel (V7), wide.
6. **Features** (white): bento tiles with pictograms and diagrams (V8–V10).
7. **Two ways to play** (white): the patched module wide, then two tiles: On the module / In your DAW.
8. **Download** (light grey tile band): version, the two downloads, notes.
9. **Footer** on a hairline.

## New tokens (replacing revision 1's colour and display values)

| Token | Value | Note |
|---|---|---|
| `--paper` | #ffffff | was #f6f5f3 (UDO's off-white) |
| `--tile` | #f0f0f2 | cool light grey for bento tiles and the download band |
| `--ink` | #0a0a0a | |
| `--ink-2` | #5c5c63 | cool secondary (contrast checked: AA on white and on tiles) |
| `--line` | #e2e2e6 | hairlines on white |
| `--dark` | #0b0b0c | hero and sound bands |
| `--signal` / `--signal-ink` | #ee5641 / #c83d16 | unchanged |
| `--wt-display` | 700 | Phonic Bold for headings and big numbers |
| `--fs-display` | clamp(3rem, 1.6rem + 6vw, 7.5rem) | bigger, heavier, tighter (line-height 0.95, tracking −0.02em) |
| `--radius-tile` | clamp(0.75rem, 0.5rem + 0.8vw, 1.25rem) | bento tiles |

`--cream` is retired. Rule-based structure gives way to tiles and space; hairlines remain only in the header, footer and tables.
