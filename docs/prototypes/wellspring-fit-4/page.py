"""Build the round-4 compare page: renders/wellspring_fit4/compare/ (gitignored), the owner's format
(renders/wellspring_fit3/compare/): one folder per version, each with 01_clicks/ 02_hits/ 04_skank/ holding
ONE render (SPLASH 0) + sidecar + manifest.json, `params` stripped so the buttons show names only.
A = the owner's Wellspring takes (A clicks, B hits, E skank), copied only into the gitignored page
folder (ADR 0009), with rv_render --analyze sidecars. B-F = Tank voicings 0, 3, 5, 6, 7 at the
corrected closest settings, DECAY re-fitted per voicing (measure.json from measure.py).

  python3 page.py      # then make_review.py from the main checkout (printed at the end)
"""
import json, os, shutil, subprocess
import wf4, wf3

PAGE = os.path.join(wf4.OUT, 'compare')
STIMS = {'01_clicks': 'A', '02_hits': 'B', '04_skank': 'E'}
IGNORE = ['click_count', 'max_step_db_100ms', 'resonance_peak_db']
VERSIONS = [  # folder, voicing, render name, README line
    ('B_Resilio_today', 0, '0_today', 'Resilio today, closest knob settings'),
    ('C_round3_sweep_together_diffusion', 3, '3_round3', 'round 3\'s version 3: smooth "pew", no left-right flicker, repeats blur'),
    ('D_plus_transducers', 5, '5_transducers', '+ transducers: highs gentle from the first moment, repeats darken slowly'),
    ('E_plus_wide', 6, '6_wide', '+ wide again: as wide as your Wellspring, still no flicker, bass centred'),
    ('F_plus_gentler', 7, '7_gentler', '+ a little less low end, level kept'),
]
meas = json.load(open(os.path.join(wf4.HERE, 'measure.json')))


def strip(d):
    for f in os.listdir(d):
        p = os.path.join(d, f)
        if not f.endswith('.json'):
            continue
        j = json.load(open(p))
        if f == 'manifest.json':
            for r in j['renders']:
                r['params'] = {}
        else:
            j.pop('params', None)
        json.dump(j, open(p, 'w'), indent=1)


if os.path.isdir(PAGE):
    shutil.rmtree(PAGE)
os.makedirs(os.path.join(wf4.OUT, 'sweeps'), exist_ok=True)

for stim, take in STIMS.items():
    d = os.path.join(PAGE, 'A_your_Wellspring', stim)
    os.makedirs(d)
    base = f'0_Wellspring__{stim}__splash0.00'
    shutil.copyfile(os.path.join(wf4.ROOT, f'renders/references/wellspring/wellspring_{take}.wav'), os.path.join(d, base + '.wav'))
    subprocess.run([wf3.RENDER, '--analyze', os.path.join(d, base + '.wav'), '--sidecar-out', os.path.join(d, base + '.json')],
                   check=True, capture_output=True)
    json.dump(dict(name=f'0_Wellspring__{stim}', created='2026-10-02T00:00:00Z', input=f'test_audio/stimulus/{stim}.wav',
                   ignore_flags=IGNORE, renders=[dict(wav=base + '.wav', sidecar=base + '.json', params={})]),
              open(os.path.join(d, 'manifest.json'), 'w'), indent=1)
    strip(d)

for folder, v, name, _ in VERSIONS:
    decay = meas[f'v{v}']['decay']
    for stim in STIMS:
        sw = dict(name=f'{name}__{stim}', input=f'test_audio/stimulus/{stim}.wav',
                  base=dict(wf4.CLOSEST, decay=decay, tank_voicing=v), grid=dict(splash=[0.0]),
                  tail_seconds=4, ignore_flags=IGNORE)
        sp = os.path.join(wf4.OUT, 'sweeps', f'{name}__{stim}.json')
        json.dump(sw, open(sp, 'w'), indent=1)
        d = os.path.join(PAGE, folder, stim)
        subprocess.run([wf3.RENDER, '--sweep', sp, '--out-dir', d], check=True, capture_output=True, cwd=wf4.WT)
        strip(d)
    print(f'{folder} = voicing {v}, DECAY {decay}', flush=True)

lines = ["Pick the version closest to your Wellspring for each sound. Each version adds one change to the one before it. "
         "Resilio settings: 2 Springs, CLEAN, TENSION noon, TONE 2 o'clock, DECAY matched to your Wellspring, SPLASH 0. Level-matched.",
         '', 'A_your_Wellspring  your recording (no SPLASH knob)'] + [f'{f}  {t}' for f, _, _, t in VERSIONS]
open(os.path.join(PAGE, 'README.txt'), 'w').write('\n'.join(lines) + '\n')
print('now, from the main checkout:\n  python3 tools/review/make_review.py renders/wellspring_fit4/compare --rows stimulus --columns none '
      '--variants folder1 --level-match --title "Your Wellspring vs Resilio, round 4"')
