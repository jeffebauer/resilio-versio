#!/bin/bash
# Repeats bit-depth listening page (PROTOTYPE; echo_bits_voicing, core/params/EchoVoicing.h kBits).
# Usage (from the repo root or a worktree):
#   tools/echo_bits_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/feat_echo_bits (in this checkout).
# A-D = echo_bits_voicing 0-3 on BBD grit A (the default) in SPRINGS 3,
# CLEAN and KICKED columns, level matching on.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/feat_echo_bits}
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

V=(A_none B_sunny_tape_12bit C_scorched_cassette_8bit D_8bit_mulaw)
for A in CLEAN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=3 --set attitude=$A --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5 --set echo_wear_voicing=3 --set echo_diffuse_voicing=0"
  for v in 0 1 2 3; do
    tag=${V[$v]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set bbd_voicing=0 --set echo_bits_voicing=$v "${@:3}" >/dev/null; }
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
SPRINGS 3 echo mode: bit depth on the repeats (prototype, branch feat/echo-mode)

You kept BBD A and asked for bit reduction instead, after Beads' "Sunny Tape" (24 kHz / 12-bit) and "Scorched Cassette" (24 kHz / 8-bit). Each version is BBD A plus a bit-depth stage inside the echo's feedback, so it builds up: the first repeat is clean, each later one has been through it once more. The 24 kHz part goes through proper filters both ways, so it can't add new pitches (the chirp you heard in the BBD round came from images left in the band, and from the clock whine sitting at 4-5 kHz there). The bits are against full scale, like real 12- or 8-bit audio, so each quieter repeat has fewer bits left and the grain grows as the echoes fade; a fading repeat ends in a soft grainy hiss and then silence, never a stuck buzz. All are position 3 with TONE noon, SPLASH 0.4, DRIVE 0.25; level matching is on.

A_none                     BBD A as you picked it, no bit reduction
B_sunny_tape_12bit         24 kHz / 12-bit: a fine grain that shows on the quiet, late repeats
C_scorched_cassette_8bit   24 kHz / 8-bit: coarse grain and hiss on every repeat; quiet repeats run out of bits and drop away sooner (8 bits end at about -42 dBFS)
D_8bit_mulaw               24 kHz / 8-bit mu-law (telephone / early digital companding): C's crunch on the loud repeats, but the quiet ones keep going under a much finer grain, so the long tails survive

Listen for: is there any new pitch or chirp after the 2nd repeat (there shouldn't be)? Which grain do you like on the rims and the skank? Does C's early drop-out of quiet repeats bother you, or is it part of the cassette feel? How do they sound under the runaway (last row, KICKED)?
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: bit depth on the repeats" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
