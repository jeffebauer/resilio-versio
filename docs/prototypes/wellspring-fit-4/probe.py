"""Quick probe: render voicings / tunings at the closest settings and print the round-4 row + rise/arc.
  python3 probe.py 'v=5' 'v=5;tdHighCeilHz=2000' ...   (';'-separated key=value; v = tank_voicing, decay=, any Tuning field)"""
import sys, os, json, uuid
import wf4, wf3
from multiprocessing import Pool
best = json.load(open(os.path.join(wf4.HERE, 'fit_transducers.json')))['ranking'][0]['tune'] if os.path.exists(os.path.join(wf4.HERE, 'fit_transducers.json')) else {}

def run(spec):
    kv = dict(x.split('=') for x in spec.split(';') if x)
    v = int(kv.pop('v', 5)); d = float(kv.pop('decay', 0.664)); base = kv.pop('base', '')
    preset = dict(wf4.CLOSEST, decay=d, tank_voicing=v)
    for k in list(kv):
        if k in preset: preset[k] = type(preset[k])(kv.pop(k)) if not isinstance(preset[k], str) else kv.pop(k)
    tune = dict(best, **kv) if base == 'best' else kv
    out = os.path.join(wf4.OUT, f'scratch/probe_{uuid.uuid4().hex[:8]}.wav')
    wf3.render(preset, out, tune or None)
    r = wf4.measure(out); os.unlink(out)
    return spec, r

if __name__ == '__main__':
    with Pool(min(9, len(sys.argv) - 1)) as p:
        for spec, r in p.map(run, sys.argv[1:]):
            print(wf4.row(spec[:16], r), '\n   ', spec, '| rise', round(r['rise'], 1), 'arc', [round(a, 1) for a in r['arc']], 'first', round(r['first_echo_ms'], 1), flush=True)
