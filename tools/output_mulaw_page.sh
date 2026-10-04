#!/bin/bash
# The output's bit depth listening page (PROTOTYPE; output_bits_voicing,
# core/params/OutputVoicing.h, docs/prototypes/output-mulaw/README.md).
# Usage (from the repo root or a worktree):
#   tools/output_mulaw_page.sh [RENDER_BIN] [OUT_DIR]
# Defaults: build/rv_render, renders/feat_output_mulaw (in this checkout).
# A = today, B = mu-law on the whole output (DRIVEN 12-bit, KICKED 8-bit);
# columns CLEAN / DRIVEN / KICKED (CLEAN: A = B, as a check); level matching on.
set -e
cd "$(dirname "$0")/.."
BIN=${1:-build/rv_render}
OUT=${2:-renders/feat_output_mulaw}
IN="$OUT/_in"
mkdir -p "$IN" "$OUT/clean" "$OUT/driven" "$OUT/kicked"

python3 - "$IN" <<'EOF'
import random, sys, wave
from pathlib import Path
sys.path.insert(0, "tools")
import make_stimulus as ms
out = Path(sys.argv[1])
ms.OUT = out
# A sparse rim: -6, -12, -18 dBFS, 5 s apart, so each tail fades out alone.
rng = random.Random(1)
hit = ms.rim(rng)
x = [0.0] * int(16.0 * ms.SR)
for k, lv in enumerate((-6, -12, -18)):
    i = int((0.5 + 5.0 * k) * ms.SR)
    for j, s in enumerate(hit):
        x[i + j] += s * ms.db(lv)
ms.write("rim_sparse.wav", x)
# The stimulus WAVs with 5 s of silence after them (room for the tail).
for name in ("02_hits", "04_skank", "10_pad_cminor"):
    with wave.open(f"test_audio/stimulus/{name}.wav") as w:
        p = w.getparams(); frames = w.readframes(p.nframes)
    with wave.open(str(out / f"{name}.wav"), "wb") as w:
        w.setparams(p)
        w.writeframes(frames + b"\0" * (p.sampwidth * p.nchannels * 5 * p.framerate))
print("stimuli ok")
EOF

V=(A_today B_mulaw)
for A in CLEAN DRIVEN KICKED; do
  a=$(echo $A | tr A-Z a-z)
  common="--set springs=2 --set attitude=$A --set decay=0.5 --set tone=0.5 --set tension=0.5 --set splash=0.3 --set drive=0.4"
  for v in 0 1; do
    tag=${V[$v]}
    r() { "$BIN" "$1" "$OUT/$a/$2_${tag}.wav" $common --set output_bits_voicing=$v "${@:3}" >/dev/null; }
    r "$IN/02_hits.wav"       1_hits_mix050        --set mix=0.5
    r "$IN/04_skank.wav"      2_skank_mix050       --set mix=0.5
    r "$IN/10_pad_cminor.wav" 3_pad_mix050         --set mix=0.5
    r "$IN/rim_sparse.wav"    4_rim_sparse_mix100  --set mix=1
    r "$IN/rim_sparse.wav"    5_rim_sparse_decay085_mix100 --set mix=1 --set decay=0.85
    r "$IN/04_skank.wav"      6_skank_mix000_dry   --set mix=0
    r "$IN/04_skank.wav"      7_skank_mix040       --set mix=0.4
    r "$IN/02_hits.wav"       8_hits_mix100        --set mix=1
  done
  echo "$A"
done
cat > "$OUT/README.txt" <<'EOF'
The whole output through mu-law in DRIVEN and KICKED (prototype, branch proto/output-mulaw)

You liked the echo's 8-bit mu-law repeats but heard them as subtle there, and asked for the same "box" on everything that comes out in DRIVEN and KICKED: dry and wet, every SPRINGS position, after MIX. DRIVEN = 24 kHz / 12-bit mu-law, KICKED = 24 kHz / 8-bit mu-law, CLEAN untouched (A and B are identical in the CLEAN column: that's the check). The 24 kHz part goes through proper filters both ways (no new pitches, nothing folds back), so the top octave above ~11 kHz is gone in DRIVEN and KICKED: that is part of the 24 kHz sound. The bits are against full scale, so the loudest parts get the coarsest steps and quiet parts keep finer ones (mu-law); a fading tail turns into soft grain, then silence, never a stuck buzz, and silence in is silence out (no hiss when nothing plays). 2 Springs, DECAY noon (the fifth row 0.85), TONE / TENSION noon, SPLASH 0.3, DRIVE 0.4; level matching is on.

A_today   what main does now
B_mulaw   the box on the whole output: DRIVEN 24 kHz / 12-bit mu-law, KICKED 24 kHz / 8-bit mu-law

Listen for: in KICKED, the grit on the dry hits and stabs themselves (rows 6 and 8 especially), not just the tail; whether DRIVEN's 12-bit reads as "a bit of tape-era digital" or as nothing (it's fine: about -57 dB under the signal, mostly heard as the missing top octave); the rims' tails (rows 4 and 5): soft grain as they fade, then clean silence, no buzz or whistle; any new pitch or chirp (there shouldn't be). Row 6 is MIX fully left: no longer a clean passthrough in DRIVEN and KICKED, by your choice.
EOF
rm -rf "$IN"
python3 tools/review/make_review.py "$OUT" --title "Mu-law on the whole output (DRIVEN 12-bit, KICKED 8-bit)" --columns attitude --variants variant \
  --rows material --level-match
echo "page: $OUT/index.html"
