#!/bin/bash
# BBD strength listening page (PROTOTYPE; bbd_voicing, core/params/EchoVoicing.h kBbd).
# Usage (from the repo root or a worktree):
#   tools/echo_bbd_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/feat_echo_bbd (in this checkout).
# A-D = bbd_voicing 0-3 with BBD grit (the default wear) in SPRINGS 3,
# CLEAN and KICKED columns, level matching on.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/feat_echo_bbd}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/clean" "$OUT/kicked"

python3 - "$IN" <<'EOF'
import random, sys, wave
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
ms.write("rim.wav", place([1.0], 9.0))
ms.write("rim_long.wav", place([1.0], 14.0))
ms.write("rim_longecho.wav", place([1.0], 16.0))
# Clocked: a rim every 4 beats at 100 bpm while TENSION steps through the seven divisions.
ms.write("rim_clocked.wav", place([0.6 + 2.4 * k for k in range(7)], 19.0))
ms.write("rim_ride.wav", place([1.0, 3.0], 16.0))
with wave.open("test_audio/stimulus/04_skank.wav") as w:
    p = w.getparams(); frames = w.readframes(p.nframes)
with wave.open(str(out / "skank.wav"), "wb") as w:
    w.setparams(p)
    w.writeframes(frames + b"\0" * (p.sampwidth * p.nchannels * 6 * p.framerate))
print("stimuli ok")
EOF
cat > "$IN/clocked.json" <<'EOF'
{ "clock_bpm": 100,
  "breakpoints": [ {"t": 0.0, "key": "tension", "value": 0.07}, {"t": 2.39, "key": "tension", "value": 0.07},
                   {"t": 2.4, "key": "tension", "value": 0.21}, {"t": 4.79, "key": "tension", "value": 0.21},
                   {"t": 4.8, "key": "tension", "value": 0.36}, {"t": 7.19, "key": "tension", "value": 0.36},
                   {"t": 7.2, "key": "tension", "value": 0.5}, {"t": 9.59, "key": "tension", "value": 0.5},
                   {"t": 9.6, "key": "tension", "value": 0.64}, {"t": 11.99, "key": "tension", "value": 0.64},
                   {"t": 12.0, "key": "tension", "value": 0.79}, {"t": 14.39, "key": "tension", "value": 0.79},
                   {"t": 14.4, "key": "tension", "value": 0.93} ] }
EOF
cat > "$IN/ride.json" <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.5}, {"t": 2.0, "key": "decay", "value": 0.5},
                   {"t": 4.0, "key": "decay", "value": 1.0}, {"t": 9.0, "key": "decay", "value": 1.0},
                   {"t": 10.0, "key": "decay", "value": 0.4} ] }
EOF

V=(A_today B_stronger C_strongest D_follows_time)
for A in CLEAN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=3 --set attitude=$A --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5 --set echo_wear_voicing=3 --set echo_diffuse_voicing=0"
  for v in 0 1 2 3; do
    tag=${V[$v]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set bbd_voicing=$v "${@:3}" >/dev/null; }
    r "$IN/rim.wav"          rim_decay050                 --set decay=0.5  --set tension=0.5  --set mix=1
    r "$IN/rim_long.wav"     rim_decay085                 --set decay=0.85 --set tension=0.5  --set mix=1
    r "$IN/rim_longecho.wav" rim_longecho_tension015_d060 --set decay=0.6  --set tension=0.15 --set mix=1
    r "$IN/skank.wav"        skank_decay050_mix100        --set decay=0.5  --set tension=0.5  --set mix=1
    r "$IN/skank.wav"        skank_decay050_mix040        --set decay=0.5  --set tension=0.5  --set mix=0.4
    r "$IN/rim_clocked.wav"  rimclocked_100bpm_divisions  --set decay=0.6  --set mix=1 --auto "$IN/clocked.json"
    r "$IN/rim_ride.wav"     rim_decay_ride_to_top        --set tension=0.6 --set mix=1 --auto "$IN/ride.json"
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
SPRINGS 3 echo mode: how gritty the BBD (prototype, branch feat/echo-mode)

You picked D, BBD grit, on every panel, and asked for more aliasing than the crushed version, which was too subtle. Why it was subtle: the repeats had already passed the tape's dark playback head before being re-sampled, so there was almost nothing high enough to fold back, and what did fold landed above that head and was erased on the next pass. Here the bucket brigade runs at a lower clock, so the folding lands at 1-5 kHz, under the head, where you hear it, and builds pass by pass. Every version keeps the compander's pumping and the faint clock whine (now at the clock's own pitch). All are position 3 with the level fix (each repeat a step down from the hit), TONE noon, SPLASH 0.4, DRIVE 0.25; level matching is on.

A_today          the BBD grit you picked (clock 9.7 kHz): the reference
B_stronger       clock 4.8 kHz, weaker filters: grainy, metallic-edged aliasing from the 2nd-3rd repeat
C_strongest      clock 3.8 kHz: gritty and "broken" by the 3rd-4th repeat, the repeats turning into a lo-fi ghost of the hit
D_follows_time   like a real Memory Man: the clock follows the echo time, so short echoes are cleaner and long ones grittier (B at TENSION noon, past C at 2 s); in the clocked row the grit changes as each division swoops in

Listen for: is the aliasing now clearly there (it should be, from the 2nd repeat)? Is C still musical or too broken? In the long-echo row and the clocked row, does D's "longer = grittier" feel right? Under the runaway (last row, KICKED), which strength do you want?
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: BBD strength" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
