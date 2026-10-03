"""F round 2: the two picking pages (renders/f2_lowend/, renders/f2_springs3/, gitignored), in the owner's
format (renders/wellspring_fit4/compare/): one folder per version, each with one rv_render --sweep per sound
(ONE render, SPLASH at its default) + sidecar + manifest.json. The buttons show names only: `params` is
stripped, but the hidden voicing keys the Tank really played (read back from it by the Renderer) are kept
in each manifest entry and sidecar as "voicing", so every column's label can be checked.

  python3 page.py [lowend|springs3|all]      # then make_review.py (printed at the end)
  python3 page.py verify                     # re-render one sound per column, cmp bit for bit, check labels

Settings: the owner's TONE noon, else the defaults (2 Springs, CLEAN, DECAY / TENSION noon, SPLASH 0.3,
DRIVE 0.25, WOBBLE 0.45), MIX 1. Needs a Release build in build-r (RV_RENDER to override).
"""
import json, os, shutil, subprocess, sys, tempfile, filecmp

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, '../../..'))
ROOT = '/Users/jesse/Documents/Sites/resilio-versio'  # the main checkout: pages are served from its renders/
RENDER = os.environ.get('RV_RENDER', os.path.join(WT, 'build-r/rv_render'))
NOON = dict(springs='2', attitude='CLEAN', decay=0.5, tension=0.5, tone=0.5, splash=0.3, drive=0.25, wobble=0.45, mix=1.0)
STIMS = ['01_clicks', '02_hits', '04_skank', '10_pad_cminor']
IGNORE = ['click_count', 'max_step_db_100ms', 'resonance_peak_db']
HIDDEN = ['tank_voicing', 'f_lowcut_voicing', 'springs3_voicing']

PAGES = {
    'lowend': dict(
        out=os.path.join(ROOT, 'renders/f2_lowend'),
        title='Candidate F at TONE noon: a little more low end?',
        intro=("Candidate F with TONE at noon, then three small steps of more low end (each a little less low cut "
               "in front of the Springs, level kept). 2 Springs, CLEAN, other knobs at their defaults. Level-matched."),
        versions=[  # folder, preset additions, README line
            ('A_Resilio_Versio_today', dict(tank_voicing=0), "today's main, for reference"),
            ('B_candidate_F', dict(tank_voicing=7, f_lowcut_voicing=0), 'candidate F as you played it'),
            ('C_F_a_touch_more_body', dict(tank_voicing=7, f_lowcut_voicing=1), 'F, a touch more low end'),
            ('D_F_a_little_more', dict(tank_voicing=7, f_lowcut_voicing=2), 'F, a little more'),
            ('E_F_the_most', dict(tank_voicing=7, f_lowcut_voicing=3), 'F, the most (still well under today)'),
        ]),
    'springs3': dict(
        out=os.path.join(ROOT, 'renders/f2_springs3'),
        title='Candidate F: what should 3 Springs be?',
        intro=("Candidate F, TONE noon, other knobs at their defaults (CLEAN). A is 2 Springs, B today's 3, then four "
               "ideas for 3, all with the same repeat timing. Level-matched."),
        versions=[
            ('A_2_Springs', dict(tank_voicing=7, springs='2'), '2 Springs, for contrast'),
            ('B_3_Springs_today', dict(tank_voicing=7, springs='3', springs3_voicing=8), "3 Springs as in candidate F (coupled)"),
            ('C_3_stronger_coupling', dict(tank_voicing=7, springs='3', springs3_voicing=11), 'the Springs share more energy each trip'),
            ('D_3_wide', dict(tank_voicing=7, springs='3', springs3_voicing=12), 'wider: more of the Springs\' differences in the sides'),
            ('E_3_wire_gauges', dict(tank_voicing=7, springs='3', springs3_voicing=13), 'three wire thicknesses: a cluster of boings'),
            ('F_3_swell', dict(tank_voicing=7, springs='3', springs3_voicing=14), 'the tail swells up after a hit, then fades'),
        ]),
}


