#!/usr/bin/env python3
"""Generate the test stimulus WAVs used for Wellspring reference recordings
(docs/recording-recipe.md) and later by the Renderer.

Stdlib only. Deterministic: fixed seed, so every run is bit-identical.
Output: 48 kHz, 24-bit, mono WAVs in test_audio/stimulus/.
"""

import math
import random
import struct
import wave
from pathlib import Path

SR = 48000
OUT = Path(__file__).resolve().parent.parent / "test_audio" / "stimulus"


def db(x):
    return 10 ** (x / 20)


def write(name, samples):
    OUT.mkdir(parents=True, exist_ok=True)
    frames = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        frames += struct.pack("<i", int(round(v * 8388607)))[:3]
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(SR)
        w.writeframes(bytes(frames))
    print(f"{name}: {len(samples) / SR:.1f} s")


def silence(sec):
    return [0.0] * int(sec * SR)


def one_pole_lp(x, fc):
    a = math.exp(-2 * math.pi * fc / SR)
    y, out = 0.0, []
    for s in x:
        y = (1 - a) * s + a * y
        out.append(y)
    return out


def one_pole_hp(x, fc):
    lp = one_pole_lp(x, fc)
    return [s - l for s, l in zip(x, lp)]


def normalise(x, peak_db):
    p = max(abs(s) for s in x) or 1.0
    g = db(peak_db) / p
    return [s * g for s in x]


def clicks():
    """6 clicks, 8 s apart. Short 2-sample pulse survives interface paths."""
    out = silence(1)
    for _ in range(6):
        out += [db(-6), db(-6)] + silence(8)[2:]
    return out


def click_single():
    """One click, then 12 s of silence. Short input for DECAY x TENSION grids."""
    return silence(1) + [db(-6), db(-6)] + silence(12)[2:]


def held_tones():
    """1 kHz sine for 8 s, 1 s gap, then a held A minor chord for 8 s, then 2 s.
    Sustained tones make pitch wobble (wow & flutter, WOBBLE) measurable."""
    out = silence(1)
    n = int(8 * SR)
    fade = int(0.01 * SR)
    env = [min(1.0, i / fade, (n - 1 - i) / fade) for i in range(n)]
    out += [db(-12) * math.sin(2 * math.pi * 1000 * i / SR) * env[i] for i in range(n)]
    out += silence(1)
    chord = (220.0, 261.63, 329.63)
    out += [db(-12) * env[i] * sum(math.sin(2 * math.pi * f * i / SR) for f in chord) / 3 for i in range(n)]
    return out + silence(2)


def sweep(level_db=-12.0):
    """Exponential sine sweep 20 Hz-20 kHz, 10 s (Farina), 12 s silence after.
    level_db: the sweep's peak (13_sweep_quiet / 14_sweep_hot: level series for
    the reference session 2, how the tank's input stage changes with level)."""
    f1, f2, T = 20.0, 20000.0, 10.0
    n = int(T * SR)
    k = math.log(f2 / f1)
    fade = int(0.05 * SR)
    out = []
    for i in range(n):
        t = i / SR
        s = math.sin(2 * math.pi * f1 * T / k * (math.exp(t * k / T) - 1))
        env = min(1.0, i / fade, (n - 1 - i) / fade)
        out.append(db(level_db) * s * env)
    return silence(1) + out + silence(12)


def snare(rng):
    n = int(0.25 * SR)
    noise = [rng.uniform(-1, 1) for _ in range(n)]
    noise = one_pole_hp(one_pole_lp(noise, 7000), 800)
    out = []
    for i in range(n):
        t = i / SR
        body = math.sin(2 * math.pi * 185 * t) * math.exp(-t / 0.03)
        out.append(0.6 * body + 1.2 * noise[i] * math.exp(-t / 0.06))
    return normalise(out, 0)


def rim(rng):
    n = int(0.06 * SR)
    noise = one_pole_hp([rng.uniform(-1, 1) for _ in range(n)], 2000)
    out = []
    for i in range(n):
        t = i / SR
        tone = math.sin(2 * math.pi * 1700 * t) + 0.5 * math.sin(2 * math.pi * 480 * t)
        out.append((tone + 0.7 * noise[i]) * math.exp(-t / 0.008))
    return normalise(out, 0)


def hits():
    """Snare at -6/-12/-18 dBFS, then rim at the same levels. 6 s apart."""
    rng = random.Random(1)
    out = silence(1)
    for voice in (snare, rim):
        for level in (-6, -12, -18):
            h = [s * db(level) for s in voice(rng)]
            out += h + silence(6 - len(h) / SR)
    return out


