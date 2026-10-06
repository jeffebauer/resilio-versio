"""Clay pass for composition review (PLAN A3, step S3): renders every variant
of R1-R5 and writes a contact sheet.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
        --python site/renders/render_clay.py -- [--only R1,R3] [--scale 50] \
        [--samples 64] [--out DIR]

Default output: renders/minisite_clay/ in the main checkout (gitignored):
one 1920 x 1080 PNG per variant + index.html (labelled grid, click to enlarge)
+ variants.json (the cameras, for the lit pass to reuse).

Clay = grey matte materials, real lighting (common.studio), the panel print
on as a decal, LEDs lit in the signal red.

Camera labels: az = azimuth from the front towards the right, el = elevation
above the horizon, h = camera height above the ground, then the focal length
(full-frame equivalent) and the f-stop when depth of field is on.
"""
import argparse
import json
import math
import os
import sys
import time

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import module  # noqa: E402
import tank  # noqa: E402

MAIN_CHECKOUT = "/Users/jesse/Documents/Sites/resilio-versio"
DEFAULT_OUT = os.path.join(MAIN_CHECKOUT, "renders", "minisite_clay")


# ------------------------------------------------------------------ variants
# Each variant: id, shot, what it shows, and a camera recipe. Targets are named
# reference points returned by tank.build() / module.build(), or callables.

def tank_variants(ref):
    c = ref["center"]
    R = ref["radius"]
    return [
        dict(id="R1_tank_a", shot="R1", note="Three-quarter from above, whole tank: the reference hero.",
             target=c, az=26, el=34, f=70, fit=0.86, by="width"),
        dict(id="R1_tank_b", shot="R1", note="High and steep, looking down into the springs.",
             target=c, az=14, el=58, f=85, fit=0.88, by="width"),
        dict(id="R1_tank_c", shot="R1", note="Low from the output end, wide lens: the springs run away down the tank.",
             target=c + C.Vector((70, 0, 0)), az=74, el=22, f=50, fit=1.35, by="width"),
        dict(id="R1_tank_d", shot="R1", note="Tight 100 mm on the input end, tank runs out of frame.",
             target=C.Vector((-110, 0, 16)), az=24, el=42, f=100, fit=2.0, by="width"),
        dict(id="R2_tank_macro_a", shot="R2", note="Macro: coils catching the light, shallow depth.",
             target=ref["spring_quarter"], az=55, el=36, f=100, dist=170, fstop=16),
        dict(id="R2_tank_macro_b", shot="R2", note="Macro: the output transducer, springs hooking onto the magnets.",
             target=ref["transducer_out"] + C.Vector((-12, 0, 6)), az=-58, el=34, f=100, dist=260, fstop=8,
             focus=ref["transducer_out"] + C.Vector((-15.5, 0, 9))),
    ]


