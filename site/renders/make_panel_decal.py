#!/usr/bin/env python3
"""Make the panel decal texture from the owner's panel art.

Ported back from Sublustris Versio's version (1 Nov 2026), which grew from this
script with one change: the drawn hardware is found by what it IS, not by its index,
so a revised export of the art (new colours, a moved label, Figma reordering
its layers) needs no edits here. Swap the SVG, re-run, done.

The art (docs/minisite/assets/panel/resilio-versio-panel-art.svg, exported
from Figma) is the panel at true scale: 191 x 486 SVG units = 50.5 x 128.5 mm
(1 mm = 3.7795 units, CSS px at 96 dpi). It draws two kinds of things:

  * the flat PRINT: labels, scale ticks, rings, the red secondary labels, the
    coloured ring round the THROW jack, the logo;
  * drawn HARDWARE: knob caps and their indicator lines, jack nuts, the toggle
    bodies, the THROW button, the LEDs and the four mounting screws. The 3D
    scene models these, so they are removed here.

How hardware is recognised (top-level children of the art's clip group):
  * the full-panel background rect (the paint: the material supplies it);
  * every element carrying an SVG `filter` (Figma exports the hardware's drop
    shadows and inner shadows as filters; the print has none);
  * every embedded <image> (the toggle bodies are bitmaps);
  * a plain white stroke directly after a removed knob cap (its indicator line).
Each removed element is matched to the nearest hole in the Versio template
(docs/minisite/panel/versio_panel_coordinates.csv) and the counts are checked:
12 jacks, 7 knobs (+7 lines), 2 toggles, 1 button, 4 LEDs, 4 screws. A
mismatch prints a warning: look at --full vs the default output before using it.

Output: the print on a transparent background (RGBA PNG, 32 px/mm), which the
Blender material lays over the panel paint.

Rasterised with headless Google Chrome (no rsvg/inkscape on this Mac).

    python3 site/renders/make_panel_decal.py            # -> site/renders/textures/panel_print.png
    python3 site/renders/make_panel_decal.py --full     # whole art, for comparison
    python3 site/renders/make_panel_decal.py --art other.svg --out /tmp/x.png
"""
import argparse
import csv
import math
import os
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
ART = os.path.join(REPO, "docs/minisite/assets/panel/resilio-versio-panel-art.svg")
COORDS = os.path.join(REPO, "docs/panel/versio_panel_coordinates.csv")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

PANEL_MM = (50.5, 128.5)
PX_PER_MM = 32  # 1616 x 4112 px: labels stay crisp in a macro crop
SVG_NS = "http://www.w3.org/2000/svg"
NS = "{%s}" % SVG_NS
EXPECTED = dict(jack=12, pot=7, toggle=2, button=1, led=4, mounting=4)


def _tag(el):
    return el.tag.replace(NS, "")


def _bbox(el):
    """Rough bbox in SVG units: rects, circles, absolute path coordinates and
    <use> targets are enough for locating hardware (all drawn in absolute
    coordinates by Figma)."""
    xs, ys = [], []
    for e in el.iter():
        t = _tag(e)
        if t in ("rect", "image") and e.get("width"):
            x, y = float(e.get("x", 0)), float(e.get("y", 0))
            xs += [x, x + float(e.get("width"))]
            ys += [y, y + float(e.get("height"))]
        elif t == "circle":
            cx, cy, r = (float(e.get(k)) for k in ("cx", "cy", "r"))
            xs += [cx - r, cx + r]
            ys += [cy - r, cy + r]
        elif t == "path":
            d = e.get("d", "")
            if re.search(r"[a-z]", d.replace("e-", "")):
                continue  # relative commands: skip rather than mislocate
            v = [float(n) for n in re.findall(r"-?\d*\.?\d+(?:e-?\d+)?", d)]
            xs += v[0::2]
            ys += v[1::2]
    if not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def _use_bbox(el, defs_by_id):
    """bbox of <use href="#id"> targets inside el (knob caps are drawn that way)."""
    for e in el.iter():
        if _tag(e) == "use":
            ref = (e.get("{http://www.w3.org/1999/xlink}href") or e.get("href") or "").lstrip("#")
            if ref in defs_by_id:
                return _bbox(defs_by_id[ref])
    return None


def load_holes():
    holes = []
    with open(COORDS) as f:
        for row in csv.DictReader(f):
            holes.append(dict(id=row["id"], kind=row["kind"],
                              x=float(row["x_mm_from_left"]), y=float(row["y_mm_from_top"])))
    return holes


def _nearest(holes, cx_mm, cy_mm):
    best = min(holes, key=lambda h: math.hypot(h["x"] - cx_mm, h["y"] - cy_mm))
    return best, math.hypot(best["x"] - cx_mm, best["y"] - cy_mm)


