"""The animated hero: the monolith emerging from the dark.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
        --python site/renders/render_hero.py -- --frames DIR [--samples 48] [--only 1,60,120]

Timeline (30 fps, 1920 x 1080):
  frames   1-210  hero_reveal (7 s): starts near-black, the only light the four
                  LEDs; from frame 50 the studio light comes up while the camera
                  pushes in and tilts down to the floor-level R7b angle, landing
                  by frame 200 on R7b's composition.
  frames 211-306  hero_hold_loop (3.2 s = one bar): camera and light still, only
                  the LEDs flicker. The LED pattern repeats every bar (96 frames),
                  so frame 307 would equal frame 211: the loop is seamless.

LEDs, hand-animated as a dub groove at 75 bpm (a beat = 24 frames):
  LED1/LED2 (IN L/R)   the skank on beats 2 and 4; beat 4 is the louder hit.
  LED3/LED4 (OUT L/R)  the dry hit, then ping-pong echoes every dotted eighth
                       (18 frames), each 0.62x the last, alternating L and R.
  Colour: green at normal level, amber on the loud hits. Each LED also drives
  a tiny point light so its glow spills onto the knobs and the panel.

Frames are written as JPEG (quality 95) to keep the sequence small; encode
with make_hero_video.sh.
"""
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402
import render_clay  # noqa: E402

FPS = 30
BEAT = 24                 # frames per beat at 75 bpm
BAR = 4 * BEAT            # 96 frames = the loop period
REVEAL_END = 210
HOLD_START, HOLD_END = 211, 306
LIGHTS_ON = (50, 195)     # studio light ramps up over these frames
CAM_MOVE = (1, 200)

GREEN = (0.06, 1.0, 0.14)
AMBER = (1.0, 0.40, 0.02)

# (beat-position-in-frames, level) per bar for the input skank
SKANK = [(BEAT * 1, 0.62), (BEAT * 3, 1.0)]
ECHO_DELAY, ECHO_FB, ECHO_N = 18, 0.62, 8


def _pulse(dt):
    """Fast attack, ~5-frame decay. dt in frames (>= 0)."""
    if dt < 0:
        return 0.0
    return (min(1.0, (dt + 1) / 2.0)) * math.exp(-dt / 5.0)


