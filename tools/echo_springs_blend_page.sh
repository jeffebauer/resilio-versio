#!/bin/bash
# Echo springs blend listening page (PROTOTYPE, owner 6 Oct 2026; echo_springs_voicing,
# core/params/EchoVoicing.h "Springs blend", dsp/EchoDirect.h,
# docs/prototypes/echo-springs-blend/README.md).
# Usage (from the repo root or a worktree):
#   tools/echo_springs_blend_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/echo_springs_blend (in this checkout).
# Versions: A = today (all through the springs, voicing 0); B/C/D = 50/25/0 %
# through the springs, the rest direct and wide (voicings 1-3); E/F/G = the same
# with the direct repeats ping-ponging (4-6). SPRINGS 3 (echo mode), columns
# CLEAN / DRIVEN / KICKED, rows: a rim at DECAY noon, at DECAY 0.8 and with a
# fast echo (TENSION 0.75, ~0.18 s), the skank and a held pad at DECAY noon.
# TENSION noon (0.4 s) unclocked, WOBBLE noon (still), MIX 0.6 everywhere (the
# dry hit in every version anchors the level matching: at MIX 1 the no-springs
# versions are 20-28 dB quieter, past the page's 12 dB cap). Level matching on.
# About 0.3 GB of WAVs.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/echo_springs_blend}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/clean" "$OUT/driven" "$OUT/kicked"

python3 - "$IN" <<'EOF'
import sys
from pathlib import Path
sys.path.insert(0, "tools")
import make_stimulus as ms
import random
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
sk = ms.skank()  # 1 s, 4 bars of offbeat stabs at 75 bpm, 10 s of tail
ms.write("skank.wav", sk + ms.silence(2))
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
print("stimuli ok")
EOF

V=(A_today B_wide_springs50 C_wide_springs25 D_wide_no_springs E_pingpong_springs50 F_pingpong_springs25 G_pingpong_no_springs)
for A in CLEAN DRIVEN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=3 --set attitude=$A --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5"
  for k in 0 1 2 3 4 5 6; do
    tag=${V[$k]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set echo_springs_voicing=$k "${@:3}" >/dev/null; }
    r "$IN/rim.wav"      1_rim_decay050_mix060     --set tension=0.5 --set decay=0.5 --set mix=0.6 &
    r "$IN/rim_long.wav" 2_rim_decay080_mix060     --set tension=0.5 --set decay=0.8 --set mix=0.6 &
    r "$IN/rim.wav"      3_rim_fast_tension075_mix060 --set tension=0.75 --set decay=0.5 --set mix=0.6 &
    r "$IN/skank.wav"    4_skank_decay050_mix060   --set tension=0.5 --set decay=0.5 --set mix=0.6 &
    r "$IN/pad.wav"      5_pad_decay050_mix060     --set tension=0.5 --set decay=0.5 --set mix=0.6 &
    wait
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
SPRINGS 3 echo mode with less (or none) of the echo through the springs. Today every repeat goes into the springs, which smear it; B-G send only part of the echo's wet through the springs and play the rest of the tape's repeats directly (the dry hit is never doubled: it reaches you through MIX as always). The direct repeats join at the springs' output and go through everything after it (the output pickups, TONE, the output grit, the limiter, MIX), so DRIVE and ATTITUDE still colour them. Every row is TENSION noon (0.4 s echo, unclocked) except the "fast" rim (TENSION 0.75, ~0.18 s), WOBBLE noon (still), TONE noon, SPLASH 0.4, DRIVE 0.25, MIX 0.6 (so the dry hit is in every version and you hear the repeats against it). Level matching is on (the raw level differences are in docs/prototypes/echo-springs-blend/README.md).
A_today                 today: the whole wet through the springs
B_wide_springs50        B-wide: half through the springs, half direct; the direct repeats wide but centred (two tape heads 8 ms apart, the right one a touch darker; the bass identical in both)
C_wide_springs25        C-wide: a quarter through the springs, three quarters direct and wide
D_wide_no_springs       D-wide: no springs at all, the tape echo alone, wide
E_pingpong_springs50    B-pp: half through the springs, half direct; the direct repeats ping-pong (1st left, 2nd right, 3rd left ...)
F_pingpong_springs25    C-pp: a quarter through the springs, three quarters direct, ping-pong
G_pingpong_no_springs   D-pp: no springs at all, the tape echo alone, ping-pong
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: echo through the springs, or not" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
