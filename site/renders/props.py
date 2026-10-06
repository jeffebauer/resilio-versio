"""Props for the module's hero shots: magnetic tape, a reel, a cassette, patch
cables. Organic counterpoints to the hardware. No brand names anywhere.

Units mm, module coordinates (see module.py): panel front at y = 0 facing -Y,
centred on x = 0, standing on the floor z = 0.
"""
import math

import bmesh
import bpy
from mathutils import Vector

import common as C

TAPE_W = 6.35  # quarter-inch reel tape


# ------------------------------------------------------------------ materials

def tape_material():
    """Oxide side glossy brown-black, base side matte grey-black."""
    m = bpy.data.materials.get("tape")
    if m:
        return m
    m = bpy.data.materials.new("tape")
    m.use_nodes = True
    nt = m.node_tree
    front = nt.nodes["Principled BSDF"]
    front.inputs["Base Color"].default_value = (0.075, 0.040, 0.022, 1)  # brown-black oxide
    front.inputs["Roughness"].default_value = 0.16
    front.inputs["Coat Weight"].default_value = 0.4
    back = nt.nodes.new("ShaderNodeBsdfPrincipled")
    back.inputs["Base Color"].default_value = (0.03, 0.03, 0.03, 1)
    back.inputs["Roughness"].default_value = 0.75
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(geo.outputs["Backfacing"], mix.inputs["Fac"])
    nt.links.new(front.outputs[0], mix.inputs[1])
    nt.links.new(back.outputs[0], mix.inputs[2])
    out = nt.nodes["Material Output"]
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def _mat(name, rgb, rough, metallic=0.0, coat=0.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    return C.clay(name, rgb=rgb, rough=rough, metallic=metallic, coat=coat)


# ------------------------------------------------------------------ tape ribbon

def tape(name, pts, tilts=None, width=TAPE_W):
    """A flat ribbon of tape through control points (NURBS). tilts: per-point
    twist in radians; pi/2 lies the ribbon flat when the path is horizontal."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.extrude = width / 2
    cu.twist_mode = "Z_UP"
    cu.resolution_u = 24
    sp = cu.splines.new("NURBS")
    sp.points.add(len(pts) - 1)
    for i, (p, co) in enumerate(zip(sp.points, pts)):
        p.co = (*co, 1.0)
        p.tilt = (tilts[i] if tilts else math.pi / 2)
    sp.use_endpoint_u = True
    sp.order_u = 4
    ob = bpy.data.objects.new(name, cu)
    C.link(ob)
    cu.materials.append(tape_material())
    return ob


def _flat(z=0.15):
    return math.pi / 2, z


def tape_spill():
    """Unspooled tape spilling across the floor and looping round the base,
    with a few soft curls standing up and a couple of twists."""
    F = math.pi / 2
    pts = [(-420, -260, 0.2), (-260, -190, 0.2), (-150, -120, 0.3), (-70, -60, 0.2),
           (-48, -20, 0.3), (-40, 10, 3.0), (-34, 40, 0.3), (0, 52, 0.4), (34, 40, 0.3),
           (44, 8, 2.5), (40, -18, 0.3), (14, -34, 6), (-6, -30, 14), (-12, -44, 6),   # a curl
           (2, -60, 0.3), (60, -80, 0.2), (110, -60, 9), (130, -95, 0.3), (90, -140, 0.2),
           (30, -150, 5), (-10, -190, 0.2), (40, -260, 0.2), (220, -330, 0.2)]
    tilts = [F, F, F + 0.6, F, F, F + 0.3, F, F, F, F - 0.5, F, F + 1.2, F + 2.2, F + 2.9,
             F + math.pi, F + math.pi, F + math.pi + 0.8, F + math.pi, F + math.pi,
             F + math.pi - 0.7, F + math.pi, F + math.pi, F + math.pi]
    tape("tape_spill", pts, tilts)
    # a second, shorter strand with a loop lying on the floor
    pts2 = [(-160, -40, 0.2), (-120, -10, 0.2), (-90, -40, 0.3), (-110, -70, 4), (-140, -55, 8),
            (-130, -25, 4), (-100, -20, 0.3), (-60, -110, 0.2), (-90, -230, 0.2)]
    tape("tape_spill2", pts2, [F, F, F, F + 0.9, F + 1.6, F + 2.4, F + math.pi, F + math.pi, F + math.pi])


def reel(center, radius=89.0, pack_r=60.0, hub_r=28.0, axis_angle=0.0):
    """A 7-inch reel standing upright: two aluminium flanges with three
    windows, a hub, a partly wound tape pack. axis along X rotated by
    axis_angle (radians, about Z). Returns the pack's tangent point at the
    bottom-front (where a strand leaves)."""
    alu = _mat("reel_alu", (0.78, 0.78, 0.80), 0.42, metallic=1.0)  # satin: mirror-bright flanges read as blobs
    hubm = _mat("reel_hub", (0.05, 0.05, 0.05), 0.4)
    gap = TAPE_W + 1.6
    objs = []
    for s in (-1, 1):
        fl = C.cylinder("flange", radius, 1.2, (s * gap / 2, 0, 0), alu, axis="Y", segs=128, bevel=0.3)
        cut = []
        for k in range(3):
            a = 2 * math.pi * k / 3 + 0.4
            c = C.cylinder("window", 17, 10, (0, (hub_r + 32) * math.cos(a), (hub_r + 32) * math.sin(a)),
                           axis="Y", segs=48)
            cut.append(c)
        cut.append(C.cylinder("bore", 4.0, 10, (0, 0, 0), axis="Y", segs=24))
        C.boolean_cut(fl, cut)
        objs.append(fl)
    objs.append(C.cylinder("hub", hub_r, gap, (0, 0, 0), hubm, axis="Y", segs=96))
    pack = C.cylinder("pack", pack_r, TAPE_W, (0, 0, 0), tape_material(), axis="Y", segs=128)
    objs.append(pack)
    root = bpy.data.objects.new("reel", None)
    C.link(root)
    for o in objs:
        o.parent = root
    root.location = center
    root.rotation_euler = (0, 0, axis_angle)
    # pack's bottom point, in world space
    return Vector(center) + Vector((0, 0, -pack_r))


def cassette(center, rot=0.0):
    """A compact cassette lying on the floor (100.4 x 63.8 x 12 mm): smoky
    shell, plain cream label (no brand), window, two hubs. Returns the point
    on the front edge where pulled-out tape leaves."""
    shell = _mat("cass_shell", (0.04, 0.04, 0.045), 0.25, coat=0.3)
    label = _mat("cass_label", (0.76, 0.70, 0.58), 0.8)
    dark = _mat("cass_window", (0.01, 0.01, 0.01), 0.05)
    root = bpy.data.objects.new("cassette", None)
    C.link(root)
    parts = [C.box("cass_body", (100.4, 63.8, 12.0), (0, 0, 6.0), shell, bevel=0.8),
             C.box("cass_label", (82, 40, 0.2), (0, 6, 12.1), label),
             C.box("cass_window", (40, 13, 0.25), (0, 4, 12.15), dark),
             C.box("cass_head_area", (60, 8, 0.2), (0, -27, 12.05), _mat("cass_head", (0.02, 0.02, 0.02), 0.6))]
    for x in (-21, 21):
        parts.append(C.cylinder("cass_hub", 4.0, 0.4, (x, 4, 12.3), _mat("cass_hubm", (0.75, 0.75, 0.72), 0.3), segs=32))
    for o in parts:
        o.parent = root
    root.location = center
    root.rotation_euler = (0, 0, rot)
    ca, sa = math.cos(rot), math.sin(rot)
    return Vector(center) + Vector((0 * ca - (-31.9) * sa, 0 * sa + (-31.9) * ca, 3.0))


def cassette_with_loops(center=(70, -95, 0), rot=math.radians(-18)):
    exit_pt = cassette(center, rot)
    F = math.pi / 2
    e = exit_pt
    pts = [tuple(e), (e.x - 6, e.y - 12, 1.0), (e.x - 30, e.y - 25, 0.3), (e.x - 55, e.y - 5, 6),
           (e.x - 40, e.y + 18, 12), (e.x - 20, e.y + 2, 5), (e.x - 35, e.y - 30, 0.3),
           (e.x - 90, e.y - 45, 0.3), (e.x - 120, e.y - 10, 7), (e.x - 95, e.y + 15, 3),
           (e.x - 80, e.y - 20, 0.3), (e.x - 150, e.y - 110, 0.2), (e.x - 260, e.y - 200, 0.2)]
    tilts = [0.2, F * 0.7, F, F + 0.8, F + 1.6, F + 2.4, F + math.pi, F + math.pi, F + math.pi + 0.9,
             F + math.pi + 1.8, F + 2 * math.pi, F + 2 * math.pi, F + 2 * math.pi]
    tape("tape_cassette", pts, tilts)


def tape_reel_strand(reel_center=(95, 330, 89)):
    """Reel out of focus behind the module, one strand running from its pack
    down to the floor and forward past the module into the foreground."""
    bottom = reel(reel_center, axis_angle=math.radians(70))  # flange turned mostly towards the camera
    F = math.pi / 2
    b = bottom
    pts = [(b.x + 2, b.y - 6, b.z + 2), (b.x - 5, b.y - 40, 10), (b.x - 30, b.y - 160, 0.3),
           (70, 140, 0.3), (60, 30, 0.3), (48, -30, 3), (40, -80, 0.3), (-20, -150, 0.3),
           (-140, -260, 0.2), (-320, -380, 0.2)]
    tilts = [0.0, 0.6, F, F, F, F + 0.7, F + math.pi, F + math.pi, F + math.pi, F + math.pi]
    tape("tape_reel_strand", pts, tilts)


def tape_drape():
    """Tape draped over the module's top edge like a cable thrown over it:
    hangs down the back, comes over the top-right corner, falls past the
    right edge and pools on the floor."""
    F = math.pi / 2
    pts = [(10, 160, 0.2), (12, 60, 0.3), (14, 30, 40), (16, 16, 110), (17, 6, 131.5),
           (20, -2, 131), (26, -6, 120), (30, -8, 80), (31, -12, 30), (40, -30, 3),
           (70, -50, 0.3), (95, -30, 6), (80, -10, 0.3), (60, -70, 0.2), (120, -160, 0.2), (300, -260, 0.2)]
    tilts = [F, F, 0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 0.6, F, F + 0.8, F + 1.8, F + math.pi,
             F + math.pi, F + math.pi, F + math.pi]
    tape("tape_drape", pts, tilts)


# ------------------------------------------------------------------ patch cables

def patch_cable(ref, hid, colour, route):
    """A 3.5 mm TS cable plugged into jack `hid`: nickel collar, moulded plug
    in `colour`, a cord that leaves the plug forward, sags to the floor and
    follows `route` (list of floor points) out of frame."""
    h = ref["holes"][hid]
    x, z = h["x"], h["z"]
    nickel = bpy.data.materials.get("mod_nickel") or _mat("mod_nickel", (0.88, 0.88, 0.88), 0.12, 1.0)
    rgb = dict(black=(0.02, 0.02, 0.02), red=(0.50, 0.025, 0.02), cream=(0.75, 0.70, 0.58),
               grey=(0.25, 0.25, 0.26))[colour]
    body = _mat(f"cable_{colour}", rgb, 0.42)
    C.cylinder("plug_tip", 1.75, 6.0, (x, 1.0, z), nickel, axis="X", segs=32)
    C.cylinder("plug_collar", 2.6, 2.0, (x, -3.0, z), nickel, axis="X", segs=48, bevel=0.3)
    C.cylinder("plug_body", 3.4, 16.0, (x, -12.0, z), body, axis="X", segs=48, r2=3.0, bevel=0.8)
    C.cylinder("plug_relief", 2.2, 8.0, (x, -24.0, z), body, axis="X", segs=32, r2=1.8, bevel=0.4)
    pts = [(x, -27.5, z), (x, -36, z - 1.5)] + [tuple(p) for p in route]
    C.curve_obj(f"cord_{hid}", pts, 1.6, body, resolution=4, spline="NURBS")


def cables_all(ref):
    """IN L, IN R, OUT L, OUT R (J9-J12) patched: black/red for each L/R pair.
    Inputs leave to the left, outputs to the right; cords sag to the floor in
    front of the module and never cross the knobs."""
    def x(h):
        return ref["holes"][h]["x"]
    z = ref["holes"]["J9"]["z"]
    patch_cable(ref, "J9", "black", [(x("J9") - 2, -55, z - 8), (x("J9") - 14, -85, 1.6),
                                      (-80, -120, 1.6), (-260, -170, 1.6), (-600, -260, 1.6)])
    patch_cable(ref, "J10", "red", [(x("J10") - 1, -58, z - 9), (x("J10") - 10, -100, 1.6),
                                     (-60, -160, 1.6), (-220, -260, 1.6), (-560, -420, 1.6)])
    patch_cable(ref, "J11", "black", [(x("J11") + 1, -58, z - 9), (x("J11") + 10, -105, 1.6),
                                       (60, -165, 1.6), (220, -250, 1.6), (560, -400, 1.6)])
    patch_cable(ref, "J12", "red", [(x("J12") + 2, -55, z - 8), (x("J12") + 14, -88, 1.6),
                                       (85, -118, 1.6), (260, -165, 1.6), (600, -250, 1.6)])


# ------------------------------------------------------------------ ribbon v2
# Real tape is a thin ribbon: it bends about its width (it can't bend in its
# own plane), stands on edge in curls, twists gently along its length and now
# and then lies flat. v1 used a curve with a flat extrude and per-point tilt,
# which mostly rotated a flat strip on the floor. v2 sweeps the ribbon
# explicitly: a smooth centre line plus a roll angle theta per point
# (0 = lying flat, 90 = standing on its edge), and the centre line is lifted
# so the lower edge always rests on the floor.

def _catmull(points, samples=18):
    """Catmull-Rom through points (tuples of any length), endpoints clamped."""
    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        for k in range(samples):
            t = k / samples
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2
                                    + (-a + 3 * b - 3 * c + d) * t3)
                             for a, b, c, d in zip(p0, p1, p2, p3)))
    out.append(tuple(points[-1]))
    return out


def ribbon(name, keys, width=TAPE_W, samples=18):
    """keys: (x, y, lift, theta_deg) control points. lift raises the centre
    line (for loops that rise off the floor); theta rolls the ribbon about its
    path. Returns the object."""
    dense = _catmull(keys, samples)
    cl = []
    for x, y, lift, th in dense:
        t = math.radians(th)
        cl.append((Vector((x, y, lift + 0.5 * width * abs(math.sin(t)) + 0.06)), t))
    bm = bmesh.new()
    rows = []
    side_prev = Vector((0, 1, 0))
    for i, (c, t) in enumerate(cl):
        a = cl[max(i - 1, 0)][0]
        b = cl[min(i + 1, len(cl) - 1)][0]
        tan = (b - a).normalized()
        side = Vector((0, 0, 1)).cross(tan)
        if side.length < 0.25:          # near-vertical: carry the last side over
            side = side_prev - tan * side_prev.dot(tan)
        side.normalize()
        if side.dot(side_prev) < 0:
            side = -side
        side_prev = side
        up = tan.cross(side).normalized()
        w = math.cos(t) * side + math.sin(t) * up
        rows.append((bm.verts.new(c - w * width / 2), bm.verts.new(c + w * width / 2)))
    for (a0, a1), (b0, b1) in zip(rows, rows[1:]):
        bm.faces.new((a0, b0, b1, a1))
    ob = C.mesh_obj(name, bm, tape_material(), smooth=True)
    ob.data.set_sharp_from_angle(angle=math.radians(80))
    return ob


def tape_spill_v2():
    """Unspooled tape spilling across the floor and round the base: flat
    runs, on-edge curls, a twist, one loop rising off the floor."""
    K = [(-430, -270, 0, 0), (-300, -215, 0, 10), (-200, -160, 0, 70), (-140, -120, 0, 90),
         # an on-edge curl
         (-95, -95, 0, 90), (-70, -115, 0, 90), (-85, -140, 0, 90), (-112, -128, 0, 88),
         (-105, -98, 0, 80), (-70, -70, 0, 45),
         # flat round the left of the base, twisting up on edge behind it
         (-48, -30, 0, 0), (-42, 10, 0, 0), (-30, 42, 0, 30), (0, 56, 0, 90), (32, 46, 0, 90),
         (46, 14, 0, 70), (40, -16, 0, 20), (22, -34, 0, 0),
         # a loop rising off the floor (bending about the width)
         (8, -44, 1, 0), (0, -58, 8, 0), (4, -66, 18, 0), (14, -60, 24, 0), (20, -50, 18, 0),
         (16, -48, 8, 0), (18, -66, 1, 0),
         # out to the right, another curl on edge, then flat to the foreground
         (50, -86, 0, 30), (95, -96, 0, 90), (126, -72, 0, 90), (112, -46, 0, 90), (86, -58, 0, 90),
         (82, -88, 0, 60), (70, -140, 0, 15), (40, -200, 0, 0), (70, -265, 0, 35),
         (160, -310, 0, 0), (300, -360, 0, 0)]
    ribbon("tape_spill", K)
    K2 = [(-170, -30, 0, 0), (-128, -6, 0, 20), (-98, -26, 0, 90), (-104, -52, 0, 90),
          (-130, -58, 0, 90), (-140, -36, 0, 80), (-118, -14, 0, 40), (-80, -70, 0, 0),
          (-96, -150, 0, 20), (-120, -240, 0, 0)]
    ribbon("tape_spill2", K2)


def tape_reel_strand_v2(reel_center=(-120, 300, 89)):
    """The reel behind, lit, and one strand from its pack: hanging down to the
    floor, then running forward past the module with twists and curls."""
    bottom = reel(reel_center, axis_angle=math.radians(98))  # flanges square to the camera
    b = bottom
    K = [(b.x, b.y - 2, b.z - 0.5, 0), (b.x - 2, b.y - 14, b.z - 30, 10),
         (b.x - 6, b.y - 30, 6, 40), (b.x - 12, b.y - 60, 0, 60), (b.x - 30, b.y - 110, 0, 90),
         (-80, 150, 0, 90), (-62, 90, 0, 70), (-56, 30, 0, 10), (-50, -20, 0, 0),
         (-40, -55, 0, 60), (-58, -82, 0, 90), (-86, -68, 0, 90), (-80, -40, 0, 90), (-54, -60, 0, 50),
         (-20, -110, 0, 0), (40, -170, 0, 30), (140, -250, 0, 0), (320, -370, 0, 0)]
    ribbon("tape_reel_strand", K)
