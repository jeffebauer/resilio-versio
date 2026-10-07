"""The animated hero: the monolith emerging from the dark.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
        --python site/renders/render_hero.py -- --frames DIR [--samples 48] [--only 1,60,120]

Timeline (30 fps, 1920 x 1080), v2 "product film" (owner, 7 Oct):
  frames   1-240  hero_reveal (8 s). The camera starts low and close in front of
                  the module, looking steeply up so it looms; it is near-black,
                  the LEDs the only light. One continuous jib-like move: the
                  camera dollies back while rising slightly and tilting down,
                  ending square to the panel on R7a's composition. Quintic
                  ease-in-out (smootherstep) for a long, soft settle, no overshoot.
                  Light in three stages: dark -> the rim and top lights trace the
                  edges and knob caps (frames 50-150) -> the soft key, fills and
                  world come up to the R7a monolith look (frames 110-225).
  frames 241-336  hero_hold_loop (3.2 s = one bar): camera and light at rest, as
                  in the R7a still; only the LEDs flicker. The LED pattern repeats
                  every bar (96 frames), so frame 337 would equal 241: seamless.

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
import module  # noqa: E402
import render_clay  # noqa: E402

FPS = 30
BEAT = 24                 # frames per beat at 75 bpm
BAR = 4 * BEAT            # 96 frames = the loop period
REVEAL_END = 240
HOLD_START, HOLD_END = 241, 336
EDGES_ON = (50, 150)      # rim + top light: the edges and knob caps appear first
LIGHTS_ON = (110, 225)    # then the soft key, fills and world
CAM_MOVE = (1, 236)
EDGE_LIGHTS = ("rim", "top")

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
    """Smootherstep: zero velocity and acceleration at both ends."""
    k = min(1.0, max(0.0, (f - f0) / (f1 - f0)))
    return k * k * k * (k * (6 * k - 15) + 10)


def build():
    ref = render_clay.build_module("monolith")
    sc = bpy.context.scene
    # every light the monolith mood made ramps from 0 to its full (R7a) energy;
    # the edge lights first, the key and fills later. Keyed every frame with an
    # eased curve so the light grows smoothly, at rest by the hold.
    for ob in [o for o in bpy.data.objects if o.type == "LIGHT"]:
        full = ob.data.energy
        span = EDGES_ON if ob.name in EDGE_LIGHTS else LIGHTS_ON
        for f in range(1, REVEAL_END + 1, 3):
            ob.data.energy = full * _ease(f, *span)
            ob.data.keyframe_insert("energy", frame=f)
        ob.data.energy = full
        ob.data.keyframe_insert("energy", frame=REVEAL_END)
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

    # camera: low and close looking steeply up -> level with the panel's centre,
    # square to it (zero tilt, zero yaw: no keystoning), 100 mm lens, the panel
    # ~72 % of the frame height. The camera dollies back and rises; the target
    # slides down to the panel centre so the tilt reaches exactly 0 as it settles.
    mid = module.PH / 2
    end_dist = (module.PH / 0.72) * 100.0 / (36.0 / (16 / 9))   # 100 mm lens, 72 % of frame height
    start, start_t = C.Vector((0, -62, 2.5)), C.Vector((0, 0, 118))
    end, end_t = C.Vector((0, -end_dist, mid)), C.Vector((0, 0, mid))
    lens0, lens1 = 20.0, 100.0
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
    # focus on the panel face: a slight pull early, sharp (f/16) once readable
    cd.dof.use_dof = True
    cd.dof.aperture_fstop = 16 * bpy.context.scene.unit_settings.scale_length
    for f in range(CAM_MOVE[0], CAM_MOVE[1] + 1):
        k = _ease(f, *CAM_MOVE)
        cam.location = start.lerp(end, k)
        tgt.location = start_t.lerp(end_t, k)
        # lens tracks the dolly so the module's apparent size shrinks steadily
        # (a pure pull-back feel, no zoom-out-then-in): lens = lens1 x dist/end_dist,
        # times an eased factor that is lens0's excess at the start and 1 at the end
        dist = (end_t - cam.location).length
        excess0 = lens0 / (lens1 * (start_t - start).length / (end_t - end).length)
        cd.lens = lens1 * dist / (end_t - end).length * excess0 ** (1 - k)
        cd.dof.focus_distance = (C.Vector((0, 0, 64)) - cam.location).length
        cd.dof.keyframe_insert("focus_distance", frame=f)
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
    samples = int(argv[argv.index("--samples") + 1]) if "--samples" in argv else 64
    only = [int(x) for x in argv[argv.index("--only") + 1].split(",")] if "--only" in argv else None
    scale = int(argv[argv.index("--scale") + 1]) if "--scale" in argv else 100
    build()
    C.setup_cycles(samples=samples)
    bpy.context.scene.render.resolution_percentage = scale
    sc = bpy.context.scene
    sc.render.use_persistent_data = True
    sc.render.image_settings.file_format = "JPEG"
    sc.render.image_settings.quality = 95
    os.makedirs(out, exist_ok=True)
    if "--still" in argv:
        # the site's hero still: the hold frame with the LEDs on the loud hit,
        # 2880 x 1620, high samples, sharpened like the other finals
        f, path = int(argv[argv.index("--still") + 1]), argv[argv.index("--still") + 2]
        sc.render.resolution_x, sc.render.resolution_y = 2880, 1620
        sc.render.image_settings.file_format = "PNG"
        sc.frame_set(f)
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        import subprocess
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", path, "-vf", "unsharp=5:5:0.7:5:5:0",
                        path + ".s.png"], check=True)
        os.replace(path + ".s.png", path)
        print(f"[hero] still {path}")
        return
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
