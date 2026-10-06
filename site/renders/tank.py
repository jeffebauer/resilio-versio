"""R1 / R2: an open long spring reverb tank (Accutronics/Belton type-4 class).

Ours and generic: no brand marks. Dimensions in mm (see common.py).

    425 x 110 x 33 mm steel pan (U channel with inward lips along the long
    sides, low end walls), open top so the springs show;
    3 springs, real helices of fine wire (each one a curve with a round
    cross-section, ~600 turns), slightly different coil sizes as in a real
    tank where the springs have different delays;
    a transducer at each end: bracket plate floating on 4 rubber grommets,
    laminated core, wound coil, one small magnet per spring that the spring
    hooks onto;
    an RCA jack on each end wall (input at -X, output at +X), wired to its coil.

Coordinates: tank centred on x = y = 0, sitting on the ground (z = 0), long
axis along X, the RCA ends at +-X. Front (camera side) is -Y.

build(params) returns a dict of handy reference points for cameras.
"""
import math

import bmesh
from mathutils import Vector

import common as C
import tank_materials as TM

DEFAULTS = dict(
    length=425.0,
    width=110.0,
    height=33.0,
    sheet=0.8,          # steel thickness
    lip=5.0,            # inward lip along the long sides
    spring_z=21.0,      # spring axis height above the ground (high, so they show over the walls)
    # one entry per spring: (y position, coil radius, wire radius, pitch)
    # Stylised so the turns read at hero distance (v1 was 0.4 mm wire on a
    # 0.52 mm pitch: true to life but a smooth rod on screen).
    springs=[(-28.0, 2.3, 0.55, 2.6),
             (0.0, 2.5, 0.6, 2.8),
             (28.0, 2.3, 0.55, 2.6)],
    pts_per_turn=20,
    transducer_inset=20.0,  # transducer centre from each end wall
    finish="galv",          # chassis: "galv" (bright galvanised) or "yellow" (yellow-chromate zinc)
    wires=dict(in_=("red", "black"), out=("white", "yellow")),
    label=True,
    waveform=False,         # "waveform springs": the helix centre line traces a decaying wave
    wave=dict(amp=13.0, cycles=5.0, decay=1.6, portion=0.75),
)

FONT_PRINT = "/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf"
FONT_HAND = "/System/Library/Fonts/Supplemental/Bradley Hand Bold.ttf"


