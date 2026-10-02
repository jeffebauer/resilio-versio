"""Size voicing 5's transducers and the tank's high-frequency loss against the Wellspring (wf4.py).

Free: the input coil's low-pass (tdInHz, tdInQ), the output pickup's (tdOutHz, tdOutQ), the Loop's
damping scale (tdDampingScale), the high path's T60 share (tdHighT60Ratio), ceiling (tdHighCeilHz) and
level (tdHighLevel).
Score (click take, closest settings, DECAY 0.664; refitted afterwards by measure.py):
  |onset HF - W| / 2  +  tail balance (the fit tool's spectral rms, dB)  +  |darkening - W| / 2
  + 5 x |T60 4 kHz - W| / W  +  the sweep's early response 1-8 kHz vs the Wellspring's take D (rms dB,
  tools/sweep_ir.py; added 2 Oct 2026 when the sweep IR became available)
Random search (seeded), then a coordinate refinement around the best. Writes fit_transducers.json.

  python3 fit_transducers.py [--n 240]
"""
import argparse, json, os, uuid
import numpy as np, soundfile as sf
from multiprocessing import Pool
import wf4, wf3

SPACE = dict(tdInHz=(2000, 4500), tdInQ=(0.5, 1.6), tdOutHz=(2000, 5500), tdOutQ=(0.5, 1.6),
             tdDampingScale=(1.0, 4.0), tdHighT60Ratio=(0.45, 1.6), tdHighCeilHz=(2500, 9000),
             tdHighLevel=(0.5, 2.0))
VOICING = 5


def evaluate(tune):
    out = os.path.join(wf4.OUT, f'scratch/fit_{uuid.uuid4().hex[:8]}.wav')
    wf3.render(dict(wf4.CLOSEST, decay=0.664, tank_voicing=VOICING), out, {k: f'{v:.4g}' for k, v in tune.items()})
    y, _ = sf.read(out)
    os.unlink(out)
    y = y[:24 * wf4.SR]
    f = wf4.WSF.features(y)
    R = wf4.ref()
    spec = float(np.sqrt(np.mean((np.array(f['spec']) - np.array(R['spec'])) ** 2)))
    dark = wf4.darkening(y.mean(1))
    o = f['onset_hf_db']
    t4 = f['t60'][4000]
    # the sweep (tools/sweep_ir.py, take D): the early response 1-8 kHz vs the Wellspring's
    wf3.render(dict(wf4.CLOSEST, decay=0.664, tank_voicing=VOICING), out, {k: f'{v:.4g}' for k, v in tune.items()}, stim=wf4.SWEEP)
    sw = wf4.sweep_err(wf4.sweep(out))
    os.unlink(out)
    score = (abs(o - R['onset_hf_db']) / 2 + spec + abs(dark - WDARK) / 2 + 5 * abs(t4 - R['t60'][4000]) / R['t60'][4000]
             + sw)
    return dict(tune=tune, score=score, onset=o, spec=spec, dark=dark, t60_4k=t4, t60_all=f['t60_all'], sweep_err=sw)


w, _ = sf.read(wf4.REF_A)
WDARK = wf4.darkening(w[:24 * wf4.SR].mean(1))

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--n', type=int, default=240)
    a = ap.parse_args()
    rng = np.random.default_rng(4)
    keys = list(SPACE)
    cands = [{k: float(rng.uniform(*SPACE[k])) for k in keys} for _ in range(a.n)]
    with Pool(9) as p:
        res = p.map(evaluate, cands)
        res.sort(key=lambda r: r['score'])
        best = res[0]
        for rnd in range(3):  # coordinate refinement, shrinking steps
            step = 0.25 / (2 ** rnd)
            trial = []
            for k in keys:
                lo, hi = SPACE[k]
                for sgn in (-1, 1):
                    t = dict(best['tune'])
                    t[k] = float(np.clip(t[k] + sgn * step * (hi - lo), lo, hi))
                    trial.append(t)
            r2 = p.map(evaluate, trial)
            res += r2
            res.sort(key=lambda r: r['score'])
            best = res[0]
            print(f'round {rnd}: {best["score"]:.2f}', {k: round(v, 3) for k, v in best['tune'].items()}, flush=True)
    print('Wellspring: onset', round(wf4.ref()['onset_hf_db'], 1), 'dark', round(WDARK, 1), 't60 4k', round(wf4.ref()['t60'][4000], 2))
    for r in res[:8]:
        print(f"{r['score']:.2f} onset {r['onset']:.1f} spec {r['spec']:.2f} dark {r['dark']:.1f} t60 4k {r['t60_4k']:.2f} sweep {r['sweep_err']:.1f}",
              {k: round(v, 3) for k, v in r['tune'].items()})
    json.dump(dict(wellspring=dict(onset=wf4.ref()['onset_hf_db'], dark=WDARK, t60_4k=wf4.ref()['t60'][4000]), ranking=res[:40]),
              open(os.path.join(wf4.HERE, 'fit_transducers.json'), 'w'), indent=1)
