#!/usr/bin/env python3
"""Chirp measurements for spring impulse responses and Renderer click renders.

Measures what the Renderer's sidecar metrics don't:
- repeat_ms: how often the boing comes back (envelope autocorrelation peak,
  15-250 ms). Close to the tank's round-trip time.
- repeat_clarity: height of that peak (0-1). High = distinct repeating chirps,
  low = smeared wash.
- dispersion_ms: EXPERIMENTAL, not reliable on Ableton IRs. How much later
  the lows (200-500 Hz) arrive than the highs (2-4 kHz) within the first
  repeat. Real-tank IRs start with a broadband pulse (dry signal or direct
  coupling) that swamps the first chirp, so this reads ~0 or negative for
  them. A proper method tracks chirp ridges in later echoes (SPEC ref 5);
  planned for M8 (ADR 0021).
- brightness_db: energy at 3-6 kHz relative to 0.7-1.4 kHz, over the tail.
- t60_s: from the file's sidecar JSON (rv_render --analyze), if present.

Usage: python3 tools/ir_analysis.py <dir-or-wav> ... [--json out.json]
Stdlib only (slow-ish, fine for a few dozen short files).
"""

import json
import math
import struct
import sys
import wave
from pathlib import Path


def read_mono(path):
    with wave.open(str(path), "rb") as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 3:
        vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    elif width == 2:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")
    mono = [sum(vals[i:i + ch]) / ch for i in range(0, len(vals), ch)]
    return mono, sr


def bandpass(x, sr, f0, q):
    """RBJ constant-peak band-pass biquad."""
    w0 = 2 * math.pi * f0 / sr
    alpha = math.sin(w0) / (2 * q)
    b0, b2 = alpha, -alpha
    a0, a1, a2 = 1 + alpha, -2 * math.cos(w0), 1 - alpha
    b0, b2, a1, a2 = b0 / a0, b2 / a0, a1 / a0, a2 / a0
    y, x1, x2, y1, y2 = [], 0.0, 0.0, 0.0, 0.0
    for s in x:
        o = b0 * s + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, s, y1, o
        y.append(o)
    return y


def envelope(x, sr, out_rate=1000, smooth_hz=150):
    """Rectified, one-pole smoothed, decimated amplitude envelope."""
    a = math.exp(-2 * math.pi * smooth_hz / sr)
    step = max(1, sr // out_rate)
    env, y = [], 0.0
    for i, s in enumerate(x):
        y = (1 - a) * abs(s) + a * y
        if i % step == 0:
            env.append(y)
    return env, sr / step


def centroid(env, rate, start, end):
    seg = env[start:end]
    total = sum(v * v for v in seg)
    if total <= 0:
        return None
    return sum(i * v * v for i, v in enumerate(seg)) / total / rate


def analyse(path):
    x, sr = read_mono(path)
    peak = max(abs(v) for v in x) or 1.0
    onset = next(i for i, v in enumerate(x) if abs(v) >= 0.1 * peak)

    env, rate = envelope(x, sr)
    o = int(onset * rate / sr)
    seg = env[o:o + int(1.5 * rate)]
    mean = sum(seg) / len(seg)
    d = [v - mean for v in seg]
    e0 = sum(v * v for v in d) or 1.0
    best_lag, best = None, -1.0
    for lag in range(int(0.015 * rate), min(int(0.25 * rate), len(d) - 1)):
        r = sum(d[i] * d[i + lag] for i in range(len(d) - lag)) / e0
        if r > best:
            best, best_lag = r, lag
    repeat_ms = best_lag / rate * 1000 if best_lag else None

    win_end = o + (best_lag if best_lag else int(0.08 * rate))
    lo_env, _ = envelope(bandpass(x, sr, 316, 1.2), sr)
    hi_env, _ = envelope(bandpass(x, sr, 2830, 1.2), sr)
    c_lo, c_hi = centroid(lo_env, rate, o, win_end), centroid(hi_env, rate, o, win_end)
    dispersion_ms = (c_lo - c_hi) * 1000 if c_lo is not None and c_hi is not None else None

    tail = x[onset:onset + int(2.0 * sr)]
    e_hi = sum(v * v for v in bandpass(tail, sr, 4240, 1.4))
    e_mid = sum(v * v for v in bandpass(tail, sr, 1000, 1.4))
    brightness_db = 10 * math.log10(e_hi / e_mid) if e_hi > 0 and e_mid > 0 else None

    t60 = None
    sidecar = Path(path).with_suffix(".json")
    if sidecar.exists():
        t60 = json.loads(sidecar.read_text()).get("metrics", {}).get("t60_s")

    return {
        "file": Path(path).name,
        "t60_s": t60,
        "repeat_ms": repeat_ms,
        "repeat_clarity": best,
        "dispersion_ms": dispersion_ms,
        "brightness_db": brightness_db,
    }


def fmt(v, n=1):
    return "—" if v is None else f"{v:.{n}f}"


def main(argv):
    out_json = None
    if "--json" in argv:
        i = argv.index("--json")
        out_json = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    files = []
    for a in argv:
        p = Path(a)
        files += sorted(p.glob("*.wav")) if p.is_dir() else [p]
    rows = [analyse(f) for f in files]
    print(f"{'file':58} {'T60 s':>6} {'repeat ms':>9} {'clarity':>7} {'disp ms':>7} {'bright dB':>9}")
    for r in rows:
        print(f"{r['file'][:58]:58} {fmt(r['t60_s'], 2):>6} {fmt(r['repeat_ms']):>9} {fmt(r['repeat_clarity'], 2):>7} "
              f"{fmt(r['dispersion_ms']):>7} {fmt(r['brightness_db']):>9}")
    if out_json:
        Path(out_json).write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    main(sys.argv[1:])