def module_variants(ref):
    c = ref["center"]
    at = ref["at"]
    v = [
        dict(id="R3_module_a", shot="R3", note="Three-quarter from the right, just above eye level: the reference.",
             target=c, az=34, el=12, f=85, fit=1.0),
        dict(id="R3_module_b", shot="R3", note="Higher and more turned: more of the PCB stack behind.",
             target=c, az=46, el=26, f=70, fit=1.0),
        dict(id="R3_module_c", shot="R3", note="Low, nearly level, long lens: monumental, flat perspective.",
             target=c, az=28, el=4, f=100, fit=1.0),
        dict(id="R3_module_d", shot="R3", note="From the left, 60 mm: the other side, slightly more drama.",
             target=c, az=-38, el=15, f=60, fit=1.0),
    ]
    for k, az in zip("abc", (24, 34, 44)):
        v.append(dict(id=f"R4_module_turn_{k}", shot="R4",
                      note=f"Turn sequence key frame (R3a orbit, az {az} deg): first / middle / last of ~20 deg.",
                      target=c, az=az, el=12, f=85, fit=1.0))
    v += [
        dict(id="R5_panel_macro_a", shot="R5", note="Macro: TANK and ATTITUDE toggles with SPLASH and WOBBLE.",
             target=(at("SW1") + at("P6")) / 2 + C.Vector((0, -6, -2)), az=30, el=16, f=100, dist=175, fstop=5.6,
             focus=at("SW1", -10)),
        dict(id="R5_panel_macro_b", shot="R5", note="Macro: BLEND and the four lit LEDs under the title.",
             target=(at("P1") + at("LED4")) / 2 + C.Vector((0, -6, 0)), az=-24, el=26, f=100, dist=170, fstop=5.6,
             focus=at("LED2", -1)),
    ]
    PH = module.PH
    v += [
        dict(id="R6_front_a", shot="R6", note="Front-on, 200 mm: flat, almost orthographic product view.",
             target=c, az=0, el=1, f=200, fit=1.08),
        dict(id="R6_front_b", shot="R6", note="Front-on with a 7° turn, 135 mm: just enough depth to show the knobs and stack.",
             target=c, az=7, el=3, f=135, fit=1.04),
        dict(id="R6_front_c", shot="R6", mood="gloss",
             note="Standing on a semi-gloss floor: its shadow and a soft reflection, 135 mm.",
             target=c + C.Vector((0, 0, -16)), az=0, el=2, f=135, fit=0.84),
        dict(id="R7_monolith_a", shot="R7", mood="monolith",
             note="Monolith, dead centre: camera on the floor, 24 mm looking up; top edge glows, LEDs are the focus.",
             loc=(0, -170, 6), target=(0, 0, 66), f=24),
        dict(id="R7_monolith_b", shot="R7", mood="monolith",
             note="Monolith, off-axis: 24 mm, closer and turned 25°, stronger convergence.",
             loc=(60, -140, 5), target=(0, 6, 62), f=24),
        dict(id="R7_monolith_c", shot="R7", mood="monolith",
             note="Monolith, 35 mm from further back: calmer verticals, more dark floor.",
             loc=(-20, -230, 8), target=(0, 0, 60), f=35),
        dict(id="R8_graze", shot="R8", mood="graze",
             note="Raking light down the panel face, near side-on 100 mm: the print and knob shadows rake.",
             target=at("P6") + C.Vector((4, -2, 4)), az=50, el=24, f=100, dist=200, fstop=11,
             focus=at("P6", -2)),
        dict(id="R8_emerge", shot="R8", mood="dark",
             note="Half out of the dark: only the top edge and the four LEDs lit, 85 mm.",
             target=c + C.Vector((0, 0, 20)), az=22, el=10, f=85, fit=1.0),
        dict(id="R8_cable", shot="R8", mood="cable",
             note="A patch cable in IN L, its cord leaving frame: the module in use, 70 mm.",
             target=c + C.Vector((0, -10, -25)), az=30, el=14, f=70, fit=1.12),
    ]
    return v


# ------------------------------------------------------------------ scenes

def build_tank(mood=None):
    C.reset_scene()
    ref = tank.build(dict(finish=mood) if mood else None)
    C.cyclorama(width=5000, front=2500, back=700, height=3000, radius=900, texture=True)
    C.world(0.4)
    C.studio(ref["center"], ref["size"], key=1.0, fill=0.22, rim=0.55, streak=ref["streak"])
    return ref


# Module moods: "studio" (default), "gloss" (semi-gloss floor for a reflection),
# "monolith" (dark backdrop, strong top/back light, face falls into shadow),
# "dark" (only the top edge and the LEDs lit), "graze" (raking light down the
# panel face), "cable" (studio + a patch cable in IN L).
DARK_RGB = (0.035, 0.033, 0.031)


def build_module(mood=None):
    mood = mood or "studio"
    C.reset_scene()
    ref = module.build()
    dark = mood in ("monolith", "dark")
    cyc = C.cyclorama(width=2400 if not dark else 6000, front=1200 if not dark else 3000, back=260,
                      height=1600, radius=380, rgb=DARK_RGB if dark else C.GROUND_RGB)
    c = ref["center"]
    if mood == "gloss":
        p = cyc.data.materials[0].node_tree.nodes["Principled BSDF"]
        p.inputs["Roughness"].default_value = 0.22
    if mood in ("studio", "gloss", "cable"):
        C.world(0.32)
        C.studio(c, ref["size"] * 1.6, key=1.0, fill=0.25, rim=0.6)
    elif mood == "monolith":
        C.world(0.012)
        L = C.studio(c, ref["size"] * 1.6, key=0.10, fill=0.02, rim=1.6)
        # the top/back light: just behind and above the top edge, raking forward
        top = C.Vector((0, 60, module.PH + 120))
        C.area_light("top", top, C.Vector((0, 0, module.PH - 10)), size=120,
                     power=C.watts(C.KEY_W * 8.0) * (0.15) ** 2)
        # a faint fill from high in front so the face falls off towards the bottom
        L["key"].location = C.Vector((-60, -260, 420))
        C.aim(L["key"], c + C.Vector((0, 0, 50)))
    elif mood == "dark":
        C.world(0.003)
        top = C.Vector((0, 35, module.PH + 70))
        C.area_light("top", top, C.Vector((0, 4, module.PH)), size=30,
                     power=C.watts(C.KEY_W * 1.5) * (0.08) ** 2, spread=40)
    elif mood == "graze":
        C.world(0.08)
        # a long strip just in front of the panel, above it, shining down the face
        C.area_light("graze", C.Vector((0, -14, module.PH + 160)), C.Vector((0, -6, 40)),
                     size=80, size_y=8, power=C.watts(C.KEY_W * 1.2) * (0.17) ** 2, spread=25)
        C.studio(c, ref["size"] * 1.6, key=0.0, fill=0.06, rim=0.3)
    if mood == "cable":
        module.patch_cable(ref, "J9")
    return ref


