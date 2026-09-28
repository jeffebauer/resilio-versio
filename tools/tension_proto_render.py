#!/usr/bin/env python3
"""TENSION prototype listening renders (docs/tension-prototype.md).

Usage:
  python3 tools/tension_proto_render.py <proto_rv_render> <today_rv_render> <out_dir> <rimshot.wav> <ir_library_dir>

  proto_rv_render : this branch's build/rv_render (TENSION in the BOING slot)
  today_rv_render : HEAD's rv_render with kChirpDirection = HighsLater (BOING)
  out_dir         : e.g. renders/tension_proto (gitignored)

Writes out_dir/{stim,tanks,tension_sweep,decay_sweep,today_boing,turning,measure}
with a manifest.json per page, then run tools/review/make_review.py on each.
Every render: MIX 1, SPRINGS 2, DRIVEN, DRIVE 0.3, SPLASH 0, WOBBLE 0, TONE 0.5,
unless noted. Sidecar "params" get "tension" instead of "boing" for the
prototype pages, plus a "stimulus" label, so the review page shows a grid.
"""
import array
import json
import shutil
import subprocess
import sys
import time
import wave
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROTO, TODAY, OUT, RIM, IRLIB = sys.argv[1], sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4]), Path(sys.argv[5])
SR = 48000
STIMDIR = ROOT / "test_audio" / "stimulus"
POS = [0.0, 0.25, 0.5, 0.75, 1.0]
BASE = {"mix": 1, "springs": 0.5, "attitude": 0.5, "drive": 0.3, "splash": 0, "wobble": 0, "tone": 0.5}
TANKS = ["ir_hybrid_Space_Echo_Spring", "ir_convpack_Amp_Spring_Bright", "ir_convpack_SNRA500_Plucky",
         "ir_convpack_Amazing_Stereo_Spring", "ir_convpack_Farfi_Spring_Dirtier_Wider_R",
         "ir_convpack_Swissecho_Spring_1.5s_Wide"]


# ---------------------------------------------------------------- wav helpers (24-bit mono in, 24-bit out)
def read_mono(p):
    w = wave.open(str(p))
    n, sw, ch = w.getnframes(), w.getsampwidth(), w.getnchannels()
    raw = w.readframes(n)
    assert sw == 3, p
    vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    return vals[::ch]


def write_mono(p, x):
    b = bytearray()
    for v in x:
        q = max(-8388608, min(8388607, int(round(v * 8388607))))
        b += q.to_bytes(3, "little", signed=True)
    w = wave.open(str(p), "wb")
    w.setnchannels(1)
    w.setsampwidth(3)
    w.setframerate(SR)
    w.writeframes(bytes(b))
    w.close()


def pad(x, tail_s, lead_s=0.0):
    return [0.0] * int(lead_s * SR) + list(x) + [0.0] * int(tail_s * SR)


def excerpt(x, t0, t1, fade_s=0.02):
    y = x[int(t0 * SR):int(t1 * SR)]
    f = int(fade_s * SR)
    for i in range(f):
        y[-1 - i] *= i / f
    return y


# ---------------------------------------------------------------- stimuli
def make_stimuli():
    d = OUT / "stim"
    d.mkdir(parents=True, exist_ok=True)
    clicks = read_mono(STIMDIR / "07_click_single.wav")
    hits = read_mono(STIMDIR / "02_hits.wav")
    held = read_mono(STIMDIR / "08_held_tones.wav")
    rim = read_mono(RIM)
    stims = {
        "click": pad(clicks[:int(6 * SR)], 0),            # one click, 6 s of tail
        "hits": pad(hits, 3),
        "rimshot": pad(rim, 3),
        "held": pad(held, 3),
        # turning: sound first, then the knob moves in the tail (see AUTO below)
        "t_chord": pad(excerpt(held, 10.0, 13.0), 7, 0.5),   # A minor 0.5-3.5 s, tail to 10.5 s
        "t_rim": pad(excerpt(rim, 0.0, 1.4), 8.6),          # first rimshot at ~0.5 s
        "t_sine": pad(excerpt(held, 1.0, 8.0), 3, 0.5),      # 1 kHz held 0.5-7.5 s (pitch track)
    }
    for k, v in stims.items():
        write_mono(d / f"{k}.wav", v)
    return {k: d / f"{k}.wav" for k in stims}


