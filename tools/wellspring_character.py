#!/usr/bin/env python3
"""Measure the three character differences the owner hears between the Wellspring and Resilio
(2 Oct 2026): diffuseness, low/mid presence and distance, left-right flicker. On click takes
(first three clicks of 01_clicks, 8 s apart). Usage:
  python3 tools/wellspring_character.py A.wav B.wav ...   (aligned to the 01_clicks timeline)
"""
import sys, numpy as np, soundfile as sf
from math import erfc, sqrt
SR = 48000
CLICKS = [1.0, 9.0, 17.0]

def ned(x, t0, t1, win=0.02):
    """Normalised echo density (Abel & Huang): ~0 sparse echoes, ~1 Gaussian (diffuse)."""
    out = []
    w = int(win * SR)
    for s in range(int(t0 * SR), int(t1 * SR) - w, w // 2):
        seg = x[s:s + w]
        sd = seg.std() + 1e-20
        out.append(np.mean(np.abs(seg) > sd) / erfc(1 / sqrt(2)))
    return np.array(out)

def band_db(x, lo, hi):
    P = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return 10 * np.log10(P[(f >= lo) & (f < hi)].sum() + 1e-30)

def measure(path):
    y, _ = sf.read(path)
    if y.ndim == 1: y = np.stack([y, y], 1)
    L, R = y[:, 0], y[:, 1]
    M = 0.5 * (L + R)
    r = {k: [] for k in ('ned50', 'ned150', 'ned400', 't_diffuse', 'c50', 'lowmid', 'flicker_db', 'lr_env_corr')}
    for c in CLICKS:
        a = int(c * SR)
        n = ned(M[a:], 0, 1.0)
        times = np.arange(len(n)) * 0.01 + 0.01
        r['ned50'].append(n[times <= 0.05].mean()); r['ned150'].append(n[(times > 0.1) & (times <= 0.2)].mean())
        r['ned400'].append(n[(times > 0.3) & (times <= 0.5)].mean())
        idx = np.where(n >= 0.8)[0]; r['t_diffuse'].append(times[idx[0]] if len(idx) else 1.0)
        e = M[a:a + int(3 * SR)] ** 2
        r['c50'].append(10 * np.log10(e[:int(0.05 * SR)].sum() / e[int(0.05 * SR):].sum()))
        tail = M[a + int(0.03 * SR): a + int(1.0 * SR)]
        r['lowmid'].append(band_db(tail, 150, 800) - band_db(tail, 800, 6000))
        h = int(0.01 * SR)
        el = np.array([np.mean(L[a + i * h: a + (i + 1) * h] ** 2) for i in range(50)]) + 1e-20
        er = np.array([np.mean(R[a + i * h: a + (i + 1) * h] ** 2) for i in range(50)]) + 1e-20
        d = 10 * np.log10(el / er)
        r['flicker_db'].append(np.std(np.diff(d)))  # how much the L/R balance jumps 10 ms to 10 ms (first 0.5 s)
        r['lr_env_corr'].append(np.corrcoef(np.sqrt(el), np.sqrt(er))[0, 1])
    return {k: float(np.mean(v)) for k, v in r.items()}

if __name__ == '__main__':
    print(f"{'file':42} {'NED 0-50ms':>10} {'100-200':>8} {'300-500':>8} {'t diffuse':>9} {'C50 dB':>7} {'lowmid dB':>9} {'L/R jump dB':>11} {'L/R env corr':>12}")
    for p in sys.argv[1:]:
        m = measure(p)
        print(f"{p.split('/')[-1][:42]:42} {m['ned50']:10.2f} {m['ned150']:8.2f} {m['ned400']:8.2f} {m['t_diffuse']:8.2f}s {m['c50']:7.1f} {m['lowmid']:9.1f} {m['flicker_db']:11.1f} {m['lr_env_corr']:12.2f}")
