#!/usr/bin/env python3
"""Demos round 2 (docs/minisite/demos-plan.md): build the dry, musical sources.

Usage: python3 build_sources.py <demo_sources dir> <out dir>
Needs numpy and soundfile. Samples come from test_audio/demo_sources/ (never in git):
  cc0/rusty/…       Karoryfer Big Rusty Drums, close mics (CC0)
  cc0/virtuosity/…  Virtuosity Drums, close mics (CC0)
  cc0/wurli/…       Greg Sullivan Wurlitzer EP200 (CC BY 3.0: credited on the site)
Writes 48 kHz stereo WAVs peaking at -6 dBFS:
  groove_rusty.wav, groove_virtuosity.wav   clip 1, a one-drop at 75 bpm, 6 bars
  wurli.wav                                 clip 6, slow C minor chords
  siren.wav                                 clip 8, a dub siren (synthesised)
"""
import random
import re
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
rng = random.Random(7)  # fixed: the same sources every run


def load(path: Path) -> np.ndarray:
    x, sr = sf.read(str(path), always_2d=True)
    x = x.mean(axis=1)
    if sr != SR:  # linear resample is fine for one-shots at these rates
        n = int(round(len(x) * SR / sr))
        x = np.interp(np.linspace(0, len(x) - 1, n), np.arange(len(x)), x)
    return x


def place(buf: np.ndarray, x: np.ndarray, t: float, gain: float, pan: float = 0.0):
    i = max(0, int(round(t * SR)))
    if i >= len(buf):
        return
    x = x[: len(buf) - i] * gain
    l, r = np.cos((pan + 1) * np.pi / 4), np.sin((pan + 1) * np.pi / 4)
    buf[i : i + len(x), 0] += x * l * np.sqrt(2)
    buf[i : i + len(x), 1] += x * r * np.sqrt(2)


def finish(buf: np.ndarray, out: Path, peak_db: float = -6.0):
    buf = buf / max(1e-9, np.abs(buf).max()) * 10 ** (peak_db / 20)
    sf.write(str(out), buf.astype(np.float32), SR, subtype="PCM_24")
    print(f"wrote {out.name}: {len(buf) / SR:.1f} s")


# ---- Clip 1: a one-drop groove ------------------------------------------------
# 75 bpm, 6 bars. Kick and rimshot on beat 3, cross-stick ghosts, closed hats on a
# light shuffle. Each hit is a different round-robin sample and a slightly different
# time and level, so it doesn't sound like a machine.

class Kit:
    def __init__(self, root: Path, pieces: dict[str, str]):
        self.files = {k: sorted((root / v).glob("*.flac")) + sorted((root / v).glob("*.wav")) for k, v in pieces.items()}
        self.cache: dict[Path, np.ndarray] = {}
        for k, fs in self.files.items():
            if not fs:
                raise SystemExit(f"no samples for {k} in {root}")

    def hit(self, piece: str, vel: float) -> np.ndarray:
        """A sample for this velocity (0-1): files sort by velocity layer, then round robin."""
        fs = self.files[piece]
        layers = sorted({re.search(r"v[l]?(\d+)", f.name).group(1) if re.search(r"v[l]?(\d+)", f.name) else "1" for f in fs}, key=int)
        want = layers[min(len(layers) - 1, int(vel * len(layers)))]
        pick = [f for f in fs if re.search(rf"v[l]?{want}(?!\d)", f.name)] or fs
        f = rng.choice(pick)
        if f not in self.cache:
            x = load(f)
            self.cache[f] = x / max(1e-9, np.abs(x).max())  # the layer sets the tone; the groove sets the level
        return self.cache[f] * (0.35 + 0.65 * vel)


def groove(kit: Kit, out: Path, bpm=75.0, bars=6, tail=7.0):
    beat = 60 / bpm
    buf = np.zeros((int((bars * 4 * beat + tail) * SR), 2))
    jit = lambda: rng.gauss(0, 0.004)
    for bar in range(bars):
        t0 = bar * 4 * beat
        # hats: eighths, the off-eighth late (shuffle), accents on the beat
        for e in range(8):
            t = t0 + e * beat / 2 + (0.06 * beat if e % 2 else 0)
            v = (0.55 if e % 2 == 0 else 0.35) + rng.uniform(-0.05, 0.05)
            place(buf, kit.hit("hh", v), t + jit(), 0.32 * v / 0.55, pan=0.35)
        # the drop: kick and rimshot together on beat 3
        place(buf, kit.hit("kick", 0.8), t0 + 2 * beat + jit(), 0.8, pan=0.0)
        place(buf, kit.hit("rim", 0.85), t0 + 2 * beat + jit(), 0.7, pan=-0.1)
        # cross-stick ghosts: on 4-and, and a pickup into the next bar on odd bars
        place(buf, kit.hit("xstick", 0.4), t0 + 3.5 * beat + 0.06 * beat + jit(), 0.22, pan=-0.1)
        if bar % 2 == 1:
            place(buf, kit.hit("xstick", 0.3), t0 + 1.5 * beat + 0.06 * beat + jit(), 0.16, pan=-0.1)
    finish(buf, out)