def _periodic(f, hits):
    """Sum of pulses for hits [(frame_in_bar, level)], repeated every bar."""
    total = 0.0
    for bar in range(-4, 1):
        base = (f // BAR + bar) * BAR
        for t, lvl in hits:
            total += lvl * _pulse(f - (base + t))
    return total


def led_levels(f):
    """Levels for LED1..LED4 at frame f (periodic in BAR)."""
    in_l = _periodic(f, SKANK)
    in_r = _periodic(f, [(t + 1, l * 0.88) for t, l in SKANK])
    out = {3: [], 4: []}
    for t, lvl in SKANK:
        out[3].append((t, lvl * 0.45))
        out[4].append((t, lvl * 0.40))
        for k in range(1, ECHO_N + 1):
            out[3 if k % 2 else 4].append((t + ECHO_DELAY * k, lvl * 0.85 * ECHO_FB ** (k - 1)))
    return [in_l, in_r, _periodic(f, out[3]), _periodic(f, out[4])]


def _colour(level):
    k = min(1.0, max(0.0, (level - 0.72) / 0.18))
    k = k * k * (3 - 2 * k)
    return tuple(g + (a - g) * k for g, a in zip(GREEN, AMBER))


def _ease(f, f0, f1):
    k = min(1.0, max(0.0, (f - f0) / (f1 - f0)))
    return k * k * (3 - 2 * k)


def build():
    ref = render_clay.build_module("monolith")
    sc = bpy.context.scene
    # every light the monolith mood made: ramp from 0 to its full energy
    for ob in [o for o in bpy.data.objects if o.type == "LIGHT"]:
        full = ob.data.energy
        for f in (1, LIGHTS_ON[0]):
            ob.data.energy = 0.0
            ob.data.keyframe_insert("energy", frame=f)
        ob.data.energy = full
        ob.data.keyframe_insert("energy", frame=LIGHTS_ON[1])
    bg = sc.world.node_tree.nodes["Background"].inputs["Strength"]
    full = bg.default_value
    for f in (1, LIGHTS_ON[0]):
        bg.default_value = 0.0
        bg.keyframe_insert("default_value", frame=f)
    bg.default_value = full
    bg.keyframe_insert("default_value", frame=LIGHTS_ON[1])

    # LEDs: one material + one spill light each, keyed every frame
    leds = []
    for i in range(1, 5):
        lens = bpy.data.objects[f"LED{i}_lens"]
        m = bpy.data.materials.new(f"led_anim_{i}")
        m.use_nodes = True
        p = m.node_tree.nodes["Principled BSDF"]
        p.inputs["Base Color"].default_value = (0.05, 0.05, 0.05, 1)
        p.inputs["Roughness"].default_value = 0.25
        C.assign(lens, m)
        h = ref["holes"][f"LED{i}"]
        ld = bpy.data.lights.new(f"led_spill_{i}", "POINT")
        ld.shadow_soft_size = 1.2
        lo = bpy.data.objects.new(f"led_spill_{i}", ld)
        C.link(lo)
        lo.location = (h["x"], -3.0, h["z"])
        leds.append((p, ld))
    for f in range(1, HOLD_END + 1):
        for (p, ld), lvl in zip(leds, led_levels(f)):
            lvl = min(1.25, lvl)
            col = _colour(lvl)
            p.inputs["Emission Color"].default_value = (*col, 1)
            p.inputs["Emission Strength"].default_value = 0.15 + 22.0 * lvl
            ld.color = col
            ld.energy = C.watts(0.0035) * lvl
            p.inputs["Emission Color"].keyframe_insert("default_value", frame=f)
            p.inputs["Emission Strength"].keyframe_insert("default_value", frame=f)
            ld.keyframe_insert("color", frame=f)
            ld.keyframe_insert("energy", frame=f)

    # camera: from the R8_emerge view (85 mm, 590 mm away) to R7b's floor angle (24 mm)
    c = ref["center"]
    start_t = c + C.Vector((0, 0, 20))
    az, el, d = math.radians(22), math.radians(10), 590.0
    start = start_t + d * C.Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
    end, end_t = C.Vector((60, -140, 5)), C.Vector((0, 6, 62))
    tgt = bpy.data.objects.new("cam_target", None)
    C.link(tgt)
    cd = bpy.data.cameras.new("hero")
    cd.sensor_width = 36.0
    cd.clip_start, cd.clip_end = 1.0, 100000.0
    cam = bpy.data.objects.new("hero", cd)
    C.link(cam)
    con = cam.constraints.new("TRACK_TO")
    con.target = tgt
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    for f in range(CAM_MOVE[0], CAM_MOVE[1] + 1, 2):
        k = _ease(f, *CAM_MOVE)
        cam.location = start.lerp(end, k)
        tgt.location = start_t.lerp(end_t, k)
        cd.lens = 85.0 + (24.0 - 85.0) * k
        cam.keyframe_insert("location", frame=f)
        tgt.keyframe_insert("location", frame=f)
        cd.keyframe_insert("lens", frame=f)
    sc.camera = cam
    sc.frame_start, sc.frame_end = 1, HOLD_END
    sc.render.fps = FPS
    return ref


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = argv[argv.index("--frames") + 1]
    samples = int(argv[argv.index("--samples") + 1]) if "--samples" in argv else 48
    only = [int(x) for x in argv[argv.index("--only") + 1].split(",")] if "--only" in argv else None
    build()
    C.setup_cycles(samples=samples)
    sc = bpy.context.scene
    sc.render.use_persistent_data = True
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 95
    os.makedirs(out, exist_ok=True)
    frames = only or range(sc.frame_start, sc.frame_end + 1)
    import time
    t0 = time.time()
    for f in frames:
        sc.frame_set(f)
        sc.render.filepath = os.path.join(out, f"f_{f:04d}.jpg")
        bpy.ops.render.render(write_still=True)
        print(f"[hero] frame {f} done, {time.time() - t0:.0f} s elapsed", flush=True)


if __name__ == "__main__":
    main()
