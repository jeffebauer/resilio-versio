#!/usr/bin/env python3
"""Demos round 2: render each clip's dry/wet pair, match loudness, encode, write demos.json.

Usage (repo root): python3 docs/minisite/tools/demos/v2/render.py <src dir> <work dir> <audio out dir> [clip ids…]
Needs numpy, soundfile, ffmpeg and the Renderer (build/rv_render; never delete build/).

Each clip is rendered twice from one source with the same settings: BLEND fully left
(the clean passthrough: the dry, time-aligned with the wet) and BLEND fully right (the
wet). Both get the same gain, chosen so the mix at the clip's own BLEND (the module's
sqrt law) sits at TARGET_LUFS, then both are checked for peaks. The site's slider mixes
them live with the same law (site/src/scripts/player.ts).
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

RENDERER = Path("build/rv_render")
TARGET_LUFS = -20.5
PEAK_CEILING_DB = -1.0
SR = 48000


def knob(v: float) -> str:
    q = round((7 + 10 * v) * 4) / 4
    h = int(q)
    m = round((q - h) * 60)
    return f"{v:g} ({(h - 1) % 12 + 1}{f':{m:02d}' if m else ''})"


# Panel names for the Settings table; renderer keys below.
NAMES = {"springs": "TANK", "attitude": "ATTITUDE", "decay": "DECAY", "tone": "TONE",
         "tension": "TENSION", "splash": "SPLASH", "drive": "DRIVE", "wobble": "WOBBLE"}

THROW_BEAT3 = lambda bpm, bars: [[0.0, 0.02]] + [
    [b * 4 * 60 / bpm + 2 * 60 / bpm - 0.03, b * 4 * 60 / bpm + 2 * 60 / bpm + 0.25] for b in bars]

CLIPS = {
    "01": dict(
        title="Tubby throw and sweep", src="groove_rusty.wav", blend=0.5, dur=24.0,
        set=dict(springs="2", attitude="TAPE", decay=0.65, tone=0.55, tension=0.35, splash=0.55, drive=0.25, wobble=0.45),
        auto={"buttons": THROW_BEAT3(75, [1, 3]),
              "breakpoints": [{"t": 0.0, "key": "tone", "value": 0.55}, {"t": 12.8, "key": "tone", "value": 0.55},
                              {"t": 16.0, "key": "tone", "value": 0.95}, {"t": 17.6, "key": "tone", "value": 0.95},
                              {"t": 20.5, "key": "tone", "value": 0.55}]},
        caption="A one-drop groove. The snare on bars 2 and 4 is thrown into the springs, then TONE rides up to the “Big Knob” and back.",
        transcript="A reggae one-drop at 75 bpm on a vintage kit: closed hats, kick and rimshot on beat three, soft cross-stick ghosts. On the second and fourth bars the rimshot is thrown into the springs and its tail rings on. In the last two bars the reverb thins to a telephone-like splash and then warms up again.",
        notes="Drums: Big Rusty Drums by Karoryfer (CC0), sequenced in code."),
    "01b": dict(
        title="Tubby throw and sweep (kit B)", src="groove_virtuosity.wav", blend=0.5, dur=24.0,
        set=dict(springs="2", attitude="TAPE", decay=0.65, tone=0.55, tension=0.35, splash=0.55, drive=0.25, wobble=0.45),
        auto=None,  # filled from 01 below
        caption="The same groove on a second kit, for comparison.",
        transcript="The same one-drop groove on a modern, tighter kit.",
        notes="Drums: Virtuosity Drums by Versilian Studios and Karoryfer (CC0), sequenced in code."),
    "06": dict(
        title="Wurlitzer in the springs", src="wurli.wav", blend=0.45, dur=21.0,
        set=dict(springs="2", attitude="CLEAN", decay=0.75, tone=0.42, tension=0.5, splash=0.15, drive=0.15, wobble=0.42),
        auto=None,
        caption="Slow C minor chords on a Wurlitzer, with a long, warm tail and a little tape drift.",
        transcript="Four slow electric piano chords in C minor, each held for a bar. The reverb is soft and wide, blooming behind each chord and drifting very slightly in pitch, then rings out after the last chord.",
        notes="Wurlitzer EP200 samples by Greg Sullivan (sullivang.net), CC BY 3.0."),
    "08": dict(
        title="The rough end", src="siren.wav", blend=0.6, dur=17.0,
        set=dict(springs="2", attitude="VALVE", decay=0.95, tone=0.4, tension=0.35, splash=0.4, drive=0.65, wobble=0.42),
        auto={"breakpoints": [{"t": 0.0, "key": "decay", "value": 0.95}, {"t": 10.5, "key": "decay", "value": 0.95},
                              {"t": 11.5, "key": "decay", "value": 0.6}]},
        caption="VALVE with DRIVE up: a dub siren tips the tank into its howl, then DECAY comes down at 10 seconds and it falls away.",
        transcript="Three short siren blips and a rising wail. The springs catch it and keep going on their own as a rough, moving roar. At ten seconds it drops back into a normal tail and fades.",
        notes="Siren synthesised in code."),
}
CLIPS["01b"]["auto"] = CLIPS["01"]["auto"]


def run(*a):
    subprocess.run([str(x) for x in a], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def lufs_and_peak(x: np.ndarray) -> tuple[float, float]:
    with tempfile.NamedTemporaryFile(suffix=".wav") as f:
        sf.write(f.name, x.astype(np.float32), SR)
        p = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", f.name, "-af", "ebur128=peak=true", "-f", "null", "-"],
                           capture_output=True, text=True)
    lines = p.stderr.splitlines()
    i = next(l for l in reversed(lines) if l.strip().startswith("I:"))
    pk = next(l for l in reversed(lines) if l.strip().startswith("Peak:"))
    return float(i.split()[1]), float(pk.split()[1])


def render_clip(cid: str, c: dict, src: Path, work: Path, out: Path) -> dict:
    sets = []
    for k, v in c["set"].items():
        sets += ["--set", f"{k}={v}"]
    auto = []
    if c["auto"]:
        a = work / f"{cid}_auto.json"
        a.write_text(json.dumps(c["auto"]))
        auto = ["--auto", a]
    files = {}
    for side, mix in (("dry", 0), ("wet", 1)):
        w = work / f"{cid}_{side}.wav"
        run(RENDERER, src / c["src"], w, *sets, "--set", f"mix={mix}", *auto)
        x, sr = sf.read(str(w), always_2d=True)
        assert sr == SR, f"{w} is {sr} Hz"
        files[side] = x[: int(c["dur"] * SR)]
    n = min(len(files["dry"]), len(files["wet"]))
    dry, wet = files["dry"][:n], files["wet"][:n]
    fade = np.linspace(1, 0, int(1.5 * SR))[:, None]  # the last 1.5 s fade out, so a cut tail never clicks
    dry[-len(fade):] *= fade
    wet[-len(fade):] *= fade
    b = c["blend"]
    mix = dry * np.sqrt(1 - b) + wet * np.sqrt(b)
    lufs, _ = lufs_and_peak(mix)
    gain_db = TARGET_LUFS - lufs
    g = 10 ** (gain_db / 20)
    peaks = {k: lufs_and_peak(v * g)[1] for k, v in (("dry", dry), ("wet", wet), ("mix", mix))}
    over = max(peaks.values()) - PEAK_CEILING_DB
    if over > 0:  # turn the pair down together rather than limit either side
        g *= 10 ** (-over / 20)
        gain_db -= over
    names = {}
    for side, x in (("dry", dry), ("wet", wet)):
        name = f"{cid}-{side}.mp3"
        with tempfile.NamedTemporaryFile(suffix=".wav") as f:
            sf.write(f.name, (x * g).astype(np.float32), SR)
            run("ffmpeg", "-y", "-loglevel", "error", "-i", f.name, "-b:a", "160k", out / name)
        names[side] = name
    final_lufs, _ = lufs_and_peak(mix * g)
    print(f"{cid}: gain {gain_db:+.1f} dB, mix {final_lufs:.1f} LUFS, peaks dry {peaks['dry']:.1f} wet {peaks['wet']:.1f} dBTP")
    settings = {NAMES[k]: (v if isinstance(v, str) else knob(v)) for k, v in c["set"].items()}
    settings = {"TANK": settings.pop("TANK"), "ATTITUDE": settings.pop("ATTITUDE"), "BLEND": knob(b) + ", the slider's start", **settings}
    return {"file": names["wet"], "dry": names["dry"], "blend": b, "title": c["title"], "caption": c["caption"],
            "transcript": c["transcript"], "duration_s": round(n / SR, 1), "settings": settings, "note": c["notes"]}


def main():
    src, work, out = (Path(a) for a in sys.argv[1:4])
    want = sys.argv[4:] or list(CLIPS)
    work.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    clips = [render_clip(cid, CLIPS[cid], src, work, out) for cid in want]
    (work / "clips.json").write_text(json.dumps(clips, indent=2, ensure_ascii=False))
    print(f"wrote {len(clips)} clips; entries in {work / 'clips.json'}")


if __name__ == "__main__":
    main()