# ---- Clip 6: Wurlitzer chords ------------------------------------------------------

def sfz_regions(sfz: Path):
    regs = []
    for line in sfz.read_text().splitlines():
        if not line.startswith("<region>"):
            continue
        kv = dict(re.findall(r"(\w+)=([^\s]+)", line))
        regs.append(kv)
    return regs


def wurli(root: Path, out: Path):
    regs = [r for r in sfz_regions(root / "wurli.sfz") if r["sample"].endswith("mp.$EXT")] or sfz_regions(root / "wurli.sfz")
    cache: dict[str, np.ndarray] = {}

    def note(midi: int) -> np.ndarray:
        r = next((r for r in regs if int(r["lokey"]) <= midi <= int(r["hikey"])), regs[-1])
        name = r["sample"].replace("$EXT", "flac")
        if name not in cache:
            cache[name] = load(root / "Samples" / name)
        x = cache[name]
        ratio = 2 ** ((midi - int(r["pitch_keycenter"]) + float(r.get("tune", 0)) / 100) / 12)
        n = int(len(x) / ratio)
        return np.interp(np.arange(n) * ratio, np.arange(len(x)), x) * 10 ** (float(r.get("volume", 0)) / 20)

    bpm, beat = 66.0, 60 / 66.0
    # C minor: Cm9, Abmaj7, Fm9, Gsus4 -> G7(b9) on the last bar; held a bar each, rolled a touch
    chords = [[48, 55, 58, 62, 63], [44, 55, 60, 63, 67], [41, 56, 60, 63, 67], [43, 55, 60, 62, 65]]
    buf = np.zeros((int((len(chords) * 4 * beat + 8) * SR), 2))
    for i, ch in enumerate(chords):
        t0 = i * 4 * beat
        for j, m in enumerate(ch):
            x = note(m)
            x = x[: int(3.7 * beat * SR)]
            x[-int(0.15 * SR):] *= np.linspace(1, 0, int(0.15 * SR))  # release
            place(buf, x, t0 + j * 0.018 + rng.uniform(0, 0.006), 0.55 if j else 0.7, pan=(j - 2) * 0.12)
        # a second, softer hit on beat 3 of every other bar keeps it moving
        if i % 2:
            for j, m in enumerate(ch[1:]):
                x = note(m)[: int(1.7 * beat * SR)]
                x[-int(0.12 * SR):] *= np.linspace(1, 0, int(0.12 * SR))
                place(buf, x, t0 + 2 * beat + j * 0.012, 0.35, pan=(j - 1.5) * 0.12)
    finish(buf, out)


# ---- Clip 8: a dub siren ----------------------------------------------------------
# The sound-system siren: a square-ish oscillator swept by a slow triangle LFO, played
# as short blips, then one long rising wail.

def siren(out: Path):
    dur, tail = 6.0, 12.0
    t = np.arange(int((dur + tail) * SR)) / SR
    gate = np.zeros_like(t)
    for s, e in [(0.0, 0.35), (0.5, 0.85), (1.0, 1.35), (2.0, 5.0)]:
        g = (t >= s) & (t < e)
        gate[g] = 1.0
    gate = np.convolve(gate, np.ones(240) / 240, mode="same")  # 5 ms edges, no clicks
    lfo_rate = np.where(t < 2.0, 6.0, 1.2)
    lfo = 2 * np.abs(((np.cumsum(lfo_rate) / SR) % 1.0) - 0.5)  # triangle 0..1
    rise = np.clip((t - 2.0) / 3.0, 0, 1)
    freq = 420 * 2 ** (lfo * 1.0 + rise * 0.8)
    phase = 2 * np.pi * np.cumsum(freq) / SR
    tone = np.tanh(2.5 * np.sin(phase)) * 0.8 + 0.2 * np.sin(2 * phase)
    x = tone * gate
    buf = np.stack([x, x], axis=1)
    finish(buf, out)


def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    cc0 = src / "cc0"
    groove(Kit(cc0 / "rusty", {"kick": "kick", "rim": "rimshot", "xstick": "sidestick", "hh": "hh_closed"}), out / "groove_rusty.wav")
    vd = cc0 / "virtuosity"
    groove(Kit(vd, {"kick": "kick", "rim": "snare_rim", "xstick": "snare_xs", "hh": "hh"}), out / "groove_virtuosity.wav")
    wurli(cc0 / "wurli", out / "wurli.wav")
    siren(out / "siren.wav")


if __name__ == "__main__":
    main()
