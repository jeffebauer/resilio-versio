"""Size voicing 7's low cut (lc*: high-pass + low shelf in front of the Springs, TankVoicing.h) on top of 6.
Score: the sweep's early response 60 Hz-1 kHz vs the Wellspring's take D (rms dB, tools/sweep_ir.py)
+ the click tail's balance (fit tool spectral rms) + |low-mid balance - W| / 1.5. Writes fit_lowcut.json.
  python3 fit_lowcut.py"""
import itertools, json, os, uuid
import numpy as np, soundfile as sf
from multiprocessing import Pool
import wf4, wf3


def evaluate(tune):
    t = {k: f'{v:.4g}' for k, v in tune.items()}
    out = os.path.join(wf4.OUT, f'scratch/lc_{uuid.uuid4().hex[:8]}.wav')
    wf3.render(dict(wf4.CLOSEST, decay=0.664, tank_voicing=7), out, t)
    r = wf4.measure(out)
    wf3.render(dict(wf4.CLOSEST, decay=0.664, tank_voicing=7), out, t, stim=wf4.SWEEP)
    sw = wf4.sweep_err(wf4.sweep(out), 60, 1000)
    os.unlink(out)
    lm = r['character']['lowmid']
    W = -6.0
    return dict(tune=tune, score=sw + r['spec_rms'] + abs(lm - W) / 1.5, sweep_low=sw, spec=r['spec_rms'], lowmid=lm)


if __name__ == '__main__':
    grid = [dict(lcHpHz=h, lcHpQ=q, lcShelfHz=sh, lcShelfDb=db) for h, q, sh, db in
            itertools.product([140, 180, 220, 260], [0.6, 0.9, 1.2], [160, 300, 600], [0, -3, -6])]
    with Pool(9) as p:
        res = sorted(p.map(evaluate, grid), key=lambda r: r['score'])
    for r in res[:8]:
        print(f"{r['score']:.2f} sweep60-1k {r['sweep_low']:.1f} spec {r['spec']:.2f} lowmid {r['lowmid']:.1f}", r['tune'])
    json.dump(res[:30], open(os.path.join(wf4.HERE, 'fit_lowcut.json'), 'w'), indent=1)
