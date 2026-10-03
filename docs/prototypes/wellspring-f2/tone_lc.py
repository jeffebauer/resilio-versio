import sys, os
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m
OUT = os.path.join(f2m.SP, 'tone_lc')
TONES = (0.0, 0.25, 0.5, 0.75, 1.0)
cases = {}
for t in TONES:
    cases[('today', t)] = dict(f2m.NOON, tone=t, tank_voicing=0)
    for s in range(4):
        cases[(f'lc{s}', t)] = dict(f2m.NOON, tone=t, tank_voicing=7, f_lowcut_voicing=s)
st = {'sweep': '03_sweep.wav', 'hits': '02_hits.wav', 'skank': '04_skank.wav'}
with ThreadPoolExecutor(8) as ex:
    js = [ex.submit(f2m.render, f, os.path.join(OUT, f'{n}_{t}', k + '.wav'), p) for (n, t), p in cases.items() for k, f in st.items()]
    [j.result() for j in js]
res = {}
for (n, t) in cases:
    d = os.path.join(OUT, f'{n}_{t}')
    sw = f2m.sweep_resp(os.path.join(d, 'sweep.wav'), freqs=(63, 100, 126, 159))
    res[(n, t)] = dict(sw=sw, low=f2m.low_share(os.path.join(d, 'hits.wav')), lowsk=f2m.low_share(os.path.join(d, 'skank.wav')),
                       rh=f2m.rms(os.path.join(d, 'hits.wav')), rs=f2m.rms(os.path.join(d, 'skank.wav')))
for t in TONES:
    print(f'TONE {t}')
    for n in ['today', 'lc0', 'lc1', 'lc2', 'lc3']:
        r = res[(n, t)]; f = res[('lc0', t)]
        print(f"  {n:6s} sweep 63/100/126/159 {' '.join(f'{x:6.1f}' for x in r['sw'])} | bass share hits {r['low']:6.1f} skank {r['lowsk']:6.1f} | level re F hits {r['rh']-f['rh']:+5.1f} skank {r['rs']-f['rs']:+5.1f}")
