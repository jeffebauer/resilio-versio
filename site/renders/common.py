"""Shared studio, camera, material and render helpers for the minisite renders.

Run inside Blender (5.x). Scene units are millimetres: 1 Blender unit = 1 mm
(unit scale 0.001), so every dimension in tank.py / module.py is the real one.

Conventions used by every scene:
  * the subject's FRONT faces -Y (towards the default camera side);
  * the ground is z = 0;
  * camera azimuth is measured from the front (-Y) towards the right (+X), in
    degrees; elevation is degrees above the horizon.

Lighting follows DESIGN.md "Imagery": a big soft key top-left (as seen from
the front), a weaker fill from the right, a rim from behind, a warm-grey
seamless ground, never a busy background.
"""
import math
import os

import bpy
import bmesh
from mathutils import Vector

# ----------------------------------------------------------------- scene setup

KEY_W = 24.0  # key light, watts as if the scene were in metres, at 1 m (see watts())
GROUND_RGB = (0.46, 0.43, 0.40)  # warm grey (linear); reads as ~#b5ada5 after AgX


def reset_scene():
    """Empty scene, millimetre units."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    us = sc.unit_settings
    us.system = "METRIC"
    us.scale_length = 0.001
    us.length_unit = "MILLIMETERS"
    return sc


def setup_cycles(samples=64, width=1920, height=1080, denoise=True):
    """Cycles on the Metal GPU when there is one, else CPU. Low samples + OIDN
    denoise is plenty for clay."""
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    cy = sc.cycles
    device = "CPU"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        prefs.compute_device_type = "METAL"
        prefs.get_devices()
        gpus = [d for d in prefs.devices if d.type == "METAL"]
        for d in prefs.devices:
            d.use = d.type == "METAL"
        if gpus:
            device = "GPU"
    except Exception as e:  # no Metal: fall back quietly
        print("Metal not available:", e)
    cy.device = device
    cy.samples = samples
    cy.use_adaptive_sampling = True
    cy.adaptive_threshold = 0.02
    cy.use_denoising = denoise
    cy.denoiser = "OPENIMAGEDENOISE"
    cy.max_bounces = 6
    cy.diffuse_bounces = 3
    cy.glossy_bounces = 3
    cy.transmission_bounces = 2
    cy.caustics_reflective = False
    cy.caustics_refractive = False
    sc.render.resolution_x = width
    sc.render.resolution_y = height
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = False
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGB"
    sc.render.image_settings.compression = 40
    sc.view_settings.view_transform = "AgX"
    try:
        sc.view_settings.look = "AgX - Base Contrast"
    except TypeError:
        pass
    print(f"[common] Cycles on {device}, {samples} spp, {width}x{height}")
    return device


def world(strength=0.25, rgb=(0.52, 0.50, 0.47)):
    """Dim, warm, even world so shadows never go fully black."""
    w = bpy.data.worlds.new("World")
    bpy.context.scene.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (*rgb, 1)
    bg.inputs["Strength"].default_value = strength
    return w


# ------------------------------------------------------------------- materials

def clay(name, value=0.55, rough=0.55, rgb=None, metallic=0.0, coat=0.0, aniso=0.0):
    """Matte clay. `value` is linear grey; `rgb` overrides it."""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*(rgb or (value, value, value)), 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    if coat:
        p.inputs["Coat Weight"].default_value = coat
    if aniso:
        p.inputs["Anisotropic"].default_value = aniso
    return m


def steel(name, value=0.62, rough=0.22, aniso=0.0):
    """Bare steel: metallic, so the lights streak across it. aniso > 0 for a
    brushed finish (highlights stretch along the tangent)."""
    return clay(name, value, rough, metallic=1.0, aniso=aniso)


def emissive(name, rgb_hex="#EE5641", strength=12.0, base=0.6):
    """A lit LED: emission in the signal red over a slightly glossy body."""
    r, g, b = hex_to_linear(rgb_hex)
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (r * base, g * base, b * base, 1)
    p.inputs["Roughness"].default_value = 0.25
    p.inputs["Emission Color"].default_value = (r, g, b, 1)
    p.inputs["Emission Strength"].default_value = strength
    return m


def decal_material(name, image_path, base_value=0.06, ink_scale=0.85, rough=0.5):
    """Panel base colour with the print (RGBA PNG) laid over it by alpha.

    base_value: the panel's clay grey (the real panel is black anodised; a dark
    clay keeps the clay pass honest while the white labels still read).
    ink_scale: dims the print a touch so white ink isn't brighter than clay."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    p.inputs["Roughness"].default_value = rough
    img = bpy.data.images.load(image_path, check_existing=True)
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Cubic"
    tex.extension = "CLIP"
    uv = nt.nodes.new("ShaderNodeUVMap")
    nt.links.new(uv.outputs["UV"], tex.inputs["Vector"])
    ink = nt.nodes.new("ShaderNodeMix")
    ink.data_type = "RGBA"
    ink.inputs["B"].default_value = (0, 0, 0, 1)
    ink.inputs["Factor"].default_value = 1 - ink_scale
    nt.links.new(tex.outputs["Color"], ink.inputs["A"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (base_value, base_value, base_value, 1)
    nt.links.new(tex.outputs["Alpha"], mix.inputs["Factor"])
    nt.links.new(ink.outputs["Result"], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], p.inputs["Base Color"])
    return m


def hex_to_linear(h):
    h = h.lstrip("#")
    srgb = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb)


