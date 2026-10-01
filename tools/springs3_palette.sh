#!/bin/bash
# SPRINGS 3 palette listening page (PROTOTYPE, branch proto/springs3-palette;
# ADR 0037 proposed; core/params/Springs3Voicing.h).
#
# Six versions on one page, played in sync:
#   A  SPRINGS 2 (today, for contrast)      D  SPRINGS 3, 2: in series
#   B  SPRINGS 3, 0: today                  E  SPRINGS 3, 3: wide
#   C  SPRINGS 3, 1: long tank              F  SPRINGS 3, 4: pan tank
# on clicks, hits, skank, a held C minor pad and the Kick; CLEAN and KICKED
# (columns); DECAY noon and 0.85. MIX 1 (the tank alone), every other knob at
# its default. ~1.2 GB of WAVs, a few minutes.
#
# Usage: tools/springs3_palette.sh [OUT_DIR]   (default renders/springs3_palette)
# Needs build/rv_render from this branch and the stimulus WAVs:
#   python3 tools/make_stimulus.py && python3 tools/make_sustain_stimulus.py
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-renders/springs3_palette}"
R=build/rv_render

VERSIONS=(
    "A_springs2:springs=2"
    "B_today:springs3_voicing=0"
    "C_long:springs3_voicing=1"
    "D_series:springs3_voicing=2"
    "E_wide:springs3_voicing=3"
    "F_pan:springs3_voicing=4"
)

mkdir -p "$OUT"
for v in "${VERSIONS[@]}"; do
    name="${v%%:*}"
    set="${v#*:}"
    for m in clicks hits skank pad; do
        "$R" --sweep "presets/sweeps/proto_springs3_$m.json" --out-dir "$OUT/$name/$m" --set "$set" >/dev/null
    done
    # The Kick: a sweep can't fire Kicks, so one render per ATTITUDE x DECAY
    # (presets/sweeps/m7_kick.json), gathered into a manifest for the page.
    mkdir -p "$OUT/$name/kick"
    for att in CLEAN KICKED; do
        for d in 0.5 0.85; do
            "$R" test_audio/stimulus/05_silence_for_kicks.wav "$OUT/$name/kick/kick_${att}_decay$d.wav" \
                --auto presets/sweeps/m7_kick.json --set springs=3 --set mix=1 --set attitude="$att" \
                --set decay="$d" --set "$set" --sidecar >/dev/null
        done
    done
    python3 - "$OUT/$name/kick" <<'PY'
import json, sys
from pathlib import Path
d = Path(sys.argv[1])
renders = []
for wav in sorted(d.glob("*.wav")):
    side = json.loads(wav.with_suffix(".json").read_text())
    renders.append({"wav": wav.name, "sidecar": wav.with_suffix(".json").name, "params": side.get("params", {})})
man = {"name": "proto_springs3_kick", "input": "test_audio/stimulus/05_silence_for_kicks.wav",
       "ignore_flags": ["click_count", "max_step_db_100ms", "resonance_peak_db"], "renders": renders}
(d / "manifest.json").write_text(json.dumps(man, indent=2))
PY
    echo "rendered $name"
done

cat > "$OUT/README.txt" <<'TXT'
SPRINGS 3 palette (prototype, ADR 0037 proposed). Positions 1 and 2 are unchanged in every version; only what SPRINGS 3 does changes. MIX 1, other knobs at their defaults; level-matched playback is on by default (each version within its panel).
A_springs2   SPRINGS 2 as today, for contrast
B_today      SPRINGS 3 as today: a third Spring in the centre, mostly more density
C_long       long tank: Springs 1.5x longer (a slower, deeper drip), a lower boing, darker, a quarter longer tail
D_series     in series: Spring A feeds Springs B and C (two tanks in a row): thicker, more washed, a doubled boing
E_wide       wide: three clearly different Springs, short and bright left, medium centre, long and dark right
F_pan        pan tank: a short, bright, metallic small tank (shorter, higher Chirp, brighter, shorter tail)
TXT

python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3 palette" --columns attitude --variants folder1 --rows stimulus,decay
echo "page -> $OUT/index.html"