def classify(root, verbose=True):
    """Return (main group, set of child indices to drop, report lines)."""
    main = [c for c in root if c.tag == NS + "g"][0]
    defs_by_id = {e.get("id"): e for e in root.iter() if e.get("id")}
    unit = 191.0 / PANEL_MM[0]  # SVG units per mm (the art is 191 x 486 for 50.5 x 128.5)
    holes = load_holes()
    kids = list(main)
    drop, found, report = set(), {}, []
    prev_knob = False
    for i, c in enumerate(kids):
        t = _tag(c)
        is_bg = (t == "rect" and float(c.get("width", 0)) > 180 and float(c.get("height", 0)) > 470)
        hardware = is_bg or c.get("filter") is not None or t == "image"
        if not hardware and prev_knob and t == "path" and c.get("stroke") == "white" and not c.get("fill"):
            drop.add(i)
            found.setdefault("pot_line", []).append(i)
            prev_knob = False
            continue
        prev_knob = False
        if not hardware:
            continue
        drop.add(i)
        if is_bg:
            report.append(f"  #{i:3d} panel background rect")
            continue
        bb = _bbox(c) or _use_bbox(c, defs_by_id)
        if bb is None:
            report.append(f"  #{i:3d} {t}: no geometry found (dropped as hardware)")
            continue
        cx, cy = (bb[0] + bb[2]) / 2 / unit, (bb[1] + bb[3]) / 2 / unit
        h, dist = _nearest(holes, cx, cy)
        found.setdefault(h["kind"], set()).add(h["id"])
        prev_knob = h["kind"] == "pot"
        if prev_knob:
            found.setdefault("pot_group", []).append(i)
        flag = "" if dist < 2.5 else "   <-- far from any hole: check"
        report.append(f"  #{i:3d} {t:5s} at ({cx:5.1f}, {cy:5.1f}) mm -> {h['id']} ({h['kind']}, {dist:.1f} mm){flag}")
    warn = []
    for kind, n in EXPECTED.items():
        got = len(found.get(kind, ()))
        if got != n:
            warn.append(f"expected {n} {kind} hardware parts, found {got}")
    # one indicator line per drawn cap (the art may stack two caps on a knob)
    if len(found.get("pot_line", [])) != len(found.get("pot_group", [])):
        warn.append(f"{len(found.get('pot_group', []))} knob caps but {len(found.get('pot_line', []))} indicator lines")
    if verbose:
        print(f"[decal] {len(drop)} hardware elements removed of {len(kids)}:")
        print("\n".join(report))
        print(f"[decal] knob indicator lines: {found.get('pot_line', [])}")
        for w in warn:
            print(f"[decal] WARNING: {w}")
        if not warn:
            print("[decal] hardware counts match the Versio template")
    return main, drop, warn


def build_svg(art, full):
    if full:  # the art as drawn, untouched (ElementTree mangles Figma's foreignObject gradients)
        with open(art) as f:
            return f.read()
    ET.register_namespace("", SVG_NS)
    ET.register_namespace("xlink", "http://www.w3.org/1999/xlink")
    tree = ET.parse(art)
    root = tree.getroot()
    main, drop, _ = classify(root, verbose=not full)
    if not full:
        kids = list(main)
        for i in sorted(drop, reverse=True):
            main.remove(kids[i])
    return ET.tostring(root, encoding="unicode")


def rasterise(svg, out_png):
    w, h = round(PANEL_MM[0] * PX_PER_MM), round(PANEL_MM[1] * PX_PER_MM)
    svg = re.sub(r'<svg([^>]*?)\swidth="[^"]+"\s+height="[^"]+"',
                 rf'<svg\1 width="{w}" height="{h}" preserveAspectRatio="none"', svg, count=1)
    html = ("<!doctype html><html><head><style>html,body{margin:0;background:transparent;}"
            f"svg{{display:block;width:{w}px;height:{h}px}}</style></head><body>" + svg + "</body></html>")
    with tempfile.TemporaryDirectory() as td:
        page = os.path.join(td, "panel.html")
        with open(page, "w") as f:
            f.write(html)
        subprocess.run(
            [CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars",
             "--default-background-color=00000000", "--virtual-time-budget=5000", f"--window-size={w},{h}",
             f"--screenshot={out_png}", "file://" + page],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f"[decal] wrote {out_png} ({w} x {h})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--art", default=ART, help="panel art SVG (default: the repo's copy)")
    ap.add_argument("--full", action="store_true", help="keep the drawn hardware too")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    out = a.out or os.path.join(HERE, "textures", "panel_full.png" if a.full else "panel_print.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    rasterise(build_svg(a.art, a.full), out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
