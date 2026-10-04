#!/usr/bin/env python3
"""How much a snare + hats loop moves the held bed (MIX 1). Usage:
  bedmove.py DIR REFDIR
For each DIR/stab_snarehats__<col>_<tag>.wav, compares with
REFDIR/stab_snarehats__<col>_none.wav (same render, no ducking): the
largest per-beat difference in level (20 ms windows just before each snare,
4-15 s) and the overall 4-15 s difference, each less the difference before
the drums start (the stab's own dip). Round 1 (A) is keyed on the
whole input, so it ducks here; B and C should not. Stdlib only.
"""
import glob
import math
import os
import sys
import wave


def read(p):
    w = wave.open(p)
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(n)
    full = 1 << (8 * sw - 1)
    return [int.from_bytes(raw[(i * ch) * sw:(i * ch + 1) * sw], "little", signed=True) / full for i in range(n)], w.getframerate()


def rms_db(x, sr, a, b):
    i0, i1 = int(a * sr), min(int(b * sr), len(x))
    return 10 * math.log10(sum(v * v for v in x[i0:i1]) / max(1, i1 - i0) + 1e-20)


d, refd = sys.argv[1], sys.argv[2]
for p in sorted(glob.glob(os.path.join(d, "stab_snarehats__*.wav"))):
    name = os.path.basename(p)
    col = name.split("__")[1].split("_")[0]
    ref, sr = read(os.path.join(refd, f"stab_snarehats__{col}_none.wav"))
    y, _ = read(p)
    # The stab itself keys a short dip at 0.5 s (and, in the layer voicing,
    # goes into the bed a little quieter for it): take the difference before
    # the drums (3.5-3.95 s) as the baseline.
    base = rms_db(ref, sr, 3.5, 3.95) - rms_db(y, sr, 3.5, 3.95)
    worst = 0.0
    t = 4.0
    while t < 15.0 - 1e-6:
        for s in (t + 0.5, t + 1.0):  # just after each snare (on 2 and 4)
            worst = max(worst, rms_db(ref, sr, s + 0.03, s + 0.08) - rms_db(y, sr, s + 0.03, s + 0.08) - base)
        t += 1.0
    overall = rms_db(ref, sr, 4.0, 15.0) - rms_db(y, sr, 4.0, 15.0) - base
    print(f"{name:40s} ducked by the snares: up to {worst:5.2f} dB just after a snare, {overall:5.2f} dB overall "
          f"(baseline before the drums {base:+.2f} dB)")
