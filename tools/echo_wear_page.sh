#!/bin/bash
# Echo break-up listening page (PROTOTYPE; echo_wear_voicing, core/params/EchoVoicing.h "Wear").
# Usage (from the repo root or a worktree):
#   tools/echo_wear_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/feat_echo_wear (in this checkout).
# A-E = voicings 0 none / 1 worn tape / 2 radio band / 3 BBD grit / 4 crushed,
# SPRINGS 3 (echo mode, with the first-repeat level fix), CLEAN and KICKED
# columns, level matching on. echo_diffuse_voicing stays 0.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/feat_echo_wear}
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
ms.write("rim_clocked.wav", place([0.6, 0.6 + 4 * 0.6], 10.0))
ms.write("rim_ride.wav", place([1.0, 3.0], 16.0))
with wave.open("test_audio/stimulus/04_skank.wav") as w:
    p = w.getparams(); frames = w.readframes(p.nframes)
with wave.open(str(out / "skank.wav"), "wb") as w:
    w.setparams(p)
    w.writeframes(frames + b"\0" * (p.sampwidth * p.nchannels * 6 * p.framerate))
print("stimuli ok")
EOF
cat > "$IN/clock.json" <<'EOF'
{ "clock_bpm": 100 }
EOF
# DECAY ridden from noon up into the top (KICKED's runaway) and back down.
cat > "$IN/ride.json" <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.5}, {"t": 2.0, "key": "decay", "value": 0.5},
                   {"t": 4.0, "key": "decay", "value": 1.0}, {"t": 9.0, "key": "decay", "value": 1.0},
                   {"t": 10.0, "key": "decay", "value": 0.4} ] }
EOF

V=(A_none B_worn_tape C_radio_band D_bbd_grit E_crushed)
for A in CLEAN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=3 --set attitude=$A --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5 --set echo_diffuse_voicing=0"
  for v in 0 1 2 3 4; do
    tag=${V[$v]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set echo_wear_voicing=$v "${@:3}" >/dev/null; }
    r "$IN/rim.wav"         rim_decay050                --set decay=0.5  --set tension=0.5 --set mix=1
    r "$IN/rim_long.wav"    rim_decay085                --set decay=0.85 --set tension=0.5 --set mix=1
    r "$IN/skank.wav"       skank_decay050_mix100       --set decay=0.5  --set tension=0.5 --set mix=1
    r "$IN/skank.wav"       skank_decay050_mix040       --set decay=0.5  --set tension=0.5 --set mix=0.4
    r "$IN/rim_clocked.wav" rimclocked_100bpm_dotted8   --set decay=0.6  --set tension=0.5 --set mix=1 --auto "$IN/clock.json"
    r "$IN/rim_ride.wav"    rim_decay_ride_to_top       --set tension=0.6 --set mix=1 --auto "$IN/ride.json"
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
SPRINGS 3 echo mode: the repeats break up (prototype, branch feat/echo-mode)

Two things are new here. First, a level correction you asked for: every repeat is now a step down from the hit, the first included (repeat n = hit x g^n, DECAY sets g; at DECAY 0 a single repeat about 10 dB down). Version A is today's echo WITH this fix, so it differs from A on the diffuse page (built before it). Second, four ways for the repeats to wear out. Each sits inside the echo's feedback, so it builds up: the first repeat is untouched, each later one has been through it once more, and by the 4th-6th it's clearly breaking up. None of them makes a repeat louder. Every version is position 3 with TONE noon, SPLASH 0.4, DRIVE 0.25; level matching is on. Rows: one rim at DECAY 0.5 and 0.85 (TENSION noon, 0.4 s), the skank at DECAY 0.5 (MIX 1 and 0.4), a rim clocked at 100 bpm on a dotted 1/8, and DECAY ridden up into the top and back down (in KICKED that's the runaway, so you hear each break-up under heavy feedback).

A_none         today's echo with the level fix: each repeat a darker, quieter copy
B_worn_tape    a worn Space Echo / Black Ark tape: each pass adds its own wow and flutter (the pitch wanders more each time), random oxide dropouts (brief dips, more on older repeats), and saturation that bites harder as a build grows
C_radio_band   dub techno's band-pass in the feedback: each pass narrower around ~450 Hz, so the repeats thin to telephone / radio while keeping their level in the band
D_bbd_grit     a Memory Man style bucket brigade: each pass through a low clock with soft filtering (a little more aliasing grit every time), a compander that pumps and breathes, and a faint clock whine that builds as the repeats pass again
E_crushed      a digital dub delay / sampler: each pass re-sampled without filtering and re-quantised to a few bits (crunch and aliasing that grow), plus sparse crackle

Listen for: is the first repeat still clean in every version? Do the 4th-6th repeats feel worn out, and in which way do you like it? Does the level now step down evenly from the hit (A)? Under the runaway (last row, KICKED), which break-up sounds best and which gets ugly?
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: the repeats break up" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
