"""Size voicing 4 ("gentler, further away", core/params/TankVoicing.h gentle*) from the measurements.

Targets (the Wellspring's click take, closest settings): the tail's low-mid balance (150-800 Hz vs
800 Hz-6 kHz, 30 ms-1 s) -6.0 dB (+-1.5), and the per-octave T60 shape 4.66 / 3.83 / 3.27 / 2.68 /
1.88 s (250 Hz-4 kHz) with the broadband T60 held at 3.13 s (DECAY re-fitted per cell, so a longer
low octave doesn't just mean a longer tail). Grid over the front low cut (high-pass, low-mid shelf),
the Loop damping and the high path's T60. fit_gentle_round1.json is a first run that also had a
"lows last longer" mechanism (a +0.3 dB low shelf inside each Loop, the Loop gain design allowing
1.5 x T60 below 250 Hz: 250 Hz T60 4.2 s vs the Wellspring's 4.7); it was dropped (it breaks
AntiRes layer 1, no band longer than designed, and stretches DECAY 1 to 12 s, ADR 0001).

  python3 fit_gentle.py
"""
import itertools, json, os
import numpy as np
import wf3

W = wf3.measure(wf3.REF_A)
rows = []
tmp = os.path.join(wf3.OUT, 'scratch/fit_gentle.wav')
grid = dict(gentleHpHz=[200, 260], gentleShelfDb=[-3, -6], gentleDampingScale=[1.0, 1.5], gentleHighT60Ratio=[0.7])
keys = list(grid)
for vals in itertools.product(*grid.values()):
    tune = dict(zip(keys, vals))
    d, _ = wf3.fit_decay(4, tune, steps=6)
    wf3.render(dict(wf3.CLOSEST, decay=d, tank_voicing=4), tmp, tune)
    r = wf3.measure(tmp)
    lm = r['character']['lowmid']
    t60e = float(np.mean([abs(a - b) / b for a, b in zip(r['t60'], W['t60'])]))
    err = abs(lm - W['character']['lowmid']) / 1.5 + 10 * t60e
    rows.append(dict(tune=tune, decay=d, err=err, lowmid=lm, t60=r['t60'], t60_err_pct=100 * t60e,
                     ned=[r['character']['ned150'], r['character']['ned400']]))
    print(f"{tune}: D {d:.3f} lowmid {lm:5.1f}  T60 {'/'.join(f'{t:.2f}' for t in r['t60'])} ({100 * t60e:.0f} %)  err {err:.2f}",
          flush=True)
rows.sort(key=lambda q: q['err'])
json.dump(dict(wellspring=dict(lowmid=W['character']['lowmid'], t60=W['t60']), ranking=rows),
          open(os.path.join(wf3.HERE, 'fit_gentle.json'), 'w'), indent=1, default=float)
print('best', rows[0])
