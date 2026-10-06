"""R3 / R4 / R5: the Versio module with the owner's Resilio panel.

Panel: 10 HP x 3U = 50.5 x 128.5 mm, 2 mm aluminium. Every hole position and
size comes from NE's official template (docs/panel/versio_panel_coordinates.csv,
the numbers behind docs/panel/versio_panel_template.svg). The owner's art
(docs/minisite/assets/panel/resilio-versio-panel-art.svg) agrees to within
0.1 mm (its P1 knob sits at x 7.68 vs NE's 7.77; everything else matches).

The panel's print is a flat decal (textures/panel_print.png, made by
make_panel_decal.py from the art's print layer only); knobs, jacks, toggles,
button, LEDs and screws are modelled in 3D on top of it.

Depth assumption (stated, not measured): ~25 mm behind the panel front.
  panel        y  0.0 .. 2.0
  pot / jack / switch bodies   2.0 .. 12.5  (Alpha 9 mm pots, Thonkiconn-type jacks)
  control PCB  12.5 .. 14.1
  parts on its back: SOIC chips, electrolytics, the 2x5 shrouded power
  header (to ~23 mm), board-to-board headers to a Daisy-Seed-sized main
  board at 22.6 .. 24.2 with its chips to ~25.6 mm.

Coordinates: panel centred on x = 0, standing on its bottom edge (z = 0),
front face at y = 0 facing -Y; hardware behind at +Y. Template coordinates
(x from left, y from top, mm) map to X = x - 25.25, Z = 128.5 - y.

build(params) returns reference points for cameras.
"""
import csv
import math
import os

import bmesh
import bpy
from mathutils import Vector

import common as C

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
COORDS = os.path.join(REPO, "docs/panel/versio_panel_coordinates.csv")
DECAL = os.path.join(HERE, "textures", "panel_print.png")

PW, PH, PT = 50.5, 128.5, 2.0

DEFAULTS = dict(
    # indicator angle per pot, degrees clockwise from noon. The art draws all
    # at noon; a few turned makes it look played. P1..P7 = BLEND, DECAY, TONE,
    # SPLASH, TENSION, WOBBLE, DRIVE (ADR 0028 / 0044).
    knob_angles=dict(P1=-35, P2=60, P3=10, P4=-80, P5=25, P6=-20, P7=95),
    # toggles: +1 up, 0 centre, -1 down. SW1 TANK 1 . 2 . ECHO, SW2 ATTITUDE.
    toggles=dict(SW1=1, SW2=0),
    leds_lit=dict(LED1=1.0, LED2=1.0, LED3=1.0, LED4=1.0),
    led_strength=14.0,
    panel_value=0.05,   # clay value of the panel base (real one: black anodised)
)


def load_holes():
    holes = {}
    with open(COORDS) as f:
        for row in csv.DictReader(f):
            x = float(row["x_mm_from_left"]) - PW / 2
            z = PH - float(row["y_mm_from_top"])
            holes[row["id"]] = dict(kind=row["kind"], x=x, z=z, d=float(row["hole_diameter_mm"]))
    return holes


def _panel(holes, mats, p):
    pan = C.box("panel", (PW, PT, PH), (0, PT / 2, PH / 2))
    cutters = [C.cylinder("hole", h["d"] / 2, PT * 3, (h["x"], PT / 2, h["z"]), axis="X", segs=40)
               for h in holes.values()]
    C.boolean_cut(pan, cutters)
    bpy.context.view_layer.update()
    # planar UVs from the front: u across the width, v up the height (world mm)
    me = pan.data
    uv = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        for li in poly.loop_indices:
            co = pan.matrix_world @ me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = ((co.x + PW / 2) / PW, co.z / PH)
    C.assign(pan, C.decal_material("panel_decal", DECAL, base_value=p["panel_value"]))
    C.add_bevel(pan, 0.25)
    return pan


