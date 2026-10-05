#!/bin/bash
# Echo tape wear listening page (PROTOTYPE, owner 5 Oct 2026; echo_wear_voicing,
# core/params/EchoVoicing.h "Tape wear", docs/prototypes/echo-tape-wear/README.md).
# Usage (from the repo root or a worktree):
#   tools/echo_tape_wear_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/echo_tape_wear (in this checkout).
# Versions: A = BBD grit (3, today's default), B = tape saturation + roll-off (5),
# C1 / C2 = B + crinkle, subtle / obvious (6 / 7). SPRINGS 3 (echo mode),
# columns CLEAN / DRIVEN / KICKED, rows: a rim, the skank and a held pad, each
# at DECAY noon and at the held top (DECAY 1, backed off to noon near the end),
# plus the rim at DECAY 0.8 (a long build, where the wear has room to pile up).
# WOBBLE noon (still), so the wear is heard on its own. Level matching on.
# About 0.5 GB of WAVs.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/echo_tape_wear}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/clean" "$OUT/driven" "$OUT/kicked"

python3 - "$IN" <<'EOF'
import math, random, sys
from pathlib import Path
sys.path.insert(0, "tools")
import make_stimulus as ms
out = Path(sys.argv[1])
ms.OUT = out
SR = ms.SR
rng = random.Random(1)
hit = [s * ms.db(-6) for s in ms.rim(rng)]
def place(times, length):
    x = [0.0] * int(length * SR)
    for t in times:
        i = int(t * SR)
        for k, s in enumerate(hit):
            if i + k < len(x): x[i + k] += s
    return x
ms.write("rim.wav", place([1.0], 10.0))
ms.write("rim_long.wav", place([1.0], 14.0))
ms.write("rim_top.wav", place([1.0], 26.0))
sk = ms.skank()  # 1 s, 4 bars of offbeat stabs at 75 bpm, 10 s of tail
ms.write("skank.wav", sk + ms.silence(2))
ms.write("skank_top.wav", sk + ms.silence(6))
# A held C minor pad: soft saws (C3 Eb3 G3 C4), 4 s, slow in and out, -9 dBFS.
def pad(length):
    n, att, rel = int(4.0 * SR), int(0.4 * SR), int(0.8 * SR)
    x = [0.0] * int(length * SR)
    for f in (130.81, 155.56, 196.0, 261.63):
        for i in range(n):
            t = i / SR
            env = min(1.0, i / att, (n - 1 - i) / rel)
            x[SR + i] += env * (2 * ((f * t + 0.31 * f / 130.81) % 1) - 1) / 4
    x = ms.one_pole_lp(ms.one_pole_lp(x, 1800), 1800)
    return ms.normalise(x, -9)
ms.write("pad.wav", pad(14.0))
ms.write("pad_top.wav", pad(26.0))
print("stimuli ok")
EOF
# The held top: DECAY 1, then back to noon (the repeats die away) for the last ~5 s.
cat > "$IN/top_rim.json" <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 1.0}, {"t": 20.0, "key": "decay", "value": 1.0},
                   {"t": 20.5, "key": "decay", "value": 0.5} ] }
EOF
cat > "$IN/top_skank.json" <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 1.0}, {"t": 25.0, "key": "decay", "value": 1.0},
                   {"t": 25.5, "key": "decay", "value": 0.5} ] }
EOF

V=(A_bbd_grit B_tape_sat C1_crinkle_subtle C2_crinkle_obvious)
W=(3 5 6 7)
for A in CLEAN DRIVEN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=3 --set attitude=$A --set tone=0.5 --set tension=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5 --set echo_diffuse_voicing=0 --set echo_bits_voicing=0"
  for k in 0 1 2 3; do
    tag=${V[$k]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set echo_wear_voicing=${W[$k]} "${@:3}" >/dev/null; }
    r "$IN/rim.wav"       1_rim_decay050            --set decay=0.5 --set mix=1
    r "$IN/rim_long.wav"  2_rim_decay080            --set decay=0.8 --set mix=1
    r "$IN/rim_top.wav"   2b_rim_held_top           --set mix=1 --auto "$IN/top_rim.json"
    r "$IN/skank.wav"     3_skank_decay050_mix060   --set decay=0.5 --set mix=0.6
    r "$IN/skank_top.wav" 4_skank_held_top_mix060   --set mix=0.6 --auto "$IN/top_skank.json"
    r "$IN/pad.wav"       5_pad_decay050_mix060     --set decay=0.5 --set mix=0.6
    r "$IN/pad_top.wav"   6_pad_held_top_mix060     --set mix=0.6 --auto "$IN/top_rim.json"
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
SPRINGS 3 echo mode: repeats that wear like tape instead of BBD grit. Each version works inside the echo's feedback, so it builds up repeat by repeat: the first repeat is the same in all four, the 4th-6th have been through the wear several times. Every row is TENSION noon (0.4 s echo), WOBBLE noon (still, so you hear the wear on its own), TONE noon, SPLASH 0.4, DRIVE 0.25; the rim also plays at DECAY 0.8 (a long build: the clearest place to hear the wear pile up); "held top" = DECAY fully up, brought back to noon for the last few seconds. Level matching is on.
A_bbd_grit           today's default: a bucket brigade's grit and pumping (its aliasing adds faint new, off-key pitches)
B_tape_sat           tape saturation and roll-off: loud, bright repeats come back thicker and duller (the highs squash first), a little more treble gone each pass, a gentle low lift around 70 Hz; no new pitches, no wow or flutter
C1_crinkle_subtle    B plus crinkled tape, subtle: now and then a short, papery flutter where the tape loses the head for a few milliseconds (level and highs dip together), more on the older repeats
C2_crinkle_obvious   B plus crinkled tape, obvious: longer, denser crinkled patches, so the older repeats break up clearly
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: tape wear" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
