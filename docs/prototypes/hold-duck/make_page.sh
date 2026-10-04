#!/bin/bash
# SUPERSEDED by make_page3.sh: round 2's low-band split is gone from the
# Tank, so this now renders round 3's ducking under round 2's labels; kept as
# the record of the round-2 page (judged 4 Oct 2026).
# Hold ducking page (ADR 0040 round 2). Run from the repo root or a worktree:
#   FEAT=build-feat/rv_render ROUND1=<round-1 rv_render (feat/throw-hold 354ad40)> \
#   MAIN=<main's rv_render> docs/prototypes/hold-duck/make_page.sh [out]
# Columns CLEAN / DRIVEN (+ KICKED for the Howl flip). Versions:
#   A round 1: layer, full-band duck (300 ms release), hold_voicing=1 on 354ad40
#   B low-band duck, 12 dB (the default now)
#   C low-band duck, 18 dB (duck_voicing=1)
# Rows: stab then 4-on-the-floor (120 bpm), stab then one drop (75 bpm), pad
# then drums; KICKED: the Howl flipped to DRIVEN at DECAY 1 (A round 1 holds,
# B today's long fade, bit for bit main: checked here).
# Also renders MIX 1 versions of the drum rows into <out>/wet and prints
# the hump (hump.py).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
FEAT=${FEAT:-build-feat/rv_render}
ROUND1=${ROUND1:?set ROUND1 to the round-1 rv_render}
MAIN=${MAIN:-build-main/rv_render}
OUT=${1:-renders/feat_hold_duck}
ST="${OUT}_stim" # outside the page folder (make_review reads every WAV under it)
python3 "$HERE/stimuli.py" "$ST"
rm -rf "$OUT"
mkdir -p "$OUT/clean" "$OUT/driven" "$OUT/kicked" "$OUT/wet"
common="--set decay=1 --set drive=0.5 --set wobble=0.5 --set springs=2 --set splash=0.3"
for col in clean driven; do
  case $col in clean) att=CLEAN ;; driven) att=DRIVEN ;; esac
  for stim in stab_four stab_onedrop pad_drums; do
    for mix in 0.5 1; do
      if [ $mix = 1 ]; then d="$OUT/wet"; pre="${stim}__${col}_"; else d="$OUT/$col"; pre="${stim}_"; fi
      a="$common --set attitude=$att --set mix=$mix"
      "$ROUND1" "$ST/$stim.wav" "$d/${pre}A_round1.wav" $a --set hold_voicing=1 > /dev/null &
      "$FEAT" "$ST/$stim.wav" "$d/${pre}B_low_duck.wav" $a > /dev/null &
      "$FEAT" "$ST/$stim.wav" "$d/${pre}C_low_duck_deeper.wav" $a --set duck_voicing=1 > /dev/null &
    done
  done
  wait
done
k="--set decay=1 --set drive=0.8 --set wobble=0.5 --set springs=2 --set mix=0.5"
"$ROUND1" "$ST/howl_beat.wav" "$OUT/kicked/howl_flip_A_round1.wav" $k --auto "$ST/howl_flip.json" > /dev/null &
"$FEAT" "$ST/howl_beat.wav" "$OUT/kicked/howl_flip_B_low_duck.wav" $k --auto "$ST/howl_flip.json" > /dev/null &
"$MAIN" "$ST/howl_beat.wav" "$OUT/wet/howl_flip_main.wav" $k --auto "$ST/howl_flip.json" > /dev/null &
wait
if cmp -s "$OUT/kicked/howl_flip_B_low_duck.wav" "$OUT/wet/howl_flip_main.wav"; then
  echo "Howl flip: B is bit for bit main"
else
  echo "Howl flip: B DIFFERS from main"
fi
rm -f "$OUT/wet/howl_flip_main.wav"
echo "Hump (MIX 1):"
python3 "$HERE/hump.py" "$OUT/wet" "$ST"
rm -rf "$OUT/wet"
cp "$HERE/README.txt" "$OUT/README.txt"
python3 tools/review/make_review.py "$OUT" --title "HOLD ducking, round 2" --level-match
echo "open $OUT/index.html"
