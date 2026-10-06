#!/usr/bin/env python3
"""Send-level calibration study, step 1: scaled stimuli + the render grid.

Makes copies of 02_hits, 04_skank and 06_noise_bursts scaled so their loudest
event peaks at -6 / -12 / -18 / -24 dBFS, then renders each at CLEAN / DRIVEN /
KICKED x DRIVE 0 / .25 / .5 / .75 / 1 x SPLASH 0 / 0.8 (2 Springs, DECAY 0.7,
TONE / TENSION noon, WOBBLE 0, MIX 1) with rv_render --preset (never --set for
switches: `--set attitude=KICKED` renders CLEAN).

Usage (from the repo root):
  python3 docs/prototypes/send-levels/render_grid.py [--render BIN] [--out DIR] [--jobs N]
Needs numpy + soundfile. Outputs are not committed (renders/ is gitignored).
"""
import argparse
import itertools
import json
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

STIM = ["02_hits", "04_skank", "06_noise_bursts"]
LEVELS = [-6, -12, -18, -24]
ATTS = ["CLEAN", "DRIVEN", "KICKED"]
DRIVES = [0.0, 0.25, 0.5, 0.75, 1.0]
SPLASHES = [0.0, 0.8]
BASE = {"springs": "2", "mix": 1.0, "decay": 0.7, "tone": 0.5, "tension": 0.5, "wobble": 0.0}


def stim_dir():
    for p in [Path("test_audio/stimulus"), Path(__file__).resolve().parents[3] / "test_audio/stimulus"]:
        if (p / "02_hits.wav").exists():
            return p
    raise SystemExit("stimulus not found: run tools/make_stimulus.py")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", default="build-agent/rv_render")
    ap.add_argument("--out", default="renders/send_levels")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    out = Path(a.out)
    (out / "stim").mkdir(parents=True, exist_ok=True)
    (out / "presets").mkdir(parents=True, exist_ok=True)
    (out / "wav").mkdir(parents=True, exist_ok=True)

    src = stim_dir()
    for s in STIM:
        x, sr = sf.read(src / f"{s}.wav", dtype="float64")
        pk = np.max(np.abs(x))
        for lv in LEVELS:
            sf.write(out / "stim" / f"{s}_{-lv:02d}.wav", x * (10 ** (lv / 20) / pk), sr, subtype="FLOAT")

    jobs = []
    for att, d, sp in itertools.product(ATTS, DRIVES, SPLASHES):
        name = f"{att}_d{d:.2f}_s{sp:.1f}"
        pre = out / "presets" / f"{name}.json"
        pre.write_text(json.dumps({**BASE, "attitude": att, "drive": d, "splash": sp}))
        for s, lv in itertools.product(STIM, LEVELS):
            w = out / "wav" / f"{s}_{-lv:02d}_{name}.wav"
            if not w.exists():
                jobs.append([a.render, str(out / "stim" / f"{s}_{-lv:02d}.wav"), str(w), "--preset", str(pre)])

    def run(cmd):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            print("FAILED", cmd, r.stderr)
    with ThreadPoolExecutor(a.jobs) as ex:
        list(ex.map(run, jobs))
    print(f"{len(jobs)} renders -> {out / 'wav'}")


if __name__ == "__main__":
    main()
