#!/bin/bash
# SPRINGS 3 echo mode listening page (ADR 0041; core/params/EchoVoicing.h).
# Usage (from the repo root or a worktree):
#   tools/echo_mode_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/feat_echo_mode (in this checkout).
# Two pages: OUT/index.html (A = SPRINGS 2, B = SPRINGS 3 echo mode, same
# knobs, CLEAN and KICKED) and OUT/gestures/index.html (moves: the swoop, the
# clock's divisions, KICKED's runaway and its way back, switching in and out).
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/feat_echo_mode}
STIM=test_audio/stimulus
mkdir -p "$OUT/_in"

# Rim stimuli (tools/make_stimulus.py's rim, -6 dBFS): one rim, and rims on
# the beat at 100 bpm for the clocked gesture.
python3 - "$OUT/_in" <<'EOF'
import random, sys
from pathlib import Path
sys.path.insert(0, "tools")
import make_stimulus as ms
out = Path(sys.argv[1])
ms.OUT = out
rng = random.Random(1)
hit = [s * ms.db(-6) for s in ms.rim(rng)]
def place(times, length):
    x = [0.0] * int(length * ms.SR)
    for t in times:
        i = int(t * ms.SR)
        for k, s in enumerate(hit):
            if i + k < len(x): x[i + k] += s
    return x
ms.write("rim_single.wav", place([1.0], 2.0))
ms.write("rim_swoop.wav", place([1.0, 5.0], 12.0))
ms.write("rim_clocked.wav", place([0.6 + 2.4 * k for k in range(6)], 16.0))   # one rim every 4 beats at 100 bpm
ms.write("rim_runaway.wav", place([1.0], 16.0))
ms.write("rim_switch.wav", place([1.0, 4.0, 7.0], 11.0))
EOF

sweep() { # name input base-json tail
  cat > "$OUT/_in/$1.json" <<EOF
{ "name": "$1", "input": "$2", "base": $3, "grid": { "attitude": [0, 1] }, "tail_seconds": $4,
  "ignore_flags": ["click_count", "max_step_db_100ms", "resonance_peak_db"] }
EOF
}
# Common knobs (as the prototype's page): TONE noon, SPLASH 0.4, DRIVE 0.25, WOBBLE still, MIX 1.
K='"tone": 0.5, "splash": 0.4, "drive": 0.25, "wobble": 0.5'
rows=(
  "hits|$STIM/02_hits.wav|\"decay\": 0.5, \"tension\": 0.5, \"mix\": 1.0|5"
  "skank|$STIM/04_skank.wav|\"decay\": 0.5, \"tension\": 0.5, \"mix\": 1.0|5"
  "skank_mix04|$STIM/04_skank.wav|\"decay\": 0.5, \"tension\": 0.5, \"mix\": 0.4|5"
  "rim|$OUT/_in/rim_single.wav|\"decay\": 0.5, \"tension\": 0.5, \"mix\": 1.0|6"
  "rim_long_time|$OUT/_in/rim_single.wav|\"decay\": 0.5, \"tension\": 0.25, \"mix\": 1.0|7"
  "rim_short_time|$OUT/_in/rim_single.wav|\"decay\": 0.5, \"tension\": 0.75, \"mix\": 1.0|5"
  "rim_one_repeat|$OUT/_in/rim_single.wav|\"decay\": 0.0, \"tension\": 0.5, \"mix\": 1.0|5"
  "rim_long_build|$OUT/_in/rim_single.wav|\"decay\": 0.85, \"tension\": 0.5, \"mix\": 1.0|12"
)
for V in A_springs2 B_echo; do
  sp=2; [ $V = B_echo ] && sp=3
  for r in "${rows[@]}"; do
    IFS='|' read -r name input knobs tail <<< "$r"
    sweep "$name" "$input" "{ \"springs\": \"$sp\", $K, $knobs }" "$tail"
    "$BIN" --sweep "$OUT/_in/$name.json" --out-dir "$OUT/$V/$name" >/dev/null
  done
  echo "$V"
done
cat > "$OUT/README.txt" <<'EOF'
SPRINGS 3 = echo mode (branch feat/echo-mode, ADR 0041)

Position 3 is now what nearly every dub rig had: a tape echo feeding the springs, so each repeat lands in the tank with its own splash and gets darker each pass. Behind the echo, the springs are fixed: the noon tank with one classic medium tail (about 1.7 s). In position 3, DECAY is the echo's feedback (0 = one repeat, noon = a few, top = a long build; in KICKED the top tips into a runaway), TENSION is the echo time (left = long, up to 2 s; noon 0.4 s; right = short, 80 ms), and a clock in the gate sets the time in beats instead (gestures page). Every panel plays the same knobs twice; level matching is on.

A_springs2   SPRINGS 2: the same knobs in position 2 (no echo), for reference
B_echo       SPRINGS 3: the tape echo into the springs (DECAY = feedback, TENSION = time)

Listen for: does each repeat splash? Is noon's 0.4 s a good resting time? Does "one repeat" (DECAY 0) feel like the bottom of the knob? Is the long build at DECAY 0.85 long enough without getting messy? Moves (the swoop, the clock, the runaway) are on gestures/index.html.
EOF
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: echo mode" \
  --columns attitude --variants folder1 --rows stimulus,decay,tension,mix --level-match