def _knurled(name, r_out, r_in, depth, loc, mat, teeth=48):
    """A short cylinder with fine vertical ribs (knurled knob skirt), along Y."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=teeth * 2, radius1=r_out, radius2=r_out, depth=depth)
    step = math.pi / teeth
    for v in bm.verts:
        k = round(math.atan2(v.co.y, v.co.x) / step)
        if k % 2:
            v.co.x *= r_in / r_out
            v.co.y *= r_in / r_out
    ob = C.mesh_obj(name, bm, mat, smooth=False)
    ob.location = loc
    ob.rotation_euler = (math.pi / 2, 0, 0)
    C.add_bevel(ob, 0.15, 1)
    return ob


KNOB_H = 6.5      # panel face to top of cap (v1: 12.4)
KNOB_SKIRT = 1.6  # knurled skirt height


def _knob(name, x, z, angle, mats):
    # knurled skirt + 9 mm cap (diameter from the owner's art), 6.5 mm tall in all
    _knurled(f"{name}_skirt", 4.8, 4.55, KNOB_SKIRT, (x, -KNOB_SKIRT / 2, z), mats["knob"])
    body_h = KNOB_H - KNOB_SKIRT
    body = C.cylinder(f"{name}_cap", 4.5, body_h, (x, -KNOB_SKIRT - body_h / 2, z), mats["knob"], axis="X",
                      segs=64, r2=4.3, bevel=0.5)
    # cylinder(axis X) points its +Z (r2 end) towards -Y: the top face is at y = -KNOB_H
    top_y = -KNOB_H
    a = math.radians(angle)
    r0, r1 = 1.2, 3.9
    rm = (r0 + r1) / 2
    C.box(f"{name}_line", (0.45, 0.12, r1 - r0),
          (x + rm * math.sin(a), top_y - 0.02, z + rm * math.cos(a)), mats["ink"],
          rot=(0, a, 0))
    # behind the panel: bushing, pot body, legs into the PCB
    C.cylinder(f"{name}_bushing", 3.5, 4.5, (x, PT + 2.25, z), mats["metal"], axis="X", segs=32)
    C.box(f"{name}_pot", (9.6, 6.0, 10.5), (x, 9.5, z - 0.5), mats["pot"], bevel=0.3)
    C.box(f"{name}_potcan", (9.8, 1.0, 11.0), (x, 6.3, z - 0.3), mats["metal"], bevel=0.2)
    return body


def _tube(name, r_out, r_in, depth, y_center, x, z, mat, segs=48, bevel=0.15):
    """A short tube along Y (a ring with a real hole through it). segs=6 = hex nut."""
    t = C.cylinder(name, r_out, depth, (x, y_center, z), mat, axis="X", segs=segs, bevel=bevel,
                   smooth=segs > 8)
    C.boolean_cut(t, [C.cylinder("bore", r_in, depth * 3, (x, y_center, z), axis="X", segs=48)])
    return t


def _jack(name, x, z, mats):
    """Thonkiconn-style 3.5 mm jack seen from the front (v1 was a solid black
    cylinder standing 3.2 mm proud). Panel hole 6.8 mm:
      hex nut, 8 mm across flats, 1.6 mm thick;
      threaded bushing (6 mm, hollow) only 0.4 mm proud of the nut;
      a lighter plastic insert ring 0.6 mm inside the bushing mouth;
      a 3.5 mm bore going dark into the body."""
    af = 8.0
    _tube(f"{name}_nut", af / math.sqrt(3), 3.02, 1.6, -0.8, x, z, mats["metal"], segs=6, bevel=0.3)
    _tube(f"{name}_bushing", 3.0, 2.55, 2.0 + PT, (-2.0 + PT) / 2, x, z, mats["metal"])
    _tube(f"{name}_insert", 2.56, 1.75, 3.0, -1.4 + 1.5, x, z, mats["insert"], bevel=0.1)
    C.cylinder(f"{name}_bore_floor", 1.8, 0.2, (x, PT + 3.0, z), mats["void"], axis="X", segs=32)
    body = C.box(f"{name}_body", (9.0, 10.5, 10.5), (x, PT + 5.25, z - 0.6), mats["black"], bevel=0.3)
    C.boolean_cut(body, [C.cylinder("bore", 1.75, 6.0, (x, PT, z), axis="X", segs=32)])


def _toggle(name, x, z, pos, mats):
    C.cylinder(f"{name}_nut", 4.4, 1.6, (x, -0.8, z), mats["metal"], axis="X", segs=6, bevel=0.3, smooth=False)
    C.cylinder(f"{name}_bushing", 2.45, 4.2, (x, -2.1, z), mats["metal"], axis="X", segs=32, bevel=0.2)
    # bat lever: pivots in the bushing, tilts up/down 18 degrees
    tilt = math.radians(18 * pos)
    length = 9.5
    base = Vector((x, -4.0, z))
    tip = base + Vector((0, -math.cos(tilt), math.sin(tilt))) * length
    lever = C.cylinder(f"{name}_lever", 1.0, length, (base + tip) / 2, mats["metal"], axis="X",
                       segs=32, r2=1.3, bevel=0.3)  # bat lever, wider at the tip
    lever.rotation_euler = (math.pi / 2 - tilt, 0, 0)
    C.box(f"{name}_body", (8.0, 10.5, 13.0), (x, PT + 5.25, z), mats["black"], bevel=0.3)


def _button(name, x, z, mats):
    # cap only, straight through the 5.33 mm hole: no bezel on the real module
    C.cylinder(f"{name}_cap", 2.45, 5.0, (x, -1.0, z), mats["knob"], axis="X", segs=48, bevel=0.5)
    C.box(f"{name}_body", (7.0, 10.5, 7.0), (x, PT + 5.25, z), mats["black"], bevel=0.3)


def _led(name, x, z, mat, mats):
    C.cylinder(f"{name}_lens", 1.48, 2.6, (x, 0.2, z), mat, axis="X", segs=32, bevel=0.7)
    C.box(f"{name}_legs", (1.2, 6.0, 0.5), (x, PT + 3.5, z), mats["metal"])


def _screw(name, x, z, mats):
    head = C.cylinder(f"{name}", 2.8, 1.8, (x, -0.9, z), mats["metal"], axis="X", segs=48, bevel=0.7)
    cross = [C.box("x1", (3.0, 1.2, 0.55), (x, -1.9, z)), C.box("x2", (0.55, 1.2, 3.0), (x, -1.9, z))]
    C.boolean_cut(head, cross)


def _pcbs(mats):
    """Control PCB behind the panel, the main board stacked behind it."""
    y0 = 12.5
    C.box("pcb_control", (47.0, 1.6, 107.0), (0, y0 + 0.8, 64.0), mats["pcb"], bevel=0.2)
    yb = y0 + 1.6  # back face of the control PCB
    # shrouded 2x5 Eurorack power header (lower left of the back)
    hdr = C.box("power_header", (20.3, 9.0, 8.9), (-10.5, yb + 4.5, 24.0), mats["black"], bevel=0.3)
    slot = C.box("power_slot", (16.0, 8.0, 5.2), (-10.5, yb + 6.0, 24.0))
    C.boolean_cut(hdr, [slot])
    for i in range(5):
        for j in range(2):
            C.box("pin", (0.64, 6.0, 0.64), (-10.5 - 5.08 + i * 2.54, yb + 5.5, 24.0 - 1.27 + j * 2.54), mats["metal"])
    # op-amps (SOIC-8) and a few passives
    for k, (x, z) in enumerate([(-12, 50), (-12, 62), (-12, 74), (12, 40), (14, 104), (-14, 100)]):
        C.box(f"soic{k}", (5.0, 1.5, 4.0), (x, yb + 0.75, z), mats["chip"], bevel=0.1)
    for k, (x, z) in enumerate([(4, 22), (14, 22), (-18, 40), (18, 66)]):
        C.cylinder(f"ecap{k}", 2.5, 7.0, (x, yb + 3.5, z), mats["cap"], axis="X", segs=32, bevel=0.3)
    # board-to-board headers + Daisy-Seed-sized main board (51 x 18 mm), vertical
    xm, zm = 8.0, 72.0
    for dx in (-7.6, 7.6):
        C.box("b2b_header", (2.5, 8.5, 51.0), (xm + dx, yb + 4.25, zm), mats["black"], bevel=0.2)
    yd = yb + 8.5
    C.box("pcb_main", (18.0, 1.6, 51.0), (xm, yd + 0.8, zm), mats["pcb"], bevel=0.2)
    ydb = yd + 1.6
    C.box("mcu", (10.0, 1.4, 10.0), (xm, ydb + 0.7, zm + 6), mats["chip"], bevel=0.15)
    C.box("sdram", (8.0, 1.1, 14.0), (xm, ydb + 0.55, zm - 12), mats["chip"], bevel=0.1)
    C.box("codec", (5.0, 0.9, 5.0), (xm + 3.5, ydb + 0.45, zm + 18), mats["chip"], bevel=0.1)
    C.box("usb", (7.6, 2.8, 5.6), (xm, ydb + 1.4, zm + 23.6), mats["metal"], bevel=0.3)


def build(params=None):
    p = dict(DEFAULTS)
    p.update(params or {})
    mats = dict(
        knob=C.clay("mod_knob", 0.035, 0.45),       # dark grey caps (owner's art)
        ink=C.clay("mod_ink", 0.85, 0.5),           # white indicator lines
        metal=C.steel("mod_metal", 0.8, 0.28),      # silver nuts, bushings, toggles, screws
        black=C.clay("mod_black", 0.04, 0.55),      # jack/switch bodies, headers
        insert=C.clay("mod_insert", 0.22, 0.5),     # jack's plastic insert ring
        void=C.clay("mod_void", 0.005, 0.9),        # inside the jack bore
        pot=C.clay("mod_pot", 0.18, 0.6),
        pcb=C.clay("mod_pcb", 0.30, 0.45),
        chip=C.clay("mod_chip", 0.05, 0.4),
        cap=C.clay("mod_cap", 0.20, 0.35),
    )
    holes = load_holes()
    _panel(holes, mats, p)
    led_mats = {}
    for hid, h in holes.items():
        x, z = h["x"], h["z"]
        if h["kind"] == "pot":
            _knob(hid, x, z, p["knob_angles"].get(hid, 0), mats)
        elif h["kind"] == "jack":
            _jack(hid, x, z, mats)
        elif h["kind"] == "toggle":
            _toggle(hid, x, z, p["toggles"].get(hid, 0), mats)
        elif h["kind"] == "button":
            _button(hid, x, z, mats)
        elif h["kind"] == "led":
            lvl = p["leds_lit"].get(hid, 0)
            m = led_mats.setdefault(lvl, C.emissive(f"led_{lvl}", "#EE5641", p["led_strength"] * lvl))
            _led(hid, x, z, m, mats)
        elif h["kind"] == "mounting":
            _screw(hid, x, z, mats)
    _pcbs(mats)

    def at(hid, dy=0.0):
        return Vector((holes[hid]["x"], dy, holes[hid]["z"]))

    return dict(
        center=Vector((0, 8.0, PH / 2)),
        radius=math.sqrt(PW ** 2 + PH ** 2 + 26 ** 2) / 2,
        size=PH,
        holes=holes,
        at=at,
        params=p,
    )
