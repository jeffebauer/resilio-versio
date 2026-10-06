#!/usr/bin/env python3
"""Cut, join, loudness-match (EBU R128) and encode the demo renders to MP3."""
import json, re, subprocess, sys

OUT = sys.argv[1]  # destination dir for the MP3s
TARGET_LUFS = -20.0
CEILING_DBTP = -1.0


def ff(*args):
    return subprocess.run(["ffmpeg", "-hide_banner", "-nostdin", "-y", *args],
                          capture_output=True, text=True, check=True)


def cut(src, dst, start, dur, fade=0.6):
    ff("-i", src, "-af", f"atrim={start}:{start + dur},asetpts=N/SR/TB,afade=t=out:st={dur - fade}:d={fade}",
       "-c:a", "pcm_s24le", dst)


def join(srcs, dst):
    ins = sum((["-i", s] for s in srcs), [])
    filt = "".join(f"[{i}:a]" for i in range(len(srcs))) + f"concat=n={len(srcs)}:v=0:a=1[a]"
    ff(*ins, "-filter_complex", filt, "-map", "[a]", "-c:a", "pcm_s24le", dst)


def measure(src):
    r = ff("-i", src, "-af", "ebur128=peak=true", "-f", "null", "-")
    tail = r.stderr[r.stderr.rfind("Summary:"):]
    lufs = float(re.search(r"I:\s+(-?[\d.]+) LUFS", tail).group(1))
    tp = float(re.search(r"Peak:\s+(-?[\d.]+) dBFS", tail).group(1))
    return lufs, tp


clips = {
    "01-dry-then-wet": ("r01.wav", 0, 18.2),
    "04-throw": ("r04.wav", 0, 18.2),
    "05-splash": ("r05.wav", 0, 18.2),
    "06-big-knob": ("r06.wav", 0, 18.2),
    "07-wobble": ("r07.wav", 0, 15.0),
    "08-echo-skank": ("r08.wav", 2.4, 17.6),
    "09-valve-howl": ("r09.wav", 0, 18.0),
    "10-hold": ("r10.wav", 0, 20.0),
}
for name, (src, start, dur) in clips.items():
    cut(src, f"{name}.wav", start, dur)
for t in ("1", "2"):
    cut(f"r02_{t}.wav", f"p02_{t}.wav", 0, 5.5)
join(["p02_1.wav", "p02_2.wav"], "02-tank-1-then-2.wav")
for a in ("CLEAN", "TAPE", "VALVE"):
    cut(f"r03_{a}.wav", f"p03_{a}.wav", 0, 5.0)
join(["p03_CLEAN.wav", "p03_TAPE.wav", "p03_VALVE.wav"], "03-clean-tape-valve.wav")

report = {}
names = sorted(list(clips) + ["02-tank-1-then-2", "03-clean-tape-valve"])
for name in names:
    lufs, tp = measure(f"{name}.wav")
    gain = min(TARGET_LUFS - lufs, CEILING_DBTP - tp)
    ff("-i", f"{name}.wav", "-af", f"volume={gain:.2f}dB", "-ar", "48000",
       "-c:a", "libmp3lame", "-b:a", "160k", "-id3v2_version", "3",
       "-metadata", "artist=Resilio Versio", "-metadata", f"title={name}", f"{OUT}/{name}.mp3")
    l2, tp2 = measure(f"{OUT}/{name}.mp3")
    report[name] = {"source_lufs": lufs, "gain_db": round(gain, 2), "lufs": l2, "true_peak_dbfs": tp2}
    print(f"{name:24s} {lufs:6.1f} LUFS -> {l2:6.1f} LUFS, peak {tp2:5.1f} dBFS (gain {gain:+.1f} dB)")
json.dump(report, open("loudness.json", "w"), indent=1)
