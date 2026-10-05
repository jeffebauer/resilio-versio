#!/usr/bin/env python3
"""Wellspring fit round 5, target 5 (report only): where our CLEAN distortion
comes from. Renders 03_sweep at the session-2 settings with one source at a
time switched off and prints, while the sweep plays 200-800 Hz, the energy in
1.1-1.8 kHz and the 2nd / 3rd harmonic re the tone (r5an sweepdist), next to
the Wellspring's sweep takes D (session 1) and D2 (session 2).

  python3 docs/prototypes/wellspring-fit-5/dist5.py [AN]
"""
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5  # noqa: E402

AN = sys.argv[1] if len(sys.argv) > 1 else str(r5.AN)
OUTD = r5.ROOT / "renders" / "fit_round5" / "dist"


def dist(wav):
    out = subprocess.run([AN, "sweepdist", str(wav)], capture_output=True, text=True, check=True).stdout
    return json.loads(out)["dist"]


def render(name, sets, tune=""):
    OUTD.mkdir(parents=True, exist_ok=True)
    out = OUTD / f"{name}.wav"
    allsets = dict(r5.BASE)
    allsets.update(sets)
    cmd = [str(r5.RENDER), str(r5.STIM / "03_sweep.wav"), str(out)]
    for k, v in allsets.items():
        cmd += ["--set", f"{k}={v}"]
    env = dict(os.environ)
    env.pop("RV_TANKV_TUNE", None)
    if tune:
        env["RV_TANKV_TUNE"] = tune
    subprocess.run(cmd, check=True, capture_output=True, env=env)
    d = dist(out)
    out.unlink()
    return d


def show(name, d):
    print(f"{name:34s} " + "  ".join(f"{f:.0f}: {a:+.0f} / {h2:+.0f} / {h3:+.0f}" for f, a, h2, h3 in d))


if __name__ == "__main__":
    print("energy 1.1-1.8 kHz / 2nd / 3rd harmonic re the tone (dB), while the sweep is at 200 / 283 / 400 / 566 / 800 Hz")
    show("Wellspring D (session 1)", dist(r5.REF / "wellspring_D_sweep.wav"))
    show("Wellspring D2 (session 2)", dist(r5.REF / "wellspring_D2_sweep_outlow.wav"))
    for v in (7, 8):
        show(f"{v} as set", render(f"v{v}", {"tank_voicing": v}))
        show(f"{v} SPLASH 0", render(f"v{v}_s0", {"tank_voicing": v, "splash": 0}))
        show(f"{v} no coil square term", render(f"v{v}_ev0", {"tank_voicing": v}, "tdEven=0"))
        show(f"{v} DRIVE 0", render(f"v{v}_d0", {"tank_voicing": v, "drive": 0}))
        show(f"{v} SPLASH 0, DRIVE 0, no square", render(f"v{v}_all0", {"tank_voicing": v, "splash": 0, "drive": 0}, "tdEven=0"))
    show("0 (today's tank before round 3)", render("v0", {"tank_voicing": 0}))
