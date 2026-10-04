#!/usr/bin/env python3
"""Stimuli for the Hold ducking page (ADR 0040 round 2). Stdlib only,
deterministic. 48 kHz 24-bit mono WAVs + automation into the folder given.

  stab_four.wav     a low chord stab at 0.5 s, then 4-on-the-floor kick +
                    off-beat hats at 120 bpm from 4 s to 16 s, 4 s of tail.
  stab_onedrop.wav  the stab, then a dub one-drop at 75 bpm from 4 s
                    (kick + rim on 3, bass on 1 and the "and" of 2, hats on
                    8ths) to 16.8 s, tail.
  pad_drums.wav     a C minor pad 0.5-4 s, then the 4-on-the-floor loop
                    from 5 s to 15 s, tail.
  kick_times_*.txt  the kick times, for hump.py.
  howl_beat.wav     snare / thump beat for the KICKED Howl flip (as the
                    throw page's dub beat), and howl_flip.json (KICKED until
                    9 s, then DRIVEN; DECAY 1).
"""
import json
import math
import random
import struct
import sys
import wave
from pathlib import Path

SR = 48000
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("stim")


def db(x):
    return 10 ** (x / 20)


def write(name, x):
    OUT.mkdir(parents=True, exist_ok=True)
    fr = bytearray()
    for s in x:
        fr += struct.pack("<i", int(round(max(-1.0, min(1.0, s)) * 8388607)))[:3]
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(SR)
        w.writeframes(bytes(fr))


def lp(x, fc):
    a = math.exp(-2 * math.pi * fc / SR)
    y, out = 0.0, []
    for s in x:
        y = (1 - a) * s + a * y
        out.append(y)
    return out


def add(dst, src, at, gain):
    i0 = int(at * SR)
    for i, s in enumerate(src):
        if 0 <= i0 + i < len(dst):
            dst[i0 + i] += gain * s


def kick():
    out, ph = [], 0.0
    for i in range(int(0.3 * SR)):
        t = i / SR
        ph += 2 * math.pi * (50 + 90 * math.exp(-t / 0.03)) / SR
        out.append(math.sin(ph) * math.exp(-t / 0.09))
    return out


def hat(rng):
    n = int(0.05 * SR)
    prev, out = 0.0, []
    for i in range(n):
        v = rng.uniform(-1, 1)
        out.append((v - prev) * 0.5 * math.exp(-i / SR / 0.01))
        prev = v
    return out


def rim(rng):
    n = int(0.06 * SR)
    out = []
    for i in range(n):
        t = i / SR
        out.append((math.sin(2 * math.pi * 1700 * t) + 0.5 * math.sin(2 * math.pi * 480 * t)
                    + 0.5 * rng.uniform(-1, 1)) * math.exp(-t / 0.008))
    p = max(abs(s) for s in out)
    return [s / p for s in out]


def bass(seconds, hz):
    n, fade = int(seconds * SR), int(0.01 * SR)
    out = []
    for i in range(n):
        env = min(1.0, i / fade, (n - 1 - i) / fade)
        t = i / SR
        out.append(env * (math.sin(2 * math.pi * hz * t) + 0.25 * math.sin(4 * math.pi * hz * t)))
    return out


def stab():
    chord = [110.0, 130.81, 164.81, 220.0]  # A minor, low
    n = int(0.25 * SR)
    out = [sum(2 * ((f * i / SR) % 1) - 1 for f in chord) / 4 * math.exp(-i / SR / 0.08) for i in range(n)]
    out = lp(lp(out, 1500), 2500)
    p = max(abs(s) for s in out)
    return [s / p for s in out]


def pad(seconds):
    chord = [130.81, 155.56, 196.0, 261.63]
    n, fade = int(seconds * SR), int(0.3 * SR)
    out = []
    for i in range(n):
        t = i / SR
        s = sum(2 * ((f * (1 + 0.002 * k) * t) % 1) - 1 for k, f in enumerate(chord)) / 4
        out.append(s * min(1.0, i / fade, (n - 1 - i) / fade))
    out = lp(lp(out, 1200), 1800)
    p = max(abs(s) for s in out)
    return [s / p for s in out]


def four(dst, start, end, rng):
    k, times = kick(), []
    t = start
    while t < end - 1e-6:
        add(dst, k, t, db(-6))
        add(dst, hat(rng), t + 0.25, db(-14))
        times.append(t)
        t += 0.5
    return times


def onedrop(dst, start, bars, rng):
    beat = 60 / 75
    k, times = kick(), []
    for b in range(bars):
        bar = start + 4 * beat * b
        add(dst, k, bar + 2 * beat, db(-6))
        add(dst, rim(rng), bar + 2 * beat, db(-12))
        times.append(bar + 2 * beat)
        add(dst, bass(0.6, 55.0), bar, db(-12))
        add(dst, bass(0.4, 73.42), bar + 1.5 * beat, db(-12))
        for e in range(8):
            add(dst, hat(rng), bar + 0.5 * beat * e, db(-20))
    return times


def main():
    rng = random.Random(3)
    st = stab()
    x = [0.0] * int(20 * SR)
    add(x, st, 0.5, db(-6))
    t4 = four(x, 4.0, 16.0, rng)
    write("stab_four.wav", x)
    (OUT / "kick_times_stab_four.txt").write_text(" ".join(f"{t:.4f}" for t in t4))

    x = [0.0] * int(21 * SR)
    add(x, st, 0.5, db(-6))
    t1 = onedrop(x, 4.0, 4, rng)
    write("stab_onedrop.wav", x)
    (OUT / "kick_times_stab_onedrop.txt").write_text(" ".join(f"{t:.4f}" for t in t1))

    x = [0.0] * int(19 * SR)
    add(x, pad(3.5), 0.5, db(-12))
    tp = four(x, 5.0, 15.0, rng)
    write("pad_drums.wav", x)
    (OUT / "kick_times_pad_drums.txt").write_text(" ".join(f"{t:.4f}" for t in tp))

    # The Howl flip: thump on 1 and 3, snare-ish rim on 2 and 4, 75 bpm.
    x = [0.0] * int(22 * SR)
    beat = 60 / 75
    k = kick()
    for b in range(16):
        t = 1.0 + b * beat
        if b % 2 == 0:
            add(x, k, t, db(-9))
        else:
            add(x, rim(rng), t, db(-9))
    write("howl_beat.wav", x)
    flip = [{"t": 0.0, "key": "attitude", "value": 1.0}, {"t": 8.999, "key": "attitude", "value": 1.0},
            {"t": 9.0, "key": "attitude", "value": 0.5}]
    json.dump({"breakpoints": flip}, open(OUT / "howl_flip.json", "w"), indent=1)


main()
