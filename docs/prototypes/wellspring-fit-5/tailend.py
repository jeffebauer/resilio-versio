#!/usr/bin/env python3
"""Round 5 fix-up probe: what a long tail ends on (test_output_bits's rim: click + 420 / 1150 / 2300 Hz partials,
-6 dBFS, DECAY 0.85, MIX 1, 2 Springs), with and without the output's mu-law box, DRIVEN and KICKED, 7 vs 8:
1/3-octave levels of the second before the box's silence, re their loudest band.
  python3 docs/prototypes/wellspring-fit-5/tailend.py
"""
import math, random, struct, subprocess, wave, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5  # noqa: E402

fs = 48000
stim = r5.WORK / "rim12.wav"
r5.WORK.mkdir(parents=True, exist_ok=True)
x = [0.0] * (12 * fs)
rng = random.Random(99)
r = []
for i in range(int(0.12 * fs)):
    t = i / fs
    r.append(0.5 * math.exp(-t / 0.002) * rng.uniform(-1, 1) + 0.35 * math.exp(-t / 0.03) * math.sin(2 * math.pi * 420 * t)
             + 0.3 * math.exp(-t / 0.02) * math.sin(2 * math.pi * 1150 * t) + 0.2 * math.exp(-t / 0.012) * math.sin(2 * math.pi * 2300 * t))
pk = max(abs(v) for v in r)
for i, v in enumerate(r):
    x[int(0.3 * fs) + i] = v / pk * 10 ** (-6 / 20)
with wave.open(str(stim), "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs)
    w.writeframes(b"".join(struct.pack("<h", int(32767 * v)) for v in x))


def last_nonzero(path):
    with wave.open(str(path)) as w:
        n = w.getnframes(); sw = w.getsampwidth(); data = w.readframes(n)
    step = sw * 2
    for i in range(n - 1, 0, -1):
        if any(data[i * step:(i + 1) * step]):
            return i / fs
    return 0


for v in (7, 8):
    for att in ("DRIVEN", "KICKED"):
        res = {}
        for box in (1, 0):
            out = r5.WORK / f"te_{v}_{att}_{box}.wav"
            subprocess.run([str(r5.RENDER), str(stim), str(out), "--set", f"tank_voicing={v}", "--set", "mix=1", "--set", "springs=2",
                            "--set", f"attitude={att}", "--set", "decay=0.85", "--set", f"output_bits_voicing={box}"], check=True, capture_output=True)
            res[box] = out
        end = last_nonzero(res[1])
        for box in (1, 0):
            th = r5.an("steady", res[box], end - 1.2, end - 0.2)["thirds"]
            top = max(d for _, d in th)
            best = sorted(th, key=lambda q: -q[1])[:4]
            print(f"v{v} {att} box {box}: silence at {end:.1f} s; loudest thirds " + ", ".join(f"{f:.0f} Hz {d - top:+.1f}" for f, d in best), flush=True)
