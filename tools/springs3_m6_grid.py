#!/usr/bin/env python3
"""M6 Ringing + Howl grids at SPRINGS 3, per SPRINGS 3 palette voicing
(PROTOTYPE, ADR 0037 proposed; core/params/Springs3Voicing.h).

Runs the six M6 sweeps (presets/sweeps/m6_*.json) with SPRINGS fixed at
position 3 and `springs3_voicing` set, reads the sidecars and prints, per
voicing: Ringing flags, steady-tone flags (SPEC 4.10) and the worst
ringing_db over the Ringing sweeps; Howl cells passing (howl_ok) over the
Howl sweeps. The WAVs are deleted as it goes (only the sidecars are kept).
Voicings run in parallel (one Renderer process each).

Usage: python3 tools/springs3_m6_grid.py [--render build/rv_render]
           [--out renders/springs3_palette2_m6] [--voicings 0,5,6,7,8,9,10]
"""
import argparse
import concurrent.futures
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RINGING = ["m6_click_ringing_d1", "m6_click_ringing_d075", "m6_bursts_ringing_d1", "m6_bursts_ringing_d075"]
HOWL = ["m6_click_howl", "m6_bursts_howl"]


def run(render, out, voicing):
    res = {"ringing": 0, "steady": 0, "cells": 0, "worst": 0.0, "worst_at": "-", "howl_ok": 0, "howl_cells": 0,
           "flagged": []}
    with tempfile.TemporaryDirectory() as tmp:
        for name in RINGING + HOWL:
            sweep = json.loads((ROOT / "presets/sweeps" / f"{name}.json").read_text())
            sweep["grid"]["springs"] = [1.0]  # position 3 only
            sweep["name"] = f"{name}_s3v{voicing}"
            path = Path(tmp) / f"{name}.json"
            path.write_text(json.dumps(sweep))
            d = out / f"v{voicing}" / name
            subprocess.run([render, "--sweep", str(path), "--out-dir", str(d), "--set", f"springs3_voicing={voicing}"],
                           check=True, cwd=ROOT, stdout=subprocess.DEVNULL)
            for side in sorted(d.glob("*.json")):
                if side.name == "manifest.json":
                    continue
                m = json.loads(side.read_text())
                met, p = m["metrics"], m["params"]
                cell = f"{name} {p.get('attitude')} TENSION {p.get('tension')} TONE {p.get('tone')}"
                if name in RINGING:
                    res["cells"] += 1
                    if met.get("ringing"):
                        res["ringing"] += 1
                        res["flagged"].append(f"ringing: {cell} ({met.get('ringing_db'):.1f} dB at {met.get('ringing_hz'):.0f} Hz)")
                    if met.get("steady_tone"):
                        res["steady"] += 1
                        res["flagged"].append(f"steady tone: {cell}")
                    rdb = met.get("ringing_db")
                    if rdb is not None and rdb > res["worst"]:
                        res["worst"], res["worst_at"] = rdb, f"{cell}, {met.get('ringing_hz'):.0f} Hz"
                else:
                    res["howl_cells"] += 1
                    if met.get("howl_ok"):
                        res["howl_ok"] += 1
                    else:
                        res["flagged"].append(f"howl: {cell}")
            for wav in d.glob("*.wav"):
                wav.unlink()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", default="build/rv_render")
    ap.add_argument("--out", default="renders/springs3_palette2_m6")
    ap.add_argument("--voicings", default="0,5,6,7,8,9,10")
    a = ap.parse_args()
    out = (ROOT / a.out).resolve()
    voicings = [int(x) for x in a.voicings.split(",")]
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(voicings)) as pool:
        results = list(pool.map(lambda v: run(str((ROOT / a.render).resolve()), out, v), voicings))
    for v, r in zip(voicings, results):
        print(f"voicing {v}: Ringing {r['ringing']} of {r['cells']} flagged, steady tone {r['steady']}, worst ringing_db "
              f"{r['worst']:.1f} ({r['worst_at']}); Howl {r['howl_ok']}/{r['howl_cells']}")
        for f in r["flagged"]:
            print(f"    {f}")


if __name__ == "__main__":
    main()
