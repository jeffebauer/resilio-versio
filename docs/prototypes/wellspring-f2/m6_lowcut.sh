#!/bin/bash
# M6 Ringing + Howl grids (presets/sweeps/m6_*.json) per F low cut step (f_lowcut_voicing, tank voicing 7).
# Usage: docs/prototypes/wellspring-f2/m6_lowcut.sh "0 1 2 3" OUT_DIR
# Renders OUT_DIR/lcN/m6_* (sidecars kept, WAVs deleted); summarise with
#   python3 docs/prototypes/wellspring-fit-3/m6_summary.py OUT_DIR/lcN
cd "$(dirname "$0")/../../.."
for v in $1; do
  out=$2/lc$v
  mkdir -p $out
  for s in m6_click_ringing_d075 m6_click_ringing_d1 m6_click_howl m6_bursts_ringing_d075 m6_bursts_ringing_d1 m6_bursts_howl; do
    build-r/rv_render --sweep presets/sweeps/$s.json --out-dir $out/$s --set f_lowcut_voicing=$v > $out/$s.log 2>&1 &
  done
  wait
  find $out -name "*.wav" -delete
  echo "grid lc$v done"
done
