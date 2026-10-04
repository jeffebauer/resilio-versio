#!/usr/bin/env python3
"""Short stimuli for the "TONE after the springs" before / after renders
(presets/sweeps/tone_after_*.json). Stdlib only.

    python3 docs/prototypes/tone-place/make_one_snare.py
    python3 docs/prototypes/tone-place/make_after_stimuli.py
    -> test_audio/stimulus/proto_skank_12s.wav   (the first 12 s of 04_skank.wav)
    -> test_audio/stimulus/proto_silence_6s.wav  (the first 6 s of 05_silence_for_kicks.wav)
    (gitignored, like every WAV)
"""
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
STIM = ROOT / "test_audio/stimulus"
for src, dst, seconds in (("04_skank.wav", "proto_skank_12s.wav", 12.0),
                          ("05_silence_for_kicks.wav", "proto_silence_6s.wav", 6.0)):
    with wave.open(str(STIM / src), "rb") as r:
        params = r.getparams()
        frames = r.readframes(int(seconds * params.framerate))
    with wave.open(str(STIM / dst), "wb") as w:
        w.setparams(params)
        w.writeframes(frames)
    print(f"{(STIM / dst).relative_to(ROOT)}: {seconds} s")
