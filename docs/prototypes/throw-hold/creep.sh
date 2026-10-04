#!/bin/bash
# Hold creep (ADR 0040): a C minor pad held 60 s at -3 / -12 dBFS into
# CLEAN / DRIVEN DECAY 1, MIX 1, voicings 0 (freeze) and 1 (layer); wet
# level per 10 s (the bed keeper was to come only above +1 dB / 10 s). Run from the repo
# root after make_page.sh has written the stimuli:
#   FEAT=build-feat/rv_render docs/prototypes/throw-hold/creep.sh [out]
set -e
FEAT=${FEAT:-build-feat/rv_render}
OUT=${1:-renders/feat_throw_hold}
ST="$OUT/stim"
D="$OUT/creep"
mkdir -p "$D"
for att in CLEAN DRIVEN; do
  for v in 0 1; do
    for pad in pad60 pad60_m12; do
      "$FEAT" "$ST/$pad.wav" "$D/${pad}_${att}_v${v}.wav" --set attitude=$att --set decay=1 --set mix=1 \
        --set springs=2 --set drive=0.5 --set hold_voicing=$v > /dev/null &
    done
  done
done
wait
python3 - "$D" <<'EOF'
import glob, math, struct, sys, wave
for p in sorted(glob.glob(sys.argv[1] + "/*.wav")):
    w = wave.open(p)
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(n)
    sr = w.getframerate()
    def sample(i):
        o = (i * ch) * sw
        return int.from_bytes(raw[o:o + sw], "little", signed=True) / float(1 << (8 * sw - 1))
    lv = []
    for k in range(6):
        a, b = int((10 * k + 8) * sr), int((10 * k + 10) * sr)
        acc = sum(sample(i) ** 2 for i in range(a, b, 4)) / ((b - a) / 4)
        lv.append(10 * math.log10(acc + 1e-20))
    # Net drift from the 20 s window to the 60 s one, per 10 s (the pad's own
    # beating moves single windows by ~1 dB either way).
    drift = (lv[5] - lv[2]) / 3.0
    print(f"{p.rsplit('/', 1)[1]:26s} " + " ".join(f"{x:6.1f}" for x in lv) + f" dBFS  drift from 20 s {drift:+.2f} dB / 10 s")
EOF
find "$D" -name '*.wav' -delete
