#!/usr/bin/env python3
"""The hump, measured on wet-only renders (MIX 1). Usage:
  hump.py DIR STIMDIR   (DIR holds <stim>__<tag>.wav; STIMDIR the kick_times_<stim>.txt)
Per kick interval: "pump" = the wet's level 60-10 ms before the next kick
minus its level halfway between kicks (> 0: the bed is still rising into
the kick, the hump the owner heard); "dip" = halfway minus 30-80 ms after
the kick. Full band and lows (a 4th-order low-pass at 200 Hz). Stdlib only.
"""
import glob
import math
import os
import sys
import wave


def read(p):
    w = wave.open(p)
    n, ch, sw, sr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(n)
    full = 1 << (8 * sw - 1)
    x = [int.from_bytes(raw[(i * ch) * sw:(i * ch + 1) * sw], "little", signed=True) / full for i in range(n)]
    return x, sr


def lp4(x, fc, sr):
    w0 = 2 * math.pi * fc / sr
    alpha = math.sin(w0) / (2 * 0.70710678)
    c = math.cos(w0)
    a0 = 1 + alpha
    b0, b1, b2 = (1 - c) / 2 / a0, (1 - c) / a0, (1 - c) / 2 / a0
    a1, a2 = -2 * c / a0, (1 - alpha) / a0
    for _ in range(2):
        y, s1, s2 = [], 0.0, 0.0
        for v in x:
            o = b0 * v + s1
            s1 = b1 * v - a1 * o + s2
            s2 = b2 * v - a2 * o
            y.append(o)
        x = y
    return x


def rms_db(x, sr, a, b):
    i0, i1 = int(a * sr), min(int(b * sr), len(x))
    acc = sum(v * v for v in x[i0:i1]) / max(1, i1 - i0)
    return 10 * math.log10(acc + 1e-20)


def measure(p, kicks):
    x, sr = read(p)
    lo = lp4(x, 200.0, sr)
    out = {"pump": 0.0, "pump_lo": 0.0, "dip": 0.0, "dip_lo": 0.0}
    n = 0
    for a, b in zip(kicks[:-1], kicks[1:]):
        mid = 0.5 * (a + b)
        for sig, sfx in ((x, ""), (lo, "_lo")):
            m = rms_db(sig, sr, mid - 0.025, mid + 0.025)
            out["pump" + sfx] += rms_db(sig, sr, b - 0.06, b - 0.01) - m
            out["dip" + sfx] += m - rms_db(sig, sr, a + 0.03, a + 0.08)
        n += 1
    return {k: v / n for k, v in out.items()}


def main():
    d = sys.argv[1]
    stimdir = sys.argv[2]
    for p in sorted(glob.glob(os.path.join(d, "*.wav"))):
        stim = os.path.basename(p).split("__")[0]
        kicks = [float(t) for t in open(os.path.join(stimdir, f"kick_times_{stim}.txt")).read().split()]
        m = measure(p, kicks)
        print(f"{os.path.basename(p):44s} pump {m['pump']:+6.2f} dB (lows {m['pump_lo']:+6.2f})   "
              f"dip {m['dip']:+6.2f} dB (lows {m['dip_lo']:+6.2f})")


main()
