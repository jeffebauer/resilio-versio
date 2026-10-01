#!/usr/bin/env python3
"""Generate held-sound stimulus WAVs for the Sustain trim (docs/briefs/sustain-trim.md).

Pads and drones fill the Tank: it keeps adding a held sound to what is still
ringing, so the wet can end up louder than the source. These three stand in
for the owner's low-mid synth pad (1 Oct 2026, the real one stays on the Mac):

  10_pad_cminor.wav   C minor pad (C2 + C3/Eb3/G3), detuned saws low-passed
                      at ~700 Hz, 3 s swell, 6 s hold, 3 s release, peak -6 dBFS.
  11_drone_c2.wav     Bass drone: C2 sine + 2nd harmonic, 1 s fade in, 10 s
                      hold, 1 s fade out, peak -6 dBFS.
  12_organ_chord.wav  Organ-like held chord (C3/Eb3/G3 + C2, drawbar-style
                      harmonics 1, 2, 3, 4, 6, 8), 20 ms attack, 8 s hold,
                      50 ms release, peak -6 dBFS.

Each starts after 1 s of silence and has 6 s of silence after, for the tail.
Stdlib only, deterministic (fixed seed and phases): every run is bit-identical.
Output: 48 kHz, 24-bit, mono, next to the other stimulus in test_audio/stimulus/
(tools/make_stimulus.py, whose helpers this reuses).
"""

import math
import random

from make_stimulus import SR, normalise, silence, write

C2, C3, EB3, G3 = 65.406, 130.813, 155.563, 195.998


def cents(c):
    return 2 ** (c / 1200)


def two_pole_lp(x, fc, q=0.707):
    """RBJ biquad low-pass (12 dB/oct): a synth-ish filter on the pad."""
    w = 2 * math.pi * fc / SR
    alpha = math.sin(w) / (2 * q)
    cw = math.cos(w)
    a0 = 1 + alpha
    b0 = b2 = (1 - cw) / 2 / a0
    b1 = (1 - cw) / a0
    a1 = -2 * cw / a0
    a2 = (1 - alpha) / a0
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for s in x:
        y = b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, s, y1, y
        out.append(y)
    return out


def envelope(n_attack, n_hold, n_release, shape="smooth"):
    """Attack / hold / release, raised-cosine ramps (a slow swell, no click)."""
    env = []
    for i in range(n_attack):
        t = i / n_attack
        env.append(0.5 - 0.5 * math.cos(math.pi * t) if shape == "smooth" else t)
    env += [1.0] * n_hold
    for i in range(n_release):
        t = i / n_release
        env.append(0.5 + 0.5 * math.cos(math.pi * t) if shape == "smooth" else 1 - t)
    return env


def pad():
    rng = random.Random(10)
    env = envelope(int(3 * SR), int(6 * SR), int(3 * SR))
    n = len(env)
    # Three saws per note, +-7 cents; C2 a bit louder (the pad's weight is low).
    voices = []
    for f, amp in ((C2, 1.0), (C3, 0.8), (EB3, 0.7), (G3, 0.7)):
        for c in (-7.0, 0.0, 7.0):
            voices.append((f * cents(c), amp / 3, rng.random()))
    x = [0.0] * n
    for f, amp, ph in voices:
        inc = f / SR
        p = ph
        for i in range(n):
            x[i] += amp * (2 * p - 1)
            p += inc
            if p >= 1.0:
                p -= 1.0
    x = two_pole_lp(two_pole_lp(x, 700), 700)  # 24 dB/oct at ~700 Hz
    x = [s * e for s, e in zip(x, env)]
    return silence(1) + normalise(x, -6) + silence(6)


def drone():
    env = envelope(int(1 * SR), int(10 * SR), int(1 * SR))
    x = [e * (math.sin(2 * math.pi * C2 * i / SR) + 0.5 * math.sin(2 * math.pi * 2 * C2 * i / SR + 0.3))
         for i, e in enumerate(env)]
    return silence(1) + normalise(x, -6) + silence(6)


def organ():
    env = envelope(int(0.02 * SR), int(8 * SR), int(0.05 * SR), shape="linear")
    drawbars = ((1, 1.0), (2, 0.8), (3, 0.6), (4, 0.5), (6, 0.3), (8, 0.25))
    notes = ((C2, 0.8), (C3, 1.0), (EB3, 1.0), (G3, 1.0))
    parts = [(f * h, a * d, 0.37 * k) for k, ((f, a), (h, d)) in
             enumerate((nt, db) for nt in notes for db in drawbars)]
    x = []
    for i, e in enumerate(env):
        t = i / SR
        x.append(e * sum(a * math.sin(2 * math.pi * f * t + ph) for f, a, ph in parts))
    return silence(1) + normalise(x, -6) + silence(6)


if __name__ == "__main__":
    write("10_pad_cminor.wav", pad())
    write("11_drone_c2.wav", drone())
    write("12_organ_chord.wav", organ())
