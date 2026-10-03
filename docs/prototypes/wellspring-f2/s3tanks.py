import sys, os
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m
OUT = os.path.join(f2m.SP, 's3tanks')
cases = {}
for tv in (0, 1, 2, 3, 5, 6, 7):
    cases[f't{tv}_S2'] = dict(f2m.NOON, tank_voicing=tv)
    cases[f't{tv}_S3'] = dict(f2m.NOON, tank_voicing=tv, springs='3', springs3_voicing=8)
with ThreadPoolExecutor(8) as ex:
    js = {n: ex.submit(f2m.render, '01_clicks.wav', os.path.join(OUT, n + '.wav'), p) for n, p in cases.items()}
    [j.result() for j in js.values()]
    res = {n: ex.submit(f2m.character, os.path.join(OUT, n + '.wav')) for n in cases}
for n in cases:
    sw, pk, late = f2m.envelope(os.path.join(OUT, n + '.wav'))
    print(f2m.char_row(n, res[n].result()) + f"  swell {sw:+5.1f} late {late:+5.1f}")
