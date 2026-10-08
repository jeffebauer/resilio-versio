#!/usr/bin/env python3
"""Demos round 2: render each clip's dry/wet pair, match loudness, encode, write clips.json.

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
PREROLL = 4.0  # seconds of clock before a clocked echo clip's music (see render_clip)
PEAK_CEILING_DB = -1.3  # -1 dBTP with room for the MP3 encoder's overshoot (about 0.2 dB)
SR = 48000


def knob(v: float) -> str:
    q = round((7 + 10 * v) * 4) / 4
    h = int(q)
    m = round((q - h) * 60)
    return f"{v:g} ({(h - 1) % 12 + 1}{f':{m:02d}' if m else ''})"


# Panel names for the Settings table; renderer keys below.
NAMES = {"springs": "TANK", "attitude": "ATTITUDE", "decay": "DECAY", "tone": "TONE",
         "tension": "TENSION", "splash": "SPLASH", "drive": "DRIVE", "wobble": "WOBBLE"}

LEAD = 0.25  # build_sources.py starts every source after this much silence
bar_t = lambda bpm, bar: LEAD + bar * 4 * 60 / bpm  # the start of a bar (0-based)
tone = lambda *pts: [{"t": t, "key": "tone", "value": v} for t, v in pts]
key = lambda k, *pts: [{"t": t, "key": k, "value": v} for t, v in pts]

DRUMS = "Virtuosity Drums by Versilian Studios and Karoryfer (CC0)"
# "buttons" for 01 and 08 come from src/cues.json, where build_sources.py records the hit times.
CLIPS = {
    "01": dict(
        title="Tubby throw and sweep", src="01_tubby.wav", blend=0.5, dur=30.0,
        set=dict(springs="2", attitude="TAPE", decay=0.72, tone=0.55, tension=0.35, splash=0.55, drive=0.22, wobble=0.45),
        # Owner, 8 Oct: the sweep was inaudible on short throws. Bars 2, 4 and 6 throw the rimshot;
        # bars 7-8 are thrown whole with DECAY up, so TONE rides a long wash and its tail.
        auto={"buttons": [[0.0, 0.02], [4.957, 5.237], [11.273, 11.553], [17.588, 17.868], [19.10, 25.86]],
              "breakpoints": tone((0.0, 0.55), (19.2, 0.55), (22.36, 0.95), (25.6, 0.95), (29.0, 0.55))
                             + [{"t": 0.0, "key": "decay", "value": 0.72}, {"t": 19.1, "key": "decay", "value": 0.72},
                                {"t": 19.6, "key": "decay", "value": 0.85}]},
        caption="A one-drop in A minor. The rimshot on bars 2, 4 and 6 is thrown into the springs, then the last two bars go in whole while TONE rides up to the “Big Knob” and back down the tail.",
        transcript="A reggae one-drop at 76 bpm. Swung hi-hats with open-hat lifts, kick and rimshot together on beat three, soft cross-stick ghosts, and a roots bassline that leaves gaps. On bars two, four and six the rimshot alone splashes into the springs and rings on. Then the whole band goes into the springs for the last two bars, and the reverb turns thin and bright like a telephone, holds there, and warms up again as the long tail fades.",
        notes=f"Drums: {DRUMS}. Bass: Baby Blue by Karoryfer (CC0). Played in code."),
    "02": dict(
        title="Organ bubble into echo", src="02_bubble.wav", blend=0.45, dur=29.5, clock=76,
        set=dict(springs="ECHO", attitude="TAPE", decay=0.55, tone=0.5, tension=0.5, splash=0.4, drive=0.18, wobble=0.44),
        auto={"clock_bpm": 76},
        caption="The reggae organ bubble and guitar chops into the tape echo, clocked to the tempo on dotted eighths. In the last bar the band stops and the echo answers.",
        transcript="An organ plays the reggae bubble, a shuffling two-handed pattern of short chords, over A minor seven and D nine at 76 bpm. A clean guitar chops on beats two and four. Each chord comes back as dotted-eighth repeats that fall between the beats and thicken the shuffle. In the last bar the organ stops after two beats and the final guitar chops repeat on their own and fade.",
        notes="Organ: FreePats Drawbar Organ (CC0). Guitar: Emilyguitar by Karoryfer (CC0). Played in code."),
    "03": dict(
        title="Trombone dub", src="03_trombone.wav", blend=0.5, dur=23.0, clock=74,
        set=dict(springs="ECHO", attitude="TAPE", decay=0.45, tone=0.5, tension=0.5, splash=0.4, drive=0.2, wobble=0.45),
        auto={"clock_bpm": 74,
              "breakpoints": key("decay", (0.0, 0.45), (bar_t(74, 4), 0.45), (bar_t(74, 4) + 2.6, 0.85),
                                 (20.6, 0.85), (21.6, 0.6))
                             + key("tension", (0.0, 0.5), (18.0, 0.5), (18.5, 0.64))},
        caption="A trombone calls and the echo answers in the gaps. On the last phrase DECAY comes up so the repeats pile up, then a turn of TENSION bends the tape.",
        transcript="A lone trombone plays three short phrases in A minor at 74 bpm, leaving a bar of space after each. The echo fills the space with a few dotted-eighth repeats. On the last phrase the repeats grow longer and louder and keep going after the horn stops. Near the end the repeats swoop up in pitch as the delay time shortens, then fade.",
        notes="Trombone: VSCO 2 CE by Versilian Studios (CC0). Played in code."),
    "04": dict(
        title="Chord stab into the bed", src="04_stabs.wav", blend=0.5, dur=21.5,
        set=dict(springs="2", attitude="CLEAN", decay=1.0, tone=0.6, tension=0.5, splash=0.4, drive=0.2, wobble=0.1),
        auto={"breakpoints": key("decay", (0.0, 1.0), (bar_t(120, 8) + 1.8, 1.0), (bar_t(120, 8) + 3.3, 0.55))},
        caption="Dub techno in F minor. Short minor-ninth stabs feed a held spring bed that ducks under each kick, with WOBBLE far left for a seasick tape warble. DECAY comes down at the end to let it go.",
        transcript="A four-on-the-floor kick at 120 bpm with a quiet offbeat hi-hat, and short, syncopated F minor ninth chord stabs on a bright FM keyboard. The springs hold every stab as a continuous wash that dips with each kick and swells between them. After the last stab the wash hangs on for two seconds, then fades away.",
        notes=f"Clavisynth by Versilian Studios (CC0). Kick and hat: {DRUMS}. Played in code."),
    "05": dict(
        title="Echo chord", src="05_echo_chord.wav", blend=0.5, dur=21.5, clock=120,
        set=dict(springs="ECHO", attitude="TAPE", decay=0.6, tone=0.35, tension=0.5, splash=0.4, drive=0.2, wobble=0.44),
        # Owner, 8 Oct: a more extreme ride, warm and dark up into telephone territory and back.
        auto={"clock_bpm": 120, "breakpoints": tone((0.0, 0.35), (9.0, 0.97), (12.0, 0.97), (19.0, 0.4))},
        caption="One chord every two bars into the tape echo on dotted eighths. TONE rides from warm and dark all the way up to a thin telephone ring, then back down, the classic dub techno filter move.",
        transcript="A single F minor ninth stab every two bars at 120 bpm, sometimes with a softer second stab pushed just after it. Each stab repeats as a trail of dotted-eighth echoes that fall across the beat. Over the clip the repeats slowly brighten, then darken again.",
        notes="Clavisynth by Versilian Studios (CC0). Played in code."),
    "06": dict(
        title="Vibes in the springs", src="06_vibes.wav", blend=0.45, dur=22.0,
        set=dict(springs="2", attitude="CLEAN", decay=0.72, tone=0.45, tension=0.5, splash=0.2, drive=0.12, wobble=0.43),
        auto=None,
        caption="Slow minor-ninth chords on soft vibraphone, with a warm tail and a little tape drift.",
        transcript="A vibraphone plays gently rolled chords in D minor, one every few seconds, with a few single notes in between. The springs add a soft, wide bloom behind each chord that drifts very slightly in pitch, and the last chord rings out with a high note on top.",
        notes="VCSL vibraphone by Versilian Studios (CC0). Played in code."),
    "07": dict(
        title="Zither drips", src="07_zither.wav", blend=0.5, dur=19.5,
        set=dict(springs="1", attitude="TAPE", decay=0.6, tension=0.6, splash=0.45, tone=0.5, drive=0.18, wobble=0.44),
        auto=None,
        caption="Sparse plucks on a Vietnamese zither, each one dripping into a single spring. Bent notes and a quick run up the strings.",
        transcript="A Dan Tranh, a plucked Vietnamese zither, plays a slow pentatonic figure with long silences. Every pluck sets off the springy drip of a single reverb tank. Midway there is a fast run up the strings, and two notes are bent upwards after they are plucked.",
        notes="Dan Tranh by Versilian Studios (CC0). Played in code."),
    "08": dict(
        title="Percussion in the springs", src="08_percussion.wav", blend=0.5, dur=20.5, cue_buttons=True,
        set=dict(springs="1", attitude="CLEAN", decay=0.55, tone=0.55, tension=0.5, splash=0.5, drive=0.15, wobble=0.44),
        auto={"breakpoints": []},
        caption="Bongos and an 808 cowbell. A few cowbell hits are thrown into the spring and splash, while everything else stays dry.",
        transcript="Bongos play a busy sixteenth-note pattern of open and muted hits at 118 bpm, with an 808 cowbell on the offbeats and a soft kick from the second bar. Now and then a single cowbell hit bursts into a short, metallic splash of spring reverb while the rest of the groove stays dry.",
        notes=f"Bongos by Versilian Studios (CC0). TR-808 cowbell from Michael Fischer's 808 set via TidalCycles (CC0). Kick: {DRUMS}. Played in code."),
    "09": dict(
        title="The rough end", src="09_siren.wav", blend=0.6, dur=22.0,
        set=dict(springs="2", attitude="VALVE", decay=0.95, tone=0.4, tension=0.35, splash=0.4, drive=0.65, wobble=0.42),
        # Owner, 8 Oct: DECAY eases off over 7 s so the howl settles instead of dropping out.
        auto={"breakpoints": [{"t": 0.0, "key": "decay", "value": 0.95}, {"t": 9.0, "key": "decay", "value": 0.95},
                              {"t": 15.0, "key": "decay", "value": 0.89}]},
        caption="VALVE with DRIVE up. A two-tone dub siren tips the tank into its howl, then DECAY eases down from 9 seconds and the howl settles into a long tail.",
        transcript="A classic sound-system siren switching between two pitches, in three bursts, the last one speeding up. The springs catch it and keep going on their own as a rough, moving roar. From nine seconds the roar slowly loosens over several seconds, then rings out as a long tail.",
        notes="Siren synthesised in code."),
}


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
    spec = dict(c["auto"] or {})
    if c.get("cue_buttons"):  # throw windows from the source's own hit times
        spec["buttons"] = json.loads((src / "cues.json").read_text())[cid]["buttons"]
    # Clocked echo clips (owner, 8 Oct): the echo glides from its free time to the clock's
    # division as the first pulses arrive, a tape swoop at the start. Run PREROLL seconds of
    # silence first so the clock has locked when the music starts, then cut them off.
    pre = PREROLL if "clock_bpm" in spec else 0.0
    src_file = src / c["src"]
    if pre:
        x, sr = sf.read(str(src_file), always_2d=True)
        src_file = work / f"{cid}_preroll.wav"
        sf.write(str(src_file), np.concatenate([np.zeros((int(pre * sr), x.shape[1])), x]).astype(np.float32), sr)
        spec = json.loads(json.dumps(spec))
        for bp in spec.get("breakpoints", []):
            bp["t"] = bp["t"] + pre if bp["t"] > 0 else 0.0
        spec["buttons"] = [[s0 + pre, s1 + pre] for s0, s1 in spec.get("buttons", [])]
        if not spec["buttons"]:
            spec.pop("buttons")
    if spec:
        a = work / f"{cid}_auto.json"
        a.write_text(json.dumps(spec))
        auto = ["--auto", a]
    files = {}
    for side, mix in (("dry", 0), ("wet", 1)):
        w = work / f"{cid}_{side}.wav"
        run(RENDERER, src_file, w, *sets, "--set", f"mix={mix}", *auto)
        x, sr = sf.read(str(w), always_2d=True)
        assert sr == SR, f"{w} is {sr} Hz"
        x = x[int(pre * SR):]
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
    if c.get("clock"):
        settings["CLOCK"] = f"{c['clock']} bpm"
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