def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    return obj


# ------------------------------------------------------------------- geometry

def link(obj, collection=None):
    (collection or bpy.context.scene.collection).objects.link(obj)
    return obj


def mesh_obj(name, bm, mat=None, smooth=False, collection=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if smooth:
        me.shade_smooth()
        me.set_sharp_from_angle(angle=math.radians(40))  # caps stay flat
    ob = bpy.data.objects.new(name, me)
    link(ob, collection)
    if mat:
        assign(ob, mat)
    return ob


def box(name, size, loc, mat=None, bevel=0.0, rot=(0, 0, 0), collection=None):
    """Axis-aligned box of `size` (x, y, z) centred at `loc` (mm)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    ob = mesh_obj(name, bm, mat, collection=collection)
    ob.location = loc
    ob.rotation_euler = rot
    if bevel:
        add_bevel(ob, bevel)
    return ob


def cylinder(name, r, depth, loc, mat=None, axis="Z", segs=48, r2=None, bevel=0.0,
             smooth=True, collection=None):
    """Cylinder (or cone frustum when r2 is given) centred at `loc`, along `axis`."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r,
                          radius2=r if r2 is None else r2, depth=depth)
    ob = mesh_obj(name, bm, mat, smooth=smooth, collection=collection)
    ob.location = loc
    ob.rotation_euler = {"Z": (0, 0, 0), "X": (math.pi / 2, 0, 0),
                         "Y": (0, math.pi / 2, 0)}[axis]
    if bevel:
        add_bevel(ob, bevel)
    return ob


def sphere(name, r, loc, mat=None, collection=None):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=r)
    ob = mesh_obj(name, bm, mat, smooth=True, collection=collection)
    ob.location = loc
    return ob


def torus(name, R, r, loc, mat=None, axis="Z", major=48, minor=16, collection=None):
    bm = bmesh.new()
    for i in range(major):
        a = 2 * math.pi * i / major
        for j in range(minor):
            b = 2 * math.pi * j / minor
            bm.verts.new(((R + r * math.cos(b)) * math.cos(a),
                          (R + r * math.cos(b)) * math.sin(a), r * math.sin(b)))
    bm.verts.ensure_lookup_table()
    for i in range(major):
        for j in range(minor):
            a = i * minor + j
            b = ((i + 1) % major) * minor + j
            c = ((i + 1) % major) * minor + (j + 1) % minor
            d = i * minor + (j + 1) % minor
            bm.faces.new((bm.verts[a], bm.verts[b], bm.verts[c], bm.verts[d]))
    ob = mesh_obj(name, bm, mat, smooth=True, collection=collection)
    ob.location = loc
    ob.rotation_euler = {"Z": (0, 0, 0), "X": (math.pi / 2, 0, 0),
                         "Y": (0, math.pi / 2, 0)}[axis]
    return ob


