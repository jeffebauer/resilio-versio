#!/usr/bin/env python3
"""Write test_audio/midi/kicks_16ths.mid: 4 bars of 1/16 notes at 120 bpm with
varied pitches and velocities, for the M2 Ableton Kick check (velocity must
be ignored, ADR 0005). Stdlib only."""

import struct
from pathlib import Path

PPQ = 480
STEP = PPQ // 4  # 1/16 note
OUT = Path(__file__).resolve().parent.parent / "test_audio" / "midi" / "kicks_16ths.mid"


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.insert(0, (n & 0x7F) | 0x80)
        n >>= 7
    return bytes(out)


events = [(0, bytes([0xFF, 0x51, 0x03]) + (500000).to_bytes(3, "big"))]  # 120 bpm
for i in range(64):
    note = 36 + (i * 7) % 48
    vel = (1, 40, 90, 127)[i % 4]
    events.append((i * STEP, bytes([0x90, note, vel])))
    events.append((i * STEP + STEP // 2, bytes([0x80, note, 0])))
events.sort(key=lambda e: e[0])

track, last = bytearray(), 0
for t, data in events:
    track += vlq(t - last) + data
    last = t
track += vlq(0) + bytes([0xFF, 0x2F, 0x00])

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ) + b"MTrk" + struct.pack(">I", len(track)) + bytes(track))
print(OUT)
