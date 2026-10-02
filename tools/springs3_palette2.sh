#!/bin/bash
# SPRINGS 3 palette, round 2 listening page (PROTOTYPE, branch
# proto/springs3-palette-2; ADR 0037 proposed; core/params/Springs3Voicing.h).
# Every round 2 voicing keeps today's Spring lengths and repeat timing.
#
# Eight versions on one page, played in sync:
#   A  SPRINGS 2 (today, for contrast)        E  SPRINGS 3, 7: mixed wire gauges
#   B  SPRINGS 3, 0: today                    F  SPRINGS 3, 8: coupled
#   C  SPRINGS 3, 5: pan, brighter only       G  SPRINGS 3, 9: diffuse
#   D  SPRINGS 3, 6: pan, higher Chirp only   H  SPRINGS 3, 10: cross-fed wide
# on clicks, hits, skank, a held C minor pad and the Kick; CLEAN and KICKED
# (columns); DECAY noon and 0.85. MIX 1 (the tank alone), every other knob at
# its default. ~1.6 GB of WAVs, a few minutes. (Round 1: tools/springs3_palette.sh.)
#
# Usage: tools/springs3_palette2.sh [OUT_DIR]   (default renders/springs3_palette2)
# RV_RENDER picks the Renderer (default build/rv_render, from this branch);
# RV_ONLY="C_pan_brighter F_coupled" re-renders only those versions.
# Needs the stimulus WAVs:
#   python3 tools/make_stimulus.py && python3 tools/make_sustain_stimulus.py
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-renders/springs3_palette2}"
R="${RV_RENDER:-build/rv_render}"

VERSIONS=(
    "A_springs2:springs=2"
    "B_today:springs3_voicing=0"
    "C_pan_brighter:springs3_voicing=5"
    "D_pan_chirp:springs3_voicing=6"
    "E_wire_gauges:springs3_voicing=7"
    "F_coupled:springs3_voicing=8"
    "G_diffuse:springs3_voicing=9"
    "H_cross_fed_wide:springs3_voicing=10"
)

mkdir -p "$OUT"
for v in "${VERSIONS[@]}"; do
    name="${v%%:*}"
    set="${v#*:}"
    # RV_ONLY="C_pan_brighter F_coupled" re-renders just those versions (then the page).
    if [ -n "${RV_ONLY:-}" ] && [[ " $RV_ONLY " != *" $name "* ]]; then continue; fi
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
    python3 - "$OUT/$name/kick" "$set" <<'PY'
import json, sys
from pathlib import Path
d = Path(sys.argv[1])
# A single render's sidecar doesn't record the hidden springs3_voicing key: add the version's --set.
key, val = sys.argv[2].split("=")
renders = []
for wav in sorted(d.glob("*.wav")):
    side = json.loads(wav.with_suffix(".json").read_text())
    params = side.get("params", {})
    params[key] = val if key == "springs" else int(val)
    renders.append({"wav": wav.name, "sidecar": wav.with_suffix(".json").name, "params": params})
man = {"name": "proto_springs3_kick", "input": "test_audio/stimulus/05_silence_for_kicks.wav",
       "ignore_flags": ["click_count", "max_step_db_100ms", "resonance_peak_db"], "renders": renders}
(d / "manifest.json").write_text(json.dumps(man, indent=2))
PY
    echo "rendered $name"
done

cat > "$OUT/README.txt" <<'TXT'
SPRINGS 3 palette, round 2 (prototype, ADR 0037 proposed). Positions 1 and 2 are unchanged in every version; only what SPRINGS 3 does changes, and every round 2 voicing keeps today's Spring lengths and repeat timing (first echo and echo spacing as B). MIX 1, other knobs at their defaults; level-matched playback is on by default (each version within its panel).
A_springs2          SPRINGS 2 as today, for contrast
B_today             SPRINGS 3 as today: a third Spring in the centre, mostly more density
C_pan_brighter      the pan tank's brightness only: less bass, a brighter tail; today's lengths, boing and tail length
D_pan_chirp         the pan tank's boing only: a higher, quicker, more metallic Chirp; today's lengths, brightness and tail length
E_wire_gauges       three wire thicknesses: each Spring its own boing (crisp left, low and long right, high centre), a cluster of boings per hit
F_coupled           the three Springs share energy every round trip: echoes multiply and bloom instead of dripping
G_diffuse           more smear inside each Spring: a smoother, softer-edged tail
H_cross_fed_wide    bright left, today's centre, dark right, the left and right Springs feeding each other so both ears hear both colours
TXT

python3 tools/review/make_review.py "$OUT" --title "SPRINGS 3 palette, round 2" --columns attitude --variants folder1 --rows stimulus,decay
echo "page -> $OUT/index.html"
