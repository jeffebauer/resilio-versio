#!/usr/bin/env python3
"""Chirp-shape (dispersion) measurement for spring impulse responses.

What it measures, per file (see docs/ir-dispersion-study.md):
- repeat_ms: the chirp repeat time at 1 kHz, i.e. the tank's round trip.
- lows_later_ms: how much longer the round trip is at 200 Hz than at 2 kHz.
  Every echo gets this much more smeared than the one before, so it is the
  "chirp shape" per trip. Positive = lows later (a normal, falling chirp).
- fc_hz: where the chirp ridge stops (transition frequency), if visible.
- quality 0-1 and a category (clean tank / processed / unclear).

Method (after Gamper, Parker & Valimaki, DAFx-11, section 3.1-3.3): split the
IR into narrow bands (1/6-octave centres, 125 Hz - 6.3 kHz). In each band,
take the amplitude envelope, flatten its decay, and autocorrelate it. A spring
repeats every round trip, and the round trip is longer for low frequencies,
so each band's autocorrelation peaks at that band's own round-trip time P(f).
Tracking that peak from band to band gives the whole curve P(f); the chirp
shape is P(200 Hz) - P(2 kHz). Autocorrelation only looks at *spacing*
between echoes, so it is immune to the broadband pulse at t = 0 (which is
gated out anyway), to filter delays and to where the first echo lands.

Usage:
  python3 tools/ir_dispersion.py --selftest
  python3 tools/ir_dispersion.py --renders renders/m1_click_grid
  python3 tools/ir_dispersion.py <dir-or-wav> ... [--json out.json] [--renders dir]

Stdlib only. Uses all CPU cores (multiprocessing); ~1 min for the library.
"""

import cmath
import json
import math
import multiprocessing
import random
import re
import struct
import sys
import wave
from pathlib import Path

# ---------------------------------------------------------------- constants

BAND_LO_HZ = 125.0
BAND_Q = 3.0                 # per biquad; two in cascade ≈ 1/4-octave wide
MAX_BW_HZ = 150.0            # above NARROW_FROM_HZ, bands are this wide (see band_q)
NARROW_FROM_HZ = 1700.0
ENV_RATE = 4000.0            # envelope sample rate (0.25 ms steps)
MAX_WINDOW_S = 2.0           # analyse at most this much after the onset
MIN_LAG_S = 0.012            # shortest round trip we look for
MAX_LAG_S = 0.30
R_MIN = 0.10                 # weakest autocorrelation peak accepted on the ridge
TRACK_TOL = 0.14             # neighbour band may differ by ±14 % (min 2.5 ms)
F_LO, F_HI, F_REPEAT = 200.0, 2000.0, 1000.0
FC_EDGE_CAL = 0.905          # half-clarity edge -> fC (see analyse_channel)


def band_centres():
    """1/6-octave centres to 1 kHz, then 1/12-octave to ~7 kHz: near fC the
    round trip changes fast with frequency, so the ridge needs finer steps."""
    out = [BAND_LO_HZ * 2 ** (k / 6) for k in range(23)]          # 125 .. 1587 Hz
    out += [1587.4 * 2 ** (k / 12) for k in range(2, 27)]         # 1.78 .. 6.7 kHz
    return out


def band_q(f):
    """Per-biquad Q. Constant-Q (≈1/4 octave for the cascade) at low f, but
    ~150 Hz bandwidth above 1.7 kHz, so one band does not straddle a
    steep stretch of the ridge (P can change 20+ ms per kHz just below fC)."""
    return BAND_Q if f < NARROW_FROM_HZ else max(BAND_Q, 0.64 * f / MAX_BW_HZ)


# ---------------------------------------------------------------- WAV + FFT

