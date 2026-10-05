#!/usr/bin/env python3
"""Wellspring fit round 5, C (held sounds flat): how much WOBBLE's pitch movement
changes when the Loops' share of the wow goes to the pickups. Renders
08_held_tones at the session-2 settings (WOBBLE 0.45, the panel default, and
0.25 / 0.75) per spec and prints the 1 kHz tone's pitch movement (tools/
pitch_track.py: p95 deviation and wobble depth, cents) while held (2-8.5 s) and
in its tail (9.05-10.5 s), and its level range while held (r5.py).

  python3 docs/prototypes/wellspring-fit-5/pitch5.py SPEC [SPEC ...]
"""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[2] / "tools"))
import pitch_track as pt  # noqa: E402
import r5  # noqa: E402

for spec in sys.argv[1:]:
    for w in (0.25, 0.45, 0.75):
        s = f"{spec}|wobble={w}" if "|" not in spec else f"{spec},wobble={w}"
        p = r5.render(s, "L")
        held = pt.analyze_file(str(p), start_s=2.0, end_s=8.5)
        tail = pt.analyze_file(str(p), start_s=9.05, end_s=10.5)
        e = r5.an("env", p, 1.0, 18.0, 0.1)["env"]
        lv = r5.held_stats(e, 1.0, 9.0)
        print(f"{spec:28s} WOBBLE {w}: held p95 {held['p95_cents']:.1f} c depth {held['wobble_depth_cents']:.1f} c | "
              f"tail p95 {tail['p95_cents']:.1f} c | level range {lv['range_db']:.1f} dB", flush=True)
        p.unlink()
