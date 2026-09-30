#!/usr/bin/env python3
"""Listening renders for the bipolar WOBBLE prototype (ADR 0034, branch
proto/bipolar-wobble): main (A) vs the prototype (B) on held tones and the
skank, at five WOBBLE knob positions.

Build an rv_render from each branch first (e.g. `cmake --build build` on main
and in a proto/bipolar-wobble worktree, or scratch build dirs), then, from the
repo root:

    python3 docs/prototypes/bipolar-wobble/render.py \\
        --main  /path/to/main/rv_render \\
        --proto /path/to/proto/rv_render \\
        --out   renders/proto_bipolar_wobble

Writes held/08_held_tones_{A_main,B_proto}_wNNN.wav and
skank/04_skank_{A_main,B_proto}_wNNN.wav (NNN = knob x 100: 000 fully left,
025 9 o'clock, 050 noon, 075 3 o'clock, 100 fully right) plus README.txt.
Settings: 2 Springs, CLEAN, DECAY / TONE / TENSION noon, SPLASH 0.3,
DRIVE 0.25, MIX 1 (fully wet). Switches go in through a --preset JSON (not
--set). WAVs stay out of git (renders/ is ignored). Missing stimuli are
generated with tools/make_stimulus.py.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
STIM = os.path.join(ROOT, "test_audio", "stimulus")
KNOBS = [(0.0, "000"), (0.25, "025"), (0.5, "050"), (0.75, "075"), (1.0, "100")]
STIMULI = [("held", "08_held_tones"), ("skank", "04_skank")]
BASE = {"springs": "2", "attitude": "CLEAN", "decay": 0.5, "tone": 0.5, "tension": 0.5, "splash": 0.3,
        "drive": 0.25, "mix": 1.0}

README = """Bipolar WOBBLE prototype (branch proto/bipolar-wobble, ADR 0034): main vs the prototype.

Two sounds: held tones (a long 1 kHz note, then chords) and the skank (offbeat
chord stabs). Each at five WOBBLE positions: w000 fully left, w025 9 o'clock,
w050 noon, w075 3 o'clock, w100 fully right. Everything else the same for all:
2 Springs, CLEAN, DECAY / TONE / TENSION at noon, SPLASH 0.3, DRIVE 0.25, fully
wet (MIX 1). Listen for how the pitch moves on the held notes and in the tail
after each stab, and whether each step of the knob is a clear step.

A (main, today): WOBBLE turns one way only: fully left is still, it barely
  moves until past noon, then a steady, mostly-sine wobble takes over towards
  fully right (the "same-same" top end).
B (prototype): noon is still. Left of noon, a smooth random wow with a
  faster flutter on top that never repeats, growing towards fully left (the
  Springs drift together while it is gentle, so chords fade evenly). Right of
  noon, a sine wobble whose speed drifts very slightly, growing to today's
  wild top end at fully right.
"""


def ensure_stimuli():
    missing = [s for _, s in STIMULI if not os.path.exists(os.path.join(STIM, s + ".wav"))]
    if missing:
        print("generating stimuli:", ", ".join(missing))
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "make_stimulus.py")], cwd=ROOT, check=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--main", required=True, help="rv_render built from main")
    ap.add_argument("--proto", required=True, help="rv_render built from proto/bipolar-wobble")
    ap.add_argument("--out", default=os.path.join(ROOT, "renders", "proto_bipolar_wobble"))
    args = ap.parse_args()
    ensure_stimuli()
    versions = [("A_main", os.path.abspath(args.main)), ("B_proto", os.path.abspath(args.proto))]
    for _, exe in versions:
        if not os.access(exe, os.X_OK):
            sys.exit(f"not an executable: {exe}")
    with tempfile.TemporaryDirectory() as tmp:
        for folder, stim in STIMULI:
            os.makedirs(os.path.join(args.out, folder), exist_ok=True)
            src = os.path.join(STIM, stim + ".wav")
            for knob, tag in KNOBS:
                preset = os.path.join(tmp, f"wobble_{tag}.json")
                with open(preset, "w") as f:
                    json.dump(dict(BASE, wobble=knob), f)
                for name, exe in versions:
                    dst = os.path.join(args.out, folder, f"{stim}_{name}_w{tag}.wav")
                    subprocess.run([exe, src, dst, "--preset", preset], cwd=ROOT, check=True,
                                   stdout=subprocess.DEVNULL)
                    print("wrote", os.path.relpath(dst, ROOT))
    with open(os.path.join(args.out, "README.txt"), "w") as f:
        f.write(README)
    print("wrote", os.path.relpath(os.path.join(args.out, "README.txt"), ROOT))


if __name__ == "__main__":
    main()
