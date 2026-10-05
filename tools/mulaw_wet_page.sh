#!/bin/bash
# The mu-law box moved to the wet, before TONE (ADR 0042 amendment, 5 Oct 2026): listening page.
# Usage (from the repo root or a worktree):
#   tools/mulaw_wet_page.sh MAIN_RENDER_BIN [BRANCH_RENDER_BIN] [OUT_DIR]
# A = MAIN_RENDER_BIN (the box after MIX, on dry and wet), B = this branch (the box on the wet, before TONE).
# Columns KICKED / DRIVEN; level matching on.
set -e
cd "$(dirname "$0")/.."
MAIN=${1:?main rv_render}
BIN=${2:-build/rv_render}
OUT=${3:-renders/mulaw_wet}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/kicked" "$OUT/driven"
python3 - "$IN" <<'EOF'
import sys, wave
from pathlib import Path
out = Path(sys.argv[1])
for name in ("02_hits", "04_skank"):
    with wave.open(f"test_audio/stimulus/{name}.wav") as w:
        p = w.getparams(); frames = w.readframes(min(p.nframes, 20 * p.framerate))
    with wave.open(str(out / f"{name}.wav"), "wb") as w:
        w.setparams(p)
        w.writeframes(frames + b"\0" * (p.sampwidth * p.nchannels * 4 * p.framerate))
EOF
for A in KICKED DRIVEN; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=2 --set attitude=$A --set decay=0.5 --set tension=0.5 --set splash=0.3 --set drive=0.4"
  for v in A_main_box_on_everything B_wet_only_pre_tone; do
    R=$BIN; [ ${v:0:1} = A ] && R=$MAIN
    r() { "$R" "$1" "$OUT/$a/$2_$v.wav" $common "${@:3}" >/dev/null; }
    r "$IN/02_hits.wav"  1_hits_mix050           --set mix=0.5 --set tone=0.5
    r "$IN/04_skank.wav" 2_skank_mix040          --set mix=0.4 --set tone=0.5
    r "$IN/02_hits.wav"  3_hits_mix100_tone050   --set mix=1   --set tone=0.5
    r "$IN/02_hits.wav"  4_hits_mix100_tone080   --set mix=1   --set tone=0.8
    r "$IN/04_skank.wav" 5_skank_mix000_dry      --set mix=0   --set tone=0.5
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
The mu-law box moved to the wet, before TONE (branch fix/mulaw-wet-pretone)

You heard KICKED's aliasing as too present on both the input and the output, and asked for the box to affect only the wet, before TONE, so you can filter its highs. B does that: the dry is clean again (row 5: B is your input exactly, in DRIVEN and KICKED), and the box sits on the wet after the pickups and before TONE's return filter, so turning TONE right thins its grit with the rest of the wet (compare rows 3 and 4). Same box otherwise: DRIVEN 24 kHz / 12-bit, KICKED 24 kHz / 10-bit mu-law. 2 Springs, DECAY / TENSION noon, SPLASH 0.3, DRIVE 0.4; level matching is on.

A_main_box_on_everything   what main does now: the box after MIX, on dry and wet
B_wet_only_pre_tone        the box on the wet only, before TONE

Listen for: the dry hits and stabs at MIX 0.5 / 0.4 (rows 1, 2): B's dry is clean, the grit only in the tail; at MIX 1 (rows 3, 4) the two should be close, and at TONE 0.8 B's grit should thin with TONE; row 5 (MIX 0): B clean, A gritty.
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "Mu-law on the wet, before TONE" --columns attitude --variants variant --rows material --level-match
echo "page: $OUT/index.html"
