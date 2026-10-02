#!/usr/bin/env python3
"""Find the Resilio knob settings that sound closest to the owner's Wellspring.

Searches SPRINGS x TENSION x TONE (ATTITUDE CLEAN, SPLASH 0, WOBBLE noon, MIX 1,
DRIVE low), fitting DECAY per combination so the broadband tail length matches,
then scores each on the Wellspring click take (renders/references/wellspring/
wellspring_A.wav, aligned by tools/ingest_references.py):
  - tail length per octave band (250 Hz .. 4 kHz), % error
  - the tail's tonal balance (1/3-octave spectrum 30 ms .. 1 s after each
    click, level-normalised), dB RMS
  - echo spacing (the first strong repeat of the 1-4 kHz envelope), ms
Writes the ranking to <out>/fit.json and prints the top ten. The recordings
never leave the Mac; outputs go under renders/ (gitignored).

  python3 tools/wellspring_settings_fit.py --render build/rv_render --out renders/wellspring_fit2
Needs numpy, scipy, soundfile.
"""
import argparse, itertools, json, os, subprocess, tempfile
import numpy as np, soundfile as sf
from scipy.signal import butter, sosfilt

SR = 48000
CLICKS = [1.0, 9.0, 17.0]     # first three clicks of 01_clicks.wav (8 s apart)
CROP_S = 24.0
BANDS = [250, 500, 1000, 2000, 4000]


def mono(x):
    return x.mean(axis=1) if x.ndim > 1 else x


def band(x, fc):
    sos = butter(4, [fc / np.sqrt(2), fc * np.sqrt(2)], btype='band', fs=SR, output='sos')
    return sosfilt(sos, x)


def t60(seg, floor):
    """T60 from the Schroeder decay curve, fitted from -5 to -20 dB, with the
    integration cut where the 50 ms-smoothed envelope meets the noise floor
    (+10 dB; floor measured before the first click), so the recording's
    noise doesn't stretch the tail."""
    hop = int(0.01 * SR)
    n = len(seg) // hop
    e = np.array([np.mean(seg[i * hop:(i + 1) * hop] ** 2) for i in range(n)])
    sm = 10 * np.log10(np.convolve(e, np.ones(5) / 5, mode='same') + 1e-30)
    pk = int(np.argmax(sm))
    below = np.where(sm[pk:] < floor + 10)[0]
    end = pk + (below[0] if len(below) else n - pk)
    edc = np.cumsum(e[pk:end][::-1])[::-1]
    edc = 10 * np.log10(edc / edc[0] + 1e-30)
    a = np.where(edc <= -5)[0]
    b = np.where(edc <= -20)[0]
    if len(a) == 0 or len(b) == 0 or b[0] - a[0] < 5:
        return float('nan')
    sl = np.polyfit(np.arange(a[0], b[0]) * 0.01, edc[a[0]:b[0]], 1)[0]
    return float(-60.0 / sl) if sl < 0 else float('nan')


def features(x):
    x = mono(x)
    segs = [x[int(c * SR): int((c + 7.5) * SR)] for c in CLICKS]
    pre = x[int(0.05 * SR): int(0.95 * SR)]  # before the first click: the noise floor
    fl = lambda y: 10 * np.log10(np.mean(y ** 2) + 1e-30)
    f = {'t60': {}, 'spec': None, 'echo_ms': None}
    for fc in BANDS:
        f['t60'][fc] = float(np.nanmedian([t60(band(s, fc), fl(band(pre, fc))) for s in segs]))
    f['t60_all'] = float(np.nanmedian([t60(s, fl(pre)) for s in segs]))
    # tonal balance of the tail, 1/3 octaves 100 Hz .. 8 kHz
    edges = 100 * 2 ** (np.arange(0, 19) / 3)
    acc = np.zeros(len(edges) - 1)
    for s in segs:
        t = s[int(0.03 * SR): int(1.0 * SR)]
        P = np.abs(np.fft.rfft(t * np.hanning(len(t)))) ** 2
        fr = np.fft.rfftfreq(len(t), 1 / SR)
        acc += [P[(fr >= a) & (fr < b)].sum() + 1e-30 for a, b in zip(edges[:-1], edges[1:])]
    spec = 10 * np.log10(acc)
    f['spec'] = (spec - spec.mean()).tolist()
    # echo spacing: envelope of 1-4 kHz, autocorrelation peak 15..150 ms
    lags = []
    for s in segs:
        sos = butter(4, [1000, 4000], btype='band', fs=SR, output='sos')
        env = np.abs(sosfilt(sos, s[: int(0.6 * SR)]))
        env = np.convolve(env, np.ones(48) / 48, mode='same')[::48]  # 1 ms
        env = env - env.mean()
        ac = np.correlate(env, env, 'full')[len(env) - 1:]
        lo, hi = 15, 150
        seg_ac = ac[lo:hi]
        # the first local peak at least half the strongest: the echo period, not a multiple of it
        thr = 0.5 * seg_ac.max()
        pk = [i for i in range(1, len(seg_ac) - 1) if seg_ac[i] >= thr and seg_ac[i] >= seg_ac[i - 1] and seg_ac[i] >= seg_ac[i + 1]]
        lags.append(lo + (pk[0] if pk else int(np.argmax(seg_ac))))
    f['echo_ms'] = float(np.median(lags))
    return f


