#!/usr/bin/env python3
"""One snare, then silence: the first 6.5 s of 02_hits.wav (its -6 dBFS snare
at 1.0 s), for the "TONE swept on a ringing tail" gesture. Stdlib only.

    python3 docs/prototypes/tone-place/make_one_snare.py
    -> test_audio/stimulus/proto_one_snare.wav (gitignored, like every WAV)
"""
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "test_audio/stimulus/02_hits.wav"
DST = ROOT / "test_audio/stimulus/proto_one_snare.wav"
SECONDS = 6.5

with wave.open(str(SRC), "rb") as r:
    params = r.getparams()
    frames = r.readframes(int(SECONDS * params.framerate))
with wave.open(str(DST), "wb") as w:
    w.setparams(params)
    w.writeframes(frames)
print(f"{DST.relative_to(ROOT)}: {SECONDS} s, one snare at 1.0 s")
