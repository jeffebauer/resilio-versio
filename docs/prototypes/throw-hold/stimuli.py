#!/usr/bin/env python3
"""Stimuli and automation for the THROW / HOLD listening page (ADR 0039, 0040).

Stdlib only, deterministic. Writes 48 kHz 24-bit mono WAVs and automation
JSONs into the folder given (default renders/feat_throw_hold/stim):

  dub_beat.wav    1 s silence, 4 bars at 75 bpm (thump on 1 and 3, snare on
                  2 and 4, offbeat Am / D stabs as 04_skank), 8 s tail.
  hold_scene.wav  a C minor pad 0.5-5 s, the beat without stabs 7-19.8 s,
                  single stabs at 22 s and 26 s, silence to 34 s.
  pad60.wav       a C minor pad held for 60 s at -3 dBFS peak (creep check);
                  pad60_m12.wav the same at -12 dBFS.

  throw.json      the gate: a 1 ms blip at 0 (switches the throw on, as
                  patching a gate does), then open for bar 2's last snare,
                  bar 3's last stab and bar 4's last snare.
  hold.json       DECAY 0.8 until 4.5 s, up to 1 by 5.5 s.
  hold_throw.json hold.json + the gate open for the 22 s stab (thrown into
                  the frozen bed; its first rise, so the pad got in before).
  howl_flip.json  ATTITUDE KICKED until 9 s, then DRIVEN (DECAY stays 1).
"""

import json
import math
import random
import struct
import sys
import wave
from pathlib import Path

SR = 48000
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("renders/feat_throw_hold/stim")
BEAT = 60 / 75


def db(x):
    return 10 ** (x / 20)


def write(name, x):
    OUT.mkdir(parents=True, exist_ok=True)
    frames = bytearray()
    for s in x:
        frames += struct.pack("<i", int(round(max(-1.0, min(1.0, s)) * 8388607)))[:3]
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(SR)
        w.writeframes(bytes(frames))


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
        if i0 + i < len(dst):
            dst[i0 + i] += gain * s


def snare(rng):
    n = int(0.25 * SR)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    lo = lp(noise, 800)
    noise = [h - l for h, l in zip(lp(noise, 7000), lo)]
    out = [0.6 * math.sin(2 * math.pi * 185 * i / SR) * math.exp(-i / SR / 0.03)
           + 1.2 * noise[i] * math.exp(-i / SR / 0.06) for i in range(n)]
    p = max(abs(s) for s in out)
    return [s / p for s in out]


def thump():
    n = int(0.3 * SR)
    out, ph = [], 0.0
    for i in range(n):
        t = i / SR
        ph += 2 * math.pi * (50 + 90 * math.exp(-t / 0.03)) / SR
        out.append(math.sin(ph) * math.exp(-t / 0.09))
    return out


def stab(chord):
    n = int(0.12 * SR)
    out = [sum(2 * ((f * i / SR) % 1) - 1 for f in chord) / 3 * math.exp(-i / SR / 0.035) for i in range(n)]
    out = lp(lp(out, 2500), 2500)
    p = max(abs(s) for s in out)
    return [s / p for s in out]


def pad(seconds, peak_db):
    n = int(seconds * SR)
    chord = [130.81, 155.56, 196.0, 261.63]  # C minor
    fade = int(0.3 * SR)
    out = []
    for i in range(n):
        t = i / SR
        s = sum(2 * ((f * (1 + 0.002 * k) * t) % 1) - 1 for k, f in enumerate(chord)) / 4
        out.append(s * min(1.0, i / fade, (n - 1 - i) / fade))
    out = lp(lp(out, 1200), 1800)
    p = max(abs(s) for s in out)
    return [s * db(peak_db) / p for s in out]


AM, D = [220.0, 261.63, 329.63], [293.66, 369.99, 440.0]


def beat(dst, start, stabs=True):
    rng = random.Random(7)
    sn, th = snare(rng), thump()
    st = {0: stab(AM), 1: stab(D)}
    for bar in range(4):
        for b in range(4):
            t = start + (bar * 4 + b) * BEAT
            if b in (0, 2):
                add(dst, th, t, db(-9))
            else:
                add(dst, sn, t, db(-9))
            if stabs:
                add(dst, st[bar % 2], t + BEAT / 2, db(-12))


def main():
    x = [0.0] * int((1 + 16 * BEAT + 8) * SR)
    beat(x, 1.0)
    write("dub_beat.wav", x)

    y = [0.0] * int(34 * SR)
    add(y, pad(4.5, -12), 0.5, 1.0)
    beat(y, 7.0, stabs=False)
    add(y, stab(AM), 22.0, db(-9))
    add(y, stab(D), 26.0, db(-9))
    write("hold_scene.wav", y)

    p60 = pad(60.0, -3)
    write("pad60.wav", p60)
    write("pad60_m12.wav", [v * db(-9) for v in p60])

    snare_at = lambda bar, b: 1.0 + (bar * 4 + b) * BEAT
    gates = [[0.0, 0.001],
             [snare_at(1, 3) - 0.02, snare_at(1, 3) + 0.15],
             [snare_at(2, 3) + BEAT / 2 - 0.02, snare_at(2, 3) + BEAT / 2 + 0.14],
             [snare_at(3, 3) - 0.02, snare_at(3, 3) + 0.15]]
    json.dump({"gates": [[round(a, 4), round(b, 4)] for a, b in gates]}, open(OUT / "throw.json", "w"), indent=1)
    hold = [{"t": 0.0, "key": "decay", "value": 0.8}, {"t": 4.5, "key": "decay", "value": 0.8},
            {"t": 5.5, "key": "decay", "value": 1.0}]
    json.dump({"breakpoints": hold}, open(OUT / "hold.json", "w"), indent=1)
    json.dump({"breakpoints": hold, "gates": [[21.98, 22.3]]}, open(OUT / "hold_throw.json", "w"), indent=1)
    flip = [{"t": 0.0, "key": "attitude", "value": 1.0}, {"t": 8.999, "key": "attitude", "value": 1.0},
            {"t": 9.0, "key": "attitude", "value": 0.5}]
    json.dump({"breakpoints": flip}, open(OUT / "howl_flip.json", "w"), indent=1)


main()
