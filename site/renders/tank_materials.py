"""Procedural materials for the tank's first real material pass.

Everything is procedural (no image textures) so it stays repeatable and small:
zinc-plated chassis (two finishes), oiled spring steel with heat tint at the
ends, copper enamel coils with visible windings, iron laminations, nickel,
phenolic, coloured hook-up wire, solder, paper.

Wear is subtle on purpose: fine directional scratches and faint fingerprint
smudges vary the roughness, and dust settles where ambient occlusion says the
corners are.
"""
import bpy

import common as C


def _new(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    return m, nt, nt.nodes["Principled BSDF"]


def _node(nt, kind, **inputs):
    n = nt.nodes.new(kind)
    for k, v in inputs.items():
        n.inputs[k].default_value = v
    return n


def _maprange(nt, src, a, b, lo, hi):
    r = _node(nt, "ShaderNodeMapRange")
    r.inputs["From Min"].default_value = a
    r.inputs["From Max"].default_value = b
    r.inputs["To Min"].default_value = lo
    r.inputs["To Max"].default_value = hi
    nt.links.new(src, r.inputs["Value"])
    return r.outputs[0]


def _mixcol(nt, fac, a, b):
    m = nt.nodes.new("ShaderNodeMix")
    m.data_type = "RGBA"
    if isinstance(fac, float):
        m.inputs["Factor"].default_value = fac
    else:
        nt.links.new(fac, m.inputs["Factor"])
    for sock, val in (("A", a), ("B", b)):
        if isinstance(val, tuple):
            m.inputs[sock].default_value = (*val, 1) if len(val) == 3 else val
        else:
            nt.links.new(val, m.inputs[sock])
    return m.outputs["Result"]


def _wear(nt, coord, base_rough):
    """Roughness with fine scratches (long thin noise along X) and faint
    fingerprint smudges (soft blotches). Returns a roughness socket."""
    mp = _node(nt, "ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.04, 3.0, 3.0)  # stretched: streaks along X
    nt.links.new(coord, mp.inputs["Vector"])
    scr = _node(nt, "ShaderNodeTexNoise", Scale=1.5, Detail=4.0, Roughness=0.6)
    nt.links.new(mp.outputs[0], scr.inputs["Vector"])
    scratches = _maprange(nt, scr.outputs["Fac"], 0.62, 0.70, 0.0, 1.0)
    smudge_n = _node(nt, "ShaderNodeTexNoise", Scale=0.08, Detail=2.0)
    nt.links.new(coord, smudge_n.inputs["Vector"])
    smudge = _maprange(nt, smudge_n.outputs["Fac"], 0.55, 0.75, 0.0, 1.0)
    # +0.12 roughness in the scratches, -0.06 in the (greasy) smudges
    s1 = nt.nodes.new("ShaderNodeMath")
    s1.operation = "MULTIPLY"
    nt.links.new(scratches, s1.inputs[0])
    s1.inputs[1].default_value = 0.12
    s2 = nt.nodes.new("ShaderNodeMath")
    s2.operation = "MULTIPLY_ADD"
    nt.links.new(smudge, s2.inputs[0])
    s2.inputs[1].default_value = -0.06
    s2.inputs[2].default_value = base_rough
    tot = nt.nodes.new("ShaderNodeMath")
    tot.operation = "ADD"
    nt.links.new(s1.outputs[0], tot.inputs[0])
    nt.links.new(s2.outputs[0], tot.inputs[1])
    return tot.outputs[0]


def _dust(nt, col, rough):
    """Dust in the corners: occlusion -> mix towards a dull warm grey."""
    ao = nt.nodes.new("ShaderNodeAmbientOcclusion")
    ao.samples = 8
    ao.inputs["Distance"].default_value = 5.0
    amount = _maprange(nt, ao.outputs["AO"], 0.35, 0.85, 0.55, 0.0)
    noise = _node(nt, "ShaderNodeTexNoise", Scale=0.5, Detail=6.0)
    patchy = _maprange(nt, noise.outputs["Fac"], 0.35, 0.65, 0.3, 1.0)
    f = nt.nodes.new("ShaderNodeMath")
    f.operation = "MULTIPLY"
    nt.links.new(amount, f.inputs[0])
    nt.links.new(patchy, f.inputs[1])
    col2 = _mixcol(nt, f.outputs[0], col, (0.42, 0.38, 0.33))
    r = nt.nodes.new("ShaderNodeMix")
    r.data_type = "FLOAT"
    nt.links.new(f.outputs[0], r.inputs["Factor"])
    nt.links.new(rough, r.inputs["A"])
    r.inputs["B"].default_value = 0.95
    return col2, r.outputs["Result"], f.outputs[0]


def zinc(finish="galv"):
    """Zinc-plated steel chassis.
    galv:   bright galvanised with spangle (crystal cells of slightly different
            brightness and sheen, ~10 mm across).
    yellow: yellow-chromate zinc: pale gold with a faint iridescence (thin film
            of varying thickness)."""
    m, nt, p = _new(f"zinc_{finish}")
    coord = nt.nodes.new("ShaderNodeTexCoord").outputs["Object"]
    p.inputs["Metallic"].default_value = 1.0
    if finish == "yellow":
        base = (0.82, 0.70, 0.38)
        rough = 0.30
        film = _node(nt, "ShaderNodeTexNoise", Scale=0.012, Detail=1.0)
        nt.links.new(coord, film.inputs["Vector"])
        thick = _maprange(nt, film.outputs["Fac"], 0.3, 0.7, 260.0, 340.0)
        nt.links.new(thick, p.inputs["Thin Film Thickness"])
        p.inputs["Thin Film IOR"].default_value = 1.38
        col = base
        col_sock = _mixcol(nt, 0.0, col, col)
    else:
        rough = 0.24
        vor = _node(nt, "ShaderNodeTexVoronoi", Scale=0.16)
        nt.links.new(coord, vor.inputs["Vector"])
        cell = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(vor.outputs["Color"], cell.inputs[0])
        val = _maprange(nt, cell.outputs[0], 0.0, 1.0, 0.74, 0.80)
        comb = nt.nodes.new("ShaderNodeCombineColor")
        for k in range(3):
            nt.links.new(val, comb.inputs[k])
        col_sock = comb.outputs[0]
        rough_cell = _maprange(nt, cell.outputs[1], 0.0, 1.0, 0.20, 0.29)
        rough = rough_cell
    if isinstance(rough, float):
        rv = nt.nodes.new("ShaderNodeValue")
        rv.outputs[0].default_value = rough
        rough = rv.outputs[0]
    wear = _wear(nt, coord, 0.0)
    rsum = nt.nodes.new("ShaderNodeMath")
    rsum.operation = "ADD"
    nt.links.new(rough, rsum.inputs[0])
    nt.links.new(wear, rsum.inputs[1])
    col2, rough2, dust = _dust(nt, col_sock, rsum.outputs[0])
    nt.links.new(col2, p.inputs["Base Color"])
    nt.links.new(rough2, p.inputs["Roughness"])
    # dust isn't metal
    met = _maprange(nt, dust, 0.0, 0.6, 1.0, 0.2)
    nt.links.new(met, p.inputs["Metallic"])
    return m


def spring_steel():
    """Darker oiled spring steel (not chrome), with heat tint (straw to blue)
    over the last ~25 mm at each end (springs are centred on x = 0)."""
    m, nt, p = _new("spring_steel")
    p.inputs["Metallic"].default_value = 1.0
    p.inputs["Roughness"].default_value = 0.3
    p.inputs["Coat Weight"].default_value = 0.35   # the oil film
    p.inputs["Coat Roughness"].default_value = 0.08
    coord = nt.nodes.new("ShaderNodeTexCoord").outputs["Object"]
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(coord, sep.inputs[0])
    ax = nt.nodes.new("ShaderNodeMath")
    ax.operation = "ABSOLUTE"
    nt.links.new(sep.outputs["X"], ax.inputs[0])
    t = _maprange(nt, ax.outputs[0], 150.0, 175.0, 0.0, 1.0)
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    cr = ramp.color_ramp
    cr.elements[0].color = (0.17, 0.17, 0.18, 1)       # dark oiled steel
    cr.elements[1].position = 1.0
    cr.elements[1].color = (0.22, 0.26, 0.50, 1)       # blued end
    e = cr.elements.new(0.55)
    e.color = (0.55, 0.42, 0.22, 1)                    # straw
    nt.links.new(t, ramp.inputs[0])
    nt.links.new(ramp.outputs[0], p.inputs["Base Color"])
    return m


def copper_coil():
    """Copper enamel winding: orange-red, glossy, with fine winding grooves
    along the coil axis (the coil object's local Z)."""
    m, nt, p = _new("copper_enamel")
    p.inputs["Base Color"].default_value = (0.80, 0.30, 0.12, 1)
    p.inputs["Metallic"].default_value = 1.0
    p.inputs["Roughness"].default_value = 0.22
    p.inputs["Coat Weight"].default_value = 0.6
    p.inputs["Coat Tint"].default_value = (1.0, 0.55, 0.35, 1)
    coord = nt.nodes.new("ShaderNodeTexCoord").outputs["Object"]
    wave = _node(nt, "ShaderNodeTexWave", Scale=0.55, Distortion=0.4, Detail=1.0)
    wave.bands_direction = "Z"
    wave.wave_profile = "SIN"
    nt.links.new(coord, wave.inputs["Vector"])
    bump = _node(nt, "ShaderNodeBump", Strength=0.6, Distance=0.05)
    nt.links.new(wave.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], p.inputs["Normal"])
    return m


def iron():
    m, nt, p = _new("iron_lamination")
    p.inputs["Base Color"].default_value = (0.20, 0.20, 0.21, 1)
    p.inputs["Metallic"].default_value = 0.8
    p.inputs["Roughness"].default_value = 0.42
    return m


def solid(name, rgb, rough=0.5, metallic=0.0, coat=0.0):
    m, nt, p = _new(name)
    p.inputs["Base Color"].default_value = (*rgb, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metallic
    if coat:
        p.inputs["Coat Weight"].default_value = coat
    return m


def wire(colour):
    rgb = dict(red=(0.55, 0.03, 0.02), white=(0.80, 0.78, 0.72), black=(0.02, 0.02, 0.02),
               yellow=(0.80, 0.55, 0.03))[colour]
    return solid(f"wire_{colour}", rgb, rough=0.35, coat=0.3)


def paper():
    m, nt, p = _new("label_paper")
    p.inputs["Base Color"].default_value = (0.78, 0.74, 0.64, 1)
    p.inputs["Roughness"].default_value = 0.85
    return m