def sweep(name, stim, preset, out_dir):
    base = dict(NOON, **preset)
    sw = dict(name=f'{name}__{stim}', input=f'test_audio/stimulus/{stim}.wav', base=base, grid=dict(splash=[NOON['splash']]),
              tail_seconds=4, ignore_flags=IGNORE)
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
        json.dump(sw, f)
    subprocess.run([RENDER, '--sweep', f.name, '--out-dir', out_dir], check=True, capture_output=True, cwd=WT)
    os.unlink(f.name)


def strip(d):
    """Names, not params, on the buttons; the voicing keys the Tank played kept as "voicing"."""
    for f in os.listdir(d):
        p = os.path.join(d, f)
        if not f.endswith('.json'):
            continue
        j = json.load(open(p))
        if f == 'manifest.json':
            for r in j['renders']:
                r['voicing'] = {k: r['params'][k] for k in HIDDEN if k in r['params']}
                r['params'] = {}
        else:
            j['voicing'] = {k: j['params'][k] for k in HIDDEN if k in j.get('params', {})}
            j.pop('params', None)
        json.dump(j, open(p, 'w'), indent=1)


def build(key):
    pg = PAGES[key]
    if os.path.isdir(pg['out']):
        shutil.rmtree(pg['out'])
    for folder, preset, _ in pg['versions']:
        for stim in STIMS:
            d = os.path.join(pg['out'], folder, stim)
            sweep(folder, stim, preset, d)
            strip(d)
        print(f'{folder}: {preset}', flush=True)
    lines = [pg['intro'], ''] + [f'{f}  {t}' for f, _, t in pg['versions']]
    open(os.path.join(pg['out'], 'README.txt'), 'w').write('\n'.join(lines) + '\n')
    print(f"  python3 tools/review/make_review.py {pg['out']} --rows stimulus --columns none --variants folder1 "
          f"--level-match --title \"{pg['title']}\"")


def verify(key):
    """Each column: the manifest's voicing = the label's; a fresh render of one sound = the page's, bit for bit;
    no two columns alike."""
    pg = PAGES[key]
    ok = True
    wavs = {}
    for folder, preset, _ in pg['versions']:
        stim = '02_hits'
        d = os.path.join(pg['out'], folder, stim)
        man = json.load(open(os.path.join(d, 'manifest.json')))
        r = man['renders'][0]
        want = {k: preset[k] for k in HIDDEN if k in preset}
        got = {k: r['voicing'].get(k) for k in want}
        side = json.load(open(os.path.join(d, r['sidecar'])))
        same_side = all(side['voicing'].get(k) == v for k, v in want.items())
        with tempfile.TemporaryDirectory() as tmp:
            sweep(folder, stim, preset, tmp)
            fresh = [f for f in os.listdir(tmp) if f.endswith('.wav')][0]
            bit = filecmp.cmp(os.path.join(tmp, fresh), os.path.join(d, r['wav']), shallow=False)
        wavs[folder] = os.path.join(d, r['wav'])
        good = got == want and same_side and bit
        ok &= good
        print(f"{'OK ' if good else 'BAD'} {folder}: label {want}, manifest {got}, sidecar {'same' if same_side else 'DIFFERENT'}, "
              f"fresh render {'bit-identical' if bit else 'DIFFERS'}")
    names = list(wavs)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            if filecmp.cmp(wavs[names[i]], wavs[names[j]], shallow=False):
                ok = False
                print(f'BAD {names[i]} and {names[j]} are the same render')
    print(f"{key}: {'every label verified' if ok else 'PROBLEMS'}")
    return ok


if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if what == 'verify':
        sys.exit(0 if all([verify(k) for k in PAGES]) else 1)
    for k in PAGES if what == 'all' else [what]:
        build(k)