def _pan(p, mats):
    """Steel pan: floor, two long walls with inward lips, two end walls,
    corner mounting holes in the lips."""
    L, W, H, t, lip = p["length"], p["width"], p["height"], p["sheet"], p["lip"]
    z0 = 1.0  # sits on 1 mm feet
    # U channel: one closed cross-section (YZ) extruded along X -> clean solid
    prof = [(-W / 2 + lip, H), (-W / 2, H), (-W / 2, 0), (W / 2, 0), (W / 2, H), (W / 2 - lip, H),
            (W / 2 - lip, H - t), (W / 2 - t, H - t), (W / 2 - t, t), (-W / 2 + t, t),
            (-W / 2 + t, H - t), (-W / 2 + lip, H - t)]
    bm = bmesh.new()
    vs = [bm.verts.new((-L / 2, y, z + z0)) for y, z in prof]
    f = bm.faces.new(vs)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    moved = [e for e in ext["geom"] if isinstance(e, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, vec=Vector((L, 0, 0)), verts=moved)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    pan = C.mesh_obj("pan", bm)
    # end walls (lower than the sides, as on real tanks), set just inside the channel
    for s in (-1, 1):
        C.box(f"pan_end{s}", (t, W - 2 * t, H * 0.72), (s * (L / 2 - t / 2), 0, z0 + t + H * 0.36),
              mats["accent"] or mats["steel"], bevel=0.3)
    # mounting holes in the lips, 12 mm from each end
    holes = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            holes.append(C.cylinder("hole", 2.0, 4, (sx * (L / 2 - 12), sy * (W / 2 - lip / 2 - 0.3), H + 1.0), segs=24))
    C.boolean_cut(pan, holes)
    C.add_bevel(pan, 0.35)
    C.assign(pan, mats["steel"])
    # four small rubber feet so the pan sits just off the ground
    for sx in (-1, 1):
        for sy in (-1, 1):
            C.cylinder("foot", 4.0, 1.0, (sx * (L / 2 - 20), sy * (W / 2 - 14), 0.5), mats["rubber"], segs=24)
    return pan


def _transducer(p, mats, side):
    """side = -1 (input, -X) or +1 (output, +X). Returns magnet tip points."""
    L, W = p["length"], p["width"]
    xc = side * (L / 2 - p["transducer_inset"])
    zc = p["spring_z"]
    # bracket plate on 4 grommets
    plate_z = 7.0
    C.box(f"bracket{side}", (30, 84, 1.0), (xc, 0, plate_z), mats["steel"], bevel=0.3)
    for gx in (-11, 11):
        for gy in (-36, 36):
            C.torus("grommet", 2.6, 1.2, (xc + gx, gy, plate_z - 0.2), mats["rubber"])
            C.cylinder("grommet_post", 1.3, plate_z - 1.0, (xc + gx, gy, 1.0 + (plate_z - 1.0) / 2), mats["steel"], segs=16)
            C.cylinder("grommet_screw", 2.0, 1.0, (xc + gx, gy, plate_z + 1.1), mats["metal_dark"], segs=24, bevel=0.3)
    # laminated core: a stack of thin plates (the edges catch light)
    n_lam = 14
    lam_t = 0.45
    for i in range(n_lam):
        z = plate_z + 0.5 + lam_t / 2 + i * (lam_t + 0.04)
        inset = 0.15 if i % 2 else 0.0
        C.box("lamination", (24 - inset, 74 - inset, lam_t), (xc + side * 1, 0, z), mats["iron"])
    core_top = plate_z + 0.5 + n_lam * (lam_t + 0.04)
    # coil on the outboard leg of the core, axis along Y, between two bobbin cheeks
    coil_x = xc + side * 7
    C.cylinder("coil", 5.0, 30, (coil_x, 0, core_top + 5.0), mats["coil"], axis="X", segs=48)
    for fy in (-15.6, 15.6):
        C.box("bobbin_cheek", (12, 1.2, 12), (coil_x, fy, core_top + 5.0), mats["bobbin"], bevel=0.4)
    # magnets: one per spring, on pole pieces on the inboard edge of the core
    tips = []
    for (y, *_rest) in p["springs"]:
        pole_x = xc - side * 7
        C.box("pole", (5, 6, zc + 2 - core_top), (pole_x, y, (core_top + zc + 2) / 2), mats["iron"], bevel=0.3)
        mag_x = pole_x - side * 5.5
        C.cylinder("magnet", 1.4, 6, (mag_x, y, zc), mats["magnet"], axis="Y", segs=24, bevel=0.2)
        tips.append(Vector((mag_x - side * 3.0, y, zc)))
    # solder tags, coloured hook-up wire to the RCA jack, solder blobs, a cable tie
    rca = Vector((side * (L / 2 + 0.4), -30.0, 15.0))
    colours = p["wires"]["in_" if side < 0 else "out"]
    ends = []
    for k, dy in enumerate((-2.0, 2.0)):
        tag = Vector((coil_x + side * 2, -20 + dy, core_top + 2))
        C.box("tag", (2.5, 1.2, 5), tag, mats["metal_light"])
        C.sphere("solder", 1.0, tag + Vector((0, 0, 2.2)), mats["solder"])
        end = rca + Vector((-side * 4, dy * 0.5, -3 + 3 * k))
        slack = 9.0 if (k == 1 and side > 0) else 4.0   # one wire with a little slack
        mid = (tag + end) / 2 + Vector((0, -2 * k, slack))
        C.curve_obj(f"wire_{colours[k]}", [tag + Vector((0, 0, 2.6)), mid, end], 0.5,
                    TM.wire(colours[k]), resolution=3, spline="NURBS")
        C.sphere("solder", 1.1, end, mats["solder"])
        ends.append((tag, mid, end))
    # cable tie round the pair, a third of the way from the tags
    a = ends[0][0].lerp(ends[0][2], 0.33) + Vector((0, 0, 3.5))
    tie = C.torus("cable_tie", 1.5, 0.3, a + Vector((0, 0.5, 0)), mats["nylon"], axis="Y", major=24, minor=6)
    tie.scale = (1.0, 1.0, 1.0)
    C.box("cable_tie_head", (1.6, 1.2, 1.4), a + Vector((0, 0.5, 1.8)), mats["nylon"], bevel=0.2)
    return tips


def _rca(p, mats, side):
    """RCA jack on the outside of an end wall: nut, shell, insulator, pin."""
    x0 = side * (p["length"] / 2)
    y, z = -30.0, 15.0
    ax = "Y"  # cylinders along X
    C.cylinder("rca_flange", 6.0, 1.2, (x0 + side * 0.6, y, z), mats["metal_light"], axis=ax, segs=6, bevel=0.2, smooth=False)
    C.cylinder("rca_nut", 4.6, 2.0, (x0 + side * 2.2, y, z), mats["metal_light"], axis=ax, segs=6, bevel=0.25, smooth=False)
    C.cylinder("rca_shell", 4.1, 8.0, (x0 + side * 7.0, y, z), mats["metal_light"], axis=ax, segs=48, bevel=0.2)
    C.cylinder("rca_insul", 3.2, 8.2, (x0 + side * 7.1, y, z), mats["phenolic"], axis=ax, segs=48)
    C.cylinder("rca_pin", 0.9, 9.0, (x0 + side * 7.4, y, z), mats["metal_light"], axis=ax, segs=24)
    # a second, ground lug tab next to it
    C.box("rca_lug", (0.6, 5, 9), (x0 - side * 0.8, y, z - 6), mats["metal_light"])


def _wave_offset(p, t):
    """Lateral (Y) offset of the spring's centre line at t in [0, 1] along it,
    for "waveform springs": a decaying sine over the first `portion`, eased
    to zero so the last quarter is a plain straight spring."""
    if not p["waveform"]:
        return 0.0
    w = p["wave"]
    u = t / w["portion"]
    if u >= 1.0:
        return 0.0
    ease = (1 - u) ** 2 * (1 + 2 * u)   # smoothstep down: zero value and slope at u = 1
    return w["amp"] * math.exp(-w["decay"] * u) * math.sin(2 * math.pi * w["cycles"] * u) * ease


def _spring(p, mats, idx, y, R, wr, pitch, a, b):
    """A helix of wire from point a to b (along X), with straight legs and
    hooks at both ends. With p["waveform"], the helix follows a curved centre
    line (sampled densely, turns spaced by arc length)."""
    x0, x1 = a.x + (8 if b.x > a.x else -8), b.x - (8 if b.x > a.x else -8)
    zc = a.z
    phase = idx * 1.3
    ns = 2000
    cl = [Vector((x0 + (x1 - x0) * i / ns, y + _wave_offset(p, i / ns), zc)) for i in range(ns + 1)]
    arc = [0.0]
    for i in range(1, ns + 1):
        arc.append(arc[-1] + (cl[i] - cl[i - 1]).length)
    total = arc[-1]
    turns = int(total / pitch)
    n = turns * p["pts_per_turn"]
    pts = []
    j = 0
    for i in range(n + 1):
        s = total * i / n
        while j < ns - 1 and arc[j + 1] < s:
            j += 1
        f = (s - arc[j]) / max(arc[j + 1] - arc[j], 1e-9)
        c = cl[j].lerp(cl[j + 1], f)
        tan = (cl[j + 1] - cl[j]).normalized()
        n1 = Vector((-tan.y, tan.x, 0.0)).normalized()   # in the floor plane
        n2 = Vector((0, 0, 1))
        ang = phase + 2 * math.pi * turns * i / n
        pts.append(tuple(c + R * (math.cos(ang) * n1 + math.sin(ang) * n2)))
    C.curve_obj(f"spring{idx}", pts, wr, mats["spring"], resolution=2)
    # legs: from the helix end, a straight run on the axis to a hook on the magnet
    for end, tip, axis_pt in ((Vector(pts[0]), a, cl[0]), (Vector(pts[-1]), b, cl[-1])):
        C.curve_obj(f"spring{idx}_leg", [end, axis_pt, tip], wr * 1.1, mats["spring"], resolution=2)
        C.torus(f"spring{idx}_hook", 1.0, wr * 1.1, tip, mats["spring"], axis="X", major=24, minor=8)
    return turns


def _spot_welds(p, mats):
    """A few resistance spot-weld marks where the end walls meet the channel."""
    L, W, H = p["length"], p["width"], p["height"]
    for s in (-1, 1):
        for z in (8.0, 20.0):
            C.cylinder("weld", 2.0, 0.1, (s * (L / 2 - 0.5), -W / 2 - 0.06, z), mats["weld"], axis="X", segs=24)
            C.cylinder("weld", 2.0, 0.1, (s * (L / 2 - 0.5), W / 2 + 0.06, z), mats["weld"], axis="X", segs=24)


def _text(body, font, size, loc, rot, mat, align="LEFT"):
    import bpy
    cu = bpy.data.curves.new("label_text", "FONT")
    cu.body = body
    try:
        cu.font = bpy.data.fonts.load(font, check_existing=True)
    except Exception:
        pass
    cu.size = size
    cu.align_x = align
    ob = bpy.data.objects.new("label_text", cu)
    C.link(ob)
    ob.location = loc
    ob.rotation_euler = (0, 0, rot)
    cu.materials.append(mat)
    return ob


def _label(p, mats):
    """A small printed paper sticker on the chassis floor, our own generic text
    plus a hand-written date and batch code. No real brand names."""
    rot = math.radians(2.5)
    cx, cy, z = -70.0, 14.0, 1.0 + p["sheet"] + 0.08  # behind the middle spring: the front wall hides y < ~-6
    C.box("label", (56, 17, 0.12), (cx, cy, z), TM.paper(), rot=(0, 0, rot))
    ink = mats["ink"]
    zt = z + 0.07
    ca, sa = math.cos(rot), math.sin(rot)

    def at(dx, dy):
        return (cx + dx * ca - dy * sa, cy + dx * sa + dy * ca, zt)
    _text("SPRING REVERB · TYPE 4", FONT_PRINT, 3.6, at(-26, 3.2), rot, ink)
    _text("IN 8Ω   OUT 2.5kΩ   DECAY LONG", FONT_PRINT, 2.4, at(-26, -0.6), rot, ink)
    _text("RESILIO", FONT_PRINT, 2.4, at(-26, -4.6), rot, ink)
    _text("07.10.26  B-17", FONT_HAND, 2.8, at(4, -5.6), rot + math.radians(-3), mats["pen"])


def _front_sticker(p, mats):
    """Painted versions get a cream sticker on the outside of the front wall,
    in the manner of classic tape-echo nameplates (our own words, no marks)."""
    L, W = p["length"], p["width"]
    y = -W / 2 - 0.08
    C.box("front_sticker", (92, 0.12, 15), (-L / 2 + 70, y, 17.5), TM.paper())
    ink = mats["ink"]

    def txt(body, font, size, dx, dz):
        o = _text(body, font, size, (-L / 2 + 70 + dx, y - 0.08, 17.5 + dz), 0.0, ink)
        o.rotation_euler = (1.5707963, 0, 0)
    txt("RESILIO  SPRING LINE", FONT_PRINT, 9.0, -43, -0.5)
    txt("TYPE 4 · 3 SPRINGS · LONG DECAY", FONT_PRINT, 4.6, -43, -6.0)


def build(params=None):
    p = dict(DEFAULTS)
    p.update(params or {})
    mats = dict(
        steel=(TM.enamel(f"paint_{p['finish']}", TM.PAINTS[p["finish"]][0]) if p["finish"] in TM.PAINTS
               else TM.zinc(p["finish"])),                         # chassis: zinc plate or enamel
        rubber=TM.solid("tank_rubber", (0.02, 0.02, 0.02), 0.7),     # black grommets, feet
        core=TM.zinc("galv" if p["finish"] in TM.PAINTS else p["finish"]),
        accent=(TM.enamel(f"accent_{p['finish']}", TM.PAINTS[p["finish"]][1], hammer=False)
                if p["finish"] in TM.PAINTS else None),
        iron=TM.iron(),                                             # laminations, pole pieces
        coil=TM.copper_coil(),                                      # copper enamel winding
        bobbin=TM.solid("tank_bobbin", (0.03, 0.03, 0.03), 0.45),    # black phenolic cheeks
        magnet=TM.solid("tank_magnet", (0.07, 0.07, 0.075), 0.5, metallic=0.4),
        spring=TM.spring_steel(),                                   # oiled spring steel, heat tint
        metal_light=TM.solid("nickel", (0.85, 0.84, 0.80), 0.12, metallic=1.0),
        metal_dark=TM.solid("tank_screw", (0.30, 0.30, 0.30), 0.35, metallic=1.0),
        solder=TM.solid("solder", (0.80, 0.80, 0.78), 0.08, metallic=1.0),
        nylon=TM.solid("nylon", (0.82, 0.80, 0.74), 0.45),
        phenolic=TM.solid("phenolic_cream", (0.72, 0.62, 0.42), 0.4, coat=0.2),
        weld=TM.solid("weld", (0.30, 0.27, 0.24), 0.6, metallic=0.6),
        ink=TM.solid("label_ink", (0.03, 0.03, 0.035), 0.6),
        pen=TM.solid("label_pen", (0.03, 0.05, 0.22), 0.5),
    )
    _pan(p, mats)
    tips_in = _transducer(p, mats, -1)
    tips_out = _transducer(p, mats, +1)
    _rca(p, mats, -1)
    _rca(p, mats, +1)
    _spot_welds(p, mats)
    if p["label"]:
        _label(p, mats)
    if p["finish"] in TM.PAINTS:
        _front_sticker(p, mats)
    turns = []
    for i, ((y, R, wr, pitch), a, b) in enumerate(zip(p["springs"], tips_in, tips_out)):
        turns.append(_spring(p, mats, i, y, R, wr, pitch, a, b))
    print(f"[tank] springs: {turns} turns")
    L, W, H = p["length"], p["width"], p["height"]
    return dict(
        center=Vector((0, 0, H / 2)),
        radius=math.sqrt(L * L + W * W + H * H) / 2,
        size=L,
        spring_mid=Vector((0, p["springs"][1][0], p["spring_z"])),
        spring_quarter=Vector((-L * 0.18, p["springs"][0][0], p["spring_z"])),
        transducer_out=Vector((L / 2 - p["transducer_inset"], 0, 12)),
        rca_out=Vector((L / 2 + 6, -30, 15)),
        streak=(Vector((-L / 2 + 40, 0, p["spring_z"])), Vector((L / 2 - 40, 0, p["spring_z"]))),
        params=p,
    )
