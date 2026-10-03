#!/bin/bash
# M6 Ringing + Howl grids (presets/sweeps/m6_*.json, docs/m6-metric-calibration.md) per Tank voicing.
# Usage: docs/prototypes/wellspring-fit-3/m6_grid.sh "1 2 3 4" OUT_DIR
# Renders OUT_DIR/vN/m6_* (sidecars kept, WAVs deleted); summarise with m6_summary.py OUT_DIR/vN.
cd "$(dirname "$0")/../../.."
for v in $1; do
  out=$2/v$v
  mkdir -p $out
  for s in m6_click_ringing_d075 m6_click_ringing_d1 m6_click_howl m6_bursts_ringing_d075 m6_bursts_ringing_d1 m6_bursts_howl; do
    build-r/rv_render --sweep presets/sweeps/$s.json --out-dir $out/$s --set tank_voicing=$v > $out/$s.log 2>&1 &
  done
  wait
  find $out -name "*.wav" -delete
  echo "grid v$v done"
done
