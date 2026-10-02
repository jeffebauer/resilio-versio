#!/usr/bin/env python3
"""SPLASH stronger: splash level per voicing, SPLASH step and DRIVE.

Reads renders of presets/sweeps/proto_splash_stronger_{hits,skank,clicks}.json
laid out as ROOT/<version>/<stim>/*.wav (rv_render --sweep --out-dir) and prints:
  splash  SPLASH s vs SPLASH 0, same DRIVE / ATTITUDE: 2-8 kHz energy of the
          wet (L+R), 20-400 ms after each event, summed over the events, dB
          (the backlog's measure: "DRIVE dampens SPLASH", send-level study).
          `loud` = only the events within 6 dB of the loudest (02_hits: the
          -6 dBFS snare and rim).
  peak    output sample peak, dBFS (the output limiter's knee is ~-1.7).
Usage: measure.py ROOT [--versions v0 v1 ...] [--json out.json]
"""
import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt

SR = 48000
STIM = {"hits": "02_hits", "skank": "04_skank", "clicks": "01_clicks"}
ATTS = {0: "CLEAN", 1: "KICKED"}
DRIVES = [0.0, 0.8]
SPLASH = [0.25, 0.5, 0.75, 1.0]


def onsets(x, rel=0.03, gap=0.3):
    thr = rel * np.max(np.abs(x))
    out, last = [], -10**9
    for i in np.flatnonzero(np.abs(x) > thr):
        if i - last > gap * SR:
            out.append(int(i))
        last = i
    peaks = [np.max(np.abs(x[o:o + int(0.05 * SR)])) for o in out]
    return out, peaks


def name(stim, att, drive, sp):
    return f"proto_splash_stronger_{stim}__attitude{att:.2f}_drive{drive:.2f}_splash{sp:.2f}.wav"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("--versions", nargs="+", default=["v0", "v1", "v2", "v3"])
    ap.add_argument("--json")
    a = ap.parse_args()
    root = Path(a.root)
    sos = butter(4, [2000, 8000], btype="band", fs=SR, output="sos")
    res = {}
    for stim, f in STIM.items():
        x, sr = sf.read(f"test_audio/stimulus/{f}.wav", dtype="float64")
        x = x.mean(1) if x.ndim == 2 else x
        on, pk = onsets(x)
        loud = [o for o, p in zip(on, pk) if p >= max(pk) / 2]
        for v in a.versions:
            d = root / v / stim
            if not d.exists():
                continue
            for att in ATTS:
                for dr in DRIVES:
                    def band(sp):
                        y, _ = sf.read(d / name(stim, att, dr, sp), dtype="float64")
                        return sosfiltfilt(sos, y.sum(1)), np.max(np.abs(y))

                    def energy(y, events):
                        return sum(np.sum(y[o + int(0.02 * SR):o + int(0.4 * SR)] ** 2) for o in events)

                    y0, p0 = band(0.0)
                    row = {"peak0": 20 * np.log10(p0)}
                    for sp in SPLASH:
                        y, p = band(sp)
                        row[sp] = {"splash": 10 * np.log10(energy(y, on) / energy(y0, on)),
                                   "loud": 10 * np.log10(energy(y, loud) / energy(y0, loud)),
                                   "peak": 20 * np.log10(p)}
                    res.setdefault(stim, {}).setdefault(v, {})[f"{ATTS[att]} d{dr:g}"] = row
    for stim, vs in res.items():
        print(f"\n{stim}: splash dB at SPLASH .25 / .5 / .75 / 1 (all events | loud events); peak dBFS at SPLASH 0 / 1")
        for v, rows in vs.items():
            for k, r in rows.items():
                s = " / ".join(f"{r[sp]['splash']:+5.1f}" for sp in SPLASH)
                l = " / ".join(f"{r[sp]['loud']:+5.1f}" for sp in SPLASH)
                print(f"  {v:4s} {k:10s} {s}  |  {l}   peak {r['peak0']:+5.1f} / {r[1.0]['peak']:+5.1f}")
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=1, default=lambda o: round(float(o), 2)))


if __name__ == "__main__":
    main()