def skank():
    """Offbeat Am-D chord stabs, 75 bpm, 4 bars, then 10 s of tail."""
    bpm, bars = 75, 4
    beat = 60 / bpm
    chords = [[220.0, 261.63, 329.63], [293.66, 369.99, 440.0]]  # Am, D
    total = int((bars * 4 * beat + 10) * SR)
    out = [0.0] * total
    for bar in range(bars):
        chord = chords[bar % 2]
        for b in range(4):
            start = int(((bar * 4 + b) * beat + beat / 2) * SR)
            n = int(0.12 * SR)
            for i in range(n):
                t = i / SR
                env = math.exp(-t / 0.035)
                s = sum(2 * ((f * t) % 1) - 1 for f in chord) / 3
                out[start + i] += s * env
    out = one_pole_lp(one_pole_lp(out, 2500), 2500)
    return silence(1) + normalise(out, -6)


def noise_bursts():
    """Pink-ish noise bursts (50 ms and 300 ms), 10 s apart. For provoking Ringing."""
    rng = random.Random(2)
    out = silence(1)
    for dur in (0.05, 0.3, 0.05, 0.3):
        n = int(dur * SR)
        b = one_pole_lp([rng.uniform(-1, 1) for _ in range(n)], 3000)
        fade = int(0.002 * SR)
        b = [s * min(1.0, i / fade, (n - 1 - i) / fade) for i, s in enumerate(b)]
        out += normalise(b, -6) + silence(10)
    return out


def pink_noise():
    """Steady pink noise, 20 s at -18 dBFS RMS (peaks about -6), 10 ms fades.
    For the M0 passthrough-vs-cable level and hiss check (docs/m0-hardware-check.md
    step 10). Paul Kellet's refined pink filter on white noise: within about
    0.05 dB of -3 dB/octave from 10 Hz to Nyquist at 48 kHz."""
    rng = random.Random(9)
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0
    out = []
    for _ in range(int(20 * SR)):
        w = rng.uniform(-1, 1)
        b0 = 0.99886 * b0 + w * 0.0555179
        b1 = 0.99332 * b1 + w * 0.0750759
        b2 = 0.96900 * b2 + w * 0.1538520
        b3 = 0.86650 * b3 + w * 0.3104856
        b4 = 0.55000 * b4 + w * 0.5329522
        b5 = -0.7616 * b5 - w * 0.0168980
        out.append(b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362)
        b6 = w * 0.115926
    rms = math.sqrt(sum(v * v for v in out) / len(out))
    out = [v * db(-18) / rms for v in out]
    fade = int(0.01 * SR)
    n = len(out)
    out = [v * min(1.0, i / fade, (n - 1 - i) / fade) for i, v in enumerate(out)]
    return silence(1) + out + silence(1)



def tone_bursts():
    """One short sine burst per octave, 125 Hz-8 kHz (60 ms, Hann), -12 dBFS,
    5 s apart, two passes: how each band decays and darkens per repeat, and
    where a tank's metallic modes sit (reference session 2, take J)."""
    out = silence(1)
    n = int(0.06 * SR)
    for _ in range(2):
        for f in (125, 250, 500, 1000, 2000, 4000, 8000):
            out += [db(-12) * math.sin(2 * math.pi * f * i / SR) * 0.5 * (1 - math.cos(2 * math.pi * i / (n - 1)))
                    for i in range(n)]
            out += silence(5)
    return out


if __name__ == "__main__":
    write("01_clicks.wav", clicks())
    write("02_hits.wav", hits())
    write("03_sweep.wav", sweep())
    write("04_skank.wav", skank())
    write("05_silence_for_kicks.wav", silence(40))
    write("06_noise_bursts.wav", noise_bursts())
    write("07_click_single.wav", click_single())
    write("08_held_tones.wav", held_tones())
    write("09_pink_noise.wav", pink_noise())
    # Reference session 2 (docs/recording-recipe.md "Session 2"): level series,
    # octave bursts, a noise-floor take. (10-12 are the sustain stimulus:
    # tools/make_sustain_stimulus.py.)
    write("13_sweep_quiet.wav", sweep(-30.0))
    write("14_sweep_hot.wav", sweep(-3.0))
    write("15_tone_bursts.wav", tone_bursts())
    write("16_silence_30s.wav", silence(30))