def camera_for(v, ref):
    if v.get("loc") is not None:
        return _camera_at(v)
    dist = v.get("dist") or C.fit_distance(ref["radius"], v["f"], v["fit"], by=v.get("by", "height"))
    cam = C.camera(v["id"], v["target"], v["az"], v["el"], v["f"], dist,
                   fstop=v.get("fstop"), focus=v.get("focus"), shift=v.get("shift", (0.0, 0.0)))
    v["dist_mm"] = round(dist)
    v["height_mm"] = round(cam.location.z)
    v["label"] = (f"h {v['height_mm']} mm · el {v['el']}° · az {v['az']}° · {v['f']} mm"
                  + (f" · f/{v['fstop']}" if v.get("fstop") else "") + f" · {v['dist_mm']} mm away")
    return cam


def _camera_at(v):
    """Camera at an explicit position (floor-level shots), aimed at target."""
    loc, tgt = C.Vector(v["loc"]), C.Vector(v["target"])
    d = tgt - loc
    tilt = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    az = math.degrees(math.atan2(loc.x - tgt.x, tgt.y - loc.y))  # from the front, towards +X
    cam = C.camera(v["id"], tgt, 0, 0, v["f"], 1.0, fstop=v.get("fstop"), focus=v.get("focus"),
                   shift=v.get("shift", (0.0, 0.0)))
    cam.location = loc
    C.aim(cam, tgt)
    if v.get("fstop"):
        cam.data.dof.focus_distance = ((C.Vector(v["focus"]) if v.get("focus") is not None else tgt) - loc).length
    v["az"], v["el"] = round(az), round(tilt)
    v["dist_mm"] = round(d.length)
    v["height_mm"] = round(loc.z)
    v["label"] = (f"h {v['height_mm']} mm · tilt up {round(tilt)}° · az {round(az)}° · {v['f']} mm"
                  + (f" · f/{v['fstop']}" if v.get("fstop") else "") + f" · {v['dist_mm']} mm to target")
    return cam


# ------------------------------------------------------------------ contact sheet

SHOTS = {
    "R1": "R1 Spring tank, hero",
    "R2": "R2 Spring tank, macros",
    "R3": "R3 Module, three-quarter",
    "R4": "R4 Module turn (key frames)",
    "R5": "R5 Panel macros",
    "R6": "R6 Module, front-on",
    "R7": "R7 Module, monolith (from the floor)",
    "R8": "R8 Module, hero ideas",
}


