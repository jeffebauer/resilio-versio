#!/usr/bin/env python3
"""Demos round 2 (docs/minisite/demos-plan.md): build the dry, musical sources.

Usage: python3 build_sources.py <demo_sources dir> <out dir> [clip ids…]
Needs numpy and soundfile. Samples come from test_audio/demo_sources/ (never in git):
  cc0/virtuosity/…      Virtuosity Drums (Versilian Studios, Karoryfer), close mics
  cc0/bass_babyblue/…   Karoryfer Baby Blue electric bass (5-string, names an octave high)
  audition/work/o1.tar.xz.x/DrawbarOrganEmulation-SFZ-*/  FreePats Drawbar Organ (.sfz keymap)
  cc0/guitar_emily/…    Karoryfer Emilyguitar (Epiphone, clean)
  cc0/trombone_short/…  VSCO 2 CE trombone, short notes (names an octave low)
  cc0/clav_fm/…         VCSL TX81Z Clavisynth (names two octaves low)
  cc0/vibes_soft/…      VCSL vibraphone, soft mallets (names an octave low)
  cc0/dantranh/…        VCSL Dan Tranh (names an octave low)
  cc0/bongos/…          VCSL bongos;  cc0/cowbell_808/CB.WAV  TR-808 cowbell
The octave offsets were measured with a pitch tracker; OFFSETS below holds them.

Writes 48 kHz stereo WAVs peaking at -6 dBFS, one per clip (01_tubby.wav … 09_siren.wav),
plus cues.json: the button (throw) windows for clips 01 and 08, taken from the actual hit
times, which render.py passes to the Renderer. Each source starts after LEAD seconds of
silence so the first button press (which switches throw mode on) hears nothing.

Every part is built on its own bus, so the level table printed at the end shows each
part's peak and its RMS while playing. Parts are humanised (a few ms of timing, a little
velocity), round robins rotate, and hits are peak-normalised before the part sets the level.
"""
import json
import random
import re
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
LEAD = 0.25
rng = random.Random(11)  # fixed: the same sources every run

# measured octave offsets: true MIDI note = named note + offset (C4 = 60)
OFFSETS = {"bass": -12, "guitar": 0, "trombone": 12, "vibes": 12, "clav": 24, "dantranh": 12}
PC = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def name_midi(s: str) -> int | None:
    m = re.search(r"(?:^|_)([A-Ga-g])([#b]?)(\d)(?=_|\.)", s)
    if not m:
        return None
    acc = {"#": 1, "b": -1, "": 0}[m.group(2)]
    return 12 * (int(m.group(3)) + 1) + PC[m.group(1).lower()] + acc


_cache: dict = {}


def load(path: Path) -> np.ndarray:
    """Mono, 48 kHz, leading silence trimmed (3 ms kept before the onset)."""
    if path in _cache:
        return _cache[path]
    x, sr = sf.read(str(path), always_2d=True)
    x = x.mean(axis=1)
    if sr != SR:
        n = int(round(len(x) * SR / sr))
        x = np.interp(np.linspace(0, len(x) - 1, n), np.arange(len(x)), x)
    a = np.abs(x)
    on = int(np.argmax(a > 0.02 * a.max()))
    x = x[max(0, on - int(0.003 * SR)):]
    _cache[path] = x
    return x


def repitch(x: np.ndarray, semis: float) -> np.ndarray:
    if abs(semis) < 1e-6:
        return x
    r = 2 ** (semis / 12)
    n = int(len(x) / r)
    return np.interp(np.arange(n) * r, np.arange(len(x)), x)


def bend(x: np.ndarray, start: float, ramp: float, semis: float) -> np.ndarray:
    """A string bend: pitch rises by `semis` from `start` s over `ramp` s, then holds."""
    t = np.arange(len(x)) / SR
    rate = 2 ** (semis / 12 * np.clip((t - start) / ramp, 0, 1))
    pos = np.cumsum(rate)
    pos = pos[pos < len(x) - 1]
    return np.interp(pos, np.arange(len(x)), x)


def env(x: np.ndarray, length: float | None, release: float = 0.04, attack: float = 0.0) -> np.ndarray:
    x = x.copy()
    if attack > 0:
        a = int(attack * SR)
        x[:a] *= np.linspace(0, 1, a)
    if length is not None:
        n, r = int(length * SR), int(release * SR)
        x = x[: n + r]
        if len(x) > n:
            x[n:] *= np.linspace(1, 0, len(x) - n) ** 2
    return x


def hpf(x: np.ndarray, hz: float) -> np.ndarray:
    a = np.exp(-2 * np.pi * hz / SR)
    y = np.empty_like(x)
    prev_x = prev_y = 0.0
    for i, v in enumerate(x):  # short one-shots only
        prev_y = a * (prev_y + v - prev_x)
        prev_x = v
        y[i] = prev_y
    return y


