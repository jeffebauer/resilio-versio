"""Stimuli for the WOBBLE hang prototype (scratch; uses tools/make_stimulus.py helpers).

hang_test.wav   : 8 x (one bar of the skank's D-chord offbeat stabs, then 7 s silence)
                  - many note-offs, each at a different WOBBLE phase, for statistics.
held_chord.wav  : D chord (the skank's saws) held 2.5 s, 7 s silence, twice.
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, "/Users/jesse/Documents/Sites/resilio-versio/.claude/worktrees/agent-ae7c4291bfddd0279/tools")
import make_stimulus as ms  # noqa: E402

SR = ms.SR
OUT = Path(sys.argv[1])
D = [293.66, 369.99, 440.0]


def saw_chord(n, env):
    return [env(i) * sum(2 * ((f * i / SR) % 1) - 1 for f in D) / 3 for i in range(n)]


def stab_bar():
    beat = 60 / 75
    total = int(4 * beat * SR)
    out = [0.0] * total
    for b in range(4):
        start = int((b * beat + beat / 2) * SR)
        n = int(0.12 * SR)
        s = saw_chord(n, lambda i: math.exp(-(i / SR) / 0.035))
        for i in range(n):
            if start + i < total:
                out[start + i] += s[i]
    return out


def lp(x):
    return ms.one_pole_lp(ms.one_pole_lp(x, 2500), 2500)


def hang_test():
    bar = stab_bar()
    out = []
    for _ in range(8):
        out += bar + [0.0] * int(7 * SR)
    return ms.silence(1) + ms.normalise(lp(out), -6)


def held_chord():
    n = int(2.5 * SR)
    fade = int(0.01 * SR)
    held = saw_chord(n, lambda i: min(1.0, i / fade, (n - 1 - i) / fade))
    out = []
    for _ in range(2):
        out += held + [0.0] * int(7 * SR)
    return ms.silence(1) + ms.normalise(lp(out), -9)


def write(path, x):
    import struct
    import wave
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(3)
        w.setframerate(SR)
        b = bytearray()
        for v in x:
            iv = max(-8388607, min(8388607, int(round(v * 8388607))))
            p = struct.pack("<i", iv)[:3]
            b += p + p
        w.writeframes(bytes(b))


OUT.mkdir(parents=True, exist_ok=True)
write(OUT / "hang_test.wav", hang_test())
write(OUT / "held_chord.wav", held_chord())
print("ok")
