#!/usr/bin/env python3
"""Wellspring fit round 5: each page render's level (whole file, mean of L and R power, dB) re version A (7).

  python3 docs/prototypes/wellspring-fit-5/levels5.py [PAGE_DIR]
"""
import subprocess
import sys
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5  # noqa: E402

page = Path(sys.argv[1]) if len(sys.argv) > 1 else r5.MAIN / "renders" / "fit_round5"
folders = sorted(p for p in page.iterdir() if p.is_dir() and p.name[:2] in ("A_", "B_", "C_", "D_"))
rows = sorted(p.name for p in folders[0].iterdir() if p.is_dir())
print("row".ljust(24) + "".join(f.name[:1].rjust(8) for f in folders[1:]) + "   (dB re A)")
for row in rows:
    lv = []
    for f in folders:
        w = next((f / row).glob("*.wav"))
        with wave.open(str(w)) as h:
            dur = h.getnframes() / h.getframerate()
        e = r5.an("env", w, 0.0, dur, dur)["env"]
        lv.append(e[0][1])
    print(row.ljust(24) + "".join(f"{x - lv[0]:+8.1f}" for x in lv[1:]))
