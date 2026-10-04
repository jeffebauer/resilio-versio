#!/bin/bash
# TONE placement prototype: every render + the listening page.
# Run from the repo (or worktree) root with a built rv_render:
#   bash docs/prototypes/tone-place/render.sh [rv_render] [out_dir]
set -e
BIN=${1:-build/rv_render}
OUT=${2:-renders/proto_tone_place}
python3 docs/prototypes/tone-place/make_one_snare.py
mkdir -p "$OUT"
for s in g1_tail g1_tail_mix04 g2_skank g2_skank_mix04 \
         static_hits_t070 static_hits_t085 static_hits_t100 \
         static_skank_t070 static_skank_t085 static_skank_t100; do
  "$BIN" --sweep presets/sweeps/proto_tone_place_$s.json --out-dir "$OUT/$s" > /dev/null &
done
wait
cp docs/prototypes/tone-place/README.txt "$OUT/README.txt"
python3 tools/review/make_review.py "$OUT" --title "TONE: filter before, after, or split" \
  --columns attitude --variants tone_place_voicing --level-match