def render(binary, src, dst, sets, auto=None):
    cmd = [binary, str(src), str(dst), "--sidecar"]
    for k, v in sets.items():
        cmd += ["--set", f"{k}={v}"]
    if auto:
        cmd += ["--auto", str(auto)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print("FAIL", " ".join(cmd), r.stderr)
    return r.returncode


def relabel(sidecar, extra, rename_boing):
    s = json.loads(sidecar.read_text())
    p = dict(s.get("params") or {})
    if rename_boing and "boing" in p:
        p["tension"] = p.pop("boing")
    p.update(extra)
    s["params"] = p
    sidecar.write_text(json.dumps(s))
    return p


def page(name, binary, stims, key, fixed, rename_boing, desc):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    jobs = []
    for st in ("click", "hits", "rimshot", "held"):
        for v in POS:
            sets = dict(BASE, **fixed)
            sets[key] = v
            label = ("tension" if (rename_boing and key == "boing") else key)
            base = f"{name}_{st}__{label}{v:.2f}"
            jobs.append((binary, stims[st], d / f"{base}.wav", sets, st, base))
    with ThreadPoolExecutor(10) as ex:
        list(ex.map(lambda j: render(j[0], j[1], j[2], j[3]), jobs))
    renders = []
    for (_, _, wav, sets, st, base) in jobs:
        p = relabel(d / f"{base}.json", {"stimulus": st}, rename_boing)
        renders.append({"wav": wav.name, "sidecar": f"{base}.json", "params": p})
    (d / "manifest.json").write_text(json.dumps({
        "name": name, "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "input": desc, "ignore_flags": ["click_count", "max_step_db_100ms", "resonance_peak_db"],
        "renders": renders}, indent=1))


# ---------------------------------------------------------------- turning (automation)
def auto_file(path, key, t0, t1, v0, v1):
    path.write_text(json.dumps({"breakpoints": [{"t": 0.0, "key": key, "value": v0}, {"t": t0, "key": key, "value": v0},
                                                {"t": t1, "key": key, "value": v1}]}))
    return path


def turning(stims):
    d = OUT / "turning"
    d.mkdir(parents=True, exist_ok=True)
    # knob window per stimulus: after the sound stops (chord/rim), or while it holds (sine, for pitch tracking)
    win = {"t_chord": (3.8, 6.8), "t_rim": (1.6, 4.6), "t_sine": (3.0, 6.0)}
    jobs = []
    for st, (t0, t1) in win.items():
        s = st[2:]
        # prototype: TENSION 0.3 -> 0.7 at DECAY 0.75 (should bend, like stretching the tank)
        jobs.append((PROTO, stims[st], d / f"new_tension_turn_{s}.wav", dict(BASE, decay=0.75),
                     auto_file(d / f"auto_tension_{s}.json", "boing", t0, t1, 0.3, 0.7), s, "TENSION 0.3->0.7 (new)"))
        # prototype: DECAY 0.3 -> 0.9 at TENSION 0.5 (should NOT bend)
        jobs.append((PROTO, stims[st], d / f"new_decay_turn_{s}.wav", dict(BASE, boing=0.5),
                     auto_file(d / f"auto_decay_{s}.json", "decay", t0, t1, 0.3, 0.9), s, "DECAY 0.3->0.9 (new)"))
        # today: DECAY 0.3 -> 0.9 at BOING 0.5 (bends: DECAY also stretches L)
        jobs.append((TODAY, stims[st], d / f"today_decay_turn_{s}.wav", dict(BASE, boing=0.5),
                     d / f"auto_decay_{s}.json", s, "DECAY 0.3->0.9 (today)"))
    # (today's DECAY turn reuses the auto file written for the new one above)
    with ThreadPoolExecutor(10) as ex:
        list(ex.map(lambda j: render(j[0], j[1], j[2], j[3], j[4]), jobs))
    renders = []
    for (_, _, wav, sets, auto, s, what) in jobs:
        sc = wav.with_suffix(".json")
        p = relabel(sc, {"stimulus": s, "move": what}, False)
        renders.append({"wav": wav.name, "sidecar": sc.name, "params": p})
    (d / "manifest.json").write_text(json.dumps({
        "name": "turning", "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "input": "stim/t_chord (A minor, ends 3.5 s; knob 3.8-6.8 s), t_rim (one rimshot; knob 1.6-4.6 s), "
                 "t_sine (1 kHz held 0.5-7.5 s; knob 3.0-6.0 s)",
        "ignore_flags": ["click_count", "max_step_db_100ms", "resonance_peak_db"], "renders": renders}, indent=1))


# ---------------------------------------------------------------- measurement renders (study conditions)
def measure(stims):
    """1 Spring (Spring A), CLEAN, DRIVE 0, click + 12 s, like docs/ir-dispersion-study.md."""
    d = OUT / "measure"
    d.mkdir(parents=True, exist_ok=True)
    click = read_mono(STIMDIR / "07_click_single.wav")[:int(2 * SR)]
    src = OUT / "stim" / "measure_click.wav"
    write_mono(src, pad(click, 12))
    base = {"mix": 1, "springs": 0, "attitude": 0, "drive": 0, "splash": 0, "wobble": 0, "tone": 0.5}
    jobs = []
    for dec in (0.0, 0.5, 1.0):
        for v in POS:
            jobs.append((PROTO, src, d / f"new_decay{dec:.2f}_tension{v:.2f}.wav", dict(base, decay=dec, boing=v)))
            jobs.append((TODAY, src, d / f"today_decay{dec:.2f}_boing{v:.2f}.wav", dict(base, decay=dec, boing=v)))
    with ThreadPoolExecutor(10) as ex:
        list(ex.map(lambda j: render(*j), jobs))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    stims = make_stimuli()
    t = OUT / "tanks"
    t.mkdir(exist_ok=True)
    for n in TANKS:
        for ext in (".wav", ".json"):
            shutil.copy2(IRLIB / (n + ext), t / (n + ext))
    desc = "click (07_click_single), 02_hits, rimshot (m8_round1 rimshot_m9), 08_held_tones; +3-6 s tail"
    page("tension_sweep", PROTO, stims, "boing", {"decay": 0.5}, True, "TENSION 0..1 at DECAY 0.5 — " + desc)
    page("decay_sweep", PROTO, stims, "decay", {"boing": 0.5}, True, "DECAY 0..1 at TENSION 0.5 — " + desc)
    page("today_boing", TODAY, stims, "boing", {"decay": 0.5}, False,
         "TODAY (HEAD 9b10fc2 + HighsLater): BOING 0..1 at DECAY 0.5 — " + desc)
    turning(stims)
    measure(stims)


if __name__ == "__main__":
    main()
