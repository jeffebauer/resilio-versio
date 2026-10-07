"""King Tubby's MCI console (mid-1960s, 12 in / 4 out) with the "Big Knob":
the Altec 9069-B high-pass filter mounted top right of the control plate.

Modelled from MoPOP's near-top-down museum photo (the primary reference) and,
for the Big Knob's dial and shape, a standalone Altec 9068-B / 9069-B unit.
Reference photos are copyrighted: they are never copied into the repo; this
file only carries measurements taken from them.

How the layout was measured
  The aluminium plate's four corners in the photo define a homography to the
  plate rectangle, so every control's position across and up the plate is a
  measured fraction. Two numbers are assumptions:
    * plate width 950 mm (channel pitch then comes out at 44.1 mm = 1.74 in;
      the Big Knob's skirt at 47 mm, matching the Altec panel's proportions);
    * plate depth 630 mm (from the ellipse aspect of round parts in the photo:
      fader caps, screws, the ring and the lamps gave 0.60-0.76 x the width).
  Knob centres were corrected ~8 mm down the plate for their height (the photo
  looks down from the front, so tall parts lean up the image).

Coordinates: mm, desk centred on x = 0, front faces -Y, floor z = 0.
Plate-frame coordinates (u across from the plate's left edge, v up the slope
from its front edge, h above its face) are what every plate part uses; an
Empty carries the 14 deg slope. build() returns reference points for cameras.
"""
import math
import random

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

import common as C

# ------------------------------------------------------------------ dimensions
W, D = 950.0, 630.0            # aluminium plate (assumed width, measured ratio)
SLOPE = 14.0                   # plate slope, deg (guess: no side view)
P0 = Vector((-475.0, -230.0, 250.0))   # plate front-left corner (world)
SURR = dict(side=45.0, front=55.0, back=45.0)   # black surround round the plate
CAB_W = W + 2 * SURR["side"]   # 1040: matches the bridge width in the photo
BRIDGE_H, BRIDGE_TILT, BRIDGE_DEPTH = 260.0, 8.0, 200.0

PITCH = 44.14                  # channel pitch (measured, given W)
CH_U = [60.0 + PITCH * i for i in range(12)]

# rows (v, mm up the plate), measured
V_CHNUM = 618.0
V_BTN_TOP, V_BTN_PITCH = 592.0, 14.8      # 8 push-buttons per channel
V_KNOB_A = 436.0
V_EQ = (380.0, 340.0, 299.0)
V_KNOB_B = 243.0
V_SCREW = 206.0
V_SLOT_TOP, V_SLOT_BOT = 190.0, 46.0
V_CAP = 58.0
V_CAPSCREW, V_FADNUM = 39.0, 21.0

BIG = dict(u=830.0, v=508.0)  # the Big Knob
BIG_STEPS = ["OFF", "70", "100", "150", "200", "500", "1K", "2K", "3K", "5K", "7.5K"]
BIG_POINTER_STEP = 4          # "200": the photo's pointer is at about 11-12 o'clock

# EQ positions: rows top -> bottom, channels 1..12. In the photo channels
# 7-12 (and ch 4 top) show bare pot stems where knobs are missing; the owner
# asked for the full grid restored with the same red skirted knobs.
EQ = [
    "RRRRRRRRRRRR",
    "RRRRRRRRRRRR",
    "RRRRRRRRRRRR",
]
BTN_LABELS = ["1", "2", "3", "4", "EKO 1", "EKO 2", "LINE", "CUE"]

INK = "#e4dccb"


# ------------------------------------------------------------------ materials

def _principled(name, rgb, rough=0.5, metallic=0.0, coat=0.0, spec=0.5):
    m = C.clay(name, rgb=rgb, rough=rough, metallic=metallic, coat=coat)
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Specular IOR Level"].default_value = spec
    return m


def _bump_noise(m, scale, strength, distance=0.05, detail=6):
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = detail
    nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = strength
    b.inputs["Distance"].default_value = distance
    nt.links.new(n.outputs["Fac"], b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], p.inputs["Normal"])
    return m


def _weather(m, ao_dist=1.6, ao_dark=0.3, scale=0.25, amt=0.3, rough_amt=0.15):
    """Age a material: grime in the crevices (ambient occlusion darkens the
    flutes and edges), blotchy fading, uneven sheen."""
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    base = tuple(p.inputs["Base Color"].default_value)
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    ao.inputs["Distance"].default_value = ao_dist
    ao.samples = 8
    aor = nt.nodes.new("ShaderNodeMapRange")
    aor.inputs["To Min"].default_value = ao_dark
    nt.links.new(ao.outputs["AO"], aor.inputs["Value"])
    tc = nt.nodes.new("ShaderNodeTexCoord")
    n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = 6
    nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
    nr = nt.nodes.new("ShaderNodeMapRange")
    nr.inputs["To Min"].default_value = 1 - amt
    nr.inputs["To Max"].default_value = 1 + amt * 0.5
    nt.links.new(n.outputs["Fac"], nr.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMath")
    mul.operation = "MULTIPLY"
    nt.links.new(aor.outputs[0], mul.inputs[0])
    nt.links.new(nr.outputs[0], mul.inputs[1])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    mix.inputs["A"].default_value = base
    comb = nt.nodes.new("ShaderNodeCombineColor")
    for k in range(3):
        nt.links.new(mul.outputs[0], comb.inputs[k])
    nt.links.new(comb.outputs[0], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], p.inputs["Base Color"])
    r0 = p.inputs["Roughness"].default_value
    rr = nt.nodes.new("ShaderNodeMapRange")
    rr.inputs["To Min"].default_value = r0 - rough_amt * 0.3
    rr.inputs["To Max"].default_value = r0 + rough_amt
    nt.links.new(n.outputs["Fac"], rr.inputs["Value"])
    nt.links.new(rr.outputs[0], p.inputs["Roughness"])
    return m