def score(f, ref):
    t = np.nanmean([abs(f['t60'][b] - ref['t60'][b]) / ref['t60'][b] for b in BANDS])
    s = float(np.sqrt(np.mean((np.array(f['spec']) - np.array(ref['spec'])) ** 2)))
    e = abs(f['echo_ms'] - ref['echo_ms'])
    return 10 * t + s + e / 5, {'t60_err_pct': 100 * t, 'spec_rms_db': s, 'echo_err_ms': e}


def render(binary, stim, preset, out):
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as fh:
        json.dump(preset, fh)
        pj = fh.name
    subprocess.run([binary, stim, out, '--preset', pj], check=True, capture_output=True)
    os.unlink(pj)
    y, _ = sf.read(out)
    return y


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--render', default='build/rv_render')
    ap.add_argument('--out', default='renders/wellspring_fit2')
    ap.add_argument('--ref', default='renders/references/wellspring/wellspring_A.wav')
    ap.add_argument('--tensions', default='0,0.25,0.5,0.75,1')
    ap.add_argument('--tones', default='0.2,0.3,0.4,0.5,0.6,0.7,0.8')
    ap.add_argument('--springs', default='1,2,3')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    x, _ = sf.read('test_audio/stimulus/01_clicks.wav')
    stim = os.path.join(a.out, 'clicks_crop.wav')
    sf.write(stim, x[: int(CROP_S * SR)], SR, subtype='FLOAT')
    w, _ = sf.read(a.ref)
    ref = features(w[: int(CROP_S * SR)])
    print('Wellspring: T60', {k: round(v, 2) for k, v in ref['t60'].items()}, 'echo', ref['echo_ms'], 'ms')
    tmp = os.path.join(a.out, 'tmp.wav')
    rows = []
    fl = lambda t: [float(v) for v in t.split(',')]
    for springs, tension, tone in itertools.product(a.springs.split(','), fl(a.tensions), fl(a.tones)):
        base = dict(springs=springs, attitude='CLEAN', mix=1.0, splash=0.0, wobble=0.5, drive=0.25,
                    tension=tension, tone=tone)
        lo, hi, f = 0.0, 1.0, None
        for _ in range(7):  # bisection on DECAY for the broadband T60
            d = 0.5 * (lo + hi)
            f = features(render(a.render, stim, dict(base, decay=d), tmp))
            if f['t60_all'] < ref['t60_all']:
                lo = d
            else:
                hi = d
        sc, parts = score(f, ref)
        rows.append(dict(settings=dict(base, decay=round(d, 3)), score=sc, **parts,
                         t60={str(k): v for k, v in f['t60'].items()}, echo_ms=f['echo_ms']))
        print(f"S{springs} TN{tension:.2f} TO{tone:.1f} D{d:.2f}: score {sc:5.2f} "
              f"(T60 {parts['t60_err_pct']:4.1f} %, tone {parts['spec_rms_db']:4.1f} dB, echo {parts['echo_err_ms']:4.0f} ms)",
              flush=True)
    rows.sort(key=lambda r: r['score'])
    json.dump(dict(reference=dict(t60=ref['t60'], echo_ms=ref['echo_ms'], spec=ref['spec']), ranking=rows),
              open(os.path.join(a.out, 'fit.json'), 'w'), indent=1)
    print('\nTop 10:')
    for r in rows[:10]:
        s = r['settings']
        print(f"  {r['score']:5.2f}  SPRINGS {s['springs']} TENSION {s['tension']} TONE {s['tone']} DECAY {s['decay']}"
              f"  (T60 {r['t60_err_pct']:.1f} %, tone {r['spec_rms_db']:.1f} dB, echo {r['echo_err_ms']:.0f} ms)")


if __name__ == '__main__':
    main()
