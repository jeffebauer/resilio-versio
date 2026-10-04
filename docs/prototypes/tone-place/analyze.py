#!/usr/bin/env python3
"""TONE placement prototype: what each placement does over a gesture.
Stdlib only (no numpy). Reads the renders made by render.sh.

    python3 docs/prototypes/tone-place/analyze.py [renders/proto_tone_place]

Per gesture render: level (dB, L+R power) and the lows' share (< 250 Hz,
two one-pole low-passes) in 100 ms windows at a few moments, and the
static renders' loudness spread (a K-weighting-free RMS of the gated
signal; the test's BS.1770 numbers are in test_tone_place).
"""
import json
import math
import struct
import sys
from pathlib import Path


def read_wav(path):
    data = Path(path).read_bytes()
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif cid == b"data":
            raw = body
        pos += 8 + size + (size & 1)
    tag, ch, sr, _, _, bits = fmt
    if tag in (3, 0xFFFE) and bits == 32:
        vals = struct.unpack("<%df" % (len(raw) // 4), raw)
    elif bits == 24:
        vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    elif bits == 16:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    else:
        raise ValueError("unsupported wav %s" % (fmt,))
    return sr, [list(vals[c::ch]) for c in range(ch)]


def low(x, sr, hz):
    c = 1.0 - math.exp(-2 * math.pi * hz / sr)
    a = b = 0.0
    out = []
    for v in x:
        a += c * (v - a)
        b += c * (a - b)
        out.append(b)
    return out


def db(p):
    return 10 * math.log10(p + 1e-30)


def window(x, sr, t, w=0.1):
    i0, i1 = int(t * sr), int((t + w) * sr)
    seg = x[i0:i1]
    return sum(v * v for v in seg) / max(1, len(seg))


def gesture(path, moments):
    sr, ch = read_wav(path)
    mono = [0.5 * (a + b) for a, b in zip(ch[0], ch[1])]
    lo = low(mono, sr, 250.0)
    rows = []
    for t in moments:
        p = window(mono, sr, t)
        pl = window(lo, sr, t)
        rows.append((t, db(p), db(pl) - db(p)))
    return rows


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "renders/proto_tone_place")
    name = {0: "A pre", 1: "B post", 2: "C split"}
    for g, moments in (("g1_tail", (1.2, 1.6, 2.0, 2.4, 2.9, 3.2, 3.8, 4.4)),
                       ("g2_skank", (1.8, 4.0, 6.0, 8.0, 10.5, 12.0))):
        man = json.loads((root / g / "manifest.json").read_text())
        print("\n%s  (t s: level dB / lows-share dB)" % g)
        for r in man["renders"]:
            p = r["params"]
            rows = gesture(root / g / r["wav"], moments)
            cells = "  ".join("%.1f: %6.1f/%5.1f" % row for row in rows)
            print("  %-6s %-7s %s" % (p["attitude"], name[int(p["tone_place_voicing"])], cells))
    print("\nstatic: RMS of the whole render vs placement A (dB)")
    for d in sorted(root.glob("static_*")):
        man = json.loads((d / "manifest.json").read_text())
        lev = {}
        for r in man["renders"]:
            p = r["params"]
            sr, ch = read_wav(d / r["wav"])
            pw = [0.5 * (a * a + b * b) for a, b in zip(ch[0], ch[1])]
            lev[(p["attitude"], int(p["tone_place_voicing"]))] = db(sum(pw) / len(pw))
        for att in ("CLEAN", "KICKED"):
            print("  %-22s %-6s B %+5.1f  C %+5.1f" % (d.name, att, lev[(att, 1)] - lev[(att, 0)], lev[(att, 2)] - lev[(att, 0)]))


if __name__ == "__main__":
    main()
