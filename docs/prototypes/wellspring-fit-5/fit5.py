#!/usr/bin/env python3
"""Wellspring fit round 5: fit voicing 8's numbers (TankVoicing.h r5*) to the
owner's Wellspring by coordinate search over RV_TANKV_TUNE overrides.

  python3 docs/prototypes/wellspring-fit-5/fit5.py [rounds] [start.json] [--freeze=key,key]   -> fit5.json (best so far)

Score (lower = closer), each term vs the Wellspring, measured by r5.py:
  T60 shape   per-octave T60 re 500 Hz's, 125 Hz-4 kHz (J), mean |log2 ratio| x 6
  front       clicks: the first 100 ms re the next 400 ms per octave 63 Hz-1 kHz (A), mean |dB| x 2
  front tone  clicks: the first 100 ms's spectrum re 500 Hz-1 kHz, 63 Hz-4 kHz (A), mean |dB| x 0.5
  colour      pink noise 1/3 octaves re 400 Hz-2 kHz, 100 Hz-4 kHz (K), rms dB
  width       L/R correlation: pink 63 Hz-4 kHz and the clicks' front 63 Hz-2 kHz, mean |diff| x 5 each
  balance     tone bursts' L/R balance 125 Hz-4 kHz, mean |dB| x 0.3
  gaps        tone bursts' envelope spread 250 Hz-2 kHz, mean |diff| x 0.5
Stdlib only. Every evaluation renders J, K and A (~6 s).
"""
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5  # noqa: E402

OUT = Path(__file__).resolve().parent / "fit5.json"

# name: (start, lo, hi, step, kind)   kind: lin / log
SPACE = {
    "r5DiffCoeff": (0.7, 0.3, 0.8, 0.05, "lin"),
    "r5DiffAlign": (0.5, 0.0, 1.0, 0.25, "lin"),
    "r5EqHz": (1100.0, 700.0, 1800.0, 1.12, "log"),
    "r5EqDb": (-0.6, -1.5, 0.0, 0.15, "lin"),
    "r5EqQ": (1.0, 0.5, 2.5, 1.25, "log"),
    "r5DcHz": (30.0, 12.0, 40.0, 1.15, "log"),
    "r5TdInHz": (1850.0, 1400.0, 2600.0, 1.06, "log"),
    "r5TdInQ": (1.4, 0.7, 2.5, 1.12, "log"),
    "r5HighT60Ratio": (1.5, 1.0, 4.5, 1.2, "log"),
    "r5HighCeilHz": (9000.0, 6000.0, 20000.0, 1.25, "log"),
    "r5HighLevel": (0.9, 0.5, 1.5, 1.15, "log"),
    "r5LcHpHz": (155.0, 100.0, 260.0, 1.12, "log"),
    "r5LcShelfDb": (-2.0, -5.0, 0.0, 0.5, "lin"),
    "r5WideSide": (0.6, 0.3, 0.9, 0.05, "lin"),
    "r5BassHz": (150.0, 80.0, 400.0, 1.2, "log"),
    "r5BassOrder2": (0.0, 0.0, 1.0, 1.0, "lin"),
}
FIXED = "r5Diff0=1.9,r5Diff1=2.6,r5Diff2=3.5"  # 8.0 ms per Spring: what the firmware pool allows


def tune(p):
    return FIXED + "," + ",".join(f"{k}={v:.5g}" for k, v in p.items())


def score(S, W):
    terms = {}
    sr = [S["t60"][i] / S["t60"][2] for i in range(6)]
    wr = [W["t60"][i] / W["t60"][2] for i in range(6)]
    terms["t60"] = 6 * sum(abs(math.log2(max(a, 1e-3) / b)) for a, b in zip(sr, wr)) / 6
    terms["front"] = 2 * sum(abs(a - b) for a, b in zip(S["clk_front_ratio"][:5], W["clk_front_ratio"][:5])) / 5
    terms["fronttone"] = 0.5 * sum(abs(a - b) for a, b in zip(S["clk_front_re"][:7], W["clk_front_re"][:7])) / 7
    ks = [f for f in W["k_rel"] if 100 <= f <= 4000]
    terms["colour"] = math.sqrt(sum((S["k_rel"][f] - W["k_rel"][f]) ** 2 for f in ks) / len(ks))
    terms["width"] = 5 * (sum(abs(a - b) for a, b in zip(S["k_corr"][:7], W["k_corr"][:7])) / 7
                          + sum(abs(a - b) for a, b in zip(S["clk_corr_front"][:6], W["clk_corr_front"][:6])) / 6)
    terms["balance"] = 0.3 * sum(abs(b) for b in S["bal"][:6]) / 6
    terms["gaps"] = 0.5 * sum(abs(a - b) for a, b in zip(S["gap"][1:5], W["gap"][1:5])) / 4
    return sum(terms.values()), terms


def evaluate(p, W, voicing=8):
    S = r5.summary(r5.ours(f"{voicing};{tune(p)}", ("J", "K", "A")))
    return score(S, W) + (S,)


def main():
    freeze = [a.split("=", 1)[1].split(",") for a in sys.argv if a.startswith("--freeze=")]
    for k in (freeze[0] if freeze else []):
        SPACE.pop(k)
    sys.argv = [a for a in sys.argv if not a.startswith("--freeze=")]
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 2
    W = r5.summary(r5.ref())
    p = {k: v[0] for k, v in SPACE.items()}
    if len(sys.argv) > 2:
        p.update(json.load(open(sys.argv[2]))["params"])  # frozen keys keep the start file's value
    best, terms, S = evaluate(p, W)
    print(f"start {best:.3f} {json.dumps({k: round(v, 2) for k, v in terms.items()})}", flush=True)
    for rnd in range(rounds):
        improved = False
        for k, (_, lo, hi, step, kind) in SPACE.items():
            for direction in (+1, -1):
                while True:
                    q = dict(p)
                    v = p[k] * (step ** direction) if kind == "log" else p[k] + direction * step
                    v = min(hi, max(lo, v))
                    if v == p[k]:
                        break
                    q[k] = v
                    sc, tq, Sq = evaluate(q, W)
                    if sc < best - 1e-3:
                        best, p, terms, S = sc, q, tq, Sq
                        improved = True
                        print(f"r{rnd} {k}={v:.4g} -> {best:.3f} {json.dumps({kk: round(vv, 2) for kk, vv in terms.items()})}", flush=True)
                        json.dump({"score": best, "terms": terms, "params": p, "fixed": FIXED, "summary": S}, open(OUT, "w"), indent=1)
                    else:
                        break
        if not improved:
            break
    json.dump({"score": best, "terms": terms, "params": p, "fixed": FIXED, "summary": S}, open(OUT, "w"), indent=1)
    print("best", best, json.dumps(p))


if __name__ == "__main__":
    main()
