#!/bin/bash
# M6 Ringing spot-check per TONE placement: the Big Knob round's M6 sweeps
# (TONE 0.7 / 0.85 / 1; DECAY 0.75 and 1; every SPRINGS and TENSION; clicks and
# noise bursts; CLEAN / DRIVEN, and KICKED in the bursts), once per placement.
#   bash docs/prototypes/tone-place/m6.sh [rv_render] [out_dir]
# Then: python3 docs/prototypes/tone-place/m6_summary.py <out_dir>
BIN=${1:-build/rv_render}
OUT=${2:-renders/proto_tone_place_m6}
for p in 0 1; do
  for s in click_ringing_d075 click_ringing_d1 bursts_ringing_d075 bursts_ringing_d1; do
    "$BIN" --sweep presets/sweeps/proto_big_knob_m6_$s.json --out-dir "$OUT/p$p/$s" --set tone_place_voicing=$p > /dev/null &
  done
done
wait
