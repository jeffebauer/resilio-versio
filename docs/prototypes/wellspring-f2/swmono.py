import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m, numpy as np, soundfile as sf
tag = sys.argv[1]
OUT = os.path.join(f2m.SP, 'swm_' + tag)
for d in (0.1, 0.5, 0.6, 0.75, 1.0):
    for t in (0.0, 1.0):
        r = {}
        for n, p in (('S2', {}), ('v14', dict(springs='3', springs3_voicing=14))):
            q = dict(f2m.NOON, decay=d, tension=t, **p)
            path = f2m.render('02_hits.wav', os.path.join(OUT, f'{n}_{d}_{t}.wav'), q)
            y, _ = sf.read(path)
            r[n] = (10*np.log10(np.mean(y**2)), 10*np.log10(np.mean(y.mean(1)**2)))
        print(f'DECAY {d} TENSION {t}: stereo {r["v14"][0]-r["S2"][0]:+5.1f} mono {r["v14"][1]-r["S2"][1]:+5.1f}')
