#!/usr/bin/env python3
"""Summarise m6.sh's renders: per TONE placement, Ringing flags and the worst
ringing_db (and steady_tone flags), at TONE 0.85 / 1 and at 0.7.
    python3 docs/prototypes/tone-place/m6_summary.py <out_dir>
"""
import json
import sys
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renders/proto_tone_place_m6")
for p in sorted(root.glob("p*")):
    for tones in ((0.85, 1.0), (0.7,)):
        cells = ring = steady = 0
        worst, at = -1e9, ""
        per = {}
        for side in sorted(p.glob("*/*.json")):
            if side.name == "manifest.json":
                continue
            d = json.loads(side.read_text())
            prm, m = d["params"], d["metrics"]
            if round(prm["tone"], 2) not in tones:
                continue
            cells += 1
            ring += bool(m.get("ringing"))
            steady += bool(m.get("steady_tone"))
            r = m.get("ringing_db")
            key = (side.parent.name, prm["attitude"], prm["springs"], round(prm["tension"], 2), round(prm["tone"], 2))
            per[key] = r
            if r is not None and r > worst:
                worst = r
                at = "%s %s %s Spring(s) TENSION %.1f TONE %.2f @ %.0f Hz" % (
                    side.parent.name, prm["attitude"], prm["springs"], prm["tension"], prm["tone"], m.get("ringing_hz") or 0)
        print("%s TONE %s: %d cells, %d Ringing, %d steady_tone, worst ringing_db %.1f (%s)"
              % (p.name, "/".join(str(t) for t in tones), cells, ring, steady, worst, at))
