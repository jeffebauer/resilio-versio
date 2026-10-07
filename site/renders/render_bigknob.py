"""Renders of King Tubby's MCI desk and its Big Knob (the Altec 9069-B
high-pass filter): front on, angled, macros.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
        --python site/renders/render_bigknob.py -- [--only ID,ID] [--samples 160] \
        [--width 2880 --height 1620] [--out DIR] [--test]

PNG stills + index.html into renders/minisite_final/bigknob/ (main checkout,
gitignored). --test renders 960 x 540 at low samples into the scratch dir
given by --out (for comparisons). "match" is a camera that mimics MoPOP's
museum photo (near top-down from the front), used to check the layout.
"""
import math
import os
import sys
import time

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import mci_desk  # noqa: E402

MAIN_CHECKOUT = "/Users/jesse/Documents/Sites/resilio-versio"
FINAL_OUT = os.path.join(MAIN_CHECKOUT, "renders", "minisite_final", "bigknob")
V = C.Vector

# Each shot: camera given in plate-frame terms where that reads best.
#   mood: "studio" (warm-grey sweep, soft key top-left) or "low" (dark, low-key)
SHOTS = [
    dict(id="BK_match", mood="studio", note="Camera mimicking MoPOP's photo: layout check only.",
         aspect=(2000, 1530)),
    dict(id="BK1_front", mood="studio",
         note="Front on: square to the desk at a seated eye-line (camera 1.25 m up, 1.3 m back), "
              "the whole console, the Big Knob top right."),
    dict(id="BK1_front_plate", mood="studio",
         note="Front on, alternative: the camera on the control plate's normal (looking straight "
              "down onto the 14 deg slope); a plan view of the plate."),
    dict(id="BK2_angled", mood="studio",
         note="Angled: three-quarter from the front right, low; the Big Knob near, channels receding."),
    dict(id="BK3_macro_knob", mood="low",
         note="Macro: the Big Knob, its HI PASS FILTER legend and steps; shallow focus on the knob."),
    dict(id="BK4_macro_area", mood="low",
         note="Macro: the Big Knob with its neighbours and the overspray plume."),
    dict(id="BK5_macro_eq", mood="graze",
         note="Macro: raking light across the red EQ knobs (channels 1-6)."),
]


def spec_softbox(ref, cam, target, size, w_metre, dist=None):
    """A softbox where the camera sees its mirror image in the plate, so the
    bare aluminium reads bright and the sprayed paint stays dark (how the
    museum photo reads)."""
    n = ref["normal"]
    t = V(target)
    v = cam.location - t
    r = 2 * v.dot(n) * n - v
    d = dist or v.length
    loc = t + r.normalized() * d
    return C.area_light("spec", loc, t, size=size, power=C.watts(C.KEY_W * w_metre) * (d / 1000) ** 2,
                        spread=40)


def lights(ref, mood, cam=None, spec_target=None):
    c = ref["desk_center"]
    if mood == "studio":
        C.world(0.32)
        C.studio(c, ref["size"], key=1.0, fill=0.28, rim=0.6)
    elif mood == "low":
        C.world(0.01)
        bk = ref["big_knob"]
        n = ref["normal"]
        # soft key from top left, close, so the knob and print read; a rim behind
        C.area_light("key", bk + V((-260, -120, 330)), bk, size=220,
                     power=C.watts(C.KEY_W * 0.12) * 0.45 ** 2, spread=60)
        C.area_light("rim", bk + V((180, 420, 240)), bk, size=160,
                     power=C.watts(C.KEY_W * 0.25) * 0.5 ** 2, spread=50)
        C.area_light("fill", bk + V((320, -260, 90)), bk, size=300,
                     power=C.watts(C.KEY_W * 0.02) * 0.45 ** 2, spread=70)
        C.area_light("top", bk + n * 380, bk, size=260,
                     power=C.watts(C.KEY_W * 0.03) * 0.38 ** 2, spread=60)
        if cam is not None:
            spec_softbox(ref, cam, spec_target or bk, 700, 0.05, dist=900)
    elif mood == "graze":
        C.world(0.02)
        f = ref["frame"]
        tgt = f.world(170, 340, 0)
        # a strip low across the plate from the left: long shadows, fluting lit
        C.area_light("graze", f.world(-260, 330, 45), tgt, size=60, size_y=400,
                     power=C.watts(C.KEY_W * 0.55) * 0.43 ** 2, spread=35)
        C.area_light("top", f.world(170, 300, 600), tgt, size=500,
                     power=C.watts(C.KEY_W * 0.015) * 0.6 ** 2, spread=60)
        if cam is not None:
            spec_softbox(ref, cam, tgt, 700, 0.03, dist=900)


