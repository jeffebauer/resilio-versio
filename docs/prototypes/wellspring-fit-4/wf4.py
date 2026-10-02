"""Wellspring fit round 4: shared helpers (render a Tank voicing at the corrected closest settings,
measure it next to the Wellspring). Builds on ../wellspring-fit-3/wf3.py (render, DECAY fit, the
round-3 metrics) and adds the round-4 numbers:

  onset_hf    energy above 4 kHz vs 200 Hz-4 kHz in the first 60 ms after each click, dB
              (tools/wellspring_settings_fit.py features; Wellspring -25.7)
  darkening   how much the repeats darken: (2-6 kHz vs 300 Hz-1 kHz) at 0.6-0.8 s minus the same
              at 0.05-0.25 s after each click, dB (Wellspring -6.4)
  spec_rms    the tail's tonal balance vs the Wellspring's (1/3 octaves 100 Hz-6.4 kHz, 30 ms-1 s,
              level-normalised), dB rms: the fit tool's spectral score
  echo_ms     main echo spacing, 200-1000 Hz envelope (fit tool, fixed 2 Oct 2026; Wellspring 65)
  stereo      tail L/R correlation 0.2-1.5 s, 200 Hz-5 kHz (wf3.stereo), and over 0.3-2 s broadband
The Wellspring takes are read in place (renders/references/wellspring/, never copied: ADR 0009).
Needs numpy, scipy, soundfile.
"""
import os, sys
import numpy as np, soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '../wellspring-fit-3'))
sys.path.insert(0, os.path.join(HERE, '../../../tools'))
import wf3                                    # noqa: E402
from wf3 import WSF, WC, SR, REF_A, ROOT, WT  # noqa: E402,F401

OUT = os.path.join(ROOT, 'renders/wellspring_fit4')
wf3.OUT = OUT
wf3.CROP = os.path.join(OUT, 'scratch/clicks_crop.wav')
# The corrected closest settings (tools/wellspring_settings_fit.py on main, 2 Oct 2026).
CLOSEST = dict(springs='2', attitude='CLEAN', mix=1.0, splash=0.0, wobble=0.5, drive=0.25, tension=0.5, tone=0.7)
wf3.CLOSEST = CLOSEST
CLICK_S = WSF.CLICKS


def _ref():
    w, _ = sf.read(REF_A)
    return WSF.features(w[:24 * SR])


REF = None


def ref():
    global REF
    if REF is None:
        REF = _ref()
    return REF


def band_power(x, lo, hi):
    P = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return P[(f >= lo) & (f < hi)].sum() + 1e-30


def darkening(m):
    out = []
    for c in CLICK_S:
        a = int(c * SR)
        e = m[a + int(.05 * SR):a + int(.25 * SR)]
        l = m[a + int(.6 * SR):a + int(.8 * SR)]
        r = lambda s: 10 * np.log10(band_power(s, 2000, 6000) / band_power(s, 300, 1000))
        out.append(r(l) - r(e))
    return float(np.mean(out))


def third_spec(m, t0, t1):
    """1/3-octave spectrum (100 Hz-6.4 kHz) of t0..t1 s after each click, summed, dB (not normalised)."""
    edges = 100 * 2 ** (np.arange(0, 19) / 3)
    acc = np.zeros(len(edges) - 1)
    for c in CLICK_S:
        s = m[int((c + t0) * SR):int((c + t1) * SR)]
        P = np.abs(np.fft.rfft(s * np.hanning(len(s)))) ** 2
        fr = np.fft.rfftfreq(len(s), 1 / SR)
        acc += [P[(fr >= a) & (fr < b)].sum() + 1e-30 for a, b in zip(edges[:-1], edges[1:])]
    return 10 * np.log10(acc), np.sqrt(edges[:-1] * edges[1:])


def stereo_wide(y):
    L = np.concatenate([y[int((c + .3) * SR):int((c + 2) * SR), 0] for c in CLICK_S])
    R = np.concatenate([y[int((c + .3) * SR):int((c + 2) * SR), 1] for c in CLICK_S])
    return float(np.corrcoef(L, R)[0, 1])


def measure(path):
    """Round 3's measure (wf3) plus the round-4 numbers."""
    r = wf3.measure(path)
    y, _ = sf.read(path)
    if y.ndim == 1:
        y = np.stack([y, y], 1)
    y = y[:24 * SR]
    f = WSF.features(y)
    r['onset_hf'] = f['onset_hf_db']
    r['echo_ms'] = f['echo_ms']
    r['spec_rms'] = float(np.sqrt(np.mean((np.array(f['spec']) - np.array(ref()['spec'])) ** 2)))
    r['darkening'] = darkening(y.mean(1))
    r['corr_wide'] = stereo_wide(y)
    return r


def row(name, r):
    c = r['character']
    return (f"{name:16} onsetHF {r['onset_hf']:6.1f}  dark {r['darkening']:5.1f}  spec {r['spec_rms']:4.1f}  "
            f"echo {r['echo_ms']:3.0f}  NED {c['ned50']:.2f}/{c['ned150']:.2f}/{c['ned400']:.2f}  lowmid {c['lowmid']:5.1f}  "
            f"jump {c['flicker_db']:4.1f}  envc {c['lr_env_corr']:.2f}  corr {r['stereo_corr']:5.2f}/{r['corr_wide']:5.2f}  "
            f"T60 {'/'.join(f'{t:.2f}' for t in r['t60'])} (all {r['t60_all']:.2f})")


# ---- Round 4, the sweep (tools/sweep_ir.py on the Wellspring's sweep take D) ----
import sweep_ir as SIR  # noqa: E402
REF_D = os.path.join(ROOT, 'renders/references/wellspring/wellspring_D.wav')
SWEEP = os.path.join(WT, 'test_audio/stimulus/03_sweep.wav')
SIR_OUT = os.path.join(OUT, 'sweep_ir')
_INV = None


def sweep(path):
    """tools/sweep_ir.py analyse(): early (0.5 s) 1/3-octave response re 500 Hz-1 kHz, -6 dB corner and
    slope, first arrival per band, 2nd / 3rd harmonic, L/R IR correlation and level."""
    global _INV
    if _INV is None:
        _INV = SIR.inverse_filter()
    os.makedirs(SIR_OUT, exist_ok=True)
    return SIR.analyse(path, _INV[0], _INV[1], SIR_OUT)


REF_SWEEP = None


def ref_sweep():
    global REF_SWEEP
    if REF_SWEEP is None:
        REF_SWEEP = sweep(REF_D)
    return REF_SWEEP


def sweep_err(r, lo=1000, hi=8000):
    """rms dB of the early response vs the Wellspring's between lo and hi Hz."""
    R = ref_sweep()
    m = (r['cents'] >= lo) & (r['cents'] <= hi)
    return float(np.sqrt(np.mean((r['early'][m] - R['early'][m]) ** 2)))


def sweep_row(name, r):
    return (f"{name:16} corner {r['corner'] or 0:5.0f} slope {r['slope'] or 0:6.1f}  arr " +
            '/'.join(f"{v:.1f}" for v in r['arr'].values()) +
            f"  h2/h3 {r['h2']:.1f}/{r['h3']:.1f}  LRcorr {r['width']:.2f} LR {r['lr_db']:+.1f}  err1-8k {sweep_err(r):.1f} err63-1k {sweep_err(r, 60, 1000):.1f}")
