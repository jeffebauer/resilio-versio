#!/bin/bash
# Hold ducking page, round 3 (ADR 0040): the whole bed ducks, keyed on the
# input's lows. Run from the repo root or a worktree:
#   FEAT=build-feat/rv_render ROUND1=<round-1 rv_render (feat/throw-hold 354ad40)> \
#   docs/prototypes/hold-duck/make_page3.sh [out]
# Columns CLEAN / DRIVEN. Versions:
#   A round 1: full-band duck keyed on the whole input, 300 ms release
#   B whole bed ducked 12 dB, keyed on the input's lows (< 120 Hz), short dip (default)
#   C as B, 18 dB (duck_voicing=1)
# Rows: stab + four on the floor (120 bpm), stab + one drop (75 bpm), pad +
# drums, stab + snare and hats only (must not duck). Also renders MIX 1
# versions into <out>/wet, prints the hump (hump.py) and how much the
# snare/hats row moves the bed against no ducking, then deletes them.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
FEAT=${FEAT:-build-feat/rv_render}
ROUND1=${ROUND1:?set ROUND1 to the round-1 rv_render}
OUT=${1:-renders/feat_hold_duck3}
ST="${OUT}_stim" # outside the page folder (make_review reads every WAV under it)
python3 "$HERE/stimuli.py" "$ST"
rm -rf "$OUT"
mkdir -p "$OUT/clean" "$OUT/driven" "$OUT/wet"
common="--set decay=1 --set drive=0.5 --set wobble=0.5 --set springs=2 --set splash=0.3"
for col in clean driven; do
  case $col in clean) att=CLEAN ;; driven) att=DRIVEN ;; esac
  for stim in stab_four stab_onedrop pad_drums stab_snarehats; do
    for mix in 0.5 1; do
      if [ $mix = 1 ]; then d="$OUT/wet"; pre="${stim}__${col}_"; else d="$OUT/$col"; pre="${stim}_"; fi
      a="$common --set attitude=$att --set mix=$mix"
      "$ROUND1" "$ST/$stim.wav" "$d/${pre}A_round1.wav" $a --set hold_voicing=1 > /dev/null &
      "$FEAT" "$ST/$stim.wav" "$d/${pre}B_duck_12dB.wav" $a > /dev/null &
      "$FEAT" "$ST/$stim.wav" "$d/${pre}C_duck_18dB.wav" $a --set duck_voicing=1 > /dev/null &
      [ $mix = 1 ] && "$FEAT" "$ST/$stim.wav" "$OUT/wet/${stim}__${col}_none.ref" $a --set duck_voicing=2 > /dev/null &
    done
  done
  wait
done
mkdir -p "$OUT/wet/ref"
for f in "$OUT"/wet/*.ref; do mv "$f" "$OUT/wet/ref/$(basename "${f%.ref}").wav"; done
echo "Hump (MIX 1):"
mkdir -p "$OUT/wet/hump"
mv "$OUT"/wet/stab_four__*.wav "$OUT"/wet/stab_onedrop__*.wav "$OUT"/wet/pad_drums__*.wav "$OUT/wet/hump/"
python3 "$HERE/hump.py" "$OUT/wet/hump" "$ST"
echo "Snare + hats only (MIX 1), the bed vs no ducking, 4-15 s:"
python3 "$HERE/bedmove.py" "$OUT/wet" "$OUT/wet/ref"
rm -rf "$OUT/wet"
cp "$HERE/README3.txt" "$OUT/README.txt"
python3 tools/review/make_review.py "$OUT" --title "HOLD ducking, round 3" --level-match
echo "open $OUT/index.html"
