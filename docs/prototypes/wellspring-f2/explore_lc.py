import sys, os
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m

# name -> (tank_voicing, tune string or None)
CANDS = {
    'F': (7, None),
    'today0': (0, None),
}
for spec in sys.argv[1:]:
    name, tune = spec.split(':', 1)
    CANDS[name] = (7, tune)

STIMS = {'sweep': '03_sweep.wav', 'clicks': '01_clicks.wav', 'hits': '02_hits.wav', 'skank': '04_skank.wav', 'pad': '10_pad_cminor.wav'}
OUT = os.path.join(f2m.SP, 'xlc')
jobs = []
with ThreadPoolExecutor(8) as ex:
    for n, (v, tune) in CANDS.items():
        for k, s in STIMS.items():
            p = dict(f2m.NOON, tank_voicing=v)
            jobs.append(ex.submit(f2m.render, s, os.path.join(OUT, n, k + '.wav'), p, tune))
    [j.result() for j in jobs]

ref = {}
print(f"{'':10s} {'63':>6s} {'100':>6s} {'126':>6s} {'159':>6s} {'200':>6s} {'300':>6s} | lowmid | rms hits skank pad clicks (re F) | <200Hz share hits skank")
for n in CANDS:
    d = os.path.join(OUT, n)
    sw = f2m.sweep_resp(os.path.join(d, 'sweep.wav'))
    lm = f2m.clicks(os.path.join(d, 'clicks.wav'))
    r = [f2m.rms(os.path.join(d, k + '.wav')) for k in ('hits', 'skank', 'pad', 'clicks')]
    ls = [f2m.low_share(os.path.join(d, k + '.wav')) for k in ('hits', 'skank')]
    if n == 'F':
        ref = r
    rr = [a - b for a, b in zip(r, ref)] if ref else r
    print(f"{n:10s} " + ' '.join(f'{x:6.1f}' for x in sw) + f" | {lm:6.1f} | " + ' '.join(f'{x:+5.1f}' for x in rr) + ' | ' + ' '.join(f'{x:6.1f}' for x in ls))
