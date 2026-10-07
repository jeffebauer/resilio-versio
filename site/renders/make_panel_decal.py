#!/usr/bin/env python3
"""Make the panel decal texture from the owner's panel art.

The owner's art (docs/minisite/assets/panel/resilio-versio-panel-art.svg,
exported from Figma) is the panel at true scale: 190.87 x 485.67 SVG units
= 50.5 x 128.5 mm (1 mm = 3.7795 units, i.e. CSS px at 96 dpi). It draws two
kinds of things:

  * the flat PRINT layer: labels, scale ticks, rings, the red secondary labels;
  * drawn HARDWARE: knob caps, jack nuts, toggle bodies, the push button, LEDs
    and the four mounting screws. The 3D scene models these, so they are
    removed here (otherwise they'd be printed under the 3D parts).

The SVG has no layer names (Figma flattens them), so the hardware is picked by
its top-level index, checked by eye against the art (see HARDWARE below). The
black panel rect is dropped too, so the output is the print on a transparent
background: the Blender material lays it over the panel base colour, which can
be clay grey, anodised black or anything else.

Rasterised with headless Google Chrome (no rsvg/inkscape on this Mac).

    python3 site/renders/make_panel_decal.py            # writes textures/panel_print.png
    python3 site/renders/make_panel_decal.py --full     # whole art, for comparison
"""
import argparse
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ART = os.path.join(REPO, "docs/minisite/assets/panel/resilio-versio-panel-art.svg")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

PANEL_MM = (50.5, 128.5)
PX_PER_MM = 32  # 1616 x 4112 px: labels stay crisp in a macro crop

# Top-level children of the art's clip group that are hardware, not print.
#   0            black panel rect (replaced by the material's base colour)
#   1-14, 16-25  jack nuts + inserts (15 is a printed white ring round OUT: kept)
#   131-146      knob caps (odd) + their white indicator lines (even)
#   147-148      toggle bodies + levers
#   151          push button
#   152-155      LEDs
#   156-159      mounting screws
HARDWARE = ({0} | set(range(1, 15)) | set(range(16, 26)) | set(range(131, 149))
            | {151} | set(range(152, 160)))

SVG_NS = "http://www.w3.org/2000/svg"


def build_svg(full: bool) -> str:
    ET.register_namespace("", SVG_NS)
    tree = ET.parse(ART)
    root = tree.getroot()
    main = [c for c in root if c.tag == f"{{{SVG_NS}}}g"][0]
    if not full:
        kids = list(main)
        for i in sorted(HARDWARE, reverse=True):
            main.remove(kids[i])
    return ET.tostring(root, encoding="unicode")


def rasterise(svg: str, out_png: str) -> None:
    w, h = round(PANEL_MM[0] * PX_PER_MM), round(PANEL_MM[1] * PX_PER_MM)
    html = (
        "<!doctype html><html><head><style>html,body{margin:0;background:transparent;}"
        f"svg{{display:block;width:{w}px;height:{h}px}}</style></head><body>"
        + svg.replace('width="191" height="486"', f'width="{w}" height="{h}" preserveAspectRatio="none"')
        + "</body></html>"
    )
    with tempfile.TemporaryDirectory() as td:
        page = os.path.join(td, "panel.html")
        with open(page, "w") as f:
            f.write(html)
        subprocess.run(
            [CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
             "--default-background-color=00000000", f"--window-size={w},{h}",
             f"--screenshot={out_png}", "file://" + page],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f"wrote {out_png} ({w} x {h})")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--full", action="store_true", help="keep the drawn hardware too")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    out = a.out or os.path.join(HERE, "textures", "panel_full.png" if a.full else "panel_print.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    rasterise(build_svg(a.full), out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
