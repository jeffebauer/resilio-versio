"""Measure Tank voicings next to the Wellspring at the closest settings (wf3.py has the definitions).

  python3 measure.py                      # every voicing at DECAY 0.68, plus the Wellspring
  python3 measure.py --fit-decay          # DECAY re-fitted per voicing to the Wellspring's broadband T60
  python3 measure.py --voicings 2,3 --tune diffCoeff=0.4,diff0=1.2
Writes measure.json (with --fit-decay) next to this file.
"""
import argparse, json, os
import wf3

ap = argparse.ArgumentParser()
ap.add_argument('--voicings', default='0,1,2,3,4')
ap.add_argument('--fit-decay', action='store_true')
ap.add_argument('--decay', type=float, default=0.68)
ap.add_argument('--tune', default='')
ap.add_argument('--springs', default='2')
a = ap.parse_args()
tune = dict(kv.split('=') for kv in a.tune.split(',')) if a.tune else None
base = dict(wf3.CLOSEST, springs=a.springs)

res = {'Wellspring': wf3.measure(wf3.REF_A)}
print(wf3.row('Wellspring', res['Wellspring']), flush=True)
for v in [int(x) for x in a.voicings.split(',')]:
    d = a.decay
    if a.fit_decay:
        d, _ = wf3.fit_decay(v, tune, base=base)
    out = os.path.join(wf3.OUT, f'scratch/measure_v{v}.wav')
    wf3.render(dict(base, decay=d, tank_voicing=v), out, tune)
    r = wf3.measure(out)
    r['decay'] = d
    res[f'v{v}'] = r
    print(wf3.row(f'v{v} D{d:.3f}', r), flush=True)
if a.fit_decay:
    json.dump(res, open(os.path.join(wf3.HERE, f'measure_s{a.springs}.json'), 'w'), indent=1, default=float)
