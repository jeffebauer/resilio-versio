#!/usr/bin/env python3
"""Pitch vs time for a held tone: median pitch, wobble depth (cents), wobble rate (Hz).

Built for the Magneto WOW & FLUTTER series (docs/recording-recipe-magneto.md,
takes MW0-MW4, stimulus `08_held_tones.wav`'s first 8 s, a held 1 kHz sine)
and reusable for any other single-pitched held tone.

Method: a coarse spectral-peak pitch estimate locates the tone, a narrow
bandpass isolates it, then an FFT-based Hilbert transform gives the analytic
signal whose *unwrapped instantaneous phase derivative* is the pitch track
(sub-cent resolution -- a per-frame autocorrelation-lag estimate is too
coarse at ~1 kHz, where one sample of lag is already ~35 cents). The
instantaneous-frequency track is low-pass smoothed (wobble is a few Hz at
most) and reported at 100 Hz.

Reported per file:
- median_hz: the tone's median pitch.
- p95_cents / peak_cents: pitch deviation from the median, in cents.
- wobble_rate_hz: frequency of the strongest periodic component of the cents
  track (sinusoid fit at that frequency).
- wobble_depth_cents: that sinusoid's amplitude.

Stdlib only.

Usage:
  python3 tools/pitch_track.py --selftest
  python3 tools/pitch_track.py <wav> [<wav> ...] [--start-s S] [--end-s S] [--json out.json]
"""

import cmath
import json
import math
import random
import struct
import sys
import wave
from pathlib import Path
from statistics import median as _median

# ---------------------------------------------------------------- WAV I/O

def read_wav(path):
    """-> (mono samples as list[float] in [-1, 1], sample rate)."""
    with wave.open(str(path), "rb") as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 3:
        vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    elif width == 2:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    elif width == 4:
        vals = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(raw) // 4), raw)]
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")
    if ch <= 1:
        return vals, sr
    return [sum(vals[i:i + ch]) / ch for i in range(0, len(vals), ch)], sr


# ---------------------------------------------------------------- FFT

def next_pow2(n):
    p = 1
    while p < n:
        p <<= 1
    return p


def fft(re_, im_, inverse=False):
    """In-place iterative radix-2 FFT. inverse=True flips the twiddle sign
    only -- callers must divide by len(re_) themselves for a true inverse."""
    n = len(re_)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            re_[i], re_[j] = re_[j], re_[i]
            im_[i], im_[j] = im_[j], im_[i]
    size = 2
    sign = 1.0 if inverse else -1.0
    while size <= n:
        half = size // 2
        ang = sign * 2 * math.pi / size
        tw = [(math.cos(ang * k), math.sin(ang * k)) for k in range(half)]
        for start in range(0, n, size):
            for k in range(half):
                wr, wi = tw[k]
                a, b = start + k, start + k + half
                xr = re_[b] * wr - im_[b] * wi
                xi = re_[b] * wi + im_[b] * wr
                re_[b] = re_[a] - xr
                im_[b] = im_[a] - xi
                re_[a] += xr
                im_[a] += xi
        size *= 2


# ---------------------------------------------------------------- filters

def one_pole_lp(x, sr, fc):
    a = math.exp(-2 * math.pi * fc / sr)
    y, out = 0.0, []
    for s in x:
        y = (1 - a) * s + a * y
        out.append(y)
    return out