def camera_for(shot, ref):
    f = ref["frame"]
    n = ref["normal"]
    sid = shot["id"]
    if sid == "BK_match":
        # fitted to the plate corners + fader caps + the Big Knob in the photo
        # (pinhole, no yaw/roll, centred principal point): rms 7 px at 2000 px
        cam = C.camera(sid, V((0, 0, 0)), 0, 0, 61.5, 1.0)
        p = math.radians(51.7)
        cam.location = f.world(472, -1103, 1776)
        C.aim(cam, f.world(472, -1103 + 2000 * math.cos(p), 1776 - 2000 * math.sin(p)))
        return cam
    if sid == "BK1_front":
        return C.camera(sid, V((0, 130, 320)), 0, 30, 50, 2650, shift=(0, -0.012))
    if sid == "BK1_front_plate":
        tgt = f.world(475, 300, 0)
        cam = C.camera(sid, tgt, 0, 90 - mci_desk.SLOPE, 50, 1900)
        return cam
    if sid == "BK2_angled":
        tgt = f.world(690, 400, 30)
        return C.camera(sid, tgt, 36, 23, 35, 1280, shift=(-0.03, 0.005))
    if sid == "BK3_macro_knob":
        tgt = f.world(828, 522, 10)
        return C.camera(sid, tgt, -12, 44, 100, 400, fstop=11, focus=f.world(830, 515, 6))
    if sid == "BK4_macro_area":
        tgt = f.world(790, 420, 0)
        return C.camera(sid, tgt, -10, 42, 70, 950, fstop=8, focus=f.world(800, 450, 10))
    if sid == "BK5_macro_eq":
        tgt = f.world(170, 340, 6)
        return C.camera(sid, tgt, -8, 30, 100, 640, fstop=8, focus=f.world(170, 345, 10))
    raise KeyError(sid)


def ground(mood):
    dark = mood in ("low", "graze")
    rgb = (0.012, 0.011, 0.010) if dark else C.GROUND_RGB
    return C.cyclorama(width=24000, front=6000, back=1400, height=6000, radius=1200, rgb=rgb,
                       texture=not dark)


def write_index(out_dir, done):
    cards = "\n".join(
        f'<figure><a href="{s["id"]}.png"><img src="{s["id"]}.png" loading="lazy" alt="{s["id"]}"></a>'
        f'<figcaption><b>{s["id"]}</b><span>{s["note"]}</span></figcaption></figure>'
        for s in done)
    html = f"""<!doctype html><html lang=en><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Big Knob renders</title>
<style>
:root{{--paper:#f6f5f3;--ink:#111;--ink2:#5c5853;--rule:#d8d4cf}}
body{{margin:0;background:var(--paper);color:var(--ink);font:15px/1.35 -apple-system,system-ui,sans-serif}}
header,section{{padding:16px;border-top:1px solid var(--rule)}}
h1{{font-weight:400;font-size:26px;margin:0 0 4px}} p{{margin:0;color:var(--ink2);max-width:60em}}
.grid{{display:grid;grid-template-columns:repeat(auto-fill,minmax(min(100%,420px),1fr));gap:20px}}
figure{{margin:0}} img{{width:100%;display:block;border-radius:6px;background:#ddd}}
figcaption{{display:flex;flex-direction:column;gap:2px;padding-top:6px;font-size:13px;color:var(--ink2)}}
figcaption b{{font-weight:500;color:var(--ink);font-family:ui-monospace,Menlo,monospace}}
</style></head><body>
<header><h1>King Tubby's MCI desk: the Big Knob</h1>
<p>The Altec 9069-B high-pass filter ("HI PASS FILTER") in Tubby's mid-1960s MCI console, modelled from
MoPOP's museum photo. Generated {time.strftime('%Y-%m-%d %H:%M')} by site/renders/render_bigknob.py.</p></header>
<section><div class=grid>{cards}</div></section></body></html>"""
    with open(os.path.join(out_dir, "index.html"), "w") as fh:
        fh.write(html)


def sharpen(p):
    """The light luma-only unsharp mask render_final.py uses on the stills."""
    import subprocess
    tmp = p + ".sharp.png"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", p, "-vf", "unsharp=5:5:0.6:5:5:0", tmp],
                   check=True)
    os.replace(tmp, p)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []

    def opt(k, d=None):
        return argv[argv.index(k) + 1] if k in argv else d

    test = "--test" in argv
    width = int(opt("--width", 960 if test else 2880))
    height = int(opt("--height", 540 if test else 1620))
    samples = int(opt("--samples", 24 if test else 160))
    out = opt("--out", FINAL_OUT)
    only = [s for s in opt("--only", "").split(",") if s]
    shots = [s for s in SHOTS if (s["id"] in only if only else s["id"] != "BK_match")]
    os.makedirs(out, exist_ok=True)
    done = []
    for shot in shots:
        C.reset_scene()
        w, h = width, height
        if shot.get("aspect"):
            aw, ah = shot["aspect"]
            h = int(w * ah / aw)
        C.setup_cycles(samples, w, h)
        ref = mci_desk.build()
        ground(shot["mood"])
        cam = camera_for(shot, ref)
        lights(ref, shot["mood"], cam)
        path = os.path.join(out, shot["id"] + ".png")
        C.render(cam, path)
        if not test:
            sharpen(path)
        done.append(shot)
    if not test:
        write_index(out, [s for s in SHOTS if os.path.exists(os.path.join(out, s["id"] + ".png"))
                          and s["id"] != "BK_match"])


if __name__ == "__main__":
    main()