def write_index(out_dir, variants):
    groups = []
    for shot, title in SHOTS.items():
        items = [v for v in variants if v["shot"] == shot]
        if not items:
            continue
        cards = "\n".join(
            f'''<figure><a href="{v["id"]}.png" data-full="{v["id"]}.png" data-cap="{v["id"]} — {v["label"]}">
<img src="{v["id"]}.png" loading="lazy" alt="{v["id"]}"></a>
<figcaption><b>{v["id"]}</b><span class="cam">{v["label"]}</span><span>{v["note"]}</span></figcaption></figure>'''
            for v in items)
        groups.append(f"<section><h2>{title}</h2><div class=grid>{cards}</div></section>")
    html = f"""<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clay renders</title>
<style>
:root{{--paper:#f6f5f3;--ink:#111;--ink2:#5c5853;--rule:#d8d4cf}}
body{{margin:0;background:var(--paper);color:var(--ink);font:15px/1.35 -apple-system,system-ui,sans-serif}}
header,section{{padding:16px 24px;border-top:1px solid var(--rule)}}
h1{{font-weight:400;font-size:28px;margin:0 0 4px}} h2{{font-weight:400;font-size:18px;margin:0 0 12px}}
p{{margin:0;color:var(--ink2);max-width:60em}}
.grid{{display:grid;grid-template-columns:repeat(auto-fill,minmax(420px,1fr));gap:20px}}
figure{{margin:0}} img{{width:100%;display:block;border-radius:6px;background:#ddd}}
figcaption{{display:flex;flex-direction:column;gap:2px;padding-top:6px;font-size:13px;color:var(--ink2)}}
figcaption b{{font-weight:500;color:var(--ink);font-family:ui-monospace,Menlo,monospace}}
.cam{{font-family:ui-monospace,Menlo,monospace;font-size:12px}}
#lb{{position:fixed;inset:0;background:rgba(20,18,16,.92);display:none;flex-direction:column;align-items:center;justify-content:center;gap:10px;cursor:zoom-out}}
#lb img{{max-width:96vw;max-height:88vh;width:auto;border-radius:0}} #lb div{{color:#eee;font:13px ui-monospace,Menlo,monospace}}
#lb.on{{display:flex}}
</style></head><body>
<header><h1>Minisite renders — clay pass</h1>
<p>Composition review (PLAN A3 / S3). Grey matte clay with bare-steel metals (tank, springs, nuts, toggles), real lighting plus a strip light along the springs (soft key top-left, fill right, rim behind,
warm-grey sweep), the panel print on as a decal, LEDs lit in signal red. Click a frame to enlarge; ← → to step.
Camera: h = height above the ground, el = elevation, az = azimuth from the front towards the right, focal length
full-frame equivalent. Generated {time.strftime('%Y-%m-%d %H:%M')} by site/renders/render_clay.py.</p></header>
{''.join(groups)}
<div id=lb><img alt=""><div></div></div>
<script>
const links=[...document.querySelectorAll('a[data-full]')],lb=document.getElementById('lb');let i=0;
function show(k){{i=(k+links.length)%links.length;lb.querySelector('img').src=links[i].dataset.full;
lb.querySelector('div').textContent=links[i].dataset.cap;lb.classList.add('on')}}
links.forEach((a,k)=>a.addEventListener('click',e=>{{e.preventDefault();show(k)}}));
lb.addEventListener('click',()=>lb.classList.remove('on'));
addEventListener('keydown',e=>{{if(!lb.classList.contains('on'))return;
if(e.key==='Escape')lb.classList.remove('on');if(e.key==='ArrowRight')show(i+1);if(e.key==='ArrowLeft')show(i-1)}});
</script></body></html>"""
    with open(os.path.join(out_dir, "index.html"), "w") as f:
        f.write(html)


# ------------------------------------------------------------------ main

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="", help="comma list of shot or variant id prefixes, e.g. R1,R3_module_a")
    ap.add_argument("--scale", type=int, default=100, help="resolution percentage (tests)")
    ap.add_argument("--samples", type=int, default=64)
    ap.add_argument("--out", default=DEFAULT_OUT)
    a = ap.parse_args(argv)
    only = [s for s in a.only.split(",") if s]
    want = (lambda vid: any(vid.startswith(s) for s in only)) if only else (lambda vid: True)

    manifest_path = os.path.join(a.out, "variants.json")
    done = {}
    if os.path.exists(manifest_path):
        with open(manifest_path) as f:
            done = {v["id"]: v for v in json.load(f)}

    os.makedirs(a.out, exist_ok=True)
    for builder, varfn in ((build_tank, tank_variants), (build_module, module_variants)):
        probe = [v for v in varfn(_probe_ref(builder)) if want(v["id"])]
        moods = []
        for v in probe:  # one scene build per mood, in first-seen order
            if v.get("mood") not in moods:
                moods.append(v.get("mood"))
        for mood in moods:
            ref = builder(mood)
            C.setup_cycles(samples=a.samples)
            bpy.context.scene.render.resolution_percentage = a.scale
            for v in varfn(ref):
                if not want(v["id"]) or v.get("mood") != mood:
                    continue
                cam = camera_for(v, ref)
                t0 = time.time()
                C.render(cam, os.path.join(a.out, v["id"] + ".png"))
                v["render_s"] = round(time.time() - t0, 1)
                print(f"[clay] {v['id']}: {v['render_s']} s  ({v['label']})")
                done[v["id"]] = {k: (list(x) if hasattr(x, "to_tuple") else x) for k, x in v.items()}
    ordered = sorted(done.values(), key=lambda v: v["id"])
    with open(manifest_path, "w") as f:
        json.dump(ordered, f, indent=1, default=str)
    write_index(a.out, ordered)
    print(f"[clay] contact sheet: {os.path.join(a.out, 'index.html')}")


def _probe_ref(builder):
    """Variant ids without building the scene (targets are dummies)."""
    from mathutils import Vector
    dummy = dict(center=Vector(), radius=1, size=1, spring_mid=Vector(), spring_quarter=Vector(),
                 transducer_out=Vector(), at=lambda *_a: Vector())
    return dummy


if __name__ == "__main__":
    main()