class Bank:
    """Pitched samples: nearest sampled note, resampled; round robins rotate."""

    def __init__(self, items: list[tuple[int, Path]], norm: str = "rms"):
        self.by: dict[int, list[Path]] = {}
        for m, p in items:
            self.by.setdefault(m, []).append(p)
        self.norm = norm
        self.last: dict[int, Path] = {}
        if not self.by:
            raise SystemExit("empty sample bank")

    def note(self, midi: int) -> np.ndarray:
        m0 = min(self.by, key=lambda k: (abs(k - midi), k < midi))  # tie: shift down from above
        fs = self.by[m0]
        pick = [f for f in fs if f != self.last.get(m0)] or fs
        f = rng.choice(pick)
        self.last[m0] = f
        x = load(f)
        if self.norm == "rms":
            seg = x[: int(0.3 * SR)]
            x = x / max(1e-9, np.sqrt(np.mean(seg ** 2))) * 0.25
        else:
            x = x / max(1e-9, np.abs(x).max())
        return repitch(x, midi - m0)


def bank_from(folder: Path, kind: str, pattern: str = "*.wav", norm: str = "rms") -> Bank:
    items = []
    for p in sorted(folder.glob(pattern)):
        m = name_midi(p.name)
        if m is not None:
            items.append((m + OFFSETS[kind], p))
    return Bank(items, norm)


def organ_bank(root: Path) -> Bank:
    sfz = next(root.glob("*.sfz"))
    items, cur = [], {}
    for line in sfz.read_text().splitlines() + ["<region>"]:
        line = line.strip()
        if line.startswith("<region>"):
            if "sample" in cur:
                items.append((int(cur["pitch_keycenter"]), root / cur["sample"]))
            cur = {}
        elif "=" in line and not line.startswith("//"):
            k, v = line.split("=", 1)
            cur[k.strip()] = v.strip()
    return Bank(items, norm="rms")


class Kit:
    """Drum pieces: velocity picks the layer (the tone), the caller sets the level."""

    def __init__(self, pieces: dict[str, list[Path]]):
        self.pieces = pieces
        self.last: dict[str, Path] = {}
        for k, fs in pieces.items():
            if not fs:
                raise SystemExit(f"no samples for {k}")

    def hit(self, piece: str, vel: float) -> np.ndarray:
        fs = self.pieces[piece]
        lay = lambda f: int(m.group(1)) if (m := re.search(r"_v[l]?(\d+)", f.name)) else 1
        layers = sorted({lay(f) for f in fs})
        want = layers[min(len(layers) - 1, max(0, int(round(vel * (len(layers) - 1)))))]
        pick = [f for f in fs if lay(f) == want]
        pick = [f for f in pick if f != self.last.get(piece)] or pick
        f = rng.choice(pick)
        self.last[piece] = f
        x = load(f)
        return x / max(1e-9, np.abs(x).max())


