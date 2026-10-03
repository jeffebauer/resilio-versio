import sys, os
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import f2m
OUT = os.path.join(f2m.SP, 's3var_' + (sys.argv[2] if len(sys.argv) > 2 else 'a'))
vs = [int(x) for x in sys.argv[1].split(',')]
d = float(os.environ.get('DECAY', '0.5'))
cases = {'F_S2': dict(f2m.NOON, decay=d)}
for v in vs:
    cases[f'F_S3_v{v}'] = dict(f2m.NOON, decay=d, springs='3', springs3_voicing=v)
with ThreadPoolExecutor(8) as ex:
    js = {n: ex.submit(f2m.render, '01_clicks.wav', os.path.join(OUT, n + '.wav'), p) for n, p in cases.items()}
    [j.result() for j in js.values()]
    res = {n: ex.submit(f2m.character, os.path.join(OUT, n + '.wav')) for n in cases}
for n in cases:
    sw, pk, late = f2m.envelope(os.path.join(OUT, n + '.wav'))
    print(f2m.char_row(n, res[n].result()) + f"  swell {sw:+5.1f} late {late:+5.1f} peak {pk:4.0f} ms")
