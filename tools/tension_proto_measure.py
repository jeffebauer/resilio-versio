#!/usr/bin/env python3
"""Measure the TENSION prototype's click renders with tools/ir_dispersion.py.

Usage: python3 tools/tension_proto_measure.py <renders/tension_proto> [--json out.json]

Reads <dir>/measure/{new,today}_*.wav (1 Spring = Spring A, CLEAN, click +
12 s, written by tools/tension_proto_render.py) and prints repeat ms, highs
later ms (tool / capped at the model's fC, as docs/ir-dispersion-study.md
"Tuned HighsLater"), fC and T60 per setting. The model fC per setting comes
from the mapping (prototype: Mappings.h TENSION anchors; today: DECAY).
"""
import json
import math
import sys
from multiprocessing import Pool
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ir_dispersion as irs  # noqa: E402

DETUNE_FC = 1.040  # Spring A


def anchor_exp(lo, mid, hi, v):
    return lo * (mid / lo) ** (2 * v) if v < 0.5 else mid * (hi / mid) ** (2 * v - 1)


def model_fc(kind, decay, knob):
    if kind == "new":
        return anchor_exp(4600.0, 3300.0, 2700.0, knob) * DETUNE_FC
    return 4200.0 * (2700.0 / 4200.0) ** decay * DETUNE_FC


def capped_highs_later(res, fc):
    """Same ridge + rule as the tool, chirp band capped at the model fC."""
    ridge = res.get("ridge") or []
    pts = [(f, p) for f, p, h in ((r["f"], r["p_ms"], r.get("r", 1)) for r in ridge)
           if p is not None and h >= 0.15 and 150 <= f <= fc]
    segs, cur = [], pts[:1]
    for q0, q1 in zip(pts, pts[1:]):
        if abs(q1[1] - q0[1]) > 0.12 * q0[1]:
            segs.append(cur)
            cur = []
        cur.append(q1)
    segs.append(cur)
    band = max(segs, key=len) if segs else []
    if len(band) < 5:
        return None
    sm = [band[0][1]] + [sorted([band[i - 1][1], band[i][1], band[i + 1][1]])[1] for i in range(1, len(band) - 1)] \
        + [band[-1][1]]
    return sm[-1] - min(sm)


def job(path):
    res = irs.analyse_file(str(path))
    return path, res


def main():
    d = Path(sys.argv[1]) / "measure"
    files = sorted(d.glob("*.wav"))
    with Pool() as p:
        out = p.map(job, files)
    rows = []
    for path, res in out:
        kind, dec, knob = path.stem.split("_")
        decay = float(dec[len("decay"):])
        knobv = float(knob.lstrip("tensionboing"))
        side = json.loads(path.with_suffix(".json").read_text())
        fc = model_fc(kind, decay, knobv)
        rows.append(dict(kind=kind, decay=decay, knob=knobv, repeat_ms=res.get("repeat_ms"),
                         highs_later_ms=res.get("highs_later_ms"), highs_later_capped_ms=capped_highs_later(res, fc),
                         fc_meas_hz=res.get("fc_hz"), fc_model_hz=fc, t60_s=side["metrics"].get("t60_s"),
                         quality=res.get("quality")))
    rows.sort(key=lambda r: (r["kind"], r["decay"], r["knob"]))
    f = lambda v, n=1: "—" if v is None else f"{v:.{n}f}"
    print("kind  decay knob | repeat ms | highs later tool/capped | fC meas / model | T60 s | q")
    for r in rows:
        print(f"{r['kind']:5} {r['decay']:.2f} {r['knob']:.2f} | {f(r['repeat_ms']):>6} | "
              f"{f(r['highs_later_ms']):>5} / {f(r['highs_later_capped_ms']):>5} | "
              f"{f(r['fc_meas_hz'], 0):>5} / {r['fc_model_hz']:.0f} | {f(r['t60_s'], 2)} | {f(r['quality'], 2)}")
    if "--json" in sys.argv:
        Path(sys.argv[sys.argv.index("--json") + 1]).write_text(json.dumps(rows, indent=1))


if __name__ == "__main__":
    main()
