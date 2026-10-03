"""Round 4 checks next to voicing 0: SPLASH still audible, and the Big Knob TONE still sensible.
  splash  02_hits, 2 Springs, DECAY / TENSION noon, TONE noon; CLEAN and KICKED, DRIVE 0 and 0.8: SPLASH s vs 0
          (2-8 kHz energy of the wet L+R, 20-400 ms after each event, summed: docs/prototypes/splash-stronger/
          measure.py's measure), at SPLASH 0.75 and 1, dB
  tone    click take crop, closest settings except TONE 0 / 0.25 / 0.5 / 0.75 / 1 (DECAY 0.664): tail (30 ms-1 s)
          brightness (2-6 kHz vs 300 Hz-1 kHz, dB) and wet level (dB re TONE 0.5)
  python3 splash_tone.py  -> splash_tone.json"""
import json, os, uuid
import numpy as np, soundfile as sf
from scipy.signal import butter, sosfiltfilt
from multiprocessing import Pool
import wf4, wf3

SR = 48000
VOICINGS = [0, 3, 5, 6, 7]
HITS = os.path.join(wf4.WT, 'test_audio/stimulus/02_hits.wav')


def onsets(x, rel=0.03, gap=0.3):
    thr = rel * np.max(np.abs(x))
    out, last = [], -10 ** 9
    for i in np.flatnonzero(np.abs(x) > thr):
        if i - last > gap * SR:
            out.append(int(i))
        last = i
    return out


x, _ = sf.read(HITS)
ON = onsets(x.mean(1) if x.ndim == 2 else x)
SOS = butter(4, [2000, 8000], btype='band', fs=SR, output='sos')


def splash_cell(args):
    v, att, drive, sp = args
    out = os.path.join(wf4.OUT, f'scratch/sp_{uuid.uuid4().hex[:8]}.wav')
    wf3.render(dict(springs='2', attitude=att, mix=1.0, splash=sp, wobble=0.5, drive=drive, tension=0.5, tone=0.5, decay=0.5,
                    tank_voicing=v), out, stim=HITS)
    y, _ = sf.read(out)
    os.unlink(out)
    b = sosfiltfilt(SOS, y.sum(1))
    return args, float(sum(np.sum(b[o + int(.02 * SR):o + int(.4 * SR)] ** 2) for o in ON))


def tone_cell(args):
    v, tone = args
    out = os.path.join(wf4.OUT, f'scratch/tn_{uuid.uuid4().hex[:8]}.wav')
    wf3.render(dict(wf4.CLOSEST, decay=0.664, tone=tone, tank_voicing=v), out)
    y, _ = sf.read(out)
    os.unlink(out)
    m = y[:24 * SR].mean(1)
    br = []
    for c in wf4.CLICK_S:
        s = m[int((c + .03) * SR):int((c + 1) * SR)]
        br.append(10 * np.log10(wf4.band_power(s, 2000, 6000) / wf4.band_power(s, 300, 1000)))
    return args, float(np.mean(br)), float(10 * np.log10(np.mean(y ** 2)))


if __name__ == '__main__':
    cells = [(v, a, d, s) for v in VOICINGS for a in ('CLEAN', 'KICKED') for d in (0.0, 0.8) for s in (0.0, 0.75, 1.0)]
    tones = [(v, t) for v in VOICINGS for t in (0.0, 0.25, 0.5, 0.75, 1.0)]
    with Pool(9) as p:
        sp = dict(p.map(splash_cell, cells))
        tn = p.map(tone_cell, tones)
    res = {'splash': {}, 'tone': {}}
    print('SPLASH 0.75 / 1 vs 0 (dB, 2-8 kHz, 20-400 ms after each hit), 02_hits:')
    for v in VOICINGS:
        row = []
        for a in ('CLEAN', 'KICKED'):
            for d in (0.0, 0.8):
                e0 = sp[(v, a, d, 0.0)]
                r = [10 * np.log10(sp[(v, a, d, s)] / e0) for s in (0.75, 1.0)]
                res['splash'][f'v{v} {a} d{d}'] = r
                row.append(f'{a} D{d}: {r[0]:4.1f} / {r[1]:4.1f}')
        print(f'  v{v}: ' + ' | '.join(row))
    print('TONE 0 / .25 / .5 / .75 / 1: tail brightness (2-6 kHz vs 300 Hz-1 kHz, dB) ; level re TONE .5 (dB):')
    for v in VOICINGS:
        rows = [r for r in tn if r[0][0] == v]
        lv = {r[0][1]: r[2] for r in rows}
        res['tone'][f'v{v}'] = [(r[0][1], r[1], r[2] - lv[0.5]) for r in rows]
        print(f'  v{v}: ' + ' / '.join(f'{r[1]:5.1f}' for r in rows) + ' ; ' + ' / '.join(f'{r[2] - lv[0.5]:+4.1f}' for r in rows))
    json.dump(res, open(os.path.join(wf4.HERE, 'splash_tone.json'), 'w'), indent=1)