# ---- Gestures: plain WAVs, CLEAN and KICKED -----------------------------------------------
G="$OUT/gestures"
mkdir -p "$G/clean" "$G/kicked"
auto() { cat > "$OUT/_in/$1.json"; }
# The swoop: TENSION moved while the repeats ring (0.5 -> 0.15 -> 0.8 -> 0.5), DECAY 0.7.
auto swoop <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "tension", "value": 0.5}, {"t": 2.0, "key": "tension", "value": 0.5},
                   {"t": 2.6, "key": "tension", "value": 0.15}, {"t": 6.0, "key": "tension", "value": 0.15},
                   {"t": 6.3, "key": "tension", "value": 0.8}, {"t": 8.5, "key": "tension", "value": 0.8},
                   {"t": 9.5, "key": "tension", "value": 0.5} ] }
EOF
# Clocked at 100 bpm (a pulse a beat); TENSION steps through the seven zones, 2.4 s each.
auto clocked <<'EOF'
{ "clock_bpm": 100,
  "breakpoints": [ {"t": 0.0, "key": "tension", "value": 0.07}, {"t": 2.39, "key": "tension", "value": 0.07},
                   {"t": 2.4, "key": "tension", "value": 0.21}, {"t": 4.79, "key": "tension", "value": 0.21},
                   {"t": 4.8, "key": "tension", "value": 0.36}, {"t": 7.19, "key": "tension", "value": 0.36},
                   {"t": 7.2, "key": "tension", "value": 0.5}, {"t": 9.59, "key": "tension", "value": 0.5},
                   {"t": 9.6, "key": "tension", "value": 0.64}, {"t": 11.99, "key": "tension", "value": 0.64},
                   {"t": 12.0, "key": "tension", "value": 0.79}, {"t": 14.39, "key": "tension", "value": 0.79},
                   {"t": 14.4, "key": "tension", "value": 0.93} ] }
EOF
# DECAY ridden to the top and back (CLEAN: a long build; KICKED: the runaway, then it dies away).
auto runaway <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.5}, {"t": 1.5, "key": "decay", "value": 0.5},
                   {"t": 3.0, "key": "decay", "value": 1.0}, {"t": 9.0, "key": "decay", "value": 1.0},
                   {"t": 10.0, "key": "decay", "value": 0.4} ] }
EOF
# Switching: SPRINGS 2 -> 3 at 3.5 s, back to 2 at 6.5 s (rims at 1, 4, 7 s).
auto switch <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "springs", "value": 0.5}, {"t": 3.49, "key": "springs", "value": 0.5},
                   {"t": 3.5, "key": "springs", "value": 1.0}, {"t": 6.49, "key": "springs", "value": 1.0},
                   {"t": 6.5, "key": "springs", "value": 0.5} ] }
EOF
for A in CLEAN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set attitude=$A --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5 --set mix=1"
  "$BIN" "$OUT/_in/rim_swoop.wav" "$G/$a/rim_A_swoop.wav" --auto "$OUT/_in/swoop.json" $common --set springs=3 --set decay=0.7 >/dev/null
  "$BIN" "$OUT/_in/rim_clocked.wav" "$G/$a/rim_B_clocked.wav" --auto "$OUT/_in/clocked.json" $common --set springs=3 --set decay=0.6 >/dev/null
  "$BIN" "$OUT/_in/rim_runaway.wav" "$G/$a/rim_C_decay_ride.wav" --auto "$OUT/_in/runaway.json" $common --set springs=3 --set tension=0.6 >/dev/null
  "$BIN" "$OUT/_in/rim_switch.wav" "$G/$a/rim_D_switch.wav" --auto "$OUT/_in/switch.json" $common --set decay=0.6 --set tension=0.5 >/dev/null
  "$BIN" "$OUT/_in/rim_swoop.wav" "$G/$a/rim_E_drift.wav" $common --set springs=3 --set decay=0.7 --set tension=0.45 --set wobble=0.25 >/dev/null
done
cat > "$G/README.txt" <<'EOF'
SPRINGS 3 echo mode: moves (branch feat/echo-mode, ADR 0041)

Each version is a different move in position 3, played in CLEAN and KICKED (TONE noon, SPLASH 0.4, DRIVE 0.25). Level matching is off here: the moves change the level on purpose.

A_swoop        TENSION moved while the repeats ring (0.4 s -> 1.2 s -> 0.13 s -> back): the tape slows down and speeds up, the repeats bend in pitch like a Space Echo's rate knob
B_clocked      a clock in the gate at 100 bpm (one pulse a beat); TENSION steps through the seven divisions every 2.4 s: 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16 (each change swoops)
C_decay_ride   DECAY ridden from noon to the top and back down: CLEAN a long build that still fades; KICKED runs away (the dub self-oscillation) and dies away once DECAY comes down
D_switch       SPRINGS 2 -> 3 at 3.5 s and back to 2 at 6.5 s: the echo fades in on a fresh tape and out (its last repeats ring on in the springs), no clicks
E_drift        WOBBLE left of noon (Drift): wow and flutter on the tape too, each repeat wavering a little more than the one before
EOF
python3 tools/review/make_review.py "$G" --title "SPRINGS 3 echo mode: moves" --no-level-match
rm -rf "$OUT/_in"
echo "pages: $OUT/index.html, $G/index.html"
