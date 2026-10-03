"""Size voicing 3's Loop diffusers (core/params/TankVoicing.h diffMs, diffCoeff) from the measurements.

Target: the Wellspring's echo density on the click take (NED 0.77 at 100-200 ms, 0.97 at 300-500 ms;
the brief: >= 0.75 and ~0.9 by 300 ms), echo spacing and first echo where they were, the pool within
30,000 floats. Grid over the coefficient and a scale on the three delays, at the closest settings.

  python3 fit_diffusion.py
"""
import json, os
import wf3

BASE_MS = [1.7, 2.9, 4.3]
rows = []
tmp = os.path.join(wf3.OUT, 'scratch/fit_diffusion.wav')
w = wf3.measure(wf3.REF_A)['character']
for c in [0.1, 0.15, 0.2, 0.3]:
    for sc in [0.3, 0.5, 0.8]:
        tune = dict(diffCoeff=c, diff0=BASE_MS[0] * sc, diff1=BASE_MS[1] * sc, diff2=BASE_MS[2] * sc)
        wf3.render(dict(wf3.CLOSEST, decay=0.68, tank_voicing=3), tmp, tune)
        r = wf3.measure(tmp)
        ch = r['character']
        err = abs(ch['ned150'] - w['ned150']) + abs(ch['ned400'] - w['ned400'])
        rows.append(dict(tune=tune, err=err, ned=[ch['ned50'], ch['ned150'], ch['ned400']], echo=r['echo_ms'],
                         first=r['first_echo_ms'], t60=r['t60']))
        print(f"c {c} scale {sc}: NED {ch['ned50']:.2f}/{ch['ned150']:.2f}/{ch['ned400']:.2f} (err {err:.2f})  echo {r['echo_ms']:.0f}  "
              f"first {r['first_echo_ms']:.1f}  T60 {'/'.join(f'{t:.2f}' for t in r['t60'])}", flush=True)
rows.sort(key=lambda q: q['err'])
json.dump(dict(wellspring=w, ranking=rows), open(os.path.join(wf3.HERE, 'fit_diffusion.json'), 'w'), indent=1, default=float)
print('best', rows[0])
