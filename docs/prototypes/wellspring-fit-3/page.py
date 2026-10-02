"""Build the round-3 listening page: renders/wellspring_fit3/page/ (gitignored).

Columns: A = the owner's Wellspring recording (takes A clicks, B hits, E skank, read in place and
copied only into the gitignored page folder, as renders/wellspring_fit2/page/A; ADR 0009), then
B-F = Tank voicings 0-4 at the closest settings (2 Springs, TENSION 0.875, TONE 0.8, CLEAN,
WOBBLE noon, DRIVE 0.25, MIX 1), DECAY re-fitted per voicing to the Wellspring's broadband T60
(measure_s2.json from `measure.py --fit-decay`). Rows: clicks / hits / skank; inside each panel
SPLASH 0 and 0.3 (the Wellspring has no SPLASH: the same take in both). Level-matched.

  python3 page.py
"""
import json, os, shutil, subprocess
import wf3

PAGE = os.path.join(wf3.OUT, 'page')
STIMS = {'01_clicks': 'A', '02_hits': 'B', '04_skank': 'E'}
NAMES = ['0_today', '1_sweep', '2_together', '3_diffusion', '4_gentler']
IGNORE = ['click_count', 'max_step_db_100ms', 'resonance_peak_db']
meas = json.load(open(os.path.join(wf3.HERE, 'measure_s2.json')))

if os.path.isdir(PAGE):
    shutil.rmtree(PAGE)
os.makedirs(os.path.join(wf3.OUT, 'sweeps'), exist_ok=True)

# A: the Wellspring takes, hand-written manifest + rv_render --analyze sidecars.
for stim, take in STIMS.items():
    d = os.path.join(PAGE, 'A', stim)
    os.makedirs(d)
    renders = []
    for sp in (0.0, 0.3):
        base = f'0_Wellspring__{stim}__splash{sp:.2f}'
        shutil.copyfile(os.path.join(wf3.ROOT, f'renders/references/wellspring/wellspring_{take}.wav'), os.path.join(d, base + '.wav'))
        subprocess.run([wf3.RENDER, '--analyze', os.path.join(d, base + '.wav'), '--sidecar-out', os.path.join(d, base + '.json')],
                       check=True, capture_output=True)
        renders.append(dict(wav=base + '.wav', sidecar=base + '.json', params=dict(splash=sp, springs='Wellspring', attitude='—')))
    json.dump(dict(name=f'0_Wellspring__{stim}', created='2026-10-02T00:00:00Z', input=f'test_audio/stimulus/{stim}.wav',
                   ignore_flags=IGNORE, renders=renders), open(os.path.join(d, 'manifest.json'), 'w'), indent=1)

# B-F: the voicings, one sweep per stimulus.
for v, name in enumerate(NAMES):
    letter = 'BCDEF'[v]
    decay = meas[f'v{v}']['decay']
    for stim in STIMS:
        sw = dict(name=f'{name}__{stim}', input=f'test_audio/stimulus/{stim}.wav',
                  base=dict(wf3.CLOSEST, decay=decay, tank_voicing=v), grid=dict(splash=[0.0, 0.3]),
                  tail_seconds=4, ignore_flags=IGNORE)
        sp = os.path.join(wf3.OUT, 'sweeps', f'{name}__{stim}.json')
        json.dump(sw, open(sp, 'w'), indent=1)
        subprocess.run([wf3.RENDER, '--sweep', sp, '--out-dir', os.path.join(PAGE, letter, stim)], check=True, capture_output=True,
                       cwd=wf3.WT)
    print(f'{letter} = voicing {v} ({name}), DECAY {decay}', flush=True)

d = {f'v{v}': meas[f'v{v}']['decay'] for v in range(5)}
open(os.path.join(PAGE, 'README.txt'), 'w').write(f"""Your Wellspring vs Resilio, round 3 (2 Oct 2026): each version adds one fix to the one before, level-matched.
A  = your Wellspring recording (takes A clicks, B hits, E skank; no SPLASH knob, same take in both slots)
B  = version 0, today (DECAY {d['v0']})
C  = version 1, Sweep: every echo the same smooth "pew" (proto/wellspring-fit's B, your pick), refitted on today's tank (DECAY {d['v1']})
D  = version 2, C + stereo together: both sides hear every spring, so the echoes stop jumping left-right; still wide, bass centred (DECAY {d['v2']})
E  = version 3, D + faster diffusion: the repeats blur into a wash within a few hundred ms; first echo and echo spacing unchanged (DECAY {d['v3']})
F  = version 4, E + gentler: less 150-800 Hz going into the springs, so the tail sits further back; 2-4 kHz last a little longer (DECAY {d['v4']})
All Resilio versions: closest knob settings (2 Springs, TENSION 0.875, TONE 0.8, CLEAN, WOBBLE noon, DRIVE 0.25, MIX 1), DECAY re-fitted per version to your Wellspring's tail length (3.1 s); SPLASH 0 and 0.3 inside each panel.
""")
subprocess.run(['python3', os.path.join(wf3.WT, 'tools/review/make_review.py'), PAGE, '--title', 'Wellspring fit round 3',
                '--columns', 'folder', '--variants', 'splash', '--rows', 'stimulus', '--level-match'], check=True, cwd=wf3.WT)
print('page:', os.path.join(PAGE, 'index.html'))