def add_bevel(ob, width, segments=2):
    """Soft edges so they catch the key light (clay looks fake with razor edges)."""
    m = ob.modifiers.new("bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.harden_normals = False
    return m


def boolean_cut(target, cutters, apply=True):
    """Subtract cutter objects (joined first) from target, then delete them."""
    if not cutters:
        return target
    bpy.ops.object.select_all(action="DESELECT")
    for c in cutters:
        c.select_set(True)
    bpy.context.view_layer.objects.active = cutters[0]
    if len(cutters) > 1:
        bpy.ops.object.join()
    cutter = bpy.context.view_layer.objects.active
    mod = target.modifiers.new("cut", "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = cutter
    mod.solver = "EXACT"
    if apply:
        bpy.context.view_layer.objects.active = target
        # bevels etc. must come after the cut
        while target.modifiers[0].name != "cut":
            bpy.ops.object.modifier_move_down(modifier=target.modifiers[0].name)
        bpy.ops.object.modifier_apply(modifier="cut")
        bpy.data.objects.remove(cutter, do_unlink=True)
    else:
        cutter.hide_render = True
        cutter.hide_viewport = True
    return target


def curve_obj(name, points, bevel_r, mat=None, resolution=3, spline="POLY", collection=None):
    """A wire: a curve through `points` with a round cross-section of radius bevel_r."""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = bevel_r
    cu.bevel_resolution = resolution
    cu.use_fill_caps = True
    sp = cu.splines.new(spline)
    sp.points.add(len(points) - 1)
    for p, co in zip(sp.points, points):
        p.co = (*co, 1.0)
    if spline == "NURBS":
        sp.use_endpoint_u = True
        sp.order_u = 3
    ob = bpy.data.objects.new(name, cu)
    link(ob, collection)
    if mat:
        cu.materials.append(mat)
    return ob


# ------------------------------------------------------------------- studio

def cyclorama(width=3000, front=1500, back=900, height=1500, radius=500, rgb=GROUND_RGB,
              center=(0, 0), texture=False):
    """Seamless sweep: flat floor running into a curved back wall (+Y side).

    The floor runs from y=-front to y=back, then bends up with `radius` into a
    wall `height` tall. Matte warm grey."""
    prof = [(y, 0.0) for y in (-front, -front / 2, 0, back * 0.5, back)]
    steps = 16
    for i in range(1, steps + 1):
        a = (math.pi / 2) * i / steps
        prof.append((back + radius * math.sin(a), radius * (1 - math.cos(a))))
    prof.append((back + radius, height))
    bm = bmesh.new()
    rows = []
    for x in (-width / 2, width / 2):
        rows.append([bm.verts.new((x + center[0], y + center[1], z)) for y, z in prof])
    for i in range(len(prof) - 1):
        bm.faces.new((rows[0][i], rows[1][i], rows[1][i + 1], rows[0][i + 1]))
    ob = mesh_obj("cyclorama", bm, clay("ground", rgb=rgb, rough=0.85), smooth=True)
    if texture:
        _paper_texture(ob.data.materials[0])
    return ob


def _paper_texture(m):
    """Faint mottling + fibre in the sweep so it isn't CG-perfect (+-4 % value,
    a little roughness variation)."""
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    base = tuple(p.inputs["Base Color"].default_value)
    coord = nt.nodes.new("ShaderNodeTexCoord")
    big = nt.nodes.new("ShaderNodeTexNoise")
    big.inputs["Scale"].default_value = 0.004   # ~250 mm blotches (object space, mm)
    big.inputs["Detail"].default_value = 3
    fine = nt.nodes.new("ShaderNodeTexNoise")
    fine.inputs["Scale"].default_value = 0.6    # ~2 mm fibre
    fine.inputs["Detail"].default_value = 6
    for n in (big, fine):
        nt.links.new(coord.outputs["Object"], n.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMath")
    mix.operation = "MULTIPLY_ADD"
    nt.links.new(big.outputs["Fac"], mix.inputs[0])
    mix.inputs[1].default_value = 0.7
    nt.links.new(fine.outputs["Fac"], mix.inputs[2])
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["From Min"].default_value = 0.6
    rng.inputs["From Max"].default_value = 1.2
    rng.inputs["To Min"].default_value = 0.92
    rng.inputs["To Max"].default_value = 1.06
    nt.links.new(mix.outputs[0], rng.inputs["Value"])
    tint = nt.nodes.new("ShaderNodeMix")
    tint.data_type = "RGBA"
    tint.blend_type = "MULTIPLY"
    tint.inputs["Factor"].default_value = 1.0
    tint.inputs["A"].default_value = base
    comb = nt.nodes.new("ShaderNodeCombineColor")
    for k in range(3):
        nt.links.new(rng.outputs[0], comb.inputs[k])
    nt.links.new(comb.outputs[0], tint.inputs["B"])
    nt.links.new(tint.outputs["Result"], p.inputs["Base Color"])
    rr = nt.nodes.new("ShaderNodeMapRange")
    rr.inputs["To Min"].default_value = 0.78
    rr.inputs["To Max"].default_value = 0.92
    nt.links.new(fine.outputs["Fac"], rr.inputs["Value"])
    nt.links.new(rr.outputs[0], p.inputs["Roughness"])


def area_light(name, loc, target, size, power, rgb=(1, 1, 1), size_y=None, spread=180):
    ld = bpy.data.lights.new(name, "AREA")
    ld.energy = power
    ld.color = rgb
    if size_y:
        ld.shape = "RECTANGLE"
        ld.size = size
        ld.size_y = size_y
    else:
        ld.shape = "DISK"
        ld.size = size
    ld.spread = math.radians(spread)
    ob = bpy.data.objects.new(name, ld)
    link(ob)
    ob.location = loc
    aim(ob, target)
    return ob


def watts(w_at_metre_scale):
    """Light power in a millimetre scene. Cycles light falloff is in scene units,
    so a lamp 1000x further away (mm vs m) needs 1e6x the power for the same
    irradiance. Think in 'watts as if the scene were in metres'."""
    return w_at_metre_scale * 1e6


def studio(subject_center, subject_size, key=1.0, fill=0.25, rim=0.6, streak=None):
    """Key top-left-front, fill right-front, rim behind-above.

    subject_size: rough longest dimension (mm); light distance and size scale
    with it so tank and module share one look.
    streak: optional (start, end) points for a long strip light whose
    reflection runs along a line (the tank springs)."""
    c = Vector(subject_center)
    s = subject_size
    d = 2.2 * s  # light distance
    lights = {}
    lights["key"] = area_light(
        "key", c + Vector((-0.75, -0.55, 0.95)).normalized() * d, c,
        size=1.4 * s, power=watts(KEY_W * key) * (d / 1000) ** 2 / 1.0,
        rgb=(1.0, 0.97, 0.92))
    lights["fill"] = area_light(
        "fill", c + Vector((0.95, -0.6, 0.25)).normalized() * d * 1.1, c,
        size=1.8 * s, power=watts(KEY_W * fill) * (d / 1000) ** 2,
        rgb=(0.92, 0.95, 1.0))
    lights["rim"] = area_light(
        "rim", c + Vector((0.35, 0.95, 0.75)).normalized() * d, c,
        size=0.8 * s, power=watts(KEY_W * rim) * (d / 1000) ** 2)
    if streak:
        a, b = Vector(streak[0]), Vector(streak[1])
        mid = (a + b) / 2
        length = (b - a).length
        ob = area_light("streak", mid + Vector((0, 0.35 * s, 0.6 * s)), mid,
                        size=length * 1.1, size_y=0.03 * s,
                        power=watts(KEY_W * 2.5) * (0.7 * s / 1000) ** 2)
        lights["streak"] = ob
    return lights


# ------------------------------------------------------------------- cameras

def aim(ob, target, roll=0.0):
    direction = Vector(target) - ob.location
    ob.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    if roll:
        ob.rotation_euler.rotate_axis("Z", math.radians(roll))


def fit_distance(radius, focal_mm, fill=0.85, sensor_mm=36.0, aspect=16 / 9, by="height"):
    """Camera distance so a sphere of `radius` spans `fill` of the frame height
    (or width, for long subjects like the tank). fill > 1 crops into it."""
    sensor = sensor_mm / aspect if by == "height" else sensor_mm
    half = math.atan((sensor / 2) / focal_mm)
    return radius / math.tan(half) / fill


def camera(name, target, azimuth, elevation, focal_mm, distance, fstop=None,
           focus=None, shift=(0.0, 0.0), roll=0.0):
    """Orbit camera: `azimuth` deg from front (-Y) towards +X, `elevation` deg up.

    fstop: enables depth of field (shallow macros); focus defaults to target."""
    cd = bpy.data.cameras.new(name)
    cd.lens = focal_mm
    cd.sensor_fit = "HORIZONTAL"
    cd.sensor_width = 36.0
    cd.clip_start = 1.0
    cd.clip_end = 100000.0
    cd.shift_x, cd.shift_y = shift
    ob = bpy.data.objects.new(name, cd)
    link(ob)
    az, el = math.radians(azimuth), math.radians(elevation)
    t = Vector(target)
    ob.location = t + distance * Vector((math.sin(az) * math.cos(el),
                                         -math.cos(az) * math.cos(el),
                                         math.sin(el)))
    aim(ob, t, roll)
    if fstop:
        cd.dof.use_dof = True
        # Cycles converts focal length to an aperture in METRES and ignores the
        # unit scale, so in a mm scene the blur would be 1000x too small. Scale
        # the f-stop by the unit scale: `fstop` here is the real-world f-number.
        cd.dof.aperture_fstop = fstop * bpy.context.scene.unit_settings.scale_length
        f = Vector(focus) if focus is not None else t
        cd.dof.focus_distance = (f - ob.location).length
    return ob


def render(cam, path):
    sc = bpy.context.scene
    sc.camera = cam
    sc.render.filepath = path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.render.render(write_still=True)
    print(f"[common] wrote {path}")
