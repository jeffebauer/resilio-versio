#!/usr/bin/env python3
"""Build the round-5 listening page in the main checkout's renders/fit_round5/
(gitignored): one folder per version (A today = voicing 7, B = 8, C = 9, D = 10),
one sweep per row inside it (rv_render --sweep, `params` stripped so the buttons
show names only), the gesture rows (TONE swept on a ringing tail, the skank at
MIX 0.4), and W, the owner's Wellspring takes, as references: symlinks in
renders/fit_round5/wellspring/ to test_audio/reference/ (never copied, never
committed: ADR 0009), with rv_render --analyze sidecars.

  python3 docs/prototypes/wellspring-fit-5/page5.py [RV_RENDER] [PAGE_DIR]
  then the make_review.py line it prints.
Stdlib only. Run from this worktree's root (stimulus in test_audio/stimulus).
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MAIN = ROOT  # run from the main checkout (renders, recordings)
RENDER = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build-r" / "rv_render"
PAGE = Path(sys.argv[2]) if len(sys.argv) > 2 else MAIN / "renders" / "fit_round5"
STIM = ROOT / "test_audio" / "stimulus"
REF = MAIN / "test_audio" / "reference"
IGNORE = ["click_count", "max_step_db_100ms", "resonance_peak_db"]

# The session-2 settings: 2 Springs, CLEAN, MIX 1, DECAY 0.70, TONE / TENSION noon, the rest at the panel defaults.
BASE = {"springs": "2", "attitude": "CLEAN", "mix": 1.0, "decay": 0.70, "tone": 0.5, "tension": 0.5,
        "splash": 0.3, "wobble": 0.45, "drive": 0.25}
VERSIONS = [  # folder, tank voicing, README line
    ("A_today", 7, "Resilio today (main: F with the F round 2 low cut)"),
    ("B_front_resonance_stereo", 8, "softer front, ring sits lower (500 Hz), presence up to ~1.5 kHz, centred bass, no left/right lean"),
    ("C_plus_held_flat", 9, "B + held notes and pads settle flat (WOBBLE moves the pitch, not the level)"),
    ("D_halfway", 10, "half way from today to C"),
]
# row folder, stimulus, extra settings, tail seconds, automation, the Wellspring take it compares with
ROWS = [
    ("01_clicks", "01_clicks.wav", {}, 2, None, "wellspring_A_clicks.wav"),
    ("02_hits", "02_hits.wav", {}, 3, None, "wellspring_B_hits.wav"),
    ("04_skank", "04_skank.wav", {}, 3, None, "wellspring_E_skank.wav"),
    ("10_pad", "10_pad_cminor.wav", {}, 0, None, "wellspring_M_pad.wav"),
    ("08_held_tone", "08_held_tones.wav", {}, 0, None, "wellspring_L_held_tones.wav"),
    ("15_tone_bursts", "15_tone_bursts.wav", {}, 0, None, "wellspring_J_tone_bursts.wav"),
    ("g1_tone_sweep_on_tail", "proto_one_snare.wav", {}, 2, "tone_sweep", None),
    ("g2_skank_mix040", "04_skank.wav", {"mix": 0.4}, 3, None, "wellspring_E2_skank_mix.wav"),
]
# TONE on a ringing tail (one snare at 1.0 s): noon, up to fully right (the Big Knob thins it), back, down to
# fully left (dark), back to noon.
TONE_SWEEP = {"breakpoints": [
    {"t": 0.0, "key": "tone", "value": 0.5}, {"t": 1.3, "key": "tone", "value": 0.5},
    {"t": 2.5, "key": "tone", "value": 1.0}, {"t": 3.0, "key": "tone", "value": 1.0},
    {"t": 3.8, "key": "tone", "value": 0.5}, {"t": 4.8, "key": "tone", "value": 0.0},
    {"t": 5.4, "key": "tone", "value": 0.0}, {"t": 6.4, "key": "tone", "value": 0.5}]}


def strip(d):
    for f in os.listdir(d):
        p = d / f
        if not f.endswith(".json"):
            continue
        j = json.load(open(p))
        if f == "manifest.json":
            for r in j["renders"]:
                r["params"] = {}
        else:
            j.pop("params", None)
        json.dump(j, open(p, "w"), indent=1)


def one_snare():
    import wave
    dst = STIM / "proto_one_snare.wav"
    if dst.exists():
        return
    with wave.open(str(STIM / "02_hits.wav"), "rb") as r:
        params = r.getparams()
        frames = r.readframes(int(6.5 * params.framerate))
    with wave.open(str(dst), "wb") as w:
        w.setparams(params)
        w.writeframes(frames)


def main():
    one_snare()
    work = PAGE / "_sweeps"
    for folder, _, _ in VERSIONS:
        if (PAGE / folder).exists():
            shutil.rmtree(PAGE / folder)
    work.mkdir(parents=True, exist_ok=True)
    auto = work / "tone_sweep_auto.json"
    json.dump(TONE_SWEEP, open(auto, "w"), indent=1)
    for folder, v, _ in VERSIONS:
        procs = []
        for row, stim, extra, tail, au, _ in ROWS:
            base = dict(BASE, tank_voicing=v, **extra)
            sw = {"name": f"{folder}__{row}", "input": str(STIM / stim), "base": base, "grid": {"splash": [base["splash"]]},
                  "tail_seconds": tail, "ignore_flags": IGNORE}
            if au:
                sw["auto"] = str(auto)
            sp = work / f"{folder}__{row}.json"
            json.dump(sw, open(sp, "w"), indent=1)
            procs.append((subprocess.Popen([str(RENDER), "--sweep", str(sp), "--out-dir", str(PAGE / folder / row)],
                                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE), PAGE / folder / row))
        for p, d in procs:
            if p.wait() != 0:
                raise SystemExit(p.stderr.read().decode())
            strip(d)
        print(f"{folder} = tank voicing {v}", flush=True)
    # W: the Wellspring takes, linked (not copied) from test_audio/reference, with analysis sidecars.
    wd = PAGE / "wellspring"
    if wd.exists():
        shutil.rmtree(wd)
    wd.mkdir()
    for row, _, _, _, _, take in ROWS:
        if not take:
            continue
        link = wd / f"W_{row}.wav"
        os.symlink(REF / take, link)
        subprocess.run([str(RENDER), "--analyze", str(link), "--sidecar-out", str(wd / f"W_{row}.json")],
                       check=True, capture_output=True)
    lines = ["Your Wellspring (W, in 'Compare with': pick the take for the row) next to four Resilio versions. "
             "Settings: 2 Springs, CLEAN, MIX 1 (the MIX 0.4 row: the skank with the wet under the dry), DECAY 0.70, "
             "TONE and TENSION noon, the rest at their defaults. Level-matched. The TONE sweep row: one snare, then TONE "
             "noon -> fully right -> noon -> fully left -> noon on its tail.", ""]
    lines += [f"{f}  {t}" for f, _, t in VERSIONS]
    open(PAGE / "README.txt", "w").write("\n".join(lines) + "\n")
    print("now:\n  python3 tools/review/make_review.py " + str(PAGE) + " --rows stimulus --columns none --variants folder1 "
          "--level-match --reference " + str(wd) + ' --title "Wellspring round 5: your Wellspring vs Resilio"')


if __name__ == "__main__":
    main()
