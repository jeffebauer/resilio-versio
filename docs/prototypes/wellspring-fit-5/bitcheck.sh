#!/bin/bash
# Round 5: a voicing renders bit for bit as main (7, with BRANCH_ARGS "--set tank_voicing=7" once 8 is the default). Usage (from the repo root):
#   docs/prototypes/wellspring-fit-5/bitcheck.sh MAIN_RV_RENDER [OUT_DIR] [BRANCH_ARGS]
# MAIN_RV_RENDER: an rv_render built from main (e.g. `git archive main | tar -x -C DIR`, then a Release build there).
# Renders 02_hits, 04_skank, 01_clicks, 10_pad_cminor and 15_tone_bursts at five settings with both binaries
# and compares the WAVs byte for byte (cmp). Prints one line per render and "bitcheck: N of N identical".
MAIN=$1
OUT=${2:-renders/fit_round5/bitcheck}
BRANCH_ARGS=${3:-}  # e.g. "--set tank_voicing=7" once 8 is the default
mkdir -p "$OUT"
n=0; same=0
settings=(
  "--set mix=1"
  "--set mix=1 --set attitude=KICKED --set drive=1 --set decay=1 --set springs=3 --set tension=0"
  "--set mix=0.5 --set attitude=DRIVEN --set tone=0 --set splash=1 --set wobble=0.1"
  "--set mix=1 --set tone=1 --set springs=1 --set decay=0.95"
  "--set mix=1 --set decay=0.7 --set springs=2 --set wobble=0.45"
)
for stim in 02_hits 04_skank 01_clicks 10_pad_cminor 15_tone_bursts; do
  i=0
  for s in "${settings[@]}"; do
    "$MAIN" test_audio/stimulus/$stim.wav "$OUT/main_${stim}_$i.wav" $s > /dev/null 2>&1
    build-r/rv_render test_audio/stimulus/$stim.wav "$OUT/r5_${stim}_$i.wav" $s $BRANCH_ARGS > /dev/null 2>&1
    n=$((n+1))
    if cmp -s "$OUT/main_${stim}_$i.wav" "$OUT/r5_${stim}_$i.wav"; then same=$((same+1)); echo "same  $stim [$s]"; else echo "DIFF  $stim [$s]"; fi
    rm -f "$OUT/main_${stim}_$i.wav" "$OUT/r5_${stim}_$i.wav"
    i=$((i+1))
  done
done
echo "bitcheck: $same of $n identical"