def read_wav(path):
    """-> (list of channels, sample rate). PCM 16/24/32 bit."""
    with wave.open(str(path), "rb") as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 3:
        b = bytes(raw)
        vals = [int.from_bytes(b[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(b), 3)]
    elif width == 2:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    elif width == 4:
        vals = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(raw) // 4), raw)]
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")
    return [vals[c::ch] for c in range(ch)], sr


def fft(re_, im_, inverse=False):
    """In-place iterative radix-2 complex FFT on two lists (length 2^k)."""
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


def next_pow2(n):
    p = 1
    while p < n:
        p <<= 1
    return p


# ---------------------------------------------------------------- band envelopes

def bandpass2(x, sr, f0, q):
    """Two cascaded RBJ constant-peak band-pass biquads (4th order)."""
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


def envelope(y, sr, smooth_hz):
    """Energy -> one-pole smoothed -> decimated to ENV_RATE -> amplitude."""
    a = math.exp(-2 * math.pi * smooth_hz / sr)
    step = max(1, int(round(sr / ENV_RATE)))
    env, e = [], 0.0
    for i, s in enumerate(y):
        e = (1 - a) * s * s + a * e
        if i % step == 0:
            env.append(math.sqrt(e))
    return env, sr / step


def acf_normalised(d):
    """Autocorrelation of zero-mean d, unbiased, normalised to 1 at lag 0."""
    n = len(d)
    m = next_pow2(2 * n)
    re_ = d + [0.0] * (m - n)
    im_ = [0.0] * m
    fft(re_, im_)
    p = [re_[i] * re_[i] + im_[i] * im_[i] for i in range(m)]
    z = [0.0] * m
    fft(p, z, inverse=True)
    r0 = p[0] / n
    if r0 <= 0:
        return [0.0] * n
    # Unbiased (divide by overlap), but keep the overlap >= n/3 so long lags
    # are not noise-amplified.
    return [(p[k] / max(n - k, n / 3)) / r0 for k in range(n)]


def peaks(r, lo, hi):
    """Local maxima (lag, height) of r in [lo, hi), parabolic-refined."""
    out = []
    for k in range(max(1, lo), min(hi, len(r) - 1)):
        if r[k] > r[k - 1] and r[k] >= r[k + 1]:
            den = r[k - 1] - 2 * r[k] + r[k + 1]
            dk = 0.5 * (r[k - 1] - r[k + 1]) / den if den != 0 else 0.0
            out.append((k + dk, r[k] - 0.25 * (r[k - 1] - r[k + 1]) * dk))
    return out


def band_acfs(x, sr, onset, gate_s, window_s):
    """Per band: (f, acf list at ENV_RATE, window seconds used)."""
    start = onset + int(gate_s * sr)
    end = min(len(x), onset + int(window_s * sr))
    seg = x[max(0, start - int(0.05 * sr)):end]   # 50 ms pre-roll lets filters settle
    pre = (start - max(0, start - int(0.05 * sr)))
    res = []
    for f in band_centres():
        if f > 0.42 * sr:
            break
        q = band_q(f)
        y = bandpass2(seg, sr, f, q)
        bw = 0.64 * f / q
        env, rate = envelope(y, sr, min(250.0, max(15.0, 0.6 * bw)))
        env = env[int(pre * rate / sr):]
        if len(env) < int(0.1 * rate):
            continue
        db = [20 * math.log10(v + 1e-12) for v in env]
        top = max(db)
        # Stop where the band has sunk into the noise: last point within 45 dB
        # of the band's maximum (smoothed over 20 ms).
        w = int(0.02 * rate)
        last = len(db) - 1
        while last > w and max(db[last - w:last + 1]) < top - 45:
            last -= w
        env, db = env[:last + 1], db[:last + 1]
        n = len(env)
        if n < int(0.1 * rate):
            continue
        # Flatten the decay: least-squares line through the dB envelope.
        mt = (n - 1) / 2
        mdb = sum(db) / n
        sxx = sum((i - mt) ** 2 for i in range(n)) or 1.0
        slope = sum((i - mt) * (db[i] - mdb) for i in range(n)) / sxx
        flat = [env[i] * 10 ** (-(slope * (i - mt)) / 20) for i in range(n)]
        mean = sum(flat) / n
        d = [v - mean for v in flat]
        res.append((f, acf_normalised(d), n / rate, rate))
    return res


# ---------------------------------------------------------------- ridge tracking

def interp_logf(ridge, f):
    """P at frequency f by linear interpolation in log f; None if outside."""
    pts = [(bf, p) for bf, p, _ in ridge if p is not None]
    for (f0, p0), (f1, p1) in zip(pts, pts[1:]):
        if f0 <= f <= f1:
            t = math.log(f / f0) / math.log(f1 / f0)
            return p0 + t * (p1 - p0)
    return None


def track_ridge(acfs, p_hint=None):
    """Follow the round-trip peak across bands. Returns (ridge, anchor_index).
    ridge = [(f, P seconds or None, r)]."""
    info = []
    for f, r, _, rate in acfs:
        lo, hi = int(MIN_LAG_S * rate), int(min(MAX_LAG_S, len(r) / (2 * rate)) * rate)
        info.append((f, r, rate, peaks(r, lo, hi)))
    # Anchor: the clearest band between 400 Hz and 1.6 kHz. Within a band,
    # prefer the shortest lag whose peak is >= 70 % of the band's best
    # (so we lock onto P, not 2P).
    best = None
    for i, (f, r, rate, pk) in enumerate(info):
        if not 400 <= f <= 1600 or not pk:
            continue
        cand = pk
        if p_hint is not None:
            cand = [q for q in pk if abs(q[0] / rate - p_hint) <= max(0.35 * p_hint, 0.004)] or []
        if not cand:
            continue
        top = max(h for _, h in cand)
        if top < R_MIN:
            continue
        lag, h = min((q for q in cand if q[1] >= 0.7 * top), key=lambda q: q[0])
        if best is None or h > best[2]:
            best = (i, lag / rate, h)
    if best is None:
        return [(f, None, 0.0) for f, *_ in info], None
    ai = best[0]
    ridge = [None] * len(info)
    ridge[ai] = (info[ai][0], best[1], best[2])
    for direction in (1, -1):
        p_prev, misses = best[1], 0
        i = ai + direction
        while 0 <= i < len(info):
            f, r, rate, pk = info[i]
            tol = max(TRACK_TOL * p_prev * (1 + misses), 0.0025)
            cand = [(lag / rate, h) for lag, h in pk if abs(lag / rate - p_prev) <= tol and h >= R_MIN]
            if cand:
                p, h = max(cand, key=lambda q: q[1] - 2.0 * abs(q[0] - p_prev) / p_prev)
                ridge[i] = (f, p, h)
                p_prev, misses = p, 0
            else:
                ridge[i] = (f, None, 0.0)
                misses += 1
                if misses >= 3:
                    for j in range(i + direction, len(info) if direction > 0 else -1, direction):
                        ridge[j] = (info[j][0], None, 0.0)
                    break
            i += direction
    return ridge, ai


def median(v):
    v = sorted(v)
    return v[len(v) // 2] if v else 0.0


def analyse_channel(x, sr):
    peak = max(abs(v) for v in x) or 1.0
    onset = next(i for i, v in enumerate(x) if abs(v) >= 0.1 * peak)
    # Pass 1: rough round trip with a short gate. Pass 2: gate out ~30 % of a
    # round trip after the onset (removes the broadband pulse at t = 0 and
    # its cross-terms with the first echo), then track the ridge.
    acfs = band_acfs(x, sr, onset, 0.004, MAX_WINDOW_S)
    ridge, ai = track_ridge(acfs)
    if ai is None:
        return {"ok": False}
    p0 = ridge[ai][1]
    acfs = band_acfs(x, sr, onset, max(0.004, 0.3 * p0), MAX_WINDOW_S)
    ridge, ai = track_ridge(acfs, p_hint=p0)
    if ai is None:
        return {"ok": False}

    p_lo, p_hi, p_rep = interp_logf(ridge, F_LO), interp_logf(ridge, F_HI), interp_logf(ridge, F_REPEAT)
    hi_used = F_HI
    if p_hi is None:   # ridge ends below 2 kHz: use its highest point >= 1.4 kHz
        top = [(f, p) for f, p, _ in ridge if p is not None and f >= 1400]
        if top:
            hi_used, p_hi = top[-1]
    lows_later = (p_lo - p_hi) * 1000 if p_lo is not None and p_hi is not None else None

    # Transition frequency + model parameters: fit the allpass-cascade shape
    # to the ridge (DAFx-11 §3.2.2). fC is where the fitted curve bottoms
    # out (dispersion stops). Only "visible" if the ridge reaches it.
    mid_r = median([h for f, p, h in ridge if p is not None and 300 <= f <= 1500])
    # Rising ridge (highs later, like real tanks)? Then fit only its rising
    # branch, from the fastest band up: a single allpass cascade cannot also
    # make the small lows-later hook many real tanks have at the bottom.
    p_top = interp_logf(ridge, 2500.0) or interp_logf(ridge, 1800.0)
    rising = p_top is not None and p_rep is not None and p_top > p_rep + 0.0005
    f_from = 150.0
    if rising:
        low_part = [(p, f) for f, p, h in ridge if p is not None and 150 <= f <= 1500 and h >= 0.45 * mid_r]
        if low_part:
            f_from = min(low_part)[1]
    fit = fit_allpass(ridge, sr, 0.45 * mid_r, f_from)
    # fC itself: the fit's fC trades off against a when the ridge simply
    # flattens out (DAFx-11 notes the same), so the reported fC is where the
    # ridge's periodicity has fallen to half its mid-band value (log-f
    # interpolated) going up from the anchor: pulses overlap / the chirp band
    # ends (DAFx-11 §3.1.2). Only reported if there is a chirp (> 2 ms).
    fc = None
    if lows_later is not None and abs(lows_later) > 1.0 and mid_r > 0:
        half = 0.5 * mid_r
        for i in range(ai, len(ridge) - 1):
            f0, _, h0 = ridge[i]
            f1, p1, h1 = ridge[i + 1]
            h1 = h1 if p1 is not None else 0.0
            if f0 >= 1000 and h0 >= half > h1:
                t = (h0 - half) / (h0 - h1)
                # The half-clarity edge sits ~10 % above the true fC
                # because the analysis bands have finite width; FC_EDGE_CAL
                # (calibrated on the synthetic IRs, --selftest) removes that.
                fc = FC_EDGE_CAL * f0 * (f1 / f0) ** t
                break

    # For rising chirps (a > 0, what real tanks do) the fitted curve has a
    # sharp knee at fC, so the fit's fC is well determined: prefer it.
    fc_edge = fc
    if rising and fit is not None and fit["a"] > 0.2 and fit["rms_err_ms"] < 1.0 and 1000 < fit["fc_hz"] < 7000:
        fc = fit["fc_hz"]

    # Chirp shape in two numbers, both >= 0, over the tracked chirp band
    # (150 Hz .. fC). Real ridges are often U-shaped: fastest somewhere in
    # the mids, a small lows-later hook at the bottom and a big highs-later
    # rise toward fC. Our model's ridge falls monotonically (all hook).
    #   highs_later_ms = P(top of band) - P(fastest band)
    #   lows_hook_ms   = P(bottom of band) - P(fastest band)
    # Ridge points are median-of-3 smoothed; a jump > 12 % between bands
    # (the tracker hopping onto another echo series) splits the ridge and
    # the longest unbroken stretch is used.
    # The band runs up to fC: the fitted fC for rising ridges (their clarity
    # fades well below fC, so the half-clarity edge would cut the chirp
    # short), else the edge; 6 kHz if neither. The jump rule below stops it
    # where the tracker hops onto another echo series.
    f_cap = fc if fc is not None else 6000.0
    pts = [(f, p) for f, p, h in ridge if p is not None and h >= 0.15 and 150 <= f <= f_cap]
    segs, cur = [], pts[:1]
    for q0, q1 in zip(pts, pts[1:]):
        if abs(q1[1] - q0[1]) > 0.12 * q0[1]:
            segs.append(cur)
            cur = []
        cur.append(q1)
    segs.append(cur)
    band = max(segs, key=len)            # longest unbroken stretch of ridge
    highs_later = lows_hook = span_lo = span_hi = None
    if len(band) >= 5:
        sm = [band[0][1]] + [median([band[i - 1][1], band[i][1], band[i + 1][1]]) for i in range(1, len(band) - 1)] \
            + [band[-1][1]]
        pmin = min(sm)
        highs_later, lows_hook = (sm[-1] - pmin) * 1000, (sm[0] - pmin) * 1000
        span_lo, span_hi = band[0][0], band[-1][0]
    direction = None
    if highs_later is not None:
        if max(highs_later, lows_hook) < 1.5:
            direction = "none (plain echo)"
        else:
            direction = "rising (highs later)" if highs_later > lows_hook else "falling (lows later)"

    # Quality: ridge clarity x coverage x smoothness between 150 Hz and 2.5 kHz.
    span = [(f, p, h) for f, p, h in ridge if 150 <= f <= 2500]
    got = [(f, p, h) for f, p, h in span if p is not None]
    coverage = len(got) / max(1, len(span))
    clarity = sum(h for *_, h in got) / max(1, len(got))
    # Smoothness: residual of P from a quadratic in log f, relative to P.
    rough = 1.0
    if len(got) >= 5:
        xs = [math.log2(f) for f, _, _ in got]
        ys = [p for _, p, _ in got]
        c = polyfit2(xs, ys)
        res = math.sqrt(sum((y - (c[0] + c[1] * x + c[2] * x * x)) ** 2 for x, y in zip(xs, ys)) / len(ys))
        rough = max(0.0, 1.0 - res / (0.03 * p0))
    quality = max(0.0, min(1.0, 1.6 * clarity)) * coverage * (0.5 + 0.5 * rough)

    return {
        "ok": True,
        "repeat_ms": p_rep * 1000 if p_rep is not None else None,
        "p200_ms": p_lo * 1000 if p_lo is not None else None,
        "p2k_ms": p_hi * 1000 if p_hi is not None else None,
        "hi_ref_hz": hi_used,
        "lows_later_ms": lows_later,
        "fc_hz": fc,
        "fc_edge_hz": fc_edge,
        "highs_later_ms": highs_later,
        "lows_hook_ms": lows_hook,
        "span_band_hz": [span_lo, span_hi],
        "direction": direction,
        "fit": fit,
        "quality": round(quality, 3),
        "clarity": round(clarity, 3),
        "coverage": round(coverage, 3),
        "smoothness": round(rough, 3),
        "ridge": [{"f": round(f, 1), "p_ms": None if p is None else round(p * 1000, 3), "r": round(h, 3)}
                  for f, p, h in ridge],
    }


def fit_allpass(ridge, sr, r_min, f_from=150.0):
    """Fit P(f) = L + M * tau_ap(f; a, K), K = sr/(2 fC), to the ridge.
    tau_ap = K (1 - a^2) / (1 + a^2 + 2 a cos(pi f / fC)) samples. Grid over
    (fC, a); L and the amplitude M·K·(1-a²) are then linear least squares,
    weighted by ridge clarity. Returns our model's equivalent a, M, fC, L."""
    pts = [(f, p, h) for f, p, h in ridge if p is not None and h >= r_min and f >= f_from]
    # Robust: fit, drop points far off the curve (e.g. the tracker hopping
    # onto the faster high-frequency echo series above fC), refit.
    for _ in range(3):
        if len(pts) < 6:
            return None
        best = _fit_grid(pts)
        if best is None:
            return None
        err, fc, a, L, amp = best
        res = [abs(p - L - amp / (1 + a * a + 2 * a * math.cos(math.pi * f / fc))) for f, p, _ in pts]
        lim = max(3 * median(res), 0.0004)
        keep = [q for q, e in zip(pts, res) if e <= lim]
        if len(keep) == len(pts):
            break
        pts = keep
    return _fit_result(best, sr, len(pts))


def _fit_grid(pts):
    best = None
    for i in range(70):
        fc = 1200.0 * 2 ** (i / 24)                     # 1.2 .. ~9.5 kHz, 1/24 oct
        for j in list(range(-95, -4)) + list(range(5, 96)):   # both chirp directions
            a = j / 100.0
            # basis u(f) = 1/(1 + a² + 2a cos θ)
            sw = swu = swuu = swp = swup = 0.0
            for f, p, h in pts:
                u = 1.0 / (1 + a * a + 2 * a * math.cos(math.pi * f / fc))
                sw += h
                swu += h * u
                swuu += h * u * u
                swp += h * p
                swup += h * u * p
            det = sw * swuu - swu * swu
            if det <= 1e-15:
                continue
            amp = (sw * swup - swu * swp) / det
            L = (swp - amp * swu) / sw
            if amp <= 0:
                continue
            err = sum(h * (p - L - amp / (1 + a * a + 2 * a * math.cos(math.pi * f / fc))) ** 2
                      for f, p, h in pts) / sw
            if best is None or err < best[0]:
                best = (err, fc, a, L, amp)
    return best


def _fit_result(best, sr, npts):
    err, fc, a, L, amp = best
    K = sr / (2 * fc)
    M = amp * sr / (K * (1 - a * a))

    def P(f):
        return L + amp / (1 + a * a + 2 * a * math.cos(math.pi * f / fc))
    return {"fc_hz": round(fc, 1), "a": a, "M": round(M, 1), "K": round(K, 2), "L_ms": round(L * 1000, 2),
            "rms_err_ms": round(math.sqrt(err) * 1000, 3), "n_points": npts,
            "lows_later_ms": round((P(F_LO) - P(min(F_HI, fc))) * 1000, 2)}


def polyfit2(xs, ys):
    """Least-squares y = c0 + c1 x + c2 x^2 (normal equations, 3x3 solve)."""
    s = [sum(x ** k for x in xs) for k in range(5)]
    t = [sum(y * x ** k for x, y in zip(xs, ys)) for k in range(3)]
    a = [[s[0], s[1], s[2], t[0]], [s[1], s[2], s[3], t[1]], [s[2], s[3], s[4], t[2]]]
    for c in range(3):
        piv = max(range(c, 3), key=lambda r_: abs(a[r_][c]))
        a[c], a[piv] = a[piv], a[c]
        if abs(a[c][c]) < 1e-18:
            return [sum(ys) / len(ys), 0.0, 0.0]
        for r_ in range(3):
            if r_ != c:
                k = a[r_][c] / a[c][c]
                a[r_] = [u - k * v for u, v in zip(a[r_], a[c])]
    return [a[i][3] / a[i][i] for i in range(3)]


# ---------------------------------------------------------------- per file

# Names that say something other than a bare tank is in the signal chain.
PROCESSED_WORDS = ("cab", "tapedelay", "delayl", "layered", "diffused", "slapback", "overdrive")


def is_plain_echo(res):
    """A clean, regular repeat with no chirp at all: a tape/digital delay,
    not a spring (e.g. the delay side of the Swissecho RevR/DelayL files)."""
    return bool(res.get("ok")) and res["clarity"] >= 0.6 and res.get("direction") == "none (plain echo)"


def category(name, res, has_delay_channel):
    """clean tank / processed / unclear. Processing comes from the file name
    or from a plain-echo channel in the file; 'unclear' when the ridge is too
    weak or broken to trust (quality < 0.25 or no chirp shape)."""
    n = name.lower()
    processed = has_delay_channel or any(w in n for w in PROCESSED_WORDS)
    if not res.get("ok") or res["quality"] < 0.25 or res.get("highs_later_ms") is None:
        return "unclear"
    return "processed" if processed else "clean tank"


def analyse_file(path):
    chans, sr = read_wav(path)
    name = Path(path).name
    options = {}
    if len(chans) == 1:
        options["mono"] = chans[0]
    else:
        options["mono"] = [sum(v) / len(v) for v in zip(*chans)]
        for c, lab in zip(chans, ("L", "R")):
            options[lab] = c
    results = {lab: analyse_channel(x, sr) for lab, x in options.items()}
    # Which channel: the clearest ridge (mono wins near-ties: it is what we'd
    # hear), but never a plain-echo channel (a delay, not the spring). The
    # Swissecho "RevR_DelayL" files turn out to have the plain delay on R
    # and the spring on L, so the name is not trusted.
    delays = [k for k, v in results.items() if k != "mono" and is_plain_echo(v)]
    cands = [k for k in results if k not in delays and not (k == "mono" and delays)] or list(results)
    pick = max(cands, key=lambda k: (results[k].get("quality", 0) if results[k].get("ok") else -1)
               + (0.02 if k == "mono" else 0))
    best = dict(results[pick])
    best["channel"] = pick
    sidecar = Path(path).with_suffix(".json")
    t60 = None
    if sidecar.exists():
        try:
            t60 = json.loads(sidecar.read_text()).get("metrics", {}).get("t60_s")
        except (ValueError, OSError):
            pass
    out = {"file": name, "t60_s": t60, **best,
           "channels": {k: {kk: v[kk] for kk in ("repeat_ms", "lows_later_ms", "highs_later_ms", "lows_hook_ms",
                                                 "fc_hz", "quality", "direction")} if v.get("ok")
                        else None for k, v in results.items()}}
    out["plain_echo_channels"] = delays
    out["category"] = category(name, best, bool(delays))
    return out


# ---------------------------------------------------------------- our model's prediction

# Mirrors core/params/Mappings.h + SpringModes.h (Spring A detune) + Spring.cpp.
# Kept in sync by hand; the renders check (--renders) catches drift.
MAP = dict(t60=(0.4, 9.0), L=(0.030, 0.100), fc=(4200.0, 2700.0), a=(-0.45, -0.72), M=(24, 64),
           damp=(1600.0, 9000.0))
DETUNE_A = dict(L=0.965, fc=1.040, a=1.030)


def exp_lerp(lo, hi, v):
    return lo * math.exp(v * math.log(hi / lo))


def model_settings(decay, boing, tone=0.5, detune=True):
    d = DETUNE_A if detune else dict(L=1.0, fc=1.0, a=1.0)
    return {
        "L_s": exp_lerp(*MAP["L"], decay) * d["L"],
        "fc_hz": exp_lerp(*MAP["fc"], decay) * d["fc"],
        "a": (MAP["a"][0] + (MAP["a"][1] - MAP["a"][0]) * boing) * d["a"],
        "M": MAP["M"][0] + int((MAP["M"][1] - MAP["M"][0]) * boing + 0.5),
        "damp_hz": exp_lerp(*MAP["damp"], tone),
    }


def loop_response(s, sr, f):
    """Complex response of one Loop trip (excluding g) at f, as Spring.cpp."""
    w = 2 * math.pi * f / sr
    z1 = cmath.exp(-1j * w)
    K = sr / (2 * s["fc_hz"])
    zk = cmath.exp(-1j * w * K)
    ap = (s["a"] + zk) / (1 + s["a"] * zk)
    # DC blocker 40 Hz
    r = 1 - 2 * math.pi * 40 / sr
    dc = 0.5 * (1 + r) * (1 - z1) / (1 - r * z1)
    # RBJ Butterworth LPF at fC
    w0 = 2 * math.pi * s["fc_hz"] / sr
    cw, al = math.cos(w0), math.sin(w0) / (2 * 0.7071)
    a0 = 1 + al
    b0 = 0.5 * (1 - cw) / a0
    lp = (b0 + 2 * b0 * z1 + b0 * z1 * z1) / (1 + (-2 * cw / a0) * z1 + ((1 - al) / a0) * z1 * z1)
    # one-pole damping
    c = 1 - math.exp(-2 * math.pi * min(s["damp_hz"], 0.45 * sr) / sr)
    op = c / (1 - (1 - c) * z1)
    return ap ** s["M"] * dc * lp * op * cmath.exp(-1j * w * s["L_s"] * sr)


def model_round_trip_s(s, sr, f):
    """Group delay of one Loop trip at f (numeric phase derivative)."""
    df = 0.5
    p0 = cmath.phase(loop_response(s, sr, f - df) / loop_response(s, sr, f + df))
    return p0 / (2 * math.pi * 2 * df)


def model_prediction(decay, boing, sr=48000, detune=True):
    s = model_settings(decay, boing, detune=detune)
    p = {f: model_round_trip_s(s, sr, f) for f in (F_LO, F_REPEAT, F_HI)}
    return {"repeat_ms": p[F_REPEAT] * 1000, "lows_later_ms": (p[F_LO] - p[F_HI]) * 1000, "fc_hz": s["fc_hz"],
            "L_ms": s["L_s"] * 1000, "M": s["M"], "a": s["a"], "K": sr / (2 * s["fc_hz"])}


# ---------------------------------------------------------------- synthetic validation

def synth_ir(sr, L_s, a, M, fc, g, dur_s, pulse_gain, hf_ratio, seed=1):
    """Known-dispersion test IR built in the frequency domain:
    a loud broadband pulse at t = 0 (like the Ableton IRs), then a feedback
    Loop of delay L + M stretched allpasses (coefficient a, K = sr/(2 fc)),
    brick-wall-free zero-phase lowpass at fc (so the group delay truth is
    exactly L + M*tau_ap), plus a non-dispersive HF echo series above fc at
    hf_ratio*L (like a high path) and a -75 dB noise floor.
    Truth round trip: P(f) = L + M * tau_ap(f)."""
    n = next_pow2(int(dur_s * sr))
    K = sr / (2 * fc)
    re_, im_ = [0.0] * n, [0.0] * n
    first_tap = 0.5 * L_s * sr
    for k in range(n // 2 + 1):
        f = k * sr / n
        w = 2 * math.pi * k / n
        zk = cmath.exp(-1j * w * K)
        ap = 1
        for ai, mi in _sections(a, M):
            ap *= ((ai + zk) / (1 + ai * zk)) ** mi
        lp = 1 / math.sqrt(1 + (f / fc) ** 8)            # zero-phase magnitude
        hp = 1 / math.sqrt(1 + (0.8 * fc / max(f, 1e-9)) ** 4)
        loop = g * ap * lp * cmath.exp(-1j * w * L_s * sr)
        low = ap * lp * cmath.exp(-1j * w * first_tap) / (1 - loop)
        hl = hf_ratio * L_s * sr
        high = 0.4 * hp * cmath.exp(-1j * w * 0.5 * hl) / (1 - 0.85 * g * cmath.exp(-1j * w * hl))
        h = low + high
        re_[k], im_[k] = h.real, h.imag
        if 0 < k < n // 2:
            re_[n - k], im_[n - k] = h.real, -h.imag
    fft(re_, im_, inverse=True)
    x = [v / n for v in re_]
    rng = random.Random(seed)
    pk = max(abs(v) for v in x)
    # Broadband pulse: a 1.5 ms noise burst at t = 0, pulse_gain x the tank's peak.
    for i in range(int(0.0015 * sr)):
        x[i] += pulse_gain * pk * rng.uniform(-1, 1) * (1 - i / (0.0015 * sr))
    pk2 = max(abs(v) for v in x)
    return [v / pk2 + 10 ** (-75 / 20) * rng.gauss(0, 1) for v in x]


def _sections(a, M):
    """(a, M) or ((a1, a2, ...), (M1, M2, ...)): cascades in series."""
    return list(zip(a, M)) if isinstance(a, tuple) else [(a, M)]


def synth_truth(sr, L_s, a, M, fc):
    K = sr / (2 * fc)

    def tau(f):
        th = 2 * math.pi * f * K / sr
        return L_s + sum(mi * K * (1 - ai * ai) / (1 + ai * ai + 2 * ai * math.cos(th))
                         for ai, mi in _sections(a, M)) / sr
    return {"repeat_ms": tau(F_REPEAT) * 1000, "lows_later_ms": (tau(F_LO) - tau(F_HI)) * 1000, "fc_hz": fc,
            "tau": tau, "a": a, "M": M}


SYNTH_CASES = [
    # name, L, a, M, fc, g, pulse gain vs tank peak
    ("mid tank", 0.050, -0.60, 44, 3400.0, 0.80, 8.0),
    ("short, soft chirp", 0.030, -0.45, 24, 4200.0, 0.70, 8.0),
    ("long, steep chirp", 0.090, -0.72, 64, 2700.0, 0.85, 8.0),
    ("Leem-like, falling", 0.045, -0.62, 100, 4300.0, 0.75, 8.0),
    # Real tanks: group delay *rises* toward fC (highs later), DAFx-11 Table 1
    # fits a = +0.62..0.69 for the Leem Pro KA-1210.
    ("Leem KA-1210 (DAFx-11, a > 0)", 0.050, 0.62, 100, 4300.0, 0.75, 8.0),
    ("rising, mid", 0.055, 0.50, 60, 3300.0, 0.80, 8.0),
    ("tiny dispersion", 0.060, -0.20, 20, 3400.0, 0.80, 8.0),
    # U-shaped ridge like most Ableton tanks: small lows-later hook at the
    # bottom (a < 0 cascade) plus a big highs-later rise toward fC (a > 0).
    ("U-shaped, like real tanks", 0.080, (-0.5, 0.5), (8, 70), 3000.0, 0.8, 8.0),
]


def _synth_job(case):
    name, L, a, M, fc, g, pg = case
    sr = 48000
    x = synth_ir(sr, L, a, M, fc, g, 2.0, pg, 0.43)
    res = analyse_channel(x, sr)
    t = synth_truth(sr, L, a, M, fc)
    lo, hi = res.get("span_band_hz", [None, None]) if res.get("ok") else (None, None)
    if lo:
        grid = [lo * (hi / lo) ** (i / 200) for i in range(201)]
        pmin = min(t["tau"](f) for f in grid)
        t["highs_later_ms"] = (t["tau"](hi) - pmin) * 1000
        t["lows_hook_ms"] = (t["tau"](lo) - pmin) * 1000
    else:
        t["highs_later_ms"] = t["lows_hook_ms"] = None
    del t["tau"]
    return name, res, t


def check(label, got, want, rel, abs_):
    ok = got is not None and abs(got - want) <= max(rel * abs(want), abs_)
    print(f"{'PASS' if ok else 'FAIL'}  {label}: measured {fmt(got, 2)}, truth {want:.2f}")
    return ok


def selftest(pool):
    print("Synthetic IRs (broadband pulse at t = 0 + known allpass-cascade dispersion)")
    fails = 0
    for name, res, truth in pool.map(_synth_job, SYNTH_CASES):
        if not res.get("ok"):
            print(f"FAIL  {name}: no ridge found")
            fails += 1
            continue
        fails += not check(f"{name} repeat ms", res["repeat_ms"], truth["repeat_ms"], 0.02, 0.5)
        fails += not check(f"{name} lows-later ms", res["lows_later_ms"], truth["lows_later_ms"], 0.10, 1.0)
        if truth["highs_later_ms"] is not None:
            band = f"{res['span_band_hz'][0]:.0f}-{res['span_band_hz'][1]:.0f} Hz"
            fails += not check(f"{name} highs-later ms ({band})", res["highs_later_ms"], truth["highs_later_ms"],
                               0.15, 1.5)
            fails += not check(f"{name} lows-hook ms ({band})", res["lows_hook_ms"], truth["lows_hook_ms"], 0.15, 1.5)
        if abs(truth["lows_later_ms"]) > 4.0 or max(_sections(truth["a"], truth["M"]))[0] > 0:   # fC is only visible when there is a chirp to end
            fails += not check(f"{name} fC Hz", res["fc_hz"], truth["fc_hz"], 0.12, 0)
        ft = res["fit"] or {}
        print(f"      fit: a {ft.get('a')}, M {ft.get('M')}, fC {ft.get('fc_hz')}, L {ft.get('L_ms')} ms"
              f" (truth a {truth['a']}, M {truth['M']});"
              f" quality {res['quality']:.2f}")
    print(("PASS" if fails == 0 else "FAIL") + f"  selftest ({fails} failures)")
    return fails


def renders_check(pool, d):
    files = sorted(Path(d).glob("*.wav"))
    rows = pool.map(analyse_file, [str(f) for f in files])
    print("Own renders vs mapping prediction (Spring A, mono sum)")
    fails, out = 0, []
    for f, r in zip(files, rows):
        m = re.search(r"decay(\d+\.\d+)_boing(\d+\.\d+)", f.name)
        if not m:
            continue
        dcy, bng = float(m.group(1)), float(m.group(2))
        pred = model_prediction(dcy, bng)
        tag = f"DECAY {dcy:.2f} BOING {bng:.2f}"
        fails += not check(f"{tag} repeat ms", r.get("repeat_ms"), pred["repeat_ms"], 0.04, 1.0)
        fails += not check(f"{tag} lows-later ms", r.get("lows_later_ms"), pred["lows_later_ms"], 0.15, 1.5)
        print(f"      fC measured {fmt(r.get('fc_hz'), 0)} (model {pred['fc_hz']:.0f}), quality {r.get('quality')}")
        out.append({"decay": dcy, "boing": bng, "measured": {k: r.get(k) for k in
                    ("repeat_ms", "lows_later_ms", "fc_hz", "quality")}, "predicted": pred})
    print(("PASS" if fails == 0 else "FAIL") + f"  renders check ({fails} failures)")
    return fails, out


def fmt(v, n=1):
    return "—" if v is None else f"{v:.{n}f}"


def main(argv):
    out_json = None
    if "--json" in argv:
        i = argv.index("--json")
        out_json = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    renders = None
    if "--renders" in argv:
        i = argv.index("--renders")
        renders = argv[i + 1]
        argv = argv[:i] + argv[i + 2:]
    do_self = "--selftest" in argv
    argv = [a for a in argv if a != "--selftest"]
    files = []
    for a in argv:
        p = Path(a)
        files += sorted(p.glob("*.wav")) if p.is_dir() else [p]
    status = 0
    doc = {}
    with multiprocessing.Pool() as pool:
        if do_self:
            status |= selftest(pool) != 0
        if renders:
            f, rows = renders_check(pool, renders)
            status |= f != 0
            doc["renders"] = rows
        if files:
            rows = pool.map(analyse_file, [str(f) for f in files])
            print(f"{'file':48} {'ch':>4} {'rep ms':>7} {'200-2k':>7} {'highs+':>7} {'lows+':>6} {'fC Hz':>6} "
                  f"{'qual':>5}  category")
            for r in rows:
                print(f"{r['file'][:48]:48} {r.get('channel', ''):>4} {fmt(r.get('repeat_ms')):>7} "
                      f"{fmt(r.get('lows_later_ms')):>7} {fmt(r.get('highs_later_ms')):>7} "
                      f"{fmt(r.get('lows_hook_ms')):>6} {fmt(r.get('fc_hz'), 0):>6} "
                      f"{fmt(r.get('quality'), 2):>5}  {r['category']}")
            doc["irs"] = rows
    if out_json:
        doc["model_grid"] = [{"decay": d, "boing": b, **model_prediction(d, b)}
                             for d in (0.0, 0.25, 0.5, 0.75, 1.0) for b in (0.0, 0.5, 1.0)]
        Path(out_json).write_text(json.dumps(doc, indent=1) + "\n")
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