def _worn_ink(m, scale=1.2, lo=0.45):
    """Old silkscreen: the print is patchy, thinner in places."""
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    base = tuple(p.inputs["Base Color"].default_value)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = 8
    nt.links.new(tc.outputs["Object"], n.inputs["Vector"])
    nr = nt.nodes.new("ShaderNodeMapRange")
    nr.inputs["From Min"].default_value = 0.35
    nr.inputs["From Max"].default_value = 0.6
    nr.inputs["To Min"].default_value = lo
    nr.inputs["To Max"].default_value = 1.0
    nt.links.new(n.outputs["Fac"], nr.inputs["Value"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    mix.inputs["A"].default_value = base
    comb = nt.nodes.new("ShaderNodeCombineColor")
    for k in range(3):
        nt.links.new(nr.outputs[0], comb.inputs[k])
    nt.links.new(comb.outputs[0], mix.inputs["B"])
    nt.links.new(mix.outputs["Result"], p.inputs["Base Color"])
    return m


def materials():
    lin = C.hex_to_linear
    M = {}
    M["red"] = _principled("knob_red", lin("#86392a"), rough=0.7, spec=0.3)
    M["red_skirt"] = _principled("knob_red_skirt", lin("#8c3e2a"), rough=0.7, spec=0.3)
    M["black_knob"] = _principled("knob_black", (0.016, 0.0155, 0.015), rough=0.62, spec=0.3)
    M["cap_grey"] = _principled("cap_grey", lin("#6c6a64"), rough=0.58, metallic=0.4)
    M["cap_light"] = _principled("cap_light", lin("#73716a"), rough=0.52, metallic=0.5)
    M["cap_silver"] = C.steel("cap_silver", 0.75, 0.18, aniso=0.5)
    M["dot"] = _principled("dot", (0.015, 0.015, 0.015), rough=0.6)
    M["ink"] = _principled("ink", lin(INK), rough=0.55)
    M["ink_faint"] = _principled("ink_faint", lin("#cfc8b8"), rough=0.55)
    M["chrome"] = C.steel("chrome", 0.82, 0.12)
    M["nickel"] = C.steel("nickel", 0.55, 0.3)
    M["brass"] = _principled("screw_brass", lin("#8f8466"), rough=0.35, metallic=1.0)
    M["slot"] = _principled("slot", (0.006, 0.006, 0.006), rough=0.7)
    M["key"] = _principled("keycap", (0.014, 0.014, 0.014), rough=0.45)
    M["fader_red"] = _principled("fader_red", lin("#8e402d"), rough=0.68, spec=0.3)
    M["fader_red_top"] = _principled("fader_red_top", lin("#a8604f"), rough=0.6)
    M["cabinet"] = _bump_noise(_principled("cabinet", (0.0055, 0.0053, 0.005), rough=0.7, spec=0.3), 0.9, 0.03)
    M["bridge"] = _bump_noise(_principled("bridge", (0.0055, 0.0053, 0.005), rough=0.75, spec=0.3), 1.2, 0.1)
    M["interior"] = _principled("interior", (0.015, 0.014, 0.013), rough=0.8)
    M["bezel"] = _principled("bezel", (0.02, 0.02, 0.02), rough=0.45)
    M["vu_face"] = _vu_face("vu_face", lin("#f2d6b6"))
    M["vu_red"] = _principled("vu_red", lin("#c0503c"), rough=0.6)
    M["vu_ink"] = _principled("vu_ink", lin("#3a302a"), rough=0.6)
    M["needle"] = _principled("needle", (0.02, 0.02, 0.02), rough=0.5)
    M["glass"] = _glass("vu_glass")
    M["vinyl"] = _vinyl("armrest_vinyl")
    M["foam"] = _bump_noise(_principled("armrest_foam", lin("#a5823f"), rough=0.75), 2.5, 0.4)
    M["housing"] = _principled("fader_housing", (0.02, 0.02, 0.02), rough=0.4)
    M["rail"] = _rail_glass("fader_rail")
    M["terminal"] = _principled("terminal", lin("#6b5a44"), rough=0.6)
    M["alu_bar"] = C.steel("alu_bar", 0.6, 0.35)
    for nm, hx in (("lamp_red", "#c0392b"), ("lamp_green", "#2e8b57"),
                   ("lamp_blue", "#3a6fc4"), ("lamp_white", "#e6e6e0"), ("lamp_amber", "#c8a050")):
        M[nm] = _jewel(nm, lin(hx))
    for nm, hx in (("mf_red", "#c0392b"), ("mf_blue", "#4a7bc8"), ("mf_green", "#4f9a3c"),
                   ("mf_white", "#e8e6df")):
        M[nm] = _principled(nm, lin(hx), rough=0.35, coat=0.3)
    M["label_plate"] = _principled("label_plate", (0.015, 0.015, 0.015), rough=0.35)
    for k in ("red", "red_skirt", "black_knob", "cap_grey", "cap_light", "fader_red", "key", "housing",
              "fader_red_top"):
        _weather(M[k])
    for k in ("ink", "ink_faint"):
        _worn_ink(M[k])
    return M


def _vu_face(name, rgb, strength=2.4):
    """Peach meter card, backlit: warm emission brightest near the top
    centre (the lamps sit behind the top edge), falling off softly."""
    m = _principled(name, rgb, rough=0.75)
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    # POINT mapping: out = in * scale + location; centre at y = +18 mm (top centre)
    mp.inputs["Location"].default_value = (0, -0.3, 0)
    mp.inputs["Scale"].default_value = (1 / 85.0, 1 / 60.0, 1)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    gr = nt.nodes.new("ShaderNodeTexGradient")
    gr.gradient_type = "SPHERICAL"
    nt.links.new(mp.outputs["Vector"], gr.inputs["Vector"])
    st = nt.nodes.new("ShaderNodeMath")
    st.operation = "MULTIPLY"
    st.inputs[1].default_value = strength
    nt.links.new(gr.outputs["Fac"], st.inputs[0])
    p.inputs["Emission Color"].default_value = (1.0, 0.62, 0.30, 1)
    nt.links.new(st.outputs[0], p.inputs["Emission Strength"])
    return m


def _glass(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    out = nt.nodes["Material Output"]
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    gl = nt.nodes.new("ShaderNodeBsdfGlossy")
    gl.inputs["Roughness"].default_value = 0.03
    fr = nt.nodes.new("ShaderNodeFresnel")
    fr.inputs["IOR"].default_value = 1.5
    mix = nt.nodes.new("ShaderNodeMixShader")
    dim = nt.nodes.new("ShaderNodeMath")          # old, slightly hazy glass: not a mirror
    dim.operation = "MULTIPLY"
    dim.inputs[1].default_value = 0.6
    nt.links.new(fr.outputs[0], dim.inputs[0])
    nt.links.new(dim.outputs[0], mix.inputs[0])
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(gl.outputs[0], mix.inputs[2])
    em = nt.nodes.new("ShaderNodeEmission")      # faint warm glow in the glass
    em.inputs["Color"].default_value = (1.0, 0.6, 0.3, 1)
    em.inputs["Strength"].default_value = 0.025
    add = nt.nodes.new("ShaderNodeAddShader")
    nt.links.new(mix.outputs[0], add.inputs[0])
    nt.links.new(em.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], out.inputs["Surface"])
    return m


def _rail_glass(name):
    """The master faders' clear plastic channels: smoky, glossy."""
    m = _principled(name, (0.35, 0.36, 0.36), rough=0.15)
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Transmission Weight"].default_value = 0.6
    return m


def _jewel(name, rgb):
    m = _principled(name, rgb, rough=0.12)
    p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Transmission Weight"].default_value = 0.5
    p.inputs["Emission Color"].default_value = (*rgb, 1)
    p.inputs["Emission Strength"].default_value = 0.0
    return m


def _vinyl(name):
    """Black padded vinyl: a soft sheen, fine grain, crinkles."""
    m = _principled(name, (0.011, 0.0105, 0.01), rough=0.58, spec=0.35)
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    grain = nt.nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 2.2
    grain.inputs["Detail"].default_value = 8
    wav = nt.nodes.new("ShaderNodeTexNoise")       # creases: stretched along z
    wav.inputs["Scale"].default_value = 0.05
    wav.inputs["Detail"].default_value = 4
    wav.inputs["Distortion"].default_value = 2.0
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (2.5, 1.0, 0.25)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], wav.inputs["Vector"])
    nt.links.new(tc.outputs["Object"], grain.inputs["Vector"])
    add = nt.nodes.new("ShaderNodeMath")
    add.operation = "MULTIPLY_ADD"
    add.inputs[1].default_value = 0.25
    nt.links.new(grain.outputs["Fac"], add.inputs[0])
    nt.links.new(wav.outputs["Fac"], add.inputs[2])
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = 0.55
    b.inputs["Distance"].default_value = 0.45
    nt.links.new(add.outputs[0], b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], p.inputs["Normal"])
    rr = nt.nodes.new("ShaderNodeMapRange")
    rr.inputs["To Min"].default_value = 0.5
    rr.inputs["To Max"].default_value = 0.72
    nt.links.new(wav.outputs["Fac"], rr.inputs["Value"])
    nt.links.new(rr.outputs[0], p.inputs["Roughness"])
    # scuffed, greyed patches where the vinyl is worn (forearms on the top edge)
    sc = nt.nodes.new("ShaderNodeTexNoise")
    sc.inputs["Scale"].default_value = 0.03
    sc.inputs["Detail"].default_value = 8
    sc.inputs["Roughness"].default_value = 0.7
    nt.links.new(tc.outputs["Object"], sc.inputs["Vector"])
    scr = nt.nodes.new("ShaderNodeMapRange")
    scr.inputs["From Min"].default_value = 0.6
    scr.inputs["From Max"].default_value = 0.75
    nt.links.new(sc.outputs["Fac"], scr.inputs["Value"])
    col = nt.nodes.new("ShaderNodeMix")
    col.data_type = "RGBA"
    col.inputs["A"].default_value = (0.011, 0.0105, 0.01, 1)
    col.inputs["B"].default_value = (0.022, 0.021, 0.019, 1)
    nt.links.new(scr.outputs[0], col.inputs["Factor"])
    nt.links.new(col.outputs["Result"], p.inputs["Base Color"])
    return m


# ------------------------------------------------------------------ procedural masks (numpy)

