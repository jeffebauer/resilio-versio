#!/usr/bin/env python3
"""Wellspring fit round 5 (docs/m8-tuning-backlog.md "Wellspring round 5"):
render a tank voicing at the session-2 settings and measure it next to the
owner's Wellspring takes (J tone bursts, K pink noise, L held tones, M pad,
A clicks). Stdlib only; the number crunching is r5an.cpp (build-r/r5an).

  python3 docs/prototypes/wellspring-fit-5/r5.py ref               the Wellspring's numbers
  python3 docs/prototypes/wellspring-fit-5/r5.py ours V [TUNE]     render voicing V (tank_voicing; or 'v;key=val,...'
                                                                    with RV_TANKV_TUNE overrides) and measure it
  python3 docs/prototypes/wellspring-fit-5/r5.py table V1 V2 ...   both, side by side

The Wellspring WAVs are read in place from the main checkout's
test_audio/reference/ (never copied, never committed; ADR 0009). Renders go
to renders/fit_round5/work/ in this checkout (gitignored), WAVs deleted after
measuring unless --keep.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MAIN = Path("/Users/jesse/Documents/Sites/resilio-versio")
REF = MAIN / "test_audio" / "reference"
STIM = ROOT / "test_audio" / "stimulus"
RENDER = ROOT / "build-r" / "rv_render"
AN = ROOT / "build-r" / "r5an"
WORK = ROOT / "renders" / "fit_round5" / "work"

# Session-2 comparison settings (backlog "Wellspring session 2" / closest settings):
# 2 Springs, CLEAN, MIX 1, DECAY 0.70, TONE / TENSION noon, the rest at the panel defaults (SPLASH 0.3, WOBBLE 0.45, DRIVE 0.25).
BASE = {"mix": 1, "attitude": "CLEAN", "decay": 0.70, "tone": 0.5, "tension": 0.5, "splash": 0.3,
        "wobble": 0.45, "drive": 0.25, "springs": "2"}

TAKES = {  # metric -> (Wellspring take, stimulus)
    "J": ("wellspring_J_tone_bursts.wav", "15_tone_bursts.wav"),
    "K": ("wellspring_K_pink_noise.wav", "09_pink_noise.wav"),
    "L": ("wellspring_L_held_tones.wav", "08_held_tones.wav"),
    "M": ("wellspring_M_pad.wav", "10_pad_cminor.wav"),
    "A": ("wellspring_A_clicks.wav", "01_clicks.wav"),
}


def an(*args):
    out = subprocess.run([str(AN)] + [str(a) for a in args], capture_output=True, text=True, check=True).stdout
    return json.loads(out)


def parse_spec(spec):
    """'7' or '8;tdDampingScale=2,...' or '8|decay=0.72' -> (voicing, tune, extra sets)."""
    sets = {}
    if "|" in spec:
        spec, s = spec.split("|", 1)
        for kv in s.split(","):
            k, v = kv.split("=")
            sets[k] = v
    tune = ""
    if ";" in spec:
        spec, tune = spec.split(";", 1)
    return int(spec), tune, sets


def render(spec, take, keep=False, tag=None):
    v, tune, sets = parse_spec(spec)
    WORK.mkdir(parents=True, exist_ok=True)
    tag = tag or (spec.replace(";", "_").replace(",", "_").replace("=", "").replace("|", "_"))[:120]
    out = WORK / f"{take}_{tag}.wav"
    allsets = dict(BASE)
    allsets.update(sets)
    cmd = [str(RENDER), str(STIM / TAKES[take][1]), str(out), "--set", f"tank_voicing={v}"]
    for k, val in allsets.items():
        cmd += ["--set", f"{k}={val}"]
    env = dict(os.environ)
    if tune:
        env["RV_TANKV_TUNE"] = tune
    else:
        env.pop("RV_TANKV_TUNE", None)
    subprocess.run(cmd, check=True, capture_output=True, env=env)
    return out


def held_stats(env, t_on, t_off):
    """Held-sound level movement: rows [t, dB, stim dB]; from 0.5 s after onset to the end."""
    rows = [r for r in env if t_on + 0.5 <= r[0] < t_off - 0.1]
    lv = [r[1] for r in rows]
    first = lv[0]
    return {"settle_db": round(max(lv) - first, 2), "range_db": round(max(lv) - min(lv), 2),
            "end_minus_start": round(lv[-1] - first, 2), "peak_t": round(rows[lv.index(max(lv))][0] - t_on, 2)}


def measure(paths):
    """paths: take -> WAV. Returns the round-5 numbers."""
    m = {}
    if "J" in paths:
        m["J"] = an("bursts", paths["J"])["bands"]
    if "K" in paths:
        m["K"] = an("steady", paths["K"], 5.0, 20.5)
    if "L" in paths:
        e = an("env", paths["L"], 1.0, 18.0, 0.1)["env"]
        m["L"] = {"tone": held_stats(e, 1.0, 9.0), "chord": held_stats(e, 10.0, 18.0)}
    if "M" in paths:
        e = an("env", paths["M"], 1.0, 13.0, 0.5, STIM / TAKES["M"][1])["env"]
        gains = [(r[0], round(r[1] - r[2], 2)) for r in e]
        sw = [g for t, g in gains if 2.0 <= t < 4.0]
        ho = [g for t, g in gains if 4.0 <= t < 10.0]
        m["M"] = {"swell_gain": round(sum(sw) / len(sw), 2), "hold_gain": round(sum(ho) / len(ho), 2),
                  "hold_range": round(max(ho) - min(ho), 2), "hold_first_to_max": round(max(ho) - ho[0], 2),
                  "curve": gains}
    if "A" in paths:
        m["A"] = an("clicks", paths["A"])["bands"]
    return m


def ref():
    return measure({k: REF / v[0] for k, v in TAKES.items()})


def ours(spec, takes=("J", "K", "L", "M", "A"), keep=False):
    paths = {t: render(spec, t) for t in takes}
    m = measure(paths)
    if not keep:
        for p in paths.values():
            p.unlink(missing_ok=True)
    return m


def presence(k):
    """Presence peak from the 1/3-octave levels (dB re the 400 Hz-2 kHz mean): the peak's centre and
    the 300-600 Hz level."""
    th = {f: d for f, d in k["thirds"]}
    mid = [th[f] for f in (400, 500, 630, 800, 1000, 1250, 1600, 2000)]
    ref_ = sum(mid) / len(mid)
    rel = {f: round(d - ref_, 2) for f, d in th.items()}
    band = {f: rel[f] for f in (630, 800, 1000, 1250, 1600, 2000, 2500)}
    peak = max(band, key=band.get)
    return {"peak_hz": peak, "rel": rel, "lowmid_300_600": round((rel[315] + rel[400] + rel[500]) / 3, 2)}


def summary(m):
    s = {}
    if "J" in m:
        J = m["J"]
        s["t60"] = [round(b["t60"], 2) for b in J]
        s["peak_ms"] = [round(b["peak_ms"]) for b in J]
        s["fall20_ms"] = [round(b["fall20_ms"]) for b in J]
        s["front_ratio"] = [round(b["front_ratio_db"], 1) for b in J]
        s["gap"] = [round(b["gap_db"], 1) for b in J]
        s["corr_front"] = [round(b["corr_front"], 2) for b in J]
        s["corr_tail"] = [round(b["corr_tail"], 2) for b in J]
        s["bal_front"] = [round(b["bal_front_db"], 1) for b in J]
        s["bal"] = [round(b["bal_db"], 1) for b in J]
        f1k = J[3]["front_db"]
        s["front_re1k"] = [round(b["front_db"] - f1k, 1) for b in J]
    if "K" in m:
        p = presence(m["K"])
        s["presence_peak"] = p["peak_hz"]
        s["lowmid"] = p["lowmid_300_600"]
        s["k_rel"] = p["rel"]
        s["k_corr"] = [round(c, 2) for _, c, _ in m["K"]["oct"]]
        s["k_bal"] = [round(b, 1) for _, _, b in m["K"]["oct"]]
    if "L" in m:
        s["held_tone"] = m["L"]["tone"]
        s["held_chord"] = m["L"]["chord"]
    if "M" in m:
        s["pad"] = {k: v for k, v in m["M"].items() if k != "curve"}
    if "A" in m:
        A = m["A"]
        s["clk_corr_front"] = [round(b["corr_front"], 2) for b in A]
        s["clk_corr_tail"] = [round(b["corr_tail"], 2) for b in A]
        f = {b["hz"]: b["front_db"] for b in A}
        t = {b["hz"]: b["tail_db"] for b in A}
        ref_f = (f[500] + f[1000]) / 2
        ref_t = (t[500] + t[1000]) / 2
        s["clk_front_re"] = [round(b["front_db"] - ref_f, 1) for b in A]
        s["clk_tail_re"] = [round(b["tail_db"] - ref_t, 1) for b in A]
        s["clk_front_ratio"] = [round(b["front_ratio_db"], 1) for b in A]
    return s


def show(name, s):
    print(f"== {name}")
    for k, v in s.items():
        if k == "k_rel":
            print(f"  {k:14s} " + " ".join(f"{f}:{d:+.1f}" for f, d in v.items()))
        else:
            print(f"  {k:14s} {v}")


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or a[0] == "ref":
        show("Wellspring", summary(ref()))
    elif a[0] == "ours":
        takes = tuple(a[3].split(",")) if len(a) > 3 else ("J", "K", "L", "M", "A")
        show(a[1], summary(ours(a[1], takes)))
    elif a[0] == "probe":
        # probe TAKES KEYS spec... : e.g. probe J,K t60,presence_peak 7 '7;tdDampingScale=1'
        takes, keys = a[1].split(","), a[2].split(",")
        for spec in a[3:]:
            s = summary(ours(spec, tuple(takes)))
            print(spec)
            for k in keys:
                v = s.get(k)
                if k == "k_rel":
                    v = " ".join(f"{f}:{d:+.1f}" for f, d in v.items() if 200 <= f <= 4000)
                print(f"   {k:14s} {v}")
    elif a[0] == "json":
        print(json.dumps({"W": summary(ref()), **{v: summary(ours(v)) for v in a[1:]}}))
