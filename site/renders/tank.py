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

DEFAULTS = dict(
    length=425.0,
    width=110.0,
    height=33.0,
    sheet=0.8,          # steel thickness
    lip=5.0,            # inward lip along the long sides
    spring_z=21.0,      # spring axis height above the ground (high, so they show over the walls)
    # one entry per spring: (y position, coil radius, wire radius, pitch)
    springs=[(-28.0, 2.15, 0.20, 0.52),
             (0.0, 2.35, 0.21, 0.55),
             (28.0, 2.15, 0.20, 0.52)],
    pts_per_turn=14,
    transducer_inset=20.0,  # transducer centre from each end wall
)


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
              mats["steel"], bevel=0.3)
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
        C.box("lamination", (24 - inset, 74 - inset, lam_t), (xc + side * 1, 0, z), mats["core"])
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
        C.box("pole", (5, 6, zc + 2 - core_top), (pole_x, y, (core_top + zc + 2) / 2), mats["core"], bevel=0.3)
        mag_x = pole_x - side * 5.5
        C.cylinder("magnet", 1.4, 6, (mag_x, y, zc), mats["magnet"], axis="Y", segs=24, bevel=0.2)
        tips.append(Vector((mag_x - side * 3.0, y, zc)))
    # solder tags + wires to the RCA jack on this end
    rca = Vector((side * (L / 2 + 0.4), -30.0, 15.0))
    for k, dy in enumerate((-2.0, 2.0)):
        tag = Vector((coil_x + side * 2, -20 + dy, core_top + 2))
        C.box("tag", (2.5, 1.2, 5), tag, mats["metal_light"])
        mid = (tag + rca) / 2 + Vector((0, 0, 6 + 3 * k))
        C.curve_obj("wire", [tag + Vector((0, 0, 2)), mid, rca + Vector((-side * 4, dy * 0.5, 0))],
                    0.45, mats["wire"], spline="NURBS")
    return tips


def _rca(p, mats, side):
    """RCA jack on the outside of an end wall: nut, shell, insulator, pin."""
    x0 = side * (p["length"] / 2)
    y, z = -30.0, 15.0
    ax = "Y"  # cylinders along X
    C.cylinder("rca_flange", 6.0, 1.2, (x0 + side * 0.6, y, z), mats["metal_light"], axis=ax, segs=6, bevel=0.2, smooth=False)
    C.cylinder("rca_nut", 4.6, 2.0, (x0 + side * 2.2, y, z), mats["metal_light"], axis=ax, segs=6, bevel=0.25, smooth=False)
    C.cylinder("rca_shell", 4.1, 8.0, (x0 + side * 7.0, y, z), mats["metal_light"], axis=ax, segs=48, bevel=0.2)
    C.cylinder("rca_insul", 3.2, 8.2, (x0 + side * 7.1, y, z), mats["rubber"], axis=ax, segs=48)
    C.cylinder("rca_pin", 0.9, 9.0, (x0 + side * 7.4, y, z), mats["metal_light"], axis=ax, segs=24)
    # a second, ground lug tab next to it
    C.box("rca_lug", (0.6, 5, 9), (x0 - side * 0.8, y, z - 6), mats["metal_light"])


def _spring(p, mats, idx, y, R, wr, pitch, a, b):
    """A real helix of wire from point a to b (along X), with straight legs and
    hooks at both ends."""
    pts = []
    x0, x1 = a.x + (8 if b.x > a.x else -8), b.x - (8 if b.x > a.x else -8)
    length = abs(x1 - x0)
    turns = int(length / pitch)
    n = turns * p["pts_per_turn"]
    zc = a.z
    phase = idx * 1.3
    for i in range(n + 1):
        t = i / n
        ang = phase + 2 * math.pi * turns * t
        pts.append((x0 + (x1 - x0) * t, y + R * math.cos(ang), zc + R * math.sin(ang)))
    C.curve_obj(f"spring{idx}", pts, wr, mats["spring"], resolution=2)
    # legs: from the helix end, a straight run on the axis to a hook on the magnet
    for end, tip in ((Vector(pts[0]), a), (Vector(pts[-1]), b)):
        on_axis = Vector((end.x, y, zc))
        C.curve_obj(f"spring{idx}_leg", [end, on_axis, tip], wr * 1.1, mats["spring"], resolution=2)
        C.torus(f"spring{idx}_hook", 1.0, wr * 1.1, tip, mats["spring"], axis="X", major=24, minor=8)
    return turns


def build(params=None):
    p = dict(DEFAULTS)
    p.update(params or {})
    mats = dict(
        steel=C.clay("tank_steel", 0.36, 0.45),
        rubber=C.clay("tank_rubber", 0.10, 0.8),
        core=C.clay("tank_core", 0.36, 0.5),
        coil=C.clay("tank_coil", 0.40, 0.4),
        bobbin=C.clay("tank_bobbin", 0.22, 0.6),
        magnet=C.clay("tank_magnet", 0.30, 0.35),
        spring=C.clay("tank_spring", 0.62, 0.22),
        metal_light=C.clay("tank_metal_light", 0.65, 0.35),
        metal_dark=C.clay("tank_metal_dark", 0.25, 0.4),
        wire=C.clay("tank_wire", 0.15, 0.6),
    )
    _pan(p, mats)
    tips_in = _transducer(p, mats, -1)
    tips_out = _transducer(p, mats, +1)
    _rca(p, mats, -1)
    _rca(p, mats, +1)
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