def decimate(x, sr, target_hz):
    """Anti-aliased decimation to close to target_hz (3x one-pole LPF then
    pick every Nth sample -- fine for isolating a narrowband tone)."""
    factor = max(1, int(sr // target_hz))
    if factor <= 1:
        return list(x), float(sr)
    new_rate = sr / factor
    y = x
    for _ in range(3):
        y = one_pole_lp(y, sr, 0.45 * new_rate)
    return y[::factor], new_rate


def bandpass(x, sr, f0, bw):
    """Two cascaded RBJ constant-peak bandpass biquads."""
    q = max(0.5, f0 / max(bw, 1.0))
    w0 = 2 * math.pi * f0 / sr
    alpha = math.sin(w0) / (2 * q)
    a0 = 1 + alpha
    b0, b2 = alpha / a0, -alpha / a0
    a1, a2 = -2 * math.cos(w0) / a0, (1 - alpha) / a0
    y = x
    for _ in range(2):
        out = [0.0] * len(y)
        x1 = x2 = y1 = y2 = 0.0
        for i, s in enumerate(y):
            o = b0 * s + b2 * x2 - a1 * y1 - a2 * y2
            x2, x1, y2, y1 = x1, s, y1, o
            out[i] = o
        y = out
    return y


def smooth_series(x, sr, cutoff_hz):
    """Forward-backward one-pole lowpass: zero-phase, no group-delay bias."""
    fwd = one_pole_lp(x, sr, cutoff_hz)
    bwd = one_pole_lp(list(reversed(fwd)), sr, cutoff_hz)
    return list(reversed(bwd))


def fade_edges(x, sr, fade_s):
    n = len(x)
    f = max(1, min(n // 2, int(fade_s * sr)))
    y = list(x)
    for i in range(f):
        g = i / f
        y[i] *= g
        y[n - 1 - i] *= g
    return y


def trim_bounds(x, thresh_ratio=0.1):
    """First/last index where |x| crosses thresh_ratio * peak."""
    peak = max((abs(v) for v in x), default=0.0)
    if peak <= 0:
        return 0, 0
    thr = thresh_ratio * peak
    n = len(x)
    start = next((i for i, v in enumerate(x) if abs(v) >= thr), 0)
    end = n - next((i for i, v in enumerate(reversed(x)) if abs(v) >= thr), 0)
    return start, end


# ---------------------------------------------------------------- pitch estimation

def spectral_peak(x, sr, fmin, fmax):
    """Coarse f0: Hann-windowed FFT magnitude peak, parabolic-refined."""
    n = len(x)
    if n < 32:
        return None
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    xw = [x[i] * win[i] for i in range(n)]
    m = next_pow2(n)
    re_ = xw + [0.0] * (m - n)
    im_ = [0.0] * m
    fft(re_, im_)
    mags = [math.hypot(re_[k], im_[k]) for k in range(m // 2 + 1)]
    lo = max(1, int(fmin * m / sr))
    hi = min(m // 2, int(fmax * m / sr))
    if hi <= lo:
        return None
    k = max(range(lo, hi), key=lambda i: mags[i])
    shift = 0.0
    if 0 < k < m // 2:
        s0, s1, s2 = mags[k - 1], mags[k], mags[k + 1]
        den = s0 - 2 * s1 + s2
        if den != 0:
            shift = 0.5 * (s0 - s2) / den
    return (k + shift) * sr / m


def hilbert_analytic(x):
    """FFT-based analytic signal (Marple's method), truncated to len(x)."""
    n = len(x)
    m = next_pow2(n)
    re_ = list(x) + [0.0] * (m - n)
    im_ = [0.0] * m
    fft(re_, im_)
    h = [0.0] * m
    h[0] = 1.0
    if m % 2 == 0:
        h[m // 2] = 1.0
        for k in range(1, m // 2):
            h[k] = 2.0
    else:
        for k in range(1, (m + 1) // 2):
            h[k] = 2.0
    re2 = [re_[k] * h[k] for k in range(m)]
    im2 = [im_[k] * h[k] for k in range(m)]
    fft(re2, im2, inverse=True)
    return [complex(re2[k] / m, im2[k] / m) for k in range(n)]


def unwrap_phase(phases):
    out = [phases[0]]
    for p in phases[1:]:
        d = p - out[-1]
        d -= 2 * math.pi * round(d / (2 * math.pi))
        out.append(out[-1] + d)
    return out


def percentile(vals, p):
    if not vals:
        return None
    s = sorted(vals)
    k = (len(s) - 1) * p / 100.0
    f, c = math.floor(k), math.ceil(k)
    if f == c:
        return s[int(k)]
    return s[f] + (s[c] - s[f]) * (k - f)


def dominant_modulation(y, rate):
    """Strongest periodic component of y (sampled at `rate` Hz): its
    frequency (parabolic-refined FFT peak, >= 0.1 Hz to skip DC/drift) and
    its amplitude (least-squares sinusoid fit at that frequency, more
    accurate than the raw FFT bin under windowing/leakage)."""
    n = len(y)
    if n < 8:
        return None, None
    mean = sum(y) / n
    d = [v - mean for v in y]
    m = next_pow2(n)
    re_ = d + [0.0] * (m - n)
    im_ = [0.0] * m
    fft(re_, im_)
    mags = [math.hypot(re_[k], im_[k]) for k in range(m // 2 + 1)]
    freqs_bin = [k * rate / m for k in range(m // 2 + 1)]
    lo = max(1, int(0.1 * m / rate))
    if lo >= len(mags):
        return None, None
    k = max(range(lo, len(mags)), key=lambda i: mags[i])
    f_peak = freqs_bin[k]
    if 0 < k < m // 2:
        s0, s1, s2 = mags[k - 1], mags[k], mags[k + 1]
        den = s0 - 2 * s1 + s2
        if den != 0:
            f_peak = (k + 0.5 * (s0 - s2) / den) * rate / m
    a = 2.0 / n * sum(v * math.cos(2 * math.pi * f_peak * i / rate) for i, v in enumerate(d))
    b = 2.0 / n * sum(v * math.sin(2 * math.pi * f_peak * i / rate) for i, v in enumerate(d))
    return f_peak, math.hypot(a, b)


# ---------------------------------------------------------------- top-level analysis

WORK_RATE = 4000.0
REPORT_RATE = 100.0


def analyze(x, sr, start_s=None, end_s=None, fmin=60.0, fmax=3000.0):
    """Analyse a held tone in samples x (mono, rate sr). Returns a dict with
    ok=False + reason on failure, else ok=True + the metrics described in
    the module docstring."""
    n = len(x)
    i0 = 0 if start_s is None else max(0, min(n, int(start_s * sr)))
    i1 = n if end_s is None else max(i0, min(n, int(end_s * sr)))
    seg = x[i0:i1]
    a, b = trim_bounds(seg)
    if b - a < int(0.5 * sr):
        return {"ok": False, "reason": "segment too short or silent"}
    seg = seg[a:b]
    seg_t0 = (i0 + a) / sr

    work, wsr = decimate(seg, sr, WORK_RATE)
    if len(work) < int(1.0 * wsr):
        return {"ok": False, "reason": "decimated segment too short"}
    work = fade_edges(work, wsr, 0.02)

    f0_rough = spectral_peak(work, wsr, fmin, min(fmax, 0.9 * wsr / 2))
    if not f0_rough or f0_rough <= 0:
        return {"ok": False, "reason": "no dominant pitch found"}

    bw = max(40.0, 0.3 * f0_rough)
    filt = bandpass(work, wsr, f0_rough, bw)
    analytic = hilbert_analytic(filt)
    phase = [math.atan2(v.imag, v.real) for v in analytic]
    unwrapped = unwrap_phase(phase)
    if len(unwrapped) < 3:
        return {"ok": False, "reason": "too short after filtering"}

    inst = [(unwrapped[i + 1] - unwrapped[i]) * wsr / (2 * math.pi) for i in range(len(unwrapped) - 1)]
    inst = smooth_series(inst, wsr, 15.0)

    step = max(1, int(round(wsr / REPORT_RATE)))
    freqs = inst[::step]
    times = [seg_t0 + i * step / wsr for i in range(len(freqs))]

    trim_n = max(1, int(0.15 * REPORT_RATE))
    if len(freqs) > 2 * trim_n + 5:
        freqs = freqs[trim_n:len(freqs) - trim_n]
        times = times[trim_n:len(times) - trim_n]

    pairs = [(t, f) for t, f in zip(times, freqs) if f == f and f > 0]  # drop nan/<=0
    if len(pairs) < 5:
        return {"ok": False, "reason": "not enough valid pitch frames"}
    times = [t for t, _ in pairs]
    freqs = [f for _, f in pairs]

    med = _median(freqs)
    cents = [1200 * math.log2(f / med) for f in freqs]
    p95 = percentile([abs(c) for c in cents], 95)
    peak = max(abs(c) for c in cents)
    rate_hz, depth_cents = dominant_modulation(cents, REPORT_RATE)

    return {
        "ok": True,
        "median_hz": med,
        "p95_cents": p95,
        "peak_cents": peak,
        "wobble_rate_hz": rate_hz,
        "wobble_depth_cents": depth_cents,
        "n_frames": len(freqs),
        "segment_s": [seg_t0, seg_t0 + len(seg) / sr],
    }


def analyze_file(path, start_s=None, end_s=None, **kw):
    x, sr = read_wav(path)
    out = analyze(x, sr, start_s=start_s, end_s=end_s, **kw)
    out["file"] = str(path)
    return out


# ---------------------------------------------------------------- synthetic selftest

def synth_drift_cents(sr, dur_s, seed, ctrl_rate=20.0, step_std=3.0, revert=0.02, clip=40.0):
    """Bounded random-walk cents trajectory, coarse control points at
    ctrl_rate then linearly interpolated -- audio-rate noise would just be
    inaudible dither, not the slow drift wow&flutter/hand recordings show."""
    n_ctrl = int(dur_s * ctrl_rate) + 2
    rng = random.Random(seed)
    ctrl = [0.0]
    for _ in range(n_ctrl):
        v = ctrl[-1] * (1 - revert) + rng.gauss(0, step_std)
        v = max(-clip, min(clip, v))
        ctrl.append(v)
    n = int(dur_s * sr)
    out = [0.0] * n
    for i in range(n):
        t = i / sr * ctrl_rate
        k = min(int(t), len(ctrl) - 2)
        frac = t - k
        out[i] = ctrl[k] * (1 - frac) + ctrl[k + 1] * frac
    return out


def synth_tone(sr, dur_s, f0, cents_fn):
    n = int(dur_s * sr)
    fade = int(0.02 * sr)
    out = []
    phase = 0.0
    for i in range(n):
        c = cents_fn(i)
        f_inst = f0 * (2 ** (c / 1200.0))
        phase += 2 * math.pi * f_inst / sr
        env = min(1.0, i / fade, (n - 1 - i) / fade)
        out.append(0.5 * math.sin(phase) * env)
    return out


def check(label, got, want, rel, abs_):
    ok = got is not None and abs(got - want) <= max(rel * abs(want), abs_)
    print(f"{'PASS' if ok else 'FAIL'}  {label}: measured {fmt(got)}, target {want:.3f}")
    return ok


def fmt(v, n=3):
    return "-" if v is None else f"{v:.{n}f}"


def selftest():
    sr = 48000
    dur = 8.0
    fails = 0

    print("Case 1: fixed 1 kHz")
    x = synth_tone(sr, dur, 1000.0, lambda i: 0.0)
    r = analyze(x, sr)
    fails += not r.get("ok")
    if r.get("ok"):
        fails += not check("median Hz", r["median_hz"], 1000.0, 0.0, 2.0)
        fails += not check("p95 cents (should be ~0)", r["p95_cents"], 0.0, 0.0, 1.5)
        fails += not check("peak cents (should be ~0)", r["peak_cents"], 0.0, 0.0, 2.5)

    print("Case 2: 1 kHz +/-10 cents @ 0.5 Hz")
    x = synth_tone(sr, dur, 1000.0, lambda i: 10.0 * math.sin(2 * math.pi * 0.5 * i / sr))
    r = analyze(x, sr)
    fails += not r.get("ok")
    if r.get("ok"):
        fails += not check("wobble depth cents", r["wobble_depth_cents"], 10.0, 0.10, 0.0)
        fails += not check("wobble rate Hz", r["wobble_rate_hz"], 0.5, 0.10, 0.0)

    print("Case 3: 1 kHz +/-50 cents @ 1.5 Hz")
    x = synth_tone(sr, dur, 1000.0, lambda i: 50.0 * math.sin(2 * math.pi * 1.5 * i / sr))
    r = analyze(x, sr)
    fails += not r.get("ok")
    if r.get("ok"):
        fails += not check("wobble depth cents", r["wobble_depth_cents"], 50.0, 0.10, 0.0)
        fails += not check("wobble rate Hz", r["wobble_rate_hz"], 1.5, 0.10, 0.0)

    print("Case 4: 1 kHz, random drift (bounded random walk, +/-40 cents clip; no single true rate/depth,")
    print("        so this checks plausibility, not exact recovery -- see docs/reference-ingest.md)")
    drift = synth_drift_cents(sr, dur, seed=3)
    x = synth_tone(sr, dur, 1000.0, lambda i: drift[i])
    r = analyze(x, sr)
    # The drift trajectory is a bounded random walk, not zero-mean over any
    # given 8 s window, so its own *median* pitch can legitimately land
    # tens of cents from the nominal 1 kHz -- that is not an error. 80
    # cents (~4.6% of 1 kHz) covers the +/-40 cent clip plus estimator
    # noise; peak/p95 are checked against that same clip.
    ok4 = (r.get("ok") and abs(1200 * math.log2(r["median_hz"] / 1000.0)) <= 80.0
           and r["peak_cents"] <= 60.0 and r["p95_cents"] <= 45.0)
    print(f"{'PASS' if ok4 else 'FAIL'}  plausible: median {fmt(r.get('median_hz'))} Hz, "
          f"p95 {fmt(r.get('p95_cents'))} cents, peak {fmt(r.get('peak_cents'))} cents, "
          f"dominant rate {fmt(r.get('wobble_rate_hz'))} Hz (slow drift, not a strict target)")
    fails += not ok4

    print(("PASS" if fails == 0 else "FAIL") + f"  selftest ({fails} failures)")
    return fails


# ---------------------------------------------------------------- CLI

def main(argv):
    if "--selftest" in argv:
        return 1 if selftest() else 0

    start_s = end_s = None
    out_json = None
    files = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--start-s":
            start_s = float(argv[i + 1]); i += 2
        elif a == "--end-s":
            end_s = float(argv[i + 1]); i += 2
        elif a == "--json":
            out_json = argv[i + 1]; i += 2
        else:
            files.append(a); i += 1

    if not files:
        print(__doc__)
        return 2

    rows = [analyze_file(f, start_s=start_s, end_s=end_s) for f in files]
    for r in rows:
        if r["ok"]:
            print(f"{r['file']}: median {r['median_hz']:.2f} Hz, p95 {r['p95_cents']:.2f}c, "
                  f"peak {r['peak_cents']:.2f}c, wobble {fmt(r['wobble_depth_cents'],2)}c "
                  f"@ {fmt(r['wobble_rate_hz'],2)} Hz ({r['n_frames']} frames)")
        else:
            print(f"{r['file']}: FAILED ({r['reason']})")
    if out_json:
        Path(out_json).write_text(json.dumps(rows, indent=2) + "\n")
    return 0 if all(r["ok"] for r in rows) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
