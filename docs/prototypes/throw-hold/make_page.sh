#!/bin/bash
# THROW / HOLD listening pages (ADR 0039, 0040). Run from the repo root (or
# a worktree) after building the branch:
#   FEAT=build-feat/rv_render MAIN=<main's rv_render> docs/prototypes/throw-hold/make_page.sh [out]
# Two pages, linked from <out>/index.html:
#   throw/  dub beat, DECAY 0.75, MIX 0.45, CLEAN / DRIVEN / KICKED:
#           A today (send always open), B thrown (bar 2's last snare, bar 3's
#           last stab, bar 4's last snare)
#   hold/   CLEAN / DRIVEN: a pad, DECAY 0.8 -> 1 at 4.5-5.5 s, a beat over
#           it, two stabs, MIX 0.5: A today (DECAY 1 = 9 s), B Hold freeze
#           (the default), C Hold layer, D freeze + the 22 s stab thrown in.
#           KICKED: the Howl under the beat, ATTITUDE -> DRIVEN at 9 s
#           (DECAY 1): A today (fades), B into the Hold (open owner question)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
FEAT=${FEAT:-build-feat/rv_render}
MAIN=${MAIN:-build-main/rv_render}
OUT=${1:-renders/feat_throw_hold}
ST="$OUT/stim"
python3 "$HERE/stimuli.py" "$ST"
rm -rf "$OUT/throw" "$OUT/hold"
common="--set drive=0.5 --set wobble=0.5 --set springs=2"
for col in clean driven kicked; do
  case $col in clean) att=CLEAN ;; driven) att=DRIVEN ;; kicked) att=KICKED ;; esac
  d="$OUT/throw/$col"
  mkdir -p "$d"
  t="$common --set attitude=$att --set decay=0.75 --set mix=0.45"
  "$FEAT" "$ST/dub_beat.wav" "$d/dub_beat_A_today.wav" $t > /dev/null &
  "$FEAT" "$ST/dub_beat.wav" "$d/dub_beat_B_thrown.wav" $t --auto "$ST/throw.json" > /dev/null &
  d="$OUT/hold/$col"
  mkdir -p "$d"
  if [ $col != kicked ]; then
    h="$common --set attitude=$att --set mix=0.5"
    "$MAIN" "$ST/hold_scene.wav" "$d/hold_scene_A_today.wav" $h --auto "$ST/hold.json" > /dev/null &
    "$FEAT" "$ST/hold_scene.wav" "$d/hold_scene_B_freeze.wav" $h --auto "$ST/hold.json" > /dev/null &
    "$FEAT" "$ST/hold_scene.wav" "$d/hold_scene_C_layer.wav" $h --set hold_voicing=1 --auto "$ST/hold.json" > /dev/null &
    "$FEAT" "$ST/hold_scene.wav" "$d/hold_scene_D_freeze_throw_in.wav" $h --auto "$ST/hold_throw.json" > /dev/null &
  else
    k="$common --set decay=1 --set mix=0.5"
    "$MAIN" "$ST/dub_beat.wav" "$d/howl_flip_A_today.wav" $k --auto "$ST/howl_flip.json" > /dev/null &
    "$FEAT" "$ST/dub_beat.wav" "$d/howl_flip_B_freeze.wav" $k --auto "$ST/howl_flip.json" > /dev/null &
  fi
done
wait
cp "$HERE/README_throw.txt" "$OUT/throw/README.txt"
cp "$HERE/README_hold.txt" "$OUT/hold/README.txt"
python3 tools/review/make_review.py "$OUT/throw" --title "THROW on the gate" --level-match
python3 tools/review/make_review.py "$OUT/hold" --title "HOLD at the top of DECAY" --no-level-match
cp "$HERE/index.html" "$OUT/index.html"
echo "open $OUT/index.html"