def _vnoise(h, w, cell, rng):
    gh, gw = int(h / cell) + 3, int(w / cell) + 3
    g = rng.random((gh, gw)).astype(np.float32)
    y = np.arange(h) / cell
    x = np.arange(w) / cell
    y0 = y.astype(int)
    x0 = x.astype(int)
    fy = (y - y0)
    fx = (x - x0)
    fy = (fy * fy * (3 - 2 * fy))[:, None]
    fx = (fx * fx * (3 - 2 * fx))[None, :]
    a = g[y0][:, x0]
    b = g[y0][:, x0 + 1]
    c = g[y0 + 1][:, x0]
    d = g[y0 + 1][:, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def _fbm(h, w, cell, rng, octaves=5, gain=0.5):
    tot = np.zeros((h, w), np.float32)
    amp, norm = 1.0, 0.0
    for _ in range(octaves):
        tot += amp * _vnoise(h, w, max(cell, 1.0), rng)
        norm += amp
        amp *= gain
        cell /= 2.0
    return tot / norm


def _smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def _image_from(name, arr):
    h, w = arr.shape
    img = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    rgba = np.ones((h, w, 4), np.float32)
    rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = arr
    img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(rgba.ravel())
    img.update()
    return img


def overspray_mask(knobs, res=2.0, seed=7):
    """Black spray overspray over the bare aluminium (1 = paint), drawn from
    the museum photo by region: dark over the button and knob rows, bare and
    bright round the faders and in a cloudy plume right of the Big Knob, dark
    halos hugging every knob, cloudy edges and a fine spray speckle."""
    rng = np.random.default_rng(seed)
    h, w = int(D * res), int(W * res)
    V, U = np.mgrid[0:h, 0:w].astype(np.float32) / res

    def g(cu, cv, su, sv):
        return np.exp(-0.5 * (((U - cu) / su) ** 2 + ((V - cv) / sv) ** 2))

    bare = np.zeros((h, w), np.float32)
    for cu, cv, su, sv, wt in [
        (400, 135, 200, 62, 1.35),     # the fader field: bare, brightest centre-right
        (480, 125, 120, 50, 0.6),
        (110, 130, 80, 45, 0.3),       # left faders: greyer
        (420, 222, 210, 7, 0.65),      # light band under knob row B
        (440, 274, 120, 5, 0.3),       # light streak under the EQ rows
        (300, 14, 330, 12, 0.55),      # bottom strip
        (8, 150, 10, 110, 0.5),        # left edge, lower half
        (918, 450, 30, 105, 1.0),      # the plume right of the Big Knob
        (872, 448, 26, 22, 0.75),
        (858, 392, 20, 18, 0.55),
        (900, 330, 40, 40, 0.6),
        (882, 205, 62, 70, 0.95),      # round the ring knob
        (830, 80, 115, 48, 0.75),      # lamps / CUE plate area
        (905, 40, 45, 30, 0.5),
    ]:
        bare += wt * g(cu, cv, su, sv)
    # cloudy, wispy edges
    n1 = _fbm(h, w, 70 * res, rng, 5)
    n2 = _fbm(h, w, 18 * res, rng, 4)
    m = 1.12 - bare + (n1 - 0.5) * 0.45 * (0.4 + bare) + (n2 - 0.5) * 0.2
    # dark halos round knobs and caps (sprayed with the knobs on)
    halo = np.zeros((h, w), np.float32)
    for (cu, cv, r, wt) in knobs:
        d = np.hypot(U - cu, V - cv)
        halo += wt * np.exp(-0.5 * ((d - r) / (0.35 * r + 2)) ** 2)
    m = m + 0.55 * halo
    m = _smooth(m, 0.25, 0.85)
    # spray speckle: in the half-covered zones the paint breaks into dots
    speck = rng.random((h, w)).astype(np.float32)
    speck = 0.3 * speck + 0.7 * _vnoise(h, w, 2.5 * res, rng)
    m = np.clip(m + (speck - 0.5) * 0.18 * (1 - np.abs(2 * m - 1)), 0, 1)
    return m.astype(np.float32)


def frame_wear_mask(res=1.0, seed=11):
    """Worn spots on the black surround, right side (chipped to tan): the
    patches seen top right, mid right and lower right of the photo."""
    rng = np.random.default_rng(seed)
    u0, v0 = -SURR["side"], -SURR["front"]
    w, h = int((W + 2 * SURR["side"]) * res), int((D + SURR["front"] + SURR["back"]) * res)
    V, U = np.mgrid[0:h, 0:w].astype(np.float32) / res
    U += u0
    V += v0

    def g(cu, cv, su, sv):
        return np.exp(-0.5 * (((U - cu) / su) ** 2 + ((V - cv) / sv) ** 2))

    gate = (g(965, 560, 14, 25) * 1.1 + g(968, 440, 10, 60) * 0.8 + g(975, 200, 14, 20)
            + g(985, 80, 16, 50) * 0.8 + g(975, 20, 25, 18) * 0.9 + g(-25, 380, 8, 18) * 0.5)
    n = _fbm(h, w, 14 * res, rng, 5)
    wear = _smooth(gate * 1.1 + (n - 0.5) * 1.2, 0.62, 0.7)
    return wear.astype(np.float32)


def _image_lookup(nt, img, scale_u, scale_v):
    """Image spanning the object (object coordinates, origin at its centre)."""
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Location"].default_value = (0.5, 0.5, 0)
    mp.inputs["Scale"].default_value = (1 / scale_u, 1 / scale_v, 1)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Linear"
    tex.extension = "EXTEND"
    nt.links.new(mp.outputs["Vector"], tex.inputs["Vector"])
    return tex


def plate_material(mask_img):
    """Bare brushed aluminium under a black spray layer (mask: 1 = paint)."""
    m = bpy.data.materials.new("plate_alu_overspray")
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tex = _image_lookup(nt, mask_img, W, D)
    # spray density mottling: even fully sprayed areas let a little metal through
    tc0 = nt.nodes.new("ShaderNodeTexCoord")
    mot = nt.nodes.new("ShaderNodeTexNoise")
    mot.inputs["Scale"].default_value = 0.05
    mot.inputs["Detail"].default_value = 10
    mot.inputs["Roughness"].default_value = 0.65
    nt.links.new(tc0.outputs["Object"], mot.inputs["Vector"])
    motr = nt.nodes.new("ShaderNodeMapRange")
    motr.inputs["To Min"].default_value = 0.93
    motr.inputs["To Max"].default_value = 1.0
    nt.links.new(mot.outputs["Fac"], motr.inputs["Value"])
    dens = nt.nodes.new("ShaderNodeMath")
    dens.operation = "MULTIPLY"
    nt.links.new(tex.outputs["Color"], dens.inputs[0])
    nt.links.new(motr.outputs[0], dens.inputs[1])
    # mask -> colour, metallic, roughness
    col = nt.nodes.new("ShaderNodeMix")
    col.data_type = "RGBA"
    col.inputs["A"].default_value = (0.66, 0.655, 0.64, 1)      # aluminium
    col.inputs["B"].default_value = (0.018, 0.0175, 0.017, 1)   # black spray
    nt.links.new(dens.outputs[0], col.inputs["Factor"])
    # faint grime in the bare metal
    tc = nt.nodes.new("ShaderNodeTexCoord")
    grime = nt.nodes.new("ShaderNodeTexNoise")
    grime.inputs["Scale"].default_value = 0.08
    grime.inputs["Detail"].default_value = 8
    nt.links.new(tc.outputs["Object"], grime.inputs["Vector"])
    gr = nt.nodes.new("ShaderNodeMapRange")
    gr.inputs["To Min"].default_value = 0.78
    gr.inputs["To Max"].default_value = 1.05
    nt.links.new(grime.outputs["Fac"], gr.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    nt.links.new(col.outputs["Result"], mul.inputs["A"])
    comb = nt.nodes.new("ShaderNodeCombineColor")
    for k in range(3):
        nt.links.new(gr.outputs[0], comb.inputs[k])
    nt.links.new(comb.outputs[0], mul.inputs["B"])
    nt.links.new(mul.outputs["Result"], p.inputs["Base Color"])
    met = nt.nodes.new("ShaderNodeMapRange")
    met.inputs["To Min"].default_value = 1.0
    met.inputs["To Max"].default_value = 0.0
    nt.links.new(dens.outputs[0], met.inputs["Value"])
    nt.links.new(met.outputs[0], p.inputs["Metallic"])
    rough = nt.nodes.new("ShaderNodeMapRange")
    rough.inputs["To Min"].default_value = 0.42
    rough.inputs["To Max"].default_value = 0.88
    spi = nt.nodes.new("ShaderNodeMapRange")       # matte spray: little sheen
    spi.inputs["To Min"].default_value = 0.5
    spi.inputs["To Max"].default_value = 0.2
    nt.links.new(dens.outputs[0], spi.inputs["Value"])
    nt.links.new(spi.outputs[0], p.inputs["Specular IOR Level"])
    nt.links.new(dens.outputs[0], rough.inputs["Value"])
    nt.links.new(rough.outputs[0], p.inputs["Roughness"])
    p.inputs["Anisotropic"].default_value = 0.25
    # brushed grain: fine streaks along u
    st = nt.nodes.new("ShaderNodeTexNoise")
    st.inputs["Scale"].default_value = 3.0
    st.inputs["Detail"].default_value = 4
    smp = nt.nodes.new("ShaderNodeMapping")
    smp.inputs["Scale"].default_value = (0.02, 1.0, 1.0)
    nt.links.new(tc.outputs["Object"], smp.inputs["Vector"])
    nt.links.new(smp.outputs["Vector"], st.inputs["Vector"])
    b = nt.nodes.new("ShaderNodeBump")
    b.inputs["Strength"].default_value = 0.06
    b.inputs["Distance"].default_value = 0.02
    nt.links.new(st.outputs["Fac"], b.inputs["Height"])
    nt.links.new(b.outputs["Normal"], p.inputs["Normal"])
    return m


def surround_material(wear_img):
    m = bpy.data.materials.new("surround_black")
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes["Principled BSDF"]
    tex = _image_lookup(nt, wear_img, W + 2 * SURR["side"], D + SURR["front"] + SURR["back"])
    col = nt.nodes.new("ShaderNodeMix")
    col.data_type = "RGBA"
    col.inputs["A"].default_value = (0.008, 0.0078, 0.0075, 1)
    col.inputs["B"].default_value = (*C.hex_to_linear("#94866d"), 1)
    nt.links.new(tex.outputs["Color"], col.inputs["Factor"])
    nt.links.new(col.outputs["Result"], p.inputs["Base Color"])
    r = nt.nodes.new("ShaderNodeMapRange")
    r.inputs["To Min"].default_value = 0.6
    r.inputs["To Max"].default_value = 0.85
    nt.links.new(tex.outputs["Color"], r.inputs["Value"])
    nt.links.new(r.outputs[0], p.inputs["Roughness"])
    return m


# ------------------------------------------------------------------ geometry helpers

class Frame:
    """A sloped (or tilted) local frame: u across, v up the surface, h out of it."""

    def __init__(self, name, origin, tilt_deg):
        self.empty = bpy.data.objects.new(name, None)
        C.link(self.empty)
        self.empty.location = origin
        self.empty.rotation_euler = (math.radians(tilt_deg), 0, 0)
        self.origin = Vector(origin)
        self.tilt = math.radians(tilt_deg)

    def put(self, ob, u, v, h=0.0, rot=0.0):
        """Parent to the frame at (u, v, h); the object's own location (as
        created, e.g. a box lifted by half its height) is kept as an offset."""
        off = Vector(ob.location)
        ob.parent = self.empty
        ob.location = Vector((u, v, h)) + off
        ob.rotation_euler = (0, 0, math.radians(rot))
        return ob

    def world(self, u, v, h=0.0):
        t = self.tilt
        return self.origin + Vector((u, v * math.cos(t) - h * math.sin(t), v * math.sin(t) + h * math.cos(t)))

    @property
    def normal(self):
        return Vector((0, -math.sin(self.tilt), math.cos(self.tilt)))


def lathe(name, prof, mat, segs=64, flutes=0, fdepth=0.0, fz=(0.0, 0.0), sharp=35):
    """Revolve a (r, z) profile about z. Profile points with r = 0 close the
    ends. Optional flutes: grooves cut into rings whose z lies within fz."""
    bm = bmesh.new()
    rings = []
    for (r, z) in prof:
        if r <= 1e-6:
            rings.append([bm.verts.new((0, 0, z))])
            continue
        ring = []
        for i in range(segs):
            a = 2 * math.pi * i / segs
            rr = r
            if flutes and fz[0] - 1e-6 <= z <= fz[1] + 1e-6:
                c = max(0.0, math.cos(flutes * a))
                rr = r - fdepth * c ** 3
            ring.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        rings.append(ring)
    for ra, rb in zip(rings, rings[1:]):
        if len(ra) == 1 and len(rb) == 1:
            continue
        if len(ra) == 1:
            for i in range(segs):
                bm.faces.new((ra[0], rb[i], rb[(i + 1) % segs]))
        elif len(rb) == 1:
            for i in range(segs):
                bm.faces.new((ra[i], rb[0], ra[(i + 1) % segs]))
        else:
            for i in range(segs):
                bm.faces.new((ra[i], ra[(i + 1) % segs], rb[(i + 1) % segs], rb[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    me.set_sharp_from_angle(angle=math.radians(sharp))
    ob = bpy.data.objects.new(name, me)
    C.link(ob)
    C.assign(ob, mat)
    return ob


class Print:
    """Flat printed marks (lines, dots, ticks) accumulated into one mesh."""

    def __init__(self, name, mat, z=0.04):
        self.bm = bmesh.new()
        self.name, self.mat, self.z = name, mat, z

    def quad(self, pts):
        vs = [self.bm.verts.new((x, y, self.z)) for x, y in pts]
        self.bm.faces.new(vs)

    def line(self, a, b, w=0.35):
        a, b = Vector((*a, 0)), Vector((*b, 0))
        d = (b - a)
        if d.length < 1e-6:
            return
        n = Vector((-d.y, d.x, 0)).normalized() * (w / 2)
        self.quad([(a + n).xy, (b + n).xy, (b - n).xy, (a - n).xy])

    def disc(self, c, r, segs=16):
        vs = [self.bm.verts.new((c[0] + r * math.cos(2 * math.pi * i / segs),
                                 c[1] + r * math.sin(2 * math.pi * i / segs), self.z)) for i in range(segs)]
        self.bm.faces.new(vs)

    def arc(self, c, r, a0, a1, w=0.35, n=48):
        pts = [(c[0] + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
                c[1] + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
        for p, q in zip(pts, pts[1:]):
            self.line(p, q, w)

    def finish(self, frame=None):
        ob = C.mesh_obj(self.name, self.bm, self.mat)
        if frame:
            frame.put(ob, 0, 0, 0)
        return ob


def text(frame, body, u, v, size, mat, h=0.05, align="CENTER", rot=0.0, condense=0.82,
         valign="CENTER", spacing=1.0):
    cu = bpy.data.curves.new("txt_" + body[:10], "FONT")
    cu.body = body
    cu.size = size
    cu.align_x = align
    cu.align_y = valign
    cu.space_character = spacing
    ob = bpy.data.objects.new("txt_" + body[:10], cu)
    C.link(ob)
    cu.materials.append(mat)
    frame.put(ob, u, v, h, rot)
    ob.scale = (condense, 1, 1)
    return ob


# ------------------------------------------------------------------ parts

def knob(frame, M, u, v, kind, pointer=0.0, cap=None):
    """Knob families seen on the desk.
      big    the Big Knob: wide thin flat skirt, tall fluted body (~60 % of the
             skirt), flat light-grey top with a pointer dot (Altec 9069-B shape)
      red    EQ knob: red flared bell skirt, red fluted body, grey top
      black  channel knob: black flared skirt, black fluted body, grey top
      star   black scalloped knob on a skirt
      ring   scalloped black knob inside a chrome ring
      large  big black knob, grey top
    pointer: indicator angle, degrees clockwise from noon (up the plate)."""
    parts = []
    if kind == "big":
        sk = lathe("big_skirt", [(0, 0), (23.5, 0), (23.6, 1.6), (23.0, 2.4), (14.6, 2.6), (0, 2.6)],
                   M["red_skirt"], segs=96)
        body = lathe("big_body", [(0, 2.4), (14.2, 2.4), (14.2, 3.2), (14.0, 24.5), (13.4, 25.6), (0, 25.6)],
                     M["red"], segs=40 * 6, flutes=40, fdepth=0.7, fz=(3.2, 24.5))
        top = lathe("big_cap", [(0, 25.4), (12.9, 25.4), (12.9, 26.0), (12.4, 26.5), (0, 26.5)],
                    cap or M["cap_light"], segs=96)
        dot = C.cylinder("big_dot", 1.0, 0.4, (0, 9.8, 26.45), M["dot"], segs=16)
        parts = [sk, body, top, dot]
        top_h = 26.5
    elif kind in ("red", "black"):
        skm = M["red_skirt"] if kind == "red" else M["black_knob"]
        bm_ = M["red"] if kind == "red" else M["black_knob"]
        rs = 18.0 if kind == "red" else 17.0
        sk = lathe(kind + "_skirt", [(0, 0), (rs, 0), (rs, 1.0), (rs - 2.5, 2.4), (11.5, 5.5), (9.6, 7.0), (0, 7.0)],
                   skm, segs=72)
        body = lathe(kind + "_body", [(0, 6.5), (9.4, 6.5), (9.3, 17.5), (8.7, 18.4), (0, 18.4)],
                     bm_, segs=20 * 8, flutes=20, fdepth=0.6, fz=(6.5, 17.5))
        top = lathe(kind + "_cap", [(0, 18.2), (8.2, 18.2), (8.2, 18.8), (7.8, 19.2), (0, 19.2)],
                    cap or M["cap_grey"], segs=48)
        dot = C.cylinder(kind + "_dot", 0.8, 0.3, (0, 5.6, 19.2), M["dot"], segs=12)
        parts = [sk, body, top, dot]
        top_h = 19.2
    elif kind == "large":
        sk = lathe("lg_skirt", [(0, 0), (22, 0), (22, 1.2), (19, 3), (14.5, 7), (0, 7)], M["black_knob"], segs=80)
        body = lathe("lg_body", [(0, 6.5), (13.8, 6.5), (13.6, 19), (12.8, 20.2), (0, 20.2)],
                     M["black_knob"], segs=28 * 6, flutes=28, fdepth=0.5, fz=(6.5, 19))
        top = lathe("lg_cap", [(0, 20.0), (12.2, 20.0), (12.2, 20.6), (11.6, 21.1), (0, 21.1)],
                    M["cap_grey"], segs=64)
        dot = C.cylinder("lg_dot", 0.9, 0.3, (0, 8.5, 21.1), M["dot"], segs=12)
        parts = [sk, body, top, dot]
        top_h = 21.1
    elif kind in ("star", "ring"):
        sk = lathe("st_skirt", [(0, 0), (19, 0), (19, 1.2), (16, 3), (12, 6), (0, 6)], M["black_knob"], segs=72)
        body = lathe("st_body", [(0, 5.5), (13.5, 5.5), (13.5, 19), (12.5, 21), (0, 21)],
                     M["black_knob"], segs=9 * 12, flutes=9, fdepth=2.4, fz=(5.5, 21), sharp=60)
        line = C.box("st_line", (1.0, 9, 0.4), (0, 4.5, 21.1), M["ink"])
        parts = [sk, body, line]
        top_h = 21
        if kind == "ring":
            ring = C.torus("st_ring", 27.5, 1.3, (0, 0, 0.8), M["chrome"], major=96, minor=12)
            parts.append(ring)
    for p in parts:
        p.parent = None
    root = bpy.data.objects.new(f"knob_{kind}", None)
    C.link(root)
    for p in parts:
        p.parent = root
    frame.put(root, u, v, 0.0, -pointer)
    return root, top_h


def toggle(frame, M, u, v, up=True):
    nut = C.cylinder("tg_nut", 5.0, 2.2, (0, 0, 1.1), M["nickel"], segs=6, smooth=False)
    bush = C.cylinder("tg_bush", 3.0, 5.5, (0, 0, 4.2), M["nickel"], segs=24)
    lev = C.cylinder("tg_lever", 1.1, 11.0, (0, 0, 0), M["chrome"], segs=16, r2=1.55)
    lev.location = (0, 2.0 if up else -2.0, 12.0)
    lev.rotation_euler = (math.radians(-18 if up else 18), 0, 0)
    root = bpy.data.objects.new("toggle", None)
    C.link(root)
    for p in (nut, bush, lev):
        p.parent = root
    frame.put(root, u, v, 0)
    return root


def screw(frame, M, u, v, r=3.3, rot=None):
    hd = lathe("screw", [(0, 0), (r, 0), (r, 0.5), (r * 0.8, 1.1), (0, 1.3)], M["brass"], segs=24)
    sl = C.box("screw_slot", (2 * r * 0.95, 0.6, 0.8), (0, 0, 1.2), M["slot"])
    sl.parent = hd
    frame.put(hd, u, v, 0, rot if rot is not None else random.uniform(0, 180))
    return hd


def arc_slab(name, width, r_out, hc, a0, a1, mat, r_in=None, n=40):
    """A slab curved about the u axis: across u it is `width` wide; in (v, h)
    it is the arc band r_in..r_out about (0, hc), angles a0..a1 (deg from +h
    towards +v). With r_in None it is solid down to h = 0 (a cheek plate)."""
    outer = [(r_out * math.sin(math.radians(a0 + (a1 - a0) * i / n)),
              hc + r_out * math.cos(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
    if r_in is None:
        inner = [(outer[-1][0], 0.0), (outer[0][0], 0.0)]
    else:
        inner = [(r_in * math.sin(math.radians(a1 - (a1 - a0) * i / n)),
                  hc + r_in * math.cos(math.radians(a1 - (a1 - a0) * i / n))) for i in range(n + 1)]
    prof = outer + inner
    bm = bmesh.new()
    L = [bm.verts.new((-width / 2, v, h)) for v, h in prof]
    Rr = [bm.verts.new((width / 2, v, h)) for v, h in prof]
    m = len(prof)
    for i in range(m):
        j = (i + 1) % m
        bm.faces.new((L[i], L[j], Rr[j], Rr[i]))
    bm.faces.new(list(reversed(L)))
    bm.faces.new(Rr)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = C.mesh_obj(name, bm, mat, smooth=True)
    ob.data.set_sharp_from_angle(angle=math.radians(30))
    C.add_bevel(ob, 0.4)
    return ob


def keycap(frame, M, u, v, label, w=25.0, d=13.0, h=6.5, size=3.0):
    k = C.box("key", (w, d, h), (0, 0, h / 2), M["key"], bevel=0.9)
    frame.put(k, u, v, 0)
    if label:
        text(frame, label, u, v, size, M["ink"], h=h + 0.04)
    return k


def lamp(frame, M, u, v, mat, r=7.0):
    bz = lathe("lamp_bezel", [(0, 0), (r + 1.4, 0), (r + 1.4, 1.2), (r + 0.6, 2.2), (0, 2.2)], M["chrome"], segs=48)
    jw = C.sphere("lamp_jewel", r, (0, 0, 0), mat)
    jw.scale = (1, 1, 0.55)
    jw.location = (0, 0, 1.6)
    jw.parent = bz
    frame.put(bz, u, v, 0)
    return bz


# ------------------------------------------------------------------ assemblies

def plate(frame, M, rng):
    """The aluminium plate and everything on it. Returns knob discs for the mask."""
    discs = []           # (u, v, radius, halo weight)
    pr = Print("plate_print", M["ink"])
    faint = Print("plate_print_faint", M["ink_faint"])

    # -- channel strips
    for i, cu in enumerate(CH_U):
        text(frame, str(i + 1), cu, V_CHNUM, 6.0, M["ink"])
        # push-button column: a black backing plate and 8 keycaps
        bk = C.box("btn_back", (28, 8 * V_BTN_PITCH + 4, 1.2), (0, 0, 0.6), M["housing"])
        frame.put(bk, cu, V_BTN_TOP - 3.5 * V_BTN_PITCH, 0)
        for j, lab in enumerate(BTN_LABELS):
            keycap(frame, M, cu, V_BTN_TOP - j * V_BTN_PITCH, lab, size=2.9 if len(lab) > 2 else 3.2)
        discs.append((cu, V_BTN_TOP - 3.5 * V_BTN_PITCH, 30, 0.3))
        # knob row A (channel 2's has a silver top in the photo)
        knob(frame, M, cu, V_KNOB_A, "black", pointer=rng.uniform(-140, 140),
             cap=M["cap_silver"] if i == 1 else None)
        discs.append((cu, V_KNOB_A, 17, 0.6))
        # EQ positions
        for row, vv in enumerate(V_EQ):
            kind = EQ[row][i]
            if kind == "R":
                knob(frame, M, cu, vv, "red", pointer=rng.uniform(-150, 150))
                discs.append((cu, vv, 18, 0.5))
            # step dots round every EQ position (the legends are unreadable: left off)
            for k in range(11):
                a = math.radians(225 - 27 * k)
                faint.disc((cu + 21.5 * math.cos(a), vv + 16.5 * math.sin(a)), 0.35, 8)
        # knob row B + its legend
        knob(frame, M, cu, V_KNOB_B, "black", pointer=rng.uniform(-140, 140))
        discs.append((cu, V_KNOB_B, 17, 0.6))
        screw(frame, M, cu, V_SCREW)
        # fader: slot, cap at the bottom of travel, scale, screw, number
        slot = C.box("slot", (2.6, V_SLOT_TOP - V_SLOT_BOT, 0.3), (0, 0, 0.1), M["slot"])
        frame.put(slot, cu, (V_SLOT_TOP + V_SLOT_BOT) / 2, 0)
        cap = lathe("fader_cap", [(0, 0), (12.0, 0), (12.0, 1.2), (10.2, 13.5), (9.6, 14.6), (0, 14.6)],
                    M["fader_red"], segs=64)
        top = lathe("fader_top", [(0, 13.8), (8.3, 13.8), (8.0, 14.9), (0, 14.5)], M["fader_red_top"], segs=48)
        top.parent = cap
        frame.put(cap, cu, V_CAP, 0)
        discs.append((cu, V_CAP, 11, 0.55))
        screw(frame, M, cu, V_CAPSCREW, r=3.6)
        discs.append((cu, V_CAPSCREW, 4, 0.4))
        text(frame, str(i + 1), cu - 3, V_FADNUM, 6.0, M["ink"])
        nu = cu + PITCH * 0.47       # fader scale, 0 (top) .. 45, right of the slot
        for k in range(10):
            vv = 181.0 - 13.1 * k
            text(frame, str(5 * k), nu, vv, 3.6, M["ink_faint"])
            faint.line((cu + 5, vv), (nu - 4.5, vv), 0.32)
            faint.line((nu + 4.5, vv), (cu + PITCH - 5, vv), 0.32)

    # -- right section
    for mu, labs in ((647.0, ["MIC", "EKO 1", "EKO 2", "EKO 3", "EKO 4"]),
                     (710.0, ["MIC", "EKO 1", "EKO 2", "EKO 3", "EKO 4"])):
        bk = C.box("btn_back", (28, 5 * V_BTN_PITCH + 4, 1.2), (0, 0, 0.6), M["housing"])
        frame.put(bk, mu, 556 - 2 * V_BTN_PITCH, 0)
        for j, lab in enumerate(labs):
            keycap(frame, M, mu, 556 - j * V_BTN_PITCH, lab, size=2.9)
        discs.append((mu, 526, 28, 0.35))
    screw(frame, M, 745, 492, r=3.5)

    # the Big Knob and its dial
    knob(frame, M, BIG["u"], BIG["v"], "big", pointer=-90 + 18 * BIG_POINTER_STEP)
    discs.append((BIG["u"], BIG["v"], 24, 0.85))
    big_dial(frame, M, pr)

    for (ku, kv, kind) in ((648, 403, "black"), (710, 404, "black"), (785, 406, "black"),
                           (649, 322, "red"), (703, 322, "black"), (785, 324, "black")):
        knob(frame, M, ku, kv, kind, pointer=rng.uniform(-120, 120),
             cap=M["cap_silver"] if (ku, kv) == (785, 324) else None)
        discs.append((ku, kv, 17, 0.6))
    knob(frame, M, 874, 327, "star", pointer=-15)
    discs.append((874, 327, 19, 0.6))
    knob(frame, M, 786, 245, "large", pointer=-60)
    discs.append((786, 245, 22, 0.7))
    knob(frame, M, 874, 245, "ring", pointer=10)
    discs.append((874, 245, 30, 0.35))

    # master faders: a quadrant bank. Each clear channel is an arc rising out
    # of the black frame (crest ~26 mm above the plate); the coloured lever
    # caps ride the curve. Caps sit low on the arc, as in the photo.
    hu, hv = 683.0, 121.0
    R, crest = 119.0, 26.0
    hc = crest - R                     # arc centre, below the plate
    half = math.degrees(math.asin(66.0 / R))
    base = C.box("mf_base", (92, 150, 4), (0, 0, 2), M["housing"], bevel=1.0)
    frame.put(base, hu, hv, 0)
    discs.append((hu, hv, 62, 0.6))
    for k, mc in enumerate(("mf_red", "mf_blue", "mf_green", "mf_white")):
        cu = hu - 30 + 20 * k
        for side in (-1, 1):           # black cheeks, solid down to the base
            ch = arc_slab("mf_cheek", 2.6, R + 1.5, hc, -half - 2, half + 2, M["housing"])
            frame.put(ch, cu + side * 8.7, hv, 0)
        rail = arc_slab("mf_rail", 13.6, R, hc, -half, half, M["rail"], r_in=R - 5.0)
        frame.put(rail, cu, hv, 0)
        slot = arc_slab("mf_slot", 2.2, R + 0.15, hc, -half + 3, half - 3, M["slot"], r_in=R - 6)
        frame.put(slot, cu + 2.5, hv, 0)
        th = math.radians(-22.0)       # cap position on the arc (towards the front)
        cap = C.box("mf_cap", (8.5, 11, 12), (0, 0, 0), M[mc], bevel=2.6)
        frame.put(cap, cu + 2.5, hv + (R + 4.5) * math.sin(th), hc + (R + 4.5) * math.cos(th))
        cap.rotation_euler = (-th, 0, 0)
        for sv, sa in ((hv + 69, half), (hv - 69, -half)):
            sb = screw(frame, M, cu, sv, r=2.6)
            sb.location.z = 4.0
    # end caps of the bank
    for side in (-1, 1):
        ec = arc_slab("mf_end", 4.0, R + 1.5, hc, -half - 2, half + 2, M["housing"])
        frame.put(ec, hu + side * 44, hv, 0)

    # small rectangular buttons, the label plate, lamps
    for bu in (824.0, 845.0):
        keycap(frame, M, bu, 202, "", w=19, d=10, h=5)
    screw(frame, M, 806, 202, r=2.5)
    screw(frame, M, 864, 202, r=2.5)
    for k in range(5):
        keycap(frame, M, 785 + 24 * k, 159, "", w=23, d=10, h=5)
    screw(frame, M, 765, 159, r=2.5)
    screw(frame, M, 903, 159, r=2.5)
    discs.append((833, 180, 40, 0.3))
    lp = C.box("label_plate", (20, 10, 2.5), (0, 0, 1.25), M["label_plate"], bevel=0.6)
    frame.put(lp, 831, 106, 0)
    text(frame, "CUE", 831, 106, 3.0, M["ink"], h=2.55)
    screw(frame, M, 813, 106, r=2.2)
    screw(frame, M, 849, 106, r=2.2)
    for k, mc in enumerate(("lamp_red", "lamp_green", "lamp_blue", "lamp_blue", "lamp_white")):
        lamp(frame, M, 777 + 33.4 * k, 50.5, M[mc])
        if k < 4:
            screw(frame, M, 777 + 33.4 * k + 16.7, 50.5, r=2.0)
    screw(frame, M, 845, 22, r=2.4)

    # plate fixing screws round the edge
    for (su, sv) in ((14, 618), (14, 430), (14, 240), (14, 22), (340, 10), (650, 10), (936, 618),
                     (936, 430), (936, 240), (936, 40), (330, 622), (700, 622), (240, 622), (520, 622),
                     (178, 612), (418, 612), (746, 600), (182, 505), (418, 505)):
        screw(frame, M, su, sv, r=2.6)

    pr.finish(frame)
    faint.finish(frame)
    return discs


def big_dial(frame, M, pr):
    """HI PASS FILTER dial: 11 steps, 18 deg apart, OFF at 9 o'clock, 500 at
    noon, 7.5K at 3 o'clock; each legend on a dog-leg leader (Altec 9069-B)."""
    cu, cv = BIG["u"], BIG["v"]
    text(frame, "HI PASS FILTER", cu, cv + 49.0, 4.6, M["ink"], spacing=1.15)
    r1, r2, run, size = 26.5, 31.0, 3.6, 3.6
    for k, lab in enumerate(BIG_STEPS):
        a = math.radians(180 - 18 * k)
        ca, sa = math.cos(a), math.sin(a)
        p1 = (cu + r1 * ca, cv + r1 * sa)
        p2 = (cu + r2 * ca, cv + r2 * sa)
        if k == 5:  # noon: a straight stroke, legend above
            pr.line(p1, (cu, cv + r2 + 1.0), 0.4)
            text(frame, lab, cu, cv + r2 + 4.2, size, M["ink"])
            continue
        side = -1 if ca < 0 else 1
        pr.line(p1, p2, 0.4)
        p3 = (p2[0] + side * run, p2[1])
        pr.line((p2[0] - side * 0.2, p2[1]), p3, 0.4)
        text(frame, lab, p3[0] + side * 0.8, p3[1], size, M["ink"],
             align="LEFT" if side > 0 else "RIGHT")


def vu_meter(frame, M, u, v, bw, bh, fw, fh, label, label_dv, size_lab=10.5, seed=0):
    """Rectangular VU meter: black stepped bezel, peach face with an arced
    scale, red zone from 0 VU, needle at rest, glass."""
    rng = random.Random(seed)
    t = 9.0  # bezel depth
    fr = (bw - fw) / 2
    parts = []
    for (du, dv, sw, sh) in ((0, (bh - fr) / 2, bw, fr), (0, -(bh - fr) / 2, bw, fr),
                             (-(bw - fr) / 2, 0, fr, bh - 2 * fr), ((bw - fr) / 2, 0, fr, bh - 2 * fr)):
        b = C.box("vu_bezel", (sw, sh, t), (0, 0, 0), M["bezel"], bevel=1.0)
        frame.put(b, u + du, v + dv, t / 2)
        parts.append(b)
    inner = 4.0
    for (du, dv, sw, sh) in ((0, (fh + inner) / 2, fw + 2 * inner, inner), (0, -(fh + inner) / 2, fw + 2 * inner, inner),
                             (-(fw + inner) / 2, 0, inner, fh), ((fw + inner) / 2, 0, inner, fh)):
        b = C.box("vu_step", (sw, sh, 3), (0, 0, 0), M["bezel"], bevel=0.5)
        frame.put(b, u + du, v + dv, t - 3.5)
    face = C.box("vu_face", (fw + 2, fh + 2, 1), (0, 0, 0), M["vu_face"])
    frame.put(face, u, v, 3.5)
    zf = 4.05
    s = fh / 84.0
    # one pivot near the bottom centre of the face, under a black cover
    piv = (u, v - 0.5 * fh + 6.0 * s)
    R = 0.74 * fh
    ink = Print("vu_ink", M["vu_ink"], z=zf)
    red = Print("vu_red", M["vu_red"], z=zf + 0.01)
    # angles (deg, 90 = straight up): -20 VU at 140, 0 VU at 62, +3 at 40
    marks = [(-20, 140), (-10, 126), (-7, 116), (-5, 107), (-3, 95), (-2, 87), (-1, 76), (0, 62),
             (1, 54), (2, 47), (3, 40)]
    a_lo, a_zero, a_hi = 140.0, 62.0, 40.0
    ink.arc(piv, R, a_zero, a_lo, 0.4 * s, n=64)
    red.arc(piv, R + 1.0 * s, a_hi, a_zero, 2.2 * s, n=24)      # the red zone band
    ink.arc(piv, R, a_hi, a_zero, 0.4 * s, n=24)
    for val, ang in marks:
        a = math.radians(ang)
        major = val in (-20, -10, -7, -5, -3, 0, 3)
        L = (5.0 if major else 3.5) * s
        p0 = (piv[0] + R * math.cos(a), piv[1] + R * math.sin(a))
        p1 = (piv[0] + (R + L) * math.cos(a), piv[1] + (R + L) * math.sin(a))
        (red if val > 0 else ink).line(p0, p1, 0.55 * s)
        lab = ("+" + str(val)) if val > 0 else str(abs(val))
        q = (piv[0] + (R + L + 4.0 * s) * math.cos(a), piv[1] + (R + L + 4.0 * s) * math.sin(a))
        text(frame, lab, q[0], q[1], 3.4 * s, M["vu_red"] if val > 0 else M["vu_ink"], h=zf + 0.02,
             rot=ang - 90)
    # second (percent) scale under the arc, 0 .. 100 with minor ticks
    Rp = R - 1.2 * s
    for k in range(21):
        ang = a_lo - (a_lo - a_zero) * k / 20
        a = math.radians(ang)
        L = (3.6 if k % 5 == 0 else 2.0) * s
        ink.line((piv[0] + Rp * math.cos(a), piv[1] + Rp * math.sin(a)),
                 (piv[0] + (Rp - L) * math.cos(a), piv[1] + (Rp - L) * math.sin(a)), 0.35 * s)
        if k % 5 == 0 and k:
            q = (piv[0] + (Rp - 7.5 * s) * math.cos(a), piv[1] + (Rp - 7.5 * s) * math.sin(a))
            text(frame, str(k * 5), q[0], q[1], 2.4 * s, M["vu_ink"], h=zf + 0.02, rot=ang - 90)
    text(frame, "VU", u, piv[1] + 0.42 * R, 7.5 * s, M["vu_ink"], h=zf + 0.02, condense=1.0)
    ink.finish(frame)
    red.finish(frame)
    # pivot cover: a small black half-dome at the bottom centre
    cov = C.sphere("vu_pivot_cover", 5.5 * s, (0, 0, 0), M["needle"])
    cov.scale = (1.0, 1.0, 0.3)
    frame.put(cov, piv[0], piv[1], zf)
    # needle at rest, slightly left of -20, from the pivot
    a = math.radians(a_lo + 4 + rng.uniform(-1, 1))
    n0 = Vector(piv)
    n1 = Vector((piv[0] + (R + 4.5 * s) * math.cos(a), piv[1] + (R + 4.5 * s) * math.sin(a)))
    nb = Print("vu_needle", M["needle"], z=zf + 1.4)
    nb.line(n0.xy, (n0 + (n1 - n0) * 0.3).xy, 0.9 * s)
    nb.line((n0 + (n1 - n0) * 0.3).xy, n1.xy, 0.45 * s)
    nb.finish(frame)
    gl = C.box("vu_glass", (fw + 4, fh + 4, 0.8), (0, 0, 0), M["glass"])
    frame.put(gl, u, v, t - 3.6)
    # four bezel screws
    for sx in (-1, 1):
        for sy in (-1, 1):
            sc = screw(frame, M, u + sx * (bw / 2 - fr / 2), v + sy * (bh / 2 - fr / 2), r=1.9)
            sc.location.z = t
    text(frame, label, u, v + label_dv, size_lab, M["ink_faint"], h=0.05, spacing=1.25, condense=0.95)


def cabinet(M):
    """Black body: side profile extruded across, sloped top round the plate,
    the meter bridge with an open top, the padded armrest along the front."""
    t = math.radians(SLOPE)
    f = Frame("tmp", P0, SLOPE)
    front_top = f.world(0, -SURR["front"], -1.5)
    back_top = f.world(0, D + SURR["back"], -1.5)
    yb0, zb0 = back_top.y, back_top.z
    bt = math.radians(BRIDGE_TILT)
    yb1, zb1 = yb0 + BRIDGE_H * math.sin(bt), zb0 + BRIDGE_H * math.cos(bt)
    yback = yb1 + BRIDGE_DEPTH
    prof = [(front_top.y, 22.0), (front_top.y, front_top.z), (yb0, zb0), (yb1, zb1), (yback, zb1), (yback, 22.0)]
    bm = bmesh.new()
    hw = CAB_W / 2
    left = [bm.verts.new((-hw, y, z)) for y, z in prof]
    right = [bm.verts.new((hw, y, z)) for y, z in prof]
    n = len(prof)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((left[i], left[j], right[j], right[i]))
    bm.faces.new(list(reversed(left)))
    bm.faces.new(right)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    body = C.mesh_obj("cabinet", bm, M["cabinet"])
    C.add_bevel(body, 2.0)
    bpy.data.objects.remove(f.empty, do_unlink=True)
    # plinth / toe recess under the body
    C.box("toe", (CAB_W - 30, yback - front_top.y - 20, 22), (0, (yback + front_top.y) / 2, 11), M["interior"])

    # open top of the bridge: a well with an aluminium bar of terminal strips
    lip = 22.0
    well_d = 120.0
    cav = C.box("cav", (CAB_W - 2 * lip, BRIDGE_DEPTH - 2 * lip, well_d + 20),
                (0, (yb1 + yback) / 2, zb1 - well_d / 2 + 10), None)
    C.boolean_cut(body, [cav])
    floor = C.box("well_floor", (CAB_W - 2 * lip, BRIDGE_DEPTH - 2 * lip, 2),
                  (0, (yb1 + yback) / 2, zb1 - well_d), M["interior"])
    bar = C.box("terminal_bar", (CAB_W * 0.62, 28, 4), (CAB_W * 0.12, yback - lip - 30, zb1 - 28), M["alu_bar"])
    for k in range(44):
        C.box("term", (9, 12, 6), (CAB_W * 0.12 - CAB_W * 0.29 + k * 14.6, yback - lip - 26, zb1 - 23),
              M["terminal"])
    C.box("bar_rail", (CAB_W * 0.5, 10, 3), (CAB_W * 0.04, yback - lip - 55, zb1 - 14), M["alu_bar"])
    # wires dropping into the well
    for k in range(5):
        x = 330 + 9 * k
        C.curve_obj("wire", [(x, yback - lip - 40, zb1 - 18), (x + 6, yback - lip - 60, zb1 - 8),
                             (x + 10, yback - lip - 90, zb1 - 60)], 0.8,
                    C.clay("wire_" + str(k), rgb=C.hex_to_linear(["#bb3333", "#dddddd", "#cc9933", "#333333", "#33aa55"][k])),
                    spline="NURBS")
    return dict(front_y=front_top.y, front_z=front_top.z, bridge=(yb0, zb0, yb1, zb1), back_y=yback,
                floor=floor, bar=bar)


def bridge_face(M, cab):
    yb0, zb0, yb1, zb1 = cab["bridge"]
    bt = BRIDGE_TILT
    fr = Frame("bridge_frame", Vector((-CAB_W / 2, yb0, zb0)), 90 - bt)
    # measured from the photo: centres (mm from the bridge's left edge)
    xs = [133, 295, 457, 618, 780, 920]
    for k, x in enumerate(xs):
        if k < 4:
            vu_meter(fr, M, x, 153, 149, 105, 127, 84, f"TRACK {k + 1}", -74, seed=k)
        else:
            vu_meter(fr, M, x, 151, 112, 87, 101, 68, f"ECHO {k - 3}", -60, seed=k)
    lamp(fr, M, 777, 36, M["lamp_amber"], r=8.0)
    return fr


def _armrest_profile(depth, height, r_top=42.0, r_bot=22.0, n=40):
    """Closed-ish (y, z) outline of the roll, back at y = 0 going -y to the
    front, z from 0 (bottom) to height: a flat front face, a big rounded top
    edge, a smaller rounded bottom edge. Sampled evenly by arc length."""
    pts = []
    def arc(cy, cz, r, a0, a1, k):
        for i in range(k + 1):
            a = math.radians(a0 + (a1 - a0) * i / k)
            pts.append((cy + r * math.cos(a), cz + r * math.sin(a)))
    pts.append((0.0, 0.0))
    arc(-depth + r_bot, r_bot, r_bot, 270, 180, 6)
    arc(-depth + r_top, height - r_top, r_top, 180, 90, 10)
    pts.append((0.0, height))
    # resample by arc length
    L = [0.0]
    for p, q in zip(pts, pts[1:]):
        L.append(L[-1] + math.dist(p, q))
    out = []
    for i in range(n + 1):
        t = L[-1] * i / n
        k = max(j for j in range(len(L)) if L[j] <= t + 1e-9)
        k = min(k, len(pts) - 2)
        f = (t - L[k]) / max(L[k + 1] - L[k], 1e-9)
        out.append((pts[k][0] + f * (pts[k + 1][0] - pts[k][0]), pts[k][1] + f * (pts[k + 1][1] - pts[k][1])))
    return out


def armrest(M, cab):
    """Twelve tufted vinyl panels along the front: a flat-fronted padded roll
    with a big rounded top edge, each panel puffed between deep seams; the
    vinyl has split along four seams, showing the tan backing."""
    y0 = cab["front_y"] + 6
    ztop = cab["front_z"] - 2
    depth, height = 88.0, 205.0
    zb = ztop - height
    n_panels = 12
    width = CAB_W + 30
    pw = width / n_panels
    prof = _armrest_profile(depth, height)
    xs_n = 24
    rng = random.Random(3)
    for p in range(n_panels):
        x0 = -width / 2 + p * pw
        bm = bmesh.new()
        grid = []
        sag = rng.uniform(0.4, 1.0)
        for ix in range(xs_n + 1):
            tx = ix / xs_n
            x = x0 + 0.8 + tx * (pw - 1.6)
            seam = (1 - math.sin(math.pi * tx)) ** 3      # 1 at the seams
            row = []
            for j, (py, pz) in enumerate(prof):
                tz = pz / height
                front = min(1.0, max(0.0, -py / depth))   # 0 at the back, 1 at the front face
                puff = 2.2 * math.sin(math.pi * tx) ** 0.5 * math.sin(math.pi * min(1, tz * 1.05)) ** 0.6
                yy = y0 + py * (1 - 0.07 * seam) - puff * front
                zz = zb + pz - 2.0 * seam * (tz > 0.8) * (tz - 0.8) * 5
                yy += sag * 1.0 * math.sin(5 * tz + p) * math.sin(math.pi * tx) * front
                row.append(bm.verts.new((x, yy, zz)))
            grid.append(row)
        for ix in range(xs_n):
            for j in range(len(prof) - 1):
                bm.faces.new((grid[ix][j], grid[ix + 1][j], grid[ix + 1][j + 1], grid[ix][j + 1]))
        for ix in (0, xs_n):   # close the ends
            bm.faces.new(grid[ix] if ix else list(reversed(grid[ix])))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        ob = C.mesh_obj(f"armrest_{p}", bm, M["vinyl"], smooth=True)
        ob.data.set_sharp_from_angle(angle=math.radians(80))
        sub = ob.modifiers.new("sub", "SUBSURF")
        sub.levels = sub.render_levels = 1
    # backing behind the panels (seen in the seams) and the splits
    C.box("armrest_back", (width, 40, height - 16), (0, y0 - 30, zb + height / 2 - 4), M["interior"])
    for s_, top in ((5, 0.95), (7, 0.7), (8, 0.9), (9, 0.95)):
        x = -width / 2 + s_ * pw
        C.box(f"split_{s_}", (3.0, 60, height * top * 0.8), (x, y0 - 50, zb + height * top * 0.45), M["foam"],
              bevel=1.0)


def build(params=None):
    rng = random.Random(5)
    random.seed(5)
    M = materials()
    cab = cabinet(M)
    frame = Frame("plate_frame", P0, SLOPE)
    discs = plate(frame, M, rng)
    # plate slab with the overspray, sitting on the surround
    mask = _image_from("overspray", overspray_mask(discs))
    pm = plate_material(mask)
    slab = C.box("plate", (W, D, 3.0), (0, 0, 0), pm, bevel=0.6)
    frame.put(slab, W / 2, D / 2, -1.5)
    wear = _image_from("frame_wear", frame_wear_mask())
    sm = surround_material(wear)
    sw, sd = W + 2 * SURR["side"], D + SURR["front"] + SURR["back"]
    surr = C.box("surround", (sw, sd, 4.0), (0, 0, 0), sm)
    # top at h = -1.0: clear of the cabinet's own sloped face (h = -1.5), no z-fighting
    frame.put(surr, W / 2, (D + SURR["back"] - SURR["front"]) / 2, -3.0)
    bridge_face(M, cab)
    armrest(M, cab)
    bpy.context.view_layer.update()
    big = frame.world(BIG["u"], BIG["v"], 0)
    return dict(
        frame=frame,
        plate_center=frame.world(W / 2, D / 2, 0),
        desk_center=Vector((0, 120, 340)),
        big_knob=big,
        big_knob_top=frame.world(BIG["u"], BIG["v"], 26.5),
        normal=frame.normal,
        size=1100.0,
        M=M,
    )
