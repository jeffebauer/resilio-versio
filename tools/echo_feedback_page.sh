#!/bin/bash
# Echo feedback cap + KICKED 10-bit listening page (ADR 0041 / 0042
# amendments, owner 5 Oct 2026; core/params/EchoVoicing.h "Feedback",
# core/params/OutputVoicing.h).
# Usage (from the repo root or a worktree):
#   tools/echo_feedback_page.sh A_RENDER_BIN B_RENDER_BIN [OUT_DIR]
# A = a Renderer built from main before the change (KICKED's top ran away,
# CLEAN / DRIVEN faded at the top, KICKED 8-bit), B = this branch.
# Columns CLEAN / DRIVEN / KICKED; level matching on.
set -e
cd "$(dirname "$0")/.."
A_BIN=${1:?A renderer (main)}
B_BIN=${2:?B renderer (this branch)}
OUT=${3:-renders/tune_echo_feedback}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/clean" "$OUT/driven" "$OUT/kicked"

python3 - "$IN" <<'EOF'
import sys, wave
from pathlib import Path
import random
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
ms.write("rim_ride.wav", place([0.5, 4.5, 8.5], 27.0))   # rims while DECAY rises, none after
ms.write("rim_single.wav", place([0.5], 21.0))            # one rim, DECAY held at 1 for 20 s
for name, tail in (("04_skank", 6), ("02_hits", 5)):
    with wave.open(f"test_audio/stimulus/{name}.wav") as w:
        p = w.getparams(); frames = w.readframes(p.nframes)
    with wave.open(str(out / f"{name}.wav"), "wb") as w:
        w.setparams(p)
        w.writeframes(frames + b"\0" * (p.sampwidth * p.nchannels * tail * p.framerate))
print("stimuli ok")
EOF

# DECAY ridden: 0 -> 1 over 10 s (rims at 0.5, 4.5, 8.5 s), held at 1 to
# 18 s with no new input, back to noon over 1 s, then 8 s to die away.
cat > "$IN/ride.json" <<'EOF'
{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.0}, {"t": 10.0, "key": "decay", "value": 1.0},
                   {"t": 18.0, "key": "decay", "value": 1.0}, {"t": 19.0, "key": "decay", "value": 0.5} ] }
EOF

V=(A_today B_new)
for A in CLEAN DRIVEN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  echo_k="--set springs=3 --set attitude=$A --set tension=0.5 --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5 --set mix=1"
  for v in 0 1; do
    tag=${V[$v]}
    BIN=$A_BIN; [ $v = 1 ] && BIN=$B_BIN
    "$BIN" "$IN/rim_ride.wav"   "$OUT/$a/1_rim_decay_ride_0_1_noon_${tag}.wav" $echo_k --auto "$IN/ride.json" >/dev/null
    "$BIN" "$IN/rim_single.wav" "$OUT/$a/2_rim_decay1_20s_${tag}.wav"         $echo_k --set decay=1 >/dev/null
    "$BIN" "$IN/04_skank.wav"   "$OUT/$a/3_skank_decay090_${tag}.wav"         $echo_k --set decay=0.9 >/dev/null
    if [ $A = KICKED ]; then # the mu-law row: KICKED only (A 8-bit, B 10-bit), SPRINGS 2, MIX 0.5
      "$BIN" "$IN/02_hits.wav" "$OUT/$a/4_hits_mulaw_mix050_${tag}.wav" --set springs=2 --set attitude=KICKED --set decay=0.5 \
        --set tone=0.5 --set tension=0.5 --set splash=0.3 --set drive=0.4 --set mix=0.5 >/dev/null
    fi
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
Echo mode: the same held feedback in every ATTITUDE, KICKED capped; KICKED's output 10-bit (branch tune/echo-feedback)

Your notes from playing the plugin: KICKED's persistent repeats around DECAY 91 % felt right, CLEAN and DRIVEN should get the same, and KICKED shouldn't run on into runaway chaos. Now, in SPRINGS 3 (echo mode), DECAY fully right gives exactly what KICKED's DECAY 0.92 gave before, in every ATTITUDE: the repeats keep coming at a roughly steady level, held by the tape's saturation, and never grow into a runaway. KICKED's knob keeps its shape (just a lower top, so the end of the knob still moves); CLEAN and DRIVEN play as before up to DECAY 0.85, then rise to the same top. Repeats stop fading from about DECAY 0.90 in KICKED and 0.94 in CLEAN and DRIVEN. Backing DECAY off to noon, the held repeats die away in about 2.5 s. Echo mode, TENSION noon (0.4 s), TONE noon, SPLASH 0.4, DRIVE 0.25, MIX 1. Row 4 is the output's bit depth in KICKED (2 Springs, DECAY noon, MIX 0.5, DRIVE 0.4): 8-bit before, 10-bit now. Level matching is on.

A_today   what main does now (KICKED's top runs away; CLEAN / DRIVEN fade at the top; KICKED 8-bit)
B_new     the held top in every ATTITUDE, KICKED capped at its old 0.92; KICKED 10-bit

Listen for: row 1, the knob going up (rims while it rises), the hold with no new input from 10 to 18 s, and the die-away after DECAY comes back to noon at 18-19 s; row 2, one rim and the repeats holding for 20 s: does the held level feel steady, or does the slow climb over the first ~10 s bother you? (It settles where the tape saturates, about the same whatever the hit.) Row 3, the skank at DECAY 0.9: just under the hold in CLEAN / DRIVEN (long build), right on it in KICKED. Row 4, KICKED's grit: a little less digital than 8-bit, still grittier than DRIVEN's 12.
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "Echo feedback: the held top in every ATTITUDE; KICKED 10-bit" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
