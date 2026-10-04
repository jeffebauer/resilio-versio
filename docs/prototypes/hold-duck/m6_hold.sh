#!/bin/bash
# M6 Ringing grid in the Hold (CLEAN / DRIVEN DECAY 1), default voicing
# (layer, low-band ducking). WAVs deleted once the sidecars are written.
#   FEAT=build-feat/rv_render docs/prototypes/hold-duck/m6_hold.sh [out]
FEAT=${FEAT:-build-feat/rv_render}
OUT=${1:-renders/feat_throw_hold/m6/hold_r2}
for s in m6_click_ringing_d1 m6_bursts_ringing_d1; do
  rm -rf "$OUT/$s"
  ("$FEAT" --sweep presets/sweeps/$s.json --out-dir "$OUT/$s" > /dev/null 2>&1; find "$OUT/$s" -name '*.wav' -delete) &
done
wait
python3 docs/prototypes/wellspring-fit-3/m6_summary.py "$OUT"
