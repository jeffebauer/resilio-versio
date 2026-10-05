#!/usr/bin/env python3
"""Wellspring fit round 5: TONE's character per voicing (left of noon, noon, right
of noon), as round 4 measured it: 01_clicks at the session-2 settings with TONE
0 / 0.25 / 0.5 / 0.75 / 1; the tail's (0.2-1.5 s) brightness = its 2 and 4 kHz
octaves re its 500 Hz and 1 kHz octaves (dB), and its level re TONE noon (dB).

  python3 docs/prototypes/wellspring-fit-5/tone5.py 7 8 9 10
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5  # noqa: E402

TONES = (0.0, 0.25, 0.5, 0.75, 1.0)
for v in sys.argv[1:]:
    rows = []
    for tone in TONES:
        p = r5.render(f"{v}|tone={tone}", "A")
        b = {x["hz"]: x["tail_db"] for x in r5.an("clicks", p)["bands"]}
        p.unlink()
        bright = (b[2000] + b[4000]) / 2 - (b[500] + b[1000]) / 2
        level = 10 * math.log10(sum(10 ** (d / 10) for d in b.values()))
        rows.append((bright, level))
    noon = rows[2][1]
    print(f"voicing {v}: brightness " + " / ".join(f"{r[0]:+.1f}" for r in rows)
          + " dB; level re noon " + " / ".join(f"{r[1] - noon:+.1f}" for r in rows) + " dB", flush=True)
