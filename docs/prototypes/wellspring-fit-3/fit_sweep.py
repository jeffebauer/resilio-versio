"""Re-fit voicing 1's Sweep on today's tank (Tank voicing 1, core/params/TankVoicing.h).

proto/wellspring-fit's B was fitted on the smooth-arc tank (Loop fC 5.3 kHz at noon); today's
Loop fC is lower (3.3 kHz noon, 4.2 kHz at the closest TENSION 0.875), so B's numbers don't
carry over as they are. Same targets as B's fit (the Wellspring click take's first arc: ms after
its 1 kHz at 2 / 2.5 / 3 / 3.56 / 4 / 4.49 kHz, and the energy-weighted arrival at 1 / 2 / 3.17 /
4 / 5 kHz), grid over the Sweep's sections (at TENSION noon; TENSION 0.875 plays 0.7 of them),
its top re the Loop's fC, its coefficient, and B's high path (crossover, pickup trim), at the
closest settings, DECAY 0.68.

  python3 docs/prototypes/wellspring-fit-3/fit_sweep.py [--quick]
"""
import itertools, json, os, sys
import numpy as np
import soundfile as sf
import wf3

w = wf3.measure(wf3.REF_A)
print(wf3.row('Wellspring', w))


def err(r):
    a = np.array(r['arc']); t = np.array(w['arc'])
    a = np.where(np.isnan(a), t + 6.0, a)  # an arc that stops counts as 6 ms off
    e_arc = np.sqrt(np.mean((a - t) ** 2)) / 1.5
    e_arr = np.sqrt(np.mean((np.array(r['arrival']) - np.array(w['arrival'])) ** 2)) / 2.5
    return float(np.sqrt((2 * e_arc ** 2 + e_arr ** 2) / 3)), e_arc, e_arr


# Round 1 (high path as today) found the Sweep alone can't draw the arc past ~3.5 kHz: the high
# path's first echo came a few ms before the Loop's, an undispersed copy on top. Round 2 adds B's
# high path (crossover x fC, pickup aligned on the Loop's first echo + hiAlignMs).
stages = [40, 50, 60]
ratios = [0.7, 0.85, 1.0]
coeffs = [0.35, 0.5]
xovers = [0.36, 0.5]
aligns = [0.0, 3.7]
tmp = os.path.join(wf3.OUT, 'scratch/fit_sweep.wav')
rows = []
for s, rt, a, hx, al in itertools.product(stages, ratios, coeffs, xovers, aligns):
    tune = dict(sweepStagesNoon=s, sweepFcRatio=rt, sweepCoeff=a, hiXoverRatio=hx, hiAlignMs=al)
    wf3.render(dict(wf3.CLOSEST, decay=0.68, tank_voicing=1), tmp, tune)
    r = wf3.measure(tmp)
    e = err(r)
    rows.append(dict(tune=tune, err=e[0], e_arc=e[1], e_arr=e[2], arc=r['arc'], arrival=r['arrival'], rise=r['rise']))
    print(f"S{s} r{rt} a{a} hx{hx} al{al}: err {e[0]:.2f} (arc {e[1]:.2f}, arr {e[2]:.2f})  arc {np.round(r['arc'], 1)}  arr {np.round(r['arrival'], 1)}", flush=True)
rows.sort(key=lambda q: q['err'])
json.dump(dict(wellspring=dict(arc=w['arc'], arrival=w['arrival']), ranking=rows),
          open(os.path.join(wf3.HERE, 'fit_sweep.json'), 'w'), indent=1, default=float)
print('best', rows[0])
