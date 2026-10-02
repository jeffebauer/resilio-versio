#!/usr/bin/env python3
"""Impulse response and response measurements from a recorded exponential sine
sweep (test_audio/stimulus/03_sweep.wav and its level variants, Farina method).

For each file (aligned to the stimulus timeline, e.g. renders/references/
wellspring/wellspring_D.wav, or a Resilio render of 03_sweep.wav):
  - deconvolves the sweep into an impulse response per output channel (the
    linear part; harmonic distortion lands before t = 0 and is measured there)
  - frequency response of the first 0.5 s and of the whole IR, 1/3 octave,
    normalised to the 500 Hz-1 kHz average; the treble roll-off (slope above
    the -6 dB point) and the -6 dB corner
  - first arrival per octave band (where each band's energy first reaches
    -10 dB of its peak in the first 300 ms): the dispersion
  - harmonic distortion: energy of the 2nd/3rd harmonic responses vs the linear
  - L/R: correlation of the two IRs' fine structure (width) and their level difference
Writes <out>/<name>_ir.wav (stereo IR, 4 s) and prints a table. The owner's
recordings never leave the Mac; outputs go under renders/ (gitignored).

  python3 tools/sweep_ir.py renders/references/wellspring/wellspring_D.wav my_render.wav --out renders/sweep_ir
Needs numpy, scipy, soundfile.
"""
import argparse, math, os
import numpy as np, soundfile as sf
from scipy.signal import fftconvolve, butter, sosfilt

SR = 48000
F1, F2, T, LEAD = 20.0, 20000.0, 10.0, 1.0  # make_stimulus.sweep(): 1 s silence, 10 s sweep


def inverse_filter():
    n = int(T * SR)
    t = np.arange(n) / SR
    k = math.log(F2 / F1)
    s = np.sin(2 * np.pi * F1 * T / k * (np.exp(t * k / T) - 1))
    fade = int(0.05 * SR)
    env = np.minimum(1.0, np.minimum(np.arange(n) / fade, (n - 1 - np.arange(n)) / fade))
    s *= env
    # time-reversed sweep with a -6 dB/oct amplitude ramp (Farina): flattens the pink spectrum
    inv = s[::-1] * np.exp(-t * k / T)
    # normalise so a pass-through gives a unit impulse
    ref = fftconvolve(s, inv)
    return inv / np.abs(ref).max(), n


def third_oct(ir, lo=50, hi=16000):
    P = np.abs(np.fft.rfft(ir, 1 << 19)) ** 2
    f = np.fft.rfftfreq(1 << 19, 1 / SR)
    cents = [lo * 2 ** (i / 3) for i in range(int(3 * math.log2(hi / lo)) + 1)]
    out = []
    for c in cents:
        m = (f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(10 * np.log10(P[m].mean() + 1e-30))
    out = np.array(out)
    ref = out[[i for i, c in enumerate(cents) if 500 <= c <= 1000]].mean()
    return np.array(cents), out - ref


def rolloff(c, r):
    """-6 dB corner above 1 kHz and the slope (dB/oct) over the octave above it."""
    above = [(ci, ri) for ci, ri in zip(c, r) if ci >= 1000]
    corner = next((ci for ci, ri in above if ri <= -6), None)
    if corner is None:
        return None, None
    a = np.interp(math.log2(corner), np.log2(c), r)
    b = np.interp(math.log2(corner * 2), np.log2(c), r)
    return corner, b - a


def arrivals(ir):
    out = {}
    for fc in (250, 500, 1000, 2000, 4000, 8000):
        sos = butter(4, [fc / math.sqrt(2), min(fc * math.sqrt(2), 23000)], btype='band', fs=SR, output='sos')
        e = sosfilt(sos, ir[: int(0.3 * SR)]) ** 2
        e = np.convolve(e, np.ones(48) / 48, 'same')
        if e.max() <= 0:
            out[fc] = float('nan')
            continue
        db = 10 * np.log10(e / e.max() + 1e-30)
        out[fc] = 1000 * np.argmax(db >= -10) / SR
    return out


def analyse(path, inv, n_sweep, out_dir):
    y, sr = sf.read(path)
    assert sr == SR, path
    if y.ndim == 1:
        y = np.stack([y, y], 1)
    res = {}
    irs = []
    for ch in range(2):
        full = fftconvolve(y[:, ch], inv)
        t0 = int(LEAD * SR) + n_sweep - 1  # the linear response's t = 0
        ir = full[t0: t0 + int(4 * SR)]
        # harmonics: the 2nd lands T*ln2/ln(F2/F1) before t0, the 3rd T*ln3/ln(F2/F1)
        k = math.log(F2 / F1)
        h = {}
        for order in (2, 3):
            d = int(T * math.log(order) / k * SR)
            seg = full[t0 - d - int(0.02 * SR): t0 - d + int(0.3 * SR)]
            h[order] = 10 * np.log10(np.sum(seg ** 2) / np.sum(ir[: int(0.32 * SR)] ** 2) + 1e-30)
        irs.append(ir)
        res[ch] = h
    L, R = irs
    M = 0.5 * (L + R)
    c, early = third_oct(M[: int(0.5 * SR)])
    _, whole = third_oct(M)
    corner, slope = rolloff(c, early)
    fs = slice(int(0.05 * SR), int(1.0 * SR))
    width = float(np.corrcoef(L[fs], R[fs])[0, 1])
    lr_db = 10 * np.log10(np.sum(L ** 2) / np.sum(R ** 2))
    name = os.path.splitext(os.path.basename(path))[0]
    sf.write(os.path.join(out_dir, f'{name}_ir.wav'), np.stack([L, R], 1) / max(np.abs(L).max(), np.abs(R).max()), SR,
             subtype='FLOAT')
    return dict(name=name, cents=c, early=early, whole=whole, corner=corner, slope=slope, arr=arrivals(M),
                h2=np.mean([res[0][2], res[1][2]]), h3=np.mean([res[0][3], res[1][3]]), width=width, lr_db=lr_db)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='+')
    ap.add_argument('--out', default='renders/sweep_ir')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    inv, n = inverse_filter()
    rs = [analyse(p, inv, n, a.out) for p in a.files]
    print('Frequency response, first 0.5 s, dB re 500 Hz-1 kHz (1/3 octave):')
    print('Hz      ' + ' '.join(f'{r["name"][:14]:>14}' for r in rs))
    for i, cen in enumerate(rs[0]['cents']):
        if cen < 60 or i % 1:
            continue
        print(f'{cen:7.0f} ' + ' '.join(f'{r["early"][i]:14.1f}' for r in rs))
    print()
    for r in rs:
        print(f"{r['name']}: -6 dB corner {r['corner']:.0f} Hz, slope over the next octave {r['slope']:.1f} dB"
              if r['corner'] else f"{r['name']}: no -6 dB point above 1 kHz")
        print(f"   first arrival (ms) per band 250/500/1k/2k/4k/8k: " +
              ' / '.join(f"{v:.1f}" for v in r['arr'].values()))
        print(f"   2nd / 3rd harmonic: {r['h2']:.1f} / {r['h3']:.1f} dB re linear; "
              f"L/R IR correlation 50 ms-1 s {r['width']:.2f}; L/R level {r['lr_db']:+.1f} dB")


if __name__ == '__main__':
    main()
