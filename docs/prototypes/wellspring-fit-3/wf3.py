"""Wellspring fit round 3: shared helpers (render a Tank voicing, measure it next to the Wellspring).

Renders the first three clicks of 01_clicks (24 s crop) at the closest knob settings
(docs/m8-tuning-backlog.md "Closest knob settings to the Wellspring"), with the hidden
`tank_voicing` key and, for fitting, RV_TANKV_TUNE (core/params/TankVoicing.h Tuning fields).
Measures:
  character   tools/wellspring_character.py: echo density (NED) 0-50 / 100-200 / 300-500 ms,
              tail low-mid balance (150-800 Hz vs 800 Hz-6 kHz, 30 ms-1 s), L/R balance jump per
              10 ms and L/R envelope correlation (first 0.5 s)
  t60         tools/wellspring_settings_fit.py features: per-octave T60 250 Hz-4 kHz, broadband
              T60 (the DECAY fit's target), echo spacing (1-4 kHz envelope repeat)
  arc         wfmetrics.py (proto/wellspring-fit): the first arc (ridge, ms after its 1 kHz) and the
              energy-weighted arrival per band
  stereo      late tail (0.2-1.5 s) L/R correlation, 200 Hz-5 kHz, and side re mid (dB)
  first echo  ms from the click to the first 1-4 kHz envelope peak within 6 dB of the largest
The Wellspring takes are read in place (renders/references/wellspring/, never copied: ADR 0009).
Needs numpy, scipy, soundfile.
"""
import json, os, subprocess, sys, tempfile
import numpy as np, soundfile as sf
from scipy.signal import butter, sosfiltfilt

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, '../../..'))          # the worktree / checkout
ROOT = str(__import__("pathlib").Path(__file__).resolve().parents[3])         # the main checkout (renders, recordings)
sys.path.insert(0, os.path.join(WT, 'tools'))
sys.path.insert(0, HERE)
import wellspring_character as WC          # noqa: E402
import wellspring_settings_fit as WSF      # noqa: E402
import wfmetrics as WM                     # noqa: E402

SR = 48000
RENDER = os.environ.get('RV_RENDER', os.path.join(WT, 'build-r/rv_render'))
OUT = os.path.join(ROOT, 'renders/wellspring_fit3')
REF_A = os.path.join(ROOT, 'renders/references/wellspring/wellspring_A.wav')
CROP = os.path.join(OUT, 'scratch/clicks_crop.wav')
CLOSEST = dict(springs='2', attitude='CLEAN', mix=1.0, splash=0.0, wobble=0.5, drive=0.25, tension=0.875, tone=0.8)
CLICKS3 = WM.CLICKS[:3]
ARC_HZ = [2000, 2500, 3000, 3560, 4000, 4490]
ARR_HZ = [1000, 2000, 3175, 4000, 5000]


def crop():
    if not os.path.exists(CROP):
        os.makedirs(os.path.dirname(CROP), exist_ok=True)
        x, _ = sf.read(os.path.join(WT, 'test_audio/stimulus/01_clicks.wav'))
        sf.write(CROP, x[:24 * SR], SR, subtype='FLOAT')
    return CROP


def render(preset, out, tune=None, stim=None):
    env = dict(os.environ)
    if tune:
        env['RV_TANKV_TUNE'] = ','.join(f'{k}={v}' for k, v in tune.items())
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as fh:
        json.dump(preset, fh)
        pj = fh.name
    subprocess.run([RENDER, stim or crop(), out, '--preset', pj], check=True, capture_output=True, env=env)
    os.unlink(pj)
    y, _ = sf.read(out)
    return y


def fit_decay(voicing, tune=None, tmp=None, base=None, steps=8, target=None):
    """Bisect DECAY so the broadband T60 (tools/wellspring_settings_fit.py) matches the Wellspring's."""
    tmp = tmp or os.path.join(OUT, f'scratch/decayfit_v{voicing}.wav')
    if target is None:
        w, _ = sf.read(REF_A)
        target = WSF.features(w[:24 * SR])['t60_all']
    lo, hi = 0.3, 1.0
    for _ in range(steps):
        d = 0.5 * (lo + hi)
        f = WSF.features(render(dict(base or CLOSEST, decay=d, tank_voicing=voicing), tmp, tune))
        if f['t60_all'] < target:
            lo = d
        else:
            hi = d
    return round(0.5 * (lo + hi), 3), target


def bp(y, lo, hi):
    return sosfiltfilt(butter(4, [lo, hi], btype='band', fs=SR, output='sos'), y, axis=0)


def stereo(y):
    b = bp(y, 200, 5000)
    L = np.concatenate([b[o + int(.2 * SR):o + int(1.5 * SR), 0] for o in CLICKS3])
    R = np.concatenate([b[o + int(.2 * SR):o + int(1.5 * SR), 1] for o in CLICKS3])
    return float(np.corrcoef(L, R)[0, 1]), float(10 * np.log10(np.sum((L - R) ** 2) / np.sum((L + R) ** 2)))


def first_echo_ms(m):
    b = np.abs(bp(m, 1000, 4000))
    env = np.convolve(b, np.ones(48) / 48, 'same')
    out = []
    for o in CLICKS3:
        e = env[o + int(.003 * SR):o + int(.2 * SR)]
        k = int(np.argmax(e > e.max() * 0.5))
        out.append((k + int(.003 * SR)) / SR * 1000)
    return float(np.median(out))


def measure(path):
    y, _ = sf.read(path)
    if y.ndim == 1:
        y = np.stack([y, y], 1)
    y = y[:24 * SR]
    r = {'character': WC.measure(path)}
    f = WSF.features(y)
    r['t60'] = [f['t60'][b] for b in WSF.BANDS]
    r['t60_all'] = f['t60_all']
    r['echo_ms'] = f['echo_ms']
    m = y.mean(1)
    ridge = WM.ridge(m, clicks=CLICKS3)
    arr, _, _ = WM.arrival(m, clicks=CLICKS3)
    r['arc'] = [float(ridge[int(np.argmin([abs(q - h) for q in WM.RIDGE_F]))]) for h in ARC_HZ]
    r['arrival'] = [float(arr[int(np.argmin([abs(q - h) for q in WM.ARR_F]))]) for h in ARR_HZ]
    r['rise'] = WM.rise(m, clicks=CLICKS3)
    r['stereo_corr'], r['side_db'] = stereo(y)
    r['first_echo_ms'] = first_echo_ms(m)
    return r


def row(name, r):
    c = r['character']
    return (f"{name:14} NED {c['ned50']:.2f}/{c['ned150']:.2f}/{c['ned400']:.2f}  lowmid {c['lowmid']:5.1f}  "
            f"jump {c['flicker_db']:4.1f}  envcorr {c['lr_env_corr']:.2f}  corr {r['stereo_corr']:5.2f} side {r['side_db']:5.1f}  "
            f"T60 {'/'.join(f'{t:.2f}' for t in r['t60'])} (all {r['t60_all']:.2f})  echo {r['echo_ms']:.0f}  "
            f"first {r['first_echo_ms']:.1f}  arc {'/'.join(f'{a:.1f}' for a in r['arc'])}  "
            f"arr {'/'.join(f'{a:.1f}' for a in r['arrival'])}  rise {r['rise']:.1f}")
