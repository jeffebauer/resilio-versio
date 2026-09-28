# Versio panel template

`versio_panel_template.svg` is an accurate, to-scale template of the Noise Engineering Versio front panel (10 HP) for designing a Resilio Versio overlay. 1 unit = 1 mm (the file is 50.5 × 128.5 mm), so it imports at true size into Figma, Illustrator, Affinity or Inkscape.

## Source and accuracy

Every hole position and diameter comes from **Noise Engineering's official printable panel template** (`versio_panel.svg` in the "Printable Panels" download on NE's [World of Versio](https://noiseengineering.us/pages/world-of-versio/) page). `tools/make_panel_svg.py` regenerates the file from those numbers, and was checked against NE's raw values (all 30 holes match). Please credit Noise Engineering if you publish derived artwork. NE provides the template for making Versio panels but doesn't state a separate licence.

## Layers

| Layer | What it is |
|---|---|
| Panel outline | 50.5 × 128.5 mm |
| HP guides | Vertical lines every 5.08 mm (1 HP), a layout aid |
| Clearance guide | Dashed rings at **hole edge + 1.5 mm**. A design margin for artwork and text, **not** measured part sizes (nuts, knob caps and switch bodies vary, so measure yours) |
| Jacks, Pots, Toggle switches, Push button, LEDs, Mounting holes | The panel holes (red), one layer per type, each circle with an id (J1…J12, P1…P7, SW1–SW2, BTN, LED1…LED4, M1…M4) |
| Centre marks | Crosshairs at each centre |
| Part labels | The ids. Hide them before export |

## Coordinates

Measured from the **top-left** corner, in mm. Full table: `versio_panel_coordinates.csv`.

| Part | Hole Ø | Positions (x, y) |
|---|---|---|
| Pots P1–P7 | 9.0 | P1 (7.77, 18.53), P2 (43.33, 18.53), P3 (25.17, 28.69), P4 (7.77, 39.49), P5 (43.33, 39.49), P6 (25.17, 49.33), P7 (43.33, 60.44) |
| LEDs 1–4 | 3.0 | y 19.80, x 17.30 / 22.38 / 29.36 / 34.44 |
| Toggles SW1–SW2 | 5.08 | (8.41, 57.90), (8.41, 67.43) |
| Button BTN | 5.334 | (26.18, 69.33) |
| Jacks J1–J12 | 6.8 | x 5.23 / 18.57 / 31.90 / 44.60 × y 83.30 / 97.27 / 111.24 (J1–J4 top row, left to right) |
| Mounting M1–M4 | 3.2 | x 7.65 / 43.21 × y 3.00 / 125.50 |

## Not in NE's file (confirm on your module)

- **Which jack is which** (the 7 CV inputs, gate, In L/R, Out L/R) and **which pot is which knob index** (K0–K6 in firmware). The M0 hardware check shows the knob mapping over serial: turn each pot and note which `K` value moves. Record the answers here, and I'll add a "functions" label layer.
- Knob-cap, nut and switch-body sizes (for artwork clearances), so measure your parts.
