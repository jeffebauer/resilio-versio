import sys, os
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m
OUT = os.path.join(f2m.SP, 's3cmp')
cases = {}
decays = [float(x) for x in (sys.argv[1].split(',') if len(sys.argv) > 1 else ['0.5'])]
for d in decays:
    for tv in (0, 7):
        cases[f'tank{tv}_S2_d{d}'] = dict(f2m.NOON, tank_voicing=tv, decay=d)
        cases[f'tank{tv}_S3unc_d{d}'] = dict(f2m.NOON, tank_voicing=tv, decay=d, springs='3', springs3_voicing=0)
        cases[f'tank{tv}_S3cpl_d{d}'] = dict(f2m.NOON, tank_voicing=tv, decay=d, springs='3', springs3_voicing=8)
with ThreadPoolExecutor(8) as ex:
    js = {n: ex.submit(f2m.render, '01_clicks.wav', os.path.join(OUT, n + '.wav'), p) for n, p in cases.items()}
    [j.result() for j in js.values()]
    res = {n: ex.submit(f2m.character, os.path.join(OUT, n + '.wav')) for n in cases}
for n in cases:
    print(f2m.char_row(n, res[n].result()))
