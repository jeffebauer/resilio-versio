#!/bin/bash
# Diffuse repeats listening page (PROTOTYPE; echo_diffuse_voicing, core/params/EchoVoicing.h kDiffuse).
# Usage (from the repo root or a worktree):
#   tools/echo_diffuse_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/feat_echo_diffuse (in this checkout).
# A/B/C/D = voicings 0 none / 1 light / 2 medium / 3 heavy, SPRINGS 3 (echo
# mode), CLEAN and KICKED columns, level matching on.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/feat_echo_diffuse}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/clean" "$OUT/kicked"

python3 - "$IN" <<'EOF'
import random, sys, wave, struct
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
ms.write("rim_clocked.wav", place([0.6, 0.6 + 4 * 0.6], 10.0))  # rims on beats 1 and 5 at 100 bpm
# The skank stimulus with 6 s of silence after it (the repeats ring out).
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

V=(A_none B_light C_medium D_heavy)
for A in CLEAN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=3 --set attitude=$A --set tone=0.5 --set splash=0.4 --set drive=0.25 --set wobble=0.5"
  for v in 0 1 2 3; do
    tag=${V[$v]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set echo_diffuse_voicing=$v "${@:3}" >/dev/null; }
    r "$IN/rim.wav"      rim_decay050_tension050      --set decay=0.5  --set tension=0.5  --set mix=1
    r "$IN/rim_long.wav" rim_decay050_tension025      --set decay=0.5  --set tension=0.25 --set mix=1
    r "$IN/rim_long.wav" rim_decay085_tension050      --set decay=0.85 --set tension=0.5  --set mix=1
    r "$IN/rim_long.wav" rim_decay085_tension025      --set decay=0.85 --set tension=0.25 --set mix=1
    r "$IN/skank.wav"    skank_decay050_mix100        --set decay=0.5  --set tension=0.5  --set mix=1
    r "$IN/skank.wav"    skank_decay050_mix040        --set decay=0.5  --set tension=0.5  --set mix=0.4
    r "$IN/rim_clocked.wav" rimclocked_100bpm_dotted8 --set decay=0.6  --set tension=0.5  --set mix=1 --auto "$IN/clock.json"
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
SPRINGS 3 echo mode: diffuse repeats (prototype, branch feat/echo-mode)

Your idea: each repeat a little more diffuse than the one before, so the sound wears out as it repeats. A short diffuser sits in the tape's feedback only, so it compounds: the first repeat is exactly today's, the second has been through the diffuser once, the third twice... It adds no level (each repeat is as loud as today's), it only smears the repeat in time. Every version is position 3 (echo mode) with TONE noon, SPLASH 0.4, DRIVE 0.25; level matching is on. Rows: one rim at DECAY 0.5 and 0.85 with the echo at 0.4 s (TENSION noon) and 0.9 s (TENSION 0.25), the skank at DECAY 0.5 (MIX 1 and 0.4), and a rim clocked at 100 bpm on a dotted 1/8.

A_none     today's echo: every repeat a darker copy of the hit
B_light    each repeat a little softer-edged; by the 5th-6th the hit is rounded but still a hit
C_medium   worn by the 3rd-4th repeat: the attack smeared into a short swish
D_heavy    blurred into a wash by the 4th-5th repeat: the later repeats are a smear, more texture than hit

Listen for: is the first repeat still clearly the hit in every version? Where does the wearing-out start to feel right (by the 4th-6th repeat)? Does any version sound metallic or "ringy" rather than worn? Does the clocked row stay rhythmic enough with the smear?
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3: diffuse repeats" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
