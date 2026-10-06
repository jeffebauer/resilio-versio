"""F round 2 measures (scratch). Renders with rv_render and measures:
  sweep  : 03_sweep response, first 0.5 s, dB re 500 Hz-1 kHz at 63/100/126/159/200/300 Hz (tools/sweep_ir.py method)
  clicks : tail low-mid balance (150-800 vs 800-6000 Hz, 30 ms-1 s after each click, mid), level
  rms    : wet RMS (dBFS, L+R) of a render
"""
import json, math, os, subprocess, sys
import numpy as np, soundfile as sf

WT = str(__import__("pathlib").Path(__file__).resolve().parents[3]) + '/.claude/worktrees/wf2'
sys.path.insert(0, os.path.join(WT, 'tools'))
import sweep_ir  # noqa: E402

SR = 48000
STIM = os.path.join(WT, 'test_audio/stimulus')
SP = os.environ.get('F2_OUT', os.path.join(WT, 'build-r/f2_measure'))  # scratch renders (not in git)
RENDER = os.environ.get('RV_RENDER', os.path.join(WT, 'build-r/rv_render'))
NOON = dict(springs='2', attitude='CLEAN', decay=0.5, tension=0.5, tone=0.5, splash=0.3, drive=0.25, wobble=0.45, mix=1.0)
CLK = (1.0, 9.0, 17.0)


def render(stim, out, params, tune=None):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    pj = out[:-4] + '.preset.json'
    json.dump(params, open(pj, 'w'))
    env = dict(os.environ)
    env.pop('RV_TANKV_TUNE', None)
    if tune:
        env['RV_TANKV_TUNE'] = tune
    subprocess.run([RENDER, os.path.join(STIM, stim), out, '--preset', pj], check=True, capture_output=True, env=env)
    return out


_INV = None


def sweep_resp(path, freqs=(63, 100, 126, 159, 200, 300)):
    global _INV
    if _INV is None:
        _INV = sweep_ir.inverse_filter()
    inv, n = _INV
    y, _ = sf.read(path)
    from scipy.signal import fftconvolve
    irs = []
    for ch in range(2):
        full = fftconvolve(y[:, ch], inv)
        t0 = int(sweep_ir.LEAD * SR) + n - 1
        irs.append(full[t0:t0 + int(4 * SR)])
    M = 0.5 * (irs[0] + irs[1])[:int(0.5 * SR)]
    P = np.abs(np.fft.rfft(M, 1 << 19)) ** 2
    f = np.fft.rfftfreq(1 << 19, 1 / SR)

    def b(c):
        m = (f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))
        return 10 * np.log10(P[m].mean() + 1e-30)
    ref = np.mean([b(50 * 2 ** (i / 3)) for i in range(31) if 500 <= 50 * 2 ** (i / 3) <= 1000])
    return [b(c) - ref for c in freqs]


def bandp(x, lo, hi):
    P = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return P[(f >= lo) & (f < hi)].sum() + 1e-30


def clicks(path):
    y, _ = sf.read(path)
    m = 0.5 * (y[:, 0] + y[:, 1])
    lm = []
    for c in CLK:
        s = m[int((c + .03) * SR):int((c + 1.0) * SR)]
        lm.append(10 * np.log10(bandp(s, 150, 800) / bandp(s, 800, 6000)))
    return float(np.mean(lm))


def rms(path):
    y, _ = sf.read(path)
    return float(10 * np.log10(np.mean(y ** 2) + 1e-30))


