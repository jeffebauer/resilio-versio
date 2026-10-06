# Style audit: UDO Audio, DMNO page

Audited 6 Oct 2026 from https://www.udo-audio.com/dmno (Webflow), at 1440 × 900 and 375 × 812, by reading computed styles and the page's own CSS custom properties. This is the reference for the **style**, not material to copy: no UDO images, copy, logos or fonts go into our site (see `DESIGN.md` → "A cousin, not a clone"). Reference screenshots stay out of the repo (UDO's copyright).

## 1. What makes it work, in one paragraph

A light, almost clinical page (off-white, a warm cream section, near-black type) carried by **one typeface in one light weight at a few big sizes**, set on a **visible structure of 1 px hairlines** and a numbered index ("01 Introduction … 05 Tech Spec"). Against that restraint sit **huge, soft, photo-real product renders** (warm grey light, shallow depth, tight macro crops of knobs and switches) and **soft, rounded UI** (pill navigation with background blur, rounded image cards, pill buttons). One red is the only colour, used once per screen for the main call to action.

## 2. Typography

| Role | Size at 1440 | vw | Line height | Weight | Notes |
|---|---|---|---|---|---|
| Display (product name, section heads, the feature stack) | 54.7–60 px | 3.8–4.2vw | 1.0 | 300 Light | Feature stack set UPPERCASE, centred, one line per feature, with tiny superscript notes ("FPGA POWERED") |
| Large paragraph (intro) | 25.2 px | 1.75vw | 1.2 | 300 | Text block starts at a column line, not the page edge |
| Body / index | 20.2 px | 1.4vw | 1.2 | 300 | Index items "01 Introduction", numbers in grey |
| Smaller paragraph | 18.7 px | 1.3vw | 1.2 | 300 | |
| Mini / labels | 15.8 px | 1.1vw | 1.0 | 400 | Grey #6c6c6c |
| UI (nav, buttons) | 13–15 px | fixed | 1.0 | 400 | letter-spacing 0.04em (0.52 px at 13 px) |
| Mobile body | 15.9 px | 4.25vw | 1.2 | 300 | Paragraphs after the first use a **first-line indent** instead of a gap |

- Families: **Helvetica Now Display** (300, 400) for nearly everything, Helvetica Now Text for small UI. Commercial (Monotype).
- No bold anywhere on the page: hierarchy comes from size, grey vs black, and position.
- Tight leading (1.0–1.2) everywhere, letter-spacing normal; large type is never tracked out.
- Type sizes are pure `vw`, so they don't respond to browser zoom/text size (an accessibility fault we won't copy).

## 3. Colour (the page's own custom properties)

| Token (UDO's name) | Value | Use |
|---|---|---|
| `--off-white` | #f6f5f3 | Page background |
| white | #ffffff | Image slider band |
| `--v-light-grey` | #ececec | Image card fill, nav pill (at 42 % with blur) |
| `--beige-bg` | #f0e7db | The tech/spec section (cream) |
| `--off-black` | #111111 | Text, dark buttons ("Connect"), dark cards |
| `--6c` / `--dark-grey` | #6c6c6c | Secondary text, index numbers |
| `--text-on-cream` | #726a56 | Secondary text on cream |
| `--line-on-white` | #cfcfcf | Hairlines on light |
| `--line-on-cream` | #cec7b1 | Hairlines on cream |
| `--26_red` / `--red` | #f2472b / #eb483c | The one accent: "Find a Dealer" |

Measured contrast: #6c6c6c on #f6f5f3 = 4.8:1 (AA). #726a56 on #f0e7db = 4.4:1 (just under AA for body text). White on #eb483c ≈ 3.7:1 (**fails AA** for its 13.7 px label).

## 4. Grid, spacing, linework

- Margins are small and fluid: 1 / 1.1 / 1.6 / 3vw (≈ 14–43 px at 1440); `--gap` 20 px; image gap 1.3vw. Content runs nearly edge to edge.
- Implicit grid (flex, not CSS grid): a narrow left column (≈ 20 %) holds labels and the numbered index; content starts at a column line to its right. Headline rows put a big title left and a single action right ("DMNO Play Seriously." … "Find a Dealer").
- **Linework:** 1 px hairlines, mostly `border-top` on rows and `border-bottom` under headers, full-bleed; a single centred **vertical hairline** links the cream section's statement to the card below it. Lines are structure, never decoration.
- Section rhythm: very generous vertical space (sections of 700–900 px) with little in them.

## 5. Components

| Component | Spec |
|---|---|
| Logo | Wordmark top-left, black, ~23 px cap height |
| Nav | Centred **pill**: radius 200 px, `rgba(228,228,228,.42)` + `backdrop-filter: blur(20px)`, items 13 px, padding 9/26 px. Right: dark pill "Connect" (#111, white text). Mobile: "Support | Menu" text links |
| Primary button | Red fill, white text 13.7 px, radius 11.5 px (0.8vw), padding 12/24 px, trailing ↗ arrow, `backdrop-filter: blur(5px)` |
| Social buttons | Off-black pills, radius 200 px, padding 9/26 px |
| Image cards | #ececec fill, radius 0.8vw (11.5 px), caption above in 20 px Light ("DMNO Top Down — White Edition"), a download icon per image |
| Slider | White band, padding 1vw, cards in a horizontal row, arrow buttons |
| Numbered index | "01 Introduction" list, number grey, label black, 20 px Light; doubles as section labels ("02 Key Features") |
| Feature stack | Centred UPPERCASE Light display lines, superscript notes in 13 px |
| Badges | Small outlined pills with tiny caps ("8 VOICE", "BINAURAL") |
| Spec section | Cream background, 15 px text, hairline rows, quick links top right |

## 6. Imagery

Photo-real 3D renders: off-white product on a white-grey ground, soft large-source light from top-left, gentle shadows, shallow depth of field on macro crops, cool-blue gradient sky behind the hero. Crops alternate full product → angled macro of a control cluster → back panel. Images are 1440 px wide WebP; macro crops are used as full-width bands with rounded corners.

## 7. Motion

Fades and slides on scroll (Webflow interactions), a pinned, word-by-word feature stack, a slider. On phones, sections stay **blank until their animation fires** (seen in the audit at 375 px): content is hidden by default and revealed by script.

## 8. What we keep, what we fix

Keep: one family in Light, few big sizes, tight leading, hairline structure, numbered index, generous space, rounded image cards, pill nav with blur, one accent per screen, macro render crops.
Fix: vw-only type (use `clamp()` with rem), the AA misses (accent button, cream secondary text), content hidden until JS runs.
