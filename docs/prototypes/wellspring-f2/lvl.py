import sys, os
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m
OUT = os.path.join(f2m.SP, 'lvl_' + sys.argv[2])
vs = [int(x) for x in sys.argv[1].split(',')]
st = ['02_hits.wav', '04_skank.wav', '10_pad_cminor.wav', '01_clicks.wav']
cases = {'S2': dict(f2m.NOON)}
for v in vs: cases[f'v{v}'] = dict(f2m.NOON, springs='3', springs3_voicing=v)
with ThreadPoolExecutor(8) as ex:
    js = [ex.submit(f2m.render, s, os.path.join(OUT, n, s), p) for n, p in cases.items() for s in st]
    [j.result() for j in js]
ref = [f2m.rms(os.path.join(OUT, 'S2', s)) for s in st]
for n in cases:
    print(n, ' '.join(f'{f2m.rms(os.path.join(OUT, n, s)) - r:+5.1f}' for s, r in zip(st, ref)), '(hits skank pad clicks, dB re S2)')