def low_share(path, lo=40, hi=200):
    """energy below 200 Hz re 200 Hz-6 kHz over the whole file, dB (how much bass the wet carries)"""
    y, _ = sf.read(path)
    m = 0.5 * (y[:, 0] + y[:, 1])
    P = np.zeros(SR // 2 + 1)
    for a in range(0, len(m) - SR, SR):
        P += np.abs(np.fft.rfft(m[a:a + SR] * np.hanning(SR))) ** 2
    f = np.fft.rfftfreq(SR, 1 / SR)
    return float(10 * np.log10(P[(f >= lo) & (f < hi)].sum() / P[(f >= 200) & (f < 6000)].sum()))


sys.path.insert(0, os.path.join(WT, 'docs/prototypes/wellspring-fit-3'))
import wellspring_character as WC  # noqa: E402
import wellspring_settings_fit as WSF  # noqa: E402
from scipy.signal import butter, sosfiltfilt  # noqa: E402


def _bp(y, lo, hi):
    return sosfiltfilt(butter(4, [lo, hi], btype='band', fs=SR, output='sos'), y, axis=0)


def character(path):
    """SPRINGS 3 measures on a 01_clicks render (clicks at 1, 9, 17 s)."""
    y, _ = sf.read(path)
    y = y[:24 * SR]
    m = y.mean(1)
    wc = WC.measure(path)
    f = WSF.features(y)
    cen, low, drip = [], [], []
    for c in CLK:
        a = int(c * SR)
        s = m[a + int(.1 * SR):a + int(1.5 * SR)]
        P = np.abs(np.fft.rfft(s * np.hanning(len(s)))) ** 2
        fr = np.fft.rfftfreq(len(s), 1 / SR)
        k = (fr >= 50) & (fr <= 12000)
        cen.append((P[k] * fr[k]).sum() / P[k].sum())
        low.append(100 * P[k & (fr < 400)].sum() / P[k].sum())
        w = int(.002 * SR)
        env = np.array([np.sqrt(np.mean(m[i:i + w] ** 2)) for i in range(a + int(.05 * SR), a + int(.5 * SR) - w + 1, w)])
        drip.append(env.std() / env.mean())
    b = _bp(y, 200, 5000)
    L = np.concatenate([b[int(c * SR) + int(.2 * SR):int(c * SR) + int(1.5 * SR), 0] for c in CLK])
    R = np.concatenate([b[int(c * SR) + int(.2 * SR):int(c * SR) + int(1.5 * SR), 1] for c in CLK])
    return dict(t60=float(f['t60_all']), t60b=[float(f['t60'][q]) for q in WSF.BANDS], echo_ms=float(f['echo_ms']),
                centroid=float(np.mean(cen)), low400=float(np.mean(low)), corr=float(np.corrcoef(L, R)[0, 1]),
                drip=float(np.mean(drip)), ned=[wc['ned50'], wc['ned150'], wc['ned400']],
                jump=wc['flicker_db'], level=float(10 * np.log10(np.mean(y ** 2) + 1e-30)))


def char_row(name, c):
    return (f"{name:22s} T60 {c['t60']:.2f}  cen {c['centroid']:5.0f} (<400 {c['low400']:2.0f}%)  corr {c['corr']:5.2f}  "
            f"drip {c['drip']:.2f}  NED {c['ned'][0]:.2f}/{c['ned'][1]:.2f}/{c['ned'][2]:.2f}  jump {c['jump']:.1f}  "
            f"echo {c['echo_ms']:.0f}  lvl {c['level']:6.1f}")


def envelope(path):
    """How the tail grows after a click (mono, 01_clicks, mean of 3 clicks):
    energy 150-400 ms vs 30-150 ms (dB; higher = more swell / bloom), the time of
    the loudest 20 ms window after 25 ms, and 400-1000 ms vs 30-150 ms."""
    y, _ = sf.read(path)
    m = y.mean(1)
    sw, pk, late = [], [], []
    for c in CLK:
        a = int(c * SR)
        e = m[a:a + SR] ** 2
        def E(t0, t1):
            return e[int(t0 * SR):int(t1 * SR)].mean() + 1e-30
        sw.append(10 * np.log10(E(.15, .4) / E(.03, .15)))
        late.append(10 * np.log10(E(.4, 1.0) / E(.03, .15)))
        w = int(.02 * SR)
        en = np.convolve(e, np.ones(w) / w, 'valid')
        pk.append(1000 * (int(.025 * SR) + np.argmax(en[int(.025 * SR):int(.8 * SR)])) / SR)
    return float(np.mean(sw)), float(np.median(pk)), float(np.mean(late))
