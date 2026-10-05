#!/bin/bash
# Round 5: the M6 DECAY 0.75 Ringing sweeps (click and bursts) for one tank voicing with RV_TANKV_TUNE overrides,
# summarised. Usage (repo root): docs/prototypes/wellspring-fit-5/m6_d075.sh VOICING "key=val,..." OUT_DIR
v=$1; tune=$2; out=$3
rm -rf "$out"; mkdir -p "$out/x"
for s in m6_bursts_ringing_d075 m6_click_ringing_d075 m6_bursts_ringing_d1 m6_click_ringing_d1; do
  RV_TANKV_TUNE="$tune" build-r/rv_render --sweep presets/sweeps/$s.json --out-dir "$out/x/$s" --set tank_voicing=$v > /dev/null 2>&1 &
done
wait
find "$out" -name "*.wav" -delete
echo "voicing $v [$tune]"
python3 docs/prototypes/wellspring-fit-3/m6_summary.py "$out/x" | head -5
