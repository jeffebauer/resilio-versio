"""Round 4's table: each Tank voicing next to the Wellspring at the corrected closest settings
(2 Springs, CLEAN, TENSION 0.5, TONE 0.7, SPLASH 0, WOBBLE 0.5, DRIVE 0.25, MIX 1), DECAY re-fitted per
voicing to the Wellspring's broadband T60 (wf3.fit_decay). wf4.py has the definitions.

  python3 measure.py [--voicings 0,3,4,5,6,7]       -> measure.json
"""
import argparse, json, os
from multiprocessing import Pool
import wf4, wf3


def one(v):
    d, _ = wf3.fit_decay(v, tmp=os.path.join(wf4.OUT, f'scratch/decayfit_v{v}.wav'), base=wf4.CLOSEST)
    out = os.path.join(wf4.OUT, f'scratch/measure_v{v}.wav')
    wf3.render(dict(wf4.CLOSEST, decay=d, tank_voicing=v), out)
    r = wf4.measure(out)
    r['decay'] = d
    os.unlink(out)
    os.unlink(os.path.join(wf4.OUT, f'scratch/decayfit_v{v}.wav'))
    return v, r


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--voicings', default='0,3,4,5,6,7')
    a = ap.parse_args()
    res = {'Wellspring': wf4.measure(wf4.REF_A)}
    print(wf4.row('Wellspring', res['Wellspring']), flush=True)
    with Pool(6) as p:
        for v, r in p.map(one, [int(x) for x in a.voicings.split(',')]):
            res[f'v{v}'] = r
            print(wf4.row(f'v{v} D{r["decay"]:.3f}', r), '| arc', [round(x, 1) for x in r['arc']], 'rise', round(r['rise'], 1),
                  'first', round(r['first_echo_ms'], 1), flush=True)
    json.dump(res, open(os.path.join(wf4.HERE, 'measure.json'), 'w'), indent=1, default=float)