class Mix:
    def __init__(self, dur: float):
        self.n = int(dur * SR)
        self.parts: dict[str, np.ndarray] = {}

    def add(self, part: str, x: np.ndarray, t: float, gain: float, pan: float = 0.0):
        buf = self.parts.setdefault(part, np.zeros((self.n, 2)))
        i = max(0, int(round(t * SR)))
        if i >= self.n:
            return
        x = x[: self.n - i] * gain
        l, r = np.cos((pan + 1) * np.pi / 4), np.sin((pan + 1) * np.pi / 4)
        buf[i: i + len(x), 0] += x * l * np.sqrt(2)
        buf[i: i + len(x), 1] += x * r * np.sqrt(2)

    def finish(self, out: Path, peak_db: float = -6.0) -> list[tuple]:
        total = sum(self.parts.values())
        g = 10 ** (peak_db / 20) / max(1e-9, np.abs(total).max())
        sf.write(str(out), (total * g).astype(np.float32), SR, subtype="PCM_24")
        rows = []
        for name, b in self.parts.items():
            b = b * g
            mono = b.mean(axis=1)
            w = int(0.05 * SR)
            win = np.sqrt(np.mean(mono[: len(mono) // w * w].reshape(-1, w) ** 2, axis=1))
            act = win[win > win.max() * 10 ** (-30 / 20)]  # windows where the part is sounding
            rows.append((out.name, name, 20 * np.log10(np.abs(b).max() + 1e-12),
                         20 * np.log10(np.sqrt(np.mean(act ** 2)) + 1e-12)))
        print(f"wrote {out.name}: {len(total) / SR:.1f} s")
        return rows


def hum(sd_ms: float = 4.0) -> float:
    return rng.gauss(0, sd_ms / 1000)


def vary(v: float, amt: float = 0.06) -> float:
    return min(1.0, max(0.05, v + rng.uniform(-amt, amt)))


def swing8(b: float, amt: float = 2 / 3) -> float:
    """Swing the offbeat eighths: x.5 moves to x+amt (2/3 is a triplet feel)."""
    i, f = divmod(b, 1.0)
    return i + amt if abs(f - 0.5) < 1e-6 else b


def swing16(b: float, amt: float = 0.06) -> float:
    """Push the second and fourth sixteenths late by `amt` beats."""
    f = b % 0.5
    return b + amt if abs(f - 0.25) < 1e-6 else b


def db(x: float) -> float:
    return 10 ** (x / 20)


# ---- 01 Tubby throw and sweep: a one-drop with a roots bassline ---------------------
# 76 bpm, A minor, 8 bars and a final stab. Hats in a triplet swing with open-hat lifts,
# kick and rimshot on beat 3, cross-stick ghosts, tom fills into bars 4 and 8.

def kit_b(vd: Path) -> Kit:
    g = lambda d, s="": sorted(f for f in (vd / d).glob("*.flac") if s in f.name)
    return Kit({"kick": g("kick"), "rim": g("snare_rim"), "xs": g("snare_xs"), "hh": g("hh"),
                "hh_open": g("hh_open", "_open_"), "hh_half": g("hh_open", "_half_"),
                "tom_hi": g("tom_hi", "center"), "tom_lo": g("tom_lo", "_center_")})


def clip01(src: Path, out: Path, cues: dict):
    bpm = 76.0
    spb = 60 / bpm
    T = lambda bar, b: LEAD + (bar * 4 + swing8(b)) * spb
    kit = kit_b(src / "cc0" / "virtuosity")
    bass = bank_from(src / "cc0" / "bass_babyblue", "bass")
    m = Mix(LEAD + 33 * spb + 4.5)
    L = {"kick": db(-3), "rim": db(-1), "xs": db(-11), "hh": db(-5), "hh_open": db(-8),
         "tom": db(-6), "bass": db(-1)}
    rims = {}

    # hats: (bar, beat) -> kind; triplets on beat 4 of bars 3 and 7, lifts into bars 2, 4 and 6
    hats = []
    for bar in range(8):
        for e in range(8):
            b = e / 2
            if bar in (3, 7) and b >= 2.5:   # the fill takes the end of bars 4 and 8
                continue
            if bar in (2, 6) and b >= 3:
                continue
            kind = "hh_open" if (bar, e) in {(0, 7), (2, 3), (4, 7), (5, 3)} else "hh"
            v = (0.7 if e % 2 == 0 else 0.45) + (0.1 if e in (0, 4) else 0)
            hats.append((T(bar, b), kind, v))
        if bar in (2, 6):
            for k, b in enumerate((3, 3 + 1 / 3, 3 + 2 / 3)):
                hats.append((LEAD + (bar * 4 + b) * spb, "hh_open" if (bar == 6 and k == 2) else "hh", (0.65, 0.4, 0.5)[k]))
    hats.append((T(8, 0), "hh_open", 0.8))
    hats.sort()
    for i, (t, kind, v) in enumerate(hats):
        x = kit.hit(kind, vary(v))
        if kind == "hh_open" and i + 1 < len(hats):  # the next closed hat chokes it
            x = env(x, hats[i + 1][0] - t, 0.02)
        g = L[kind] * (0.45 + 0.55 * v)
        m.add("hats", x, t + hum(3), g, pan=0.3)

    # the drop: kick and rimshot on beat 3; a final stab on bar 9
    for bar in range(9):
        t = T(bar, 2 if bar < 8 else 0)
        tk = t + hum(3)
        m.add("kick", kit.hit("kick", vary(0.85)), tk, L["kick"])
        m.add("rim", kit.hit("rim", vary(0.85, 0.08)), t + hum(3), L["rim"], pan=-0.1)
        rims[bar] = t

    # cross-stick ghosts: the swung "and" of 4, and a "let" before the drop on bars 2 and 6
    for bar in range(8):
        if bar not in (3, 7):
            m.add("xs", kit.hit("xs", vary(0.35)), LEAD + (bar * 4 + 3 + 2 / 3) * spb + hum(4), L["xs"], pan=-0.1)
        if bar in (1, 5):
            m.add("xs", kit.hit("xs", vary(0.25)), LEAD + (bar * 4 + 1 + 2 / 3) * spb + hum(4), L["xs"] * 0.8, pan=-0.1)
        if bar == 4:
            m.add("xs", kit.hit("xs", vary(0.3)), T(bar, 0.5) + hum(4), L["xs"] * 0.8, pan=-0.1)

    # fills: triplets on beat 4 of bar 4; a longer one from the drop of bar 8
    fills = [(3, 3, "tom_hi", 0.6), (3, 3 + 1 / 3, "tom_hi", 0.45), (3, 3 + 2 / 3, "tom_lo", 0.7),
             (7, 2 + 1 / 3, "tom_hi", 0.45), (7, 2 + 2 / 3, "tom_hi", 0.6), (7, 3, "tom_lo", 0.7),
             (7, 3 + 1 / 3, "tom_lo", 0.6), (7, 3 + 2 / 3, "rim", 0.75)]
    for bar, b, piece, v in fills:
        part = "rim" if piece == "rim" else "toms"
        gain = L["rim"] * 0.8 if piece == "rim" else L["tom"] * (0.6 + 0.4 * v)
        m.add(part, kit.hit(piece, vary(v)), LEAD + (bar * 4 + b) * spb + hum(4), gain,
              pan={"tom_hi": 0.2, "tom_lo": -0.25, "rim": -0.1}[piece])

    # the bassline: two-bar phrases, rests across the thrown drops, out for the last fill
    A1, B1, C2, D2, E2, F2, G2, A2, G1, C3 = 33, 35, 36, 38, 40, 41, 43, 45, 31, 48
    line = {
        0: [(0, A1, 1.25, .9), (1.5, E2, .4, .7), (2, G2, .45, .8), (2.5, E2, .4, .65), (3.5, D2, .4, .7)],
        1: [(0, A1, 1.4, .95), (1.5, C2, .35, .7), (3, G1, .4, .75), (3.5, A1, .3, .65)],
        2: [(0, D2, 1.25, .9), (1.5, F2, .4, .7), (2, A2, .45, .8), (2.5, G2, .4, .7), (3, F2, .45, .75), (3.5, E2, .35, .65)],
        3: [(0, E2, 1.0, .9), (1, D2, .4, .7), (1.5, C2, .4, .75)],
        4: [(0, A1, 1.25, .9), (1.5, E2, .4, .7), (2, A2, .35, .8), (2.5, G2, .35, .7), (3, E2, .4, .75), (3.5, D2, .4, .7)],
        5: [(0, C2, 1.0, .9), (1, A1, .45, .75), (1.5, E2, .4, .7), (3, G1, .4, .75), (3.5, A1, .3, .65)],
        6: [(0, D2, 1.25, .9), (1.5, F2, .4, .7), (2, A2, .45, .8), (2.5, C3, .35, .7), (3, A2, .4, .75), (3.5, G2, .35, .7)],
        7: [(0, E2, 1.25, .9), (1.5, G2, .4, .75)],
        8: [(0, A1, 2.5, 1.0)],
    }
    for bar, notes in line.items():
        for b, n, ln, v in notes:
            x = env(bass.note(n), ln * spb, 0.06)
            m.add("bass", x, T(bar, b) + hum(5), L["bass"] * vary(v, 0.05))

    # throws: the drops of bars 2, 4, 6 and 7, then the whole last fill and the final stab
    btn = [[0.0, 0.02]] + [[round(rims[b] - 0.03, 3), round(rims[b] + 0.25, 3)] for b in (1, 3, 5, 6)]
    btn.append([round(rims[7] - 0.03, 3), round(rims[8] + 0.35, 3)])
    cues["01"] = {"buttons": btn, "bars": {"7": round(T(6, 0), 3), "8": round(T(7, 0), 3), "end": round(T(8, 0), 3)}}
    return m.finish(out / "01_tubby.wav")


# ---- 02 Organ bubble into echo -------------------------------------------------------
# 76 bpm, Am7 to D9. The bubble: left hand on the 2nd and 4th sixteenths (shuffled), right
# hand on the offbeat eighths; guitar chops on 2 and 4. Bar 8 drops out so the echo answers.

def clip02(src: Path, out: Path, cues: dict):
    bpm = 76.0
    spb = 60 / bpm
    organ = organ_bank(next((src / "audition" / "work" / "o1.tar.xz.x").glob("DrawbarOrgan*")))
    gtr = bank_from(src / "cc0" / "guitar_emily", "guitar")
    m = Mix(LEAD + 32 * spb + 4.5)
    T = lambda bar, b: LEAD + (bar * 4 + swing16(b, 0.05)) * spb
    chords = {"Am7": ([57, 60, 64], [67, 69, 72, 76], [69, 72, 76]),
              "D9": ([54, 57, 62], [64, 66, 69, 72], [66, 69, 72, 76])}
    prog = ["Am7", "Am7", "D9", "D9", "Am7", "Am7", "D9", "Am7"]
    for bar, ch in enumerate(prog):
        lh, rh, gv = chords[ch]
        for beat in range(4):
            if bar == 7 and beat >= 2:
                break
            for sub, hand, v in ((0.25, lh, 0.55), (0.5, rh, 0.85), (0.75, lh, 0.65)):
                if hand is lh and rng.random() < 0.08 and not (beat == 0 and sub == 0.25):
                    continue  # a dropped left-hand push now and then
                if bar == 3 and beat == 3 and hand is lh:
                    continue
                t = T(bar, beat + sub) + hum(4)
                vv = vary(v, 0.07)
                for k, n in enumerate(hand):
                    x = env(organ.note(n), 0.11 if hand is lh else 0.15, 0.03, attack=0.003)
                    m.add("organ LH" if hand is lh else "organ RH", x, t + k * 0.002,
                          db(-6 if hand is lh else -5) * vv / len(hand) ** 0.5, pan=-0.25 if hand is lh else -0.05)
        # guitar chops on 2 and 4 (and a double chop in bars 3 and 7); bar 8 keeps only the chops
        chops = [(1, 0.85), (3, 0.8)]
        if bar in (2, 6):
            chops.append((3.25, 0.5))
        for b, v in chops:
            t = T(bar, b) + hum(4)
            vv = vary(v, 0.08)
            up = b == 3.25
            for k, n in enumerate(reversed(gv) if up else gv):
                x = hpf(env(gtr.note(n), 0.06, 0.035), 220)
                m.add("guitar", x, t + k * 0.007, db(-6) * vv / len(gv) ** 0.5, pan=0.35)
    return m.finish(out / "02_bubble.wav")


# ---- 03 Trombone dub ---------------------------------------------------------------------
# 74 bpm, A minor. Three short calls with bars of space between them for the echo to answer;
# the last phrase ends open on the seventh.

def clip03(src: Path, out: Path, cues: dict):
    bpm = 74.0
    spb = 60 / bpm
    tb = bank_from(src / "cc0" / "trombone_short", "trombone")
    m = Mix(LEAD + 20 * spb + 7.0)
    T = lambda bar, b: LEAD + (bar * 4 + swing8(b, 0.6)) * spb
    G3, A3, C4, D4, E4, G4 = 55, 57, 60, 62, 64, 67
    # (bar, beat, note, length s or None for the full note, velocity)
    phrase = [
        (0, 0, A3, .32, .8), (0, .5, C4, .28, .7), (0, 1, E4, None, .9), (0, 2, D4, .28, .75), (0, 2.5, C4, .3, .7), (0, 3, A3, None, .85),
        (2, 0, G4, .3, .85), (2, .5, E4, .28, .75), (2, 1, D4, None, .85), (2, 2.5, C4, .25, .7), (2, 3, D4, .25, .75), (2, 3.5, E4, .3, .8),
        (3, 0, A3, None, .9),
        (4, 0, A3, .22, .85), (4, .5, A3, .22, .75), (4, 1, C4, .28, .8), (4, 1.5, E4, .28, .85), (4, 2, G4, None, .95),
    ]
    for bar, b, n, ln, v in phrase:
        x = tb.note(n)
        x = env(x, ln, 0.05) if ln else env(x, None)
        m.add("trombone", x, T(bar, b) + hum(6), db(-3) * vary(v, 0.05), pan=0.05)
    cues["03"] = {"last_phrase": round(T(4, 0), 3), "last_note": round(T(4, 2), 3)}
    return m.finish(out / "03_trombone.wav")


# ---- 04 / 05 dub techno, F minor on the FM clav ---------------------------------------------

CLAV = {"Fm9": [53, 56, 60, 63, 67], "Fm11": [53, 58, 60, 63, 68], "Dbmaj9": [49, 56, 60, 63, 65]}


def stab(m: Mix, clav: Bank, chord: str, t: float, v: float, part="clav", length=0.16):
    for k, n in enumerate(CLAV[chord]):
        x = env(clav.note(n), length, 0.06)
        m.add(part, x, t + k * 0.0015, db(-4) * v / len(CLAV[chord]) ** 0.5, pan=(k - 2) * 0.08)


def clip04(src: Path, out: Path, cues: dict):
    bpm = 120.0
    spb = 60 / bpm
    kit = kit_b(src / "cc0" / "virtuosity")
    clav = bank_from(src / "cc0" / "clav_fm", "clav")
    m = Mix(LEAD + 33 * spb + 5.0)
    T = lambda bar, s: LEAD + (bar * 4 + swing16(s / 4, 0.03)) * spb  # s in sixteenths
    prog = ["Fm9", "Fm9", "Fm11", "Dbmaj9", "Fm9", "Fm9", "Fm11", "Fm9"]
    rhythms = [[2, 6, 11], [3, 6, 10, 14], [2, 7, 10], [6, 11, 14], [2, 6, 11], [3, 7, 10, 13], [2, 6, 10], [3, 6, 11, 14]]
    for bar in range(8):
        for i, s in enumerate(rhythms[bar]):
            stab(m, clav, prog[bar], T(bar, s) + hum(3), vary(0.85 if i == 0 else 0.7, 0.08),
                 length=0.14 if s % 2 else 0.2)
        for beat in range(4):
            m.add("kick", kit.hit("kick", vary(0.8, 0.04)), T(bar, beat * 4) + hum(2), db(-3))
            m.add("hats", kit.hit("hh", vary(0.45)), T(bar, beat * 4 + 2) + hum(3), db(-8), pan=0.3)
    stab(m, clav, "Fm9", T(8, 0), 0.85, length=0.3)
    m.add("kick", kit.hit("kick", 0.8), T(8, 0), db(-3))
    cues["04"] = {"end": round(T(8, 0), 3)}
    return m.finish(out / "04_stabs.wav")


def clip05(src: Path, out: Path, cues: dict):
    bpm = 120.0
    spb = 60 / bpm
    clav = bank_from(src / "cc0" / "clav_fm", "clav")
    m = Mix(LEAD + 32 * spb + 5.5)
    T = lambda bar, s: LEAD + (bar * 4 + s / 4) * spb
    stabs = [(0, 2, "Fm9", .85), (2, 3, "Fm9", .8), (2, 10, "Fm11", .6), (4, 2, "Dbmaj9", .85),
             (6, 3, "Fm9", .8), (7, 14, "Fm11", .7)]
    for bar, s, ch, v in stabs:
        stab(m, clav, ch, T(bar, s) + hum(3), vary(v, 0.04), length=0.18)
    return m.finish(out / "05_echo_chord.wav")


# ---- 06 Vibes in the springs ------------------------------------------------------------------
# 66 bpm, D minor: Dm9, Bbmaj7, Gm9, A7sus4 to A7, home to Dm9. Rolled gently, damped at
# each change, with a short melodic fill before the turnaround.

def clip06(src: Path, out: Path, cues: dict):
    bpm = 66.0
    spb = 60 / bpm
    vib = bank_from(src / "cc0" / "vibes_soft", "vibes")
    m = Mix(LEAD + 18 * spb + 6.0)
    T = lambda b: LEAD + b * spb
    chords = [(0, [53, 57, 60, 64, 69]), (4, [53, 57, 58, 62, 65]), (8, [58, 62, 65, 69, 72]),
              (12, [55, 57, 62, 64, 67]), (14, [55, 57, 61, 64, 67]), (16, [53, 57, 60, 64, 76])]
    melody = [(2.5, 76, .45), (3.0, 74, .4), (10.0, 69, .45), (10.5, 72, .5), (11.0, 74, .55), (17.5, 81, .4)]
    events = []
    for i, (b, notes) in enumerate(chords):
        nxt = chords[i + 1][0] if i + 1 < len(chords) else None
        roll = 0.045 if i < 5 else 0.075
        for k, n in enumerate(notes):
            t = T(b) + k * roll + abs(hum(5))
            v = (0.8 if k in (0, len(notes) - 1) else 0.62) * vary(1.0, 0.06)
            ln = (T(nxt) - t + 0.05) if nxt is not None else None
            events.append((t, n, v, ln, "vibes chords"))
    for b, n, v in melody:
        events.append((T(b) + hum(6), n, v, (T(b + 1.0) - T(b)) if b < 16 else None, "vibes melody"))
    for t, n, v, ln, part in events:
        x = vib.note(n)
        x = env(x, ln, 0.18) if ln else x
        m.add(part, x, t, db(-6) * v, pan=(n - 64) / 40)
    return m.finish(out / "06_vibes.wav")


# ---- 10 Guitar warble ------------------------------------------------------------------------
# The clean Epiphone, a slow let-ring arpeggio in E minor at 72 bpm (Em9, Cmaj7, Am9, B7sus, Em),
# eighths with a light swing and the odd skipped note, so the warble and the loose boing show.

def clip10(src: Path, out: Path, cues: dict):
    bpm = 72.0
    spb = 60 / bpm
    gtr = bank_from(src / "cc0" / "guitar_emily", "guitar")
    m = Mix(LEAD + 8 * 4 * spb + 6.0)
    T = lambda b: LEAD + swing8(b, 0.6) * spb
    shapes = [
        [40, 47, 50, 54, 55, 54, 50, 47],   # Em9: E B D F# G
        [36, 43, 47, 52, 55, 52, 47, 43],   # Cmaj7: C G B E G
        [45, 52, 55, 59, 60, 59, 55, 52],   # Am9: A E G B C
        [47, 52, 54, 57, 59, 57, 54, 52],   # B7sus: B E F# A B
    ]
    skip = {(1, 6), (3, 3), (5, 5), (6, 7)}  # a few rests so it breathes
    for bar in range(7):
        notes = shapes[bar % 4]
        for k, n in enumerate(notes):
            if (bar, k) in skip:
                continue
            b = bar * 4 + k * 0.5
            v = (0.85 if k == 0 else 0.6 if k % 2 == 0 else 0.5) * vary(1.0, 0.08)
            ring = (4 - k * 0.5) * spb + 0.6       # let each note ring to the end of the bar
            x = env(gtr.note(n), ring, 0.25)
            m.add("guitar", x, T(b) + hum(5), db(-6) * v, pan=(n - 50) / 30)
    for k, n in enumerate([40, 47, 52, 55, 59, 66]):  # the last bar: a slow Em(add9) strum
        x = gtr.note(n)
        m.add("guitar", x, T(28) + k * 0.05 + abs(hum(3)), db(-7) * vary(0.75, 0.05), pan=(n - 50) / 30)
    return m.finish(out / "10_guitar.wav")


# ---- 07 Zither drips --------------------------------------------------------------------------
# Dan Tranh in G# minor pentatonic (the sampled strings, so nothing is repitched), 80 bpm, with
# rests, a quick upward glissando and two bends.

def clip07(src: Path, out: Path, cues: dict):
    bpm = 80.0
    spb = 60 / bpm
    dt = bank_from(src / "cc0" / "dantranh", "dantranh")
    m = Mix(LEAD + 17 * spb + 6.5)
    T = lambda b: LEAD + b * spb
    Gs3, B3, Cs4, Ds4, Fs4, Gs4, B4, Cs5, Ds5 = 56, 59, 61, 63, 66, 68, 71, 73, 75
    # (beat, note, velocity, bend semitones)
    notes = [(0.0, Ds5, .85, 0), (1.5, Cs5, .7, 0), (2.0, B4, .8, 0), (3.5, Gs4, .75, 0),
             (8.0, Fs4, .85, 2), (9.5, Ds4, .7, 0), (10.25, Cs4, .65, 0), (12.0, Gs3, .8, 0),
             (13.5, Ds5, .75, 0), (14.0, B4, .7, 1), (16.0, Gs4, .85, 0), (16.0, Gs3, .6, 0)]
    for b, n, v, bnd in notes:
        x = dt.note(n)
        if bnd:
            x = bend(x, 0.28, 0.16, bnd)
        m.add("dan tranh", x, T(b) + hum(7), db(-3) * vary(v, 0.05), pan=(n - 66) / 30)
    # the glissando: a fast run up the strings, landing on B4
    for k, n in enumerate([Gs3, B3, Cs4, Ds4, Fs4, Gs4, B4]):
        v = 0.4 + 0.07 * k
        m.add("dan tranh", dt.note(n), T(5.5) + k * 0.048 + hum(3), db(-3) * v, pan=(n - 66) / 30)
    return m.finish(out / "07_zither.wav")


# ---- 08 Percussion in the springs --------------------------------------------------------------
# 118 bpm: 808 cowbell on syncopated offbeats, bongos in sixteenths (open and muted, accents and
# rests), a soft kick from bar 2. Some cowbell hits are thrown into the springs.

def clip08(src: Path, out: Path, cues: dict):
    bpm = 118.0
    spb = 60 / bpm
    kit = kit_b(src / "cc0" / "virtuosity")
    bdir = src / "cc0" / "bongos"
    bong = {k: sorted(bdir.glob(p)) for k, p in
            {"H": "BongoH_Hit1_*", "Hm": "BongoH_HitMuted1_*", "L": "BongoL_Hit1_*", "Lm": "BongoL_HitMuted2_*",
             "roll": "BongoH_Roll_v3*"}.items()}
    bk = Kit(bong)
    bell = load(src / "cc0" / "cowbell_808" / "CB.WAV")
    bell = bell / np.abs(bell).max()
    m = Mix(LEAD + 33 * spb + 4.0)
    T = lambda bar, s: LEAD + (bar * 4 + swing16(s / 4, 0.04)) * spb
    pat_a = {0: ("L", .8), 2: ("Hm", .45), 3: ("H", .85), 6: ("H", .6), 7: ("Hm", .4), 8: ("L", .7),
             10: ("Hm", .45), 11: ("H", .9), 14: ("H", .6), 15: ("Lm", .45)}
    pat_b = {0: ("L", .8), 3: ("H", .85), 4: ("Hm", .4), 6: ("H", .6), 8: ("Lm", .5), 9: ("L", .75),
             11: ("H", .9), 12: ("Hm", .45), 14: ("H", .65), 15: ("H", .5)}
    bells = [[3, 10], [3, 6, 10, 13], [3, 10, 13], [6, 10, 14], [3, 10], [3, 6, 11, 13], [3, 10, 13], [3, 6, 10, 13]]
    thrown = {(1, 13), (2, 10), (3, 6), (5, 11), (6, 13), (7, 13)}
    btn = [[0.0, 0.02]]
    for bar in range(8):
        pat = pat_a if bar % 2 == 0 else pat_b
        for s, (k, v) in pat.items():
            if (bar, s) in thrown or (bar in (3, 7) and s >= 12):
                continue
            if v < 0.5 and rng.random() < 0.15:
                continue  # ghost notes come and go
            pan = 0.35 if k.startswith("H") else -0.3
            m.add("bongos", bk.hit(k, vary(v, 0.08)), T(bar, s) + hum(4), db(-5) * (0.4 + 0.6 * vary(v)), pan=pan)
        if bar in (3, 7):
            m.add("bongos", bk.hit("roll", 0.8), T(bar, 12) + hum(3), db(-9), pan=0.35)
        for s in bells[bar]:
            t = T(bar, s) + hum(3)
            v = vary(0.85 if s in (3, 10) else 0.7, 0.08)
            m.add("cowbell", repitch(bell, rng.uniform(-0.08, 0.08)), t, db(-4) * v, pan=0.15)
            if (bar, s) in thrown:
                btn.append([round(t - 0.02, 3), round(t + 0.11, 3)])
        if bar >= 1:
            for beat in range(4):
                m.add("kick", kit.hit("kick", vary(0.6, 0.05)), T(bar, beat * 4) + hum(2), db(-13))
    t = T(8, 0)
    m.add("cowbell", bell, t, db(-4) * 0.9, pan=0.15)
    m.add("bongos", bk.hit("L", 0.85), t + 0.004, db(-5) * 0.9, pan=-0.3)
    m.add("kick", kit.hit("kick", 0.6), t, db(-13))
    btn.append([round(t - 0.02, 3), round(t + 0.2, 3)])
    cues["08"] = {"buttons": btn}
    return m.finish(out / "08_percussion.wav")


# ---- 09 The rough end: a dub siren -----------------------------------------------------------
# The sound-system siren: a square-ish oscillator swept by a slow triangle LFO, played
# as short blips, then one long rising wail.

def siren(out: Path):
    """A sound-system "wheel up" siren (owner, 8 Oct), synthesised from scratch to the character
    of a reference the owner chose (measured, never used: it comes from a commercial record).
    A soft, rounded tone (the fundamental strong, a little 3rd and 7th harmonic) swapping
    between two pitches a minor third apart (418 and 500 Hz) about six times a second, in short
    repeated calls with a longer one to finish."""
    dur, tail = 6.4, 20.0
    t = np.arange(int((dur + tail) * SR)) / SR
    gate = np.zeros_like(t)
    for s0, e0 in [(0.0, 0.85), (1.0, 1.85), (2.0, 2.85), (3.2, 6.2)]:
        gate[(t >= s0) & (t < e0)] = 1.0
    gate = np.convolve(gate, np.ones(240) / 240, mode="same")  # 5 ms edges, no clicks
    sq = (((t * 6.2) % 1.0) < 0.5).astype(float)               # ~80 ms on each pitch
    glide = np.ones(int(0.003 * SR)) / int(0.003 * SR)          # 3 ms between the pitches
    pitch = np.convolve(sq, glide, mode="same")
    freq = 418.0 * (500.0 / 418.0) ** pitch
    phase = 2 * np.pi * np.cumsum(freq) / SR
    tone = np.sin(phase) + 0.08 * np.sin(3 * phase) + 0.05 * np.sin(7 * phase)  # -22 / -26 dB, as measured
    x = tone * gate
    buf = np.stack([x, x], axis=1)
    buf = buf / max(1e-9, np.abs(buf).max()) * 10 ** (-6 / 20)
    sf.write(str(out), buf.astype(np.float32), SR, subtype="PCM_24")
    print(f"wrote {out.name}: {len(buf) / SR:.1f} s")
    return []


BUILDERS = {"01": clip01, "02": clip02, "03": clip03, "04": clip04, "05": clip05,
            "06": clip06, "07": clip07, "08": clip08, "10": clip10}


def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    want = sys.argv[3:] or [*BUILDERS, "09"]
    out.mkdir(parents=True, exist_ok=True)
    cue_file = out / "cues.json"
    cues = json.loads(cue_file.read_text()) if cue_file.exists() else {}
    rows = []
    for cid in want:
        rows += siren(out / "09_siren.wav") if cid == "09" else BUILDERS[cid](src, out, cues)
    cue_file.write_text(json.dumps(cues, indent=1))
    if rows:
        print(f"\n{'source':<20}{'part':<16}{'peak dBFS':>10}{'RMS playing':>13}{'vs loudest':>12}")
        by: dict[str, float] = {}
        for f, _, _, r in rows:
            by[f] = max(by.get(f, -999), r)
        for f, part, pk, r in rows:
            print(f"{f:<20}{part:<16}{pk:>10.1f}{r:>13.1f}{r - by[f]:>12.1f}")


if __name__ == "__main__":
    main()
