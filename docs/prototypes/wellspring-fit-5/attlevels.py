#!/usr/bin/env python3
"""Round 5 fix-up: the wet's level per ATTITUDE (CLEAN / DRIVEN / KICKED), 7 vs 8, on held noise
(test_drive's Morph check material: noise 0.1 peak, DRIVE 0.6, DECAY 0.7) and on 02_hits (TONE noon /
fully left), 2 Springs, MIX 1. Prints dB re CLEAN per voicing.
  python3 docs/prototypes/wellspring-fit-5/attlevels.py [tune] [voicing ...]
"""
import os, random, struct, subprocess, sys, wave
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5  # noqa: E402

noise = r5.WORK / "heldnoise.wav"
r5.WORK.mkdir(parents=True, exist_ok=True)
if not noise.exists():
    rng = random.Random(3)
    with wave.open(str(noise), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
        w.writeframes(b"".join(struct.pack("<h", int(32767 * 0.1 * rng.uniform(-1, 1))) for _ in range(4 * 48000)))
tune = sys.argv[1] if len(sys.argv) > 1 else ""
vs = sys.argv[2:] or ["7", "8"]


def level(stim, sets, t0, t1):
    out = r5.WORK / "att.wav"
    cmd = [str(r5.RENDER), str(stim), str(out)]
    for k, v in sets.items():
        cmd += ["--set", f"{k}={v}"]
    env = dict(os.environ)
    env.pop("RV_TANKV_TUNE", None)
    if tune:
        env["RV_TANKV_TUNE"] = tune
    subprocess.run(cmd, check=True, capture_output=True, env=env)
    e = r5.an("env", out, t0, t1, t1 - t0)["env"][0][1]
    out.unlink()
    return e


for v in vs:
    for name, stim, extra, t0, t1 in (("held noise DRIVE 0.6", noise, {"drive": 0.6, "decay": 0.7}, 2.5, 4.0),
                                     ("hits TONE noon", r5.STIM / "02_hits.wav", {"tone": 0.5}, 0.5, 20.0),
                                     ("hits TONE 0", r5.STIM / "02_hits.wav", {"tone": 0.0}, 0.5, 20.0)):
        lv = []
        for att in ("CLEAN", "DRIVEN", "KICKED"):
            sets = {"tank_voicing": v, "mix": 1, "springs": "2", "attitude": att}
            sets.update(extra)
            lv.append(level(stim, sets, t0, t1))
        print(f"voicing {v} {name:22s} CLEAN {lv[0]:6.1f} dB, DRIVEN {lv[1]-lv[0]:+5.1f}, KICKED {lv[2]-lv[0]:+5.1f}", flush=True)
