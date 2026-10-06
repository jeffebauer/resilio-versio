#!/bin/bash
# Minisite demos: each render is our own Renderer on our own stimulus.
# To reproduce (from the repo root, never in build/):
#   cmake -S . -B build-site -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF
#   cmake --build build-site --target rv_render
#   mkdir -p build-site/demo && cp docs/minisite/tools/demos/*.{py,sh} build-site/demo/
#   python3 build-site/demo/build_stimuli.py test_audio/stimulus build-site/demo
#   bash build-site/demo/render.sh
#   (cd build-site/demo && python3 assemble.py "$PWD/../../docs/minisite/assets/audio")
set -euo pipefail
cd "$(dirname "$0")"
R=../rv_render
P=../../presets/starting_points
w() { printf '%s\n' "$2" > "$1"; }
w a01.json '{ "breakpoints": [ {"t": 0.0, "key": "mix", "value": 0.0}, {"t": 6.70, "key": "mix", "value": 0.0}, {"t": 6.75, "key": "mix", "value": 0.45} ] }'
w a04.json '{ "buttons": [[0.0, 0.02], [5.17, 5.45], [11.57, 11.85]] }'
w a05.json '{ "breakpoints": [ {"t": 0.0, "key": "splash", "value": 0.0}, {"t": 6.70, "key": "splash", "value": 0.0}, {"t": 6.75, "key": "splash", "value": 0.9} ] }'
w a06.json '{ "breakpoints": [ {"t": 0.0, "key": "tone", "value": 0.5}, {"t": 2.6, "key": "tone", "value": 0.5}, {"t": 7.0, "key": "tone", "value": 1.0}, {"t": 10.0, "key": "tone", "value": 1.0}, {"t": 13.5, "key": "tone", "value": 0.5} ] }'
w a07.json '{ "breakpoints": [ {"t": 0.0, "key": "wobble", "value": 0.1}, {"t": 6.8, "key": "wobble", "value": 0.1}, {"t": 7.6, "key": "wobble", "value": 0.9} ] }'
w a08.json '{ "clock_bpm": 75 }'
w a09.json '{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.95}, {"t": 10.0, "key": "decay", "value": 0.95}, {"t": 11.0, "key": "decay", "value": 0.6} ] }'
w a10.json '{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.5}, {"t": 0.3, "key": "decay", "value": 1.0} ] }'

# 01 dry, then wet (Tubby snare splash)
$R groove.wav r01.wav --preset $P/tubby_snare_splash.json --auto a01.json --sidecar
# 02 TANK 1, then TANK 2 (two renders, back to back)
for t in 1 2; do $R snare_one.wav r02_$t.wav --set springs=$t --set attitude=TAPE --set mix=0.5 --set decay=0.5 --set tone=0.5 --set tension=0.5 --set splash=0.5 --set drive=0.4 --set wobble=0.45 --sidecar; done
# 03 CLEAN, TAPE, VALVE (three renders, back to back)
for a in CLEAN TAPE VALVE; do $R snare_one.wav r03_$a.wav --set springs=2 --set attitude=$a --set mix=0.5 --set decay=0.55 --set tone=0.5 --set tension=0.45 --set splash=0.6 --set drive=0.65 --set wobble=0.45 --sidecar; done
# 04 throw: the button held on the snares of bars 2 and 4
$R groove.wav r04.wav --set springs=2 --set attitude=TAPE --set mix=0.5 --set decay=0.7 --set tone=0.55 --set tension=0.35 --set splash=0.6 --set drive=0.5 --set wobble=0.45 --auto a04.json --sidecar
# 05 SPLASH 0 for two bars, then 0.9
$R groove.wav r05.wav --set springs=2 --set attitude=TAPE --set mix=0.5 --set decay=0.5 --set tone=0.5 --set tension=0.45 --set drive=0.4 --set wobble=0.45 --auto a05.json --sidecar
# 06 Big Knob ride: TONE noon -> fully right -> noon
$R groove.wav r06.wav --set springs=2 --set attitude=TAPE --set mix=0.6 --set decay=0.75 --set tension=0.4 --set splash=0.6 --set drive=0.5 --set wobble=0.45 --auto a06.json --sidecar
# 07 WOBBLE: drift (left), then warble (right), on a held pad
$R pad.wav r07.wav --set springs=2 --set attitude=CLEAN --set mix=0.7 --set decay=0.6 --set tone=0.45 --set tension=0.5 --set splash=0.2 --set drive=0.25 --auto a07.json --sidecar
# 08 echo skank: TANK ECHO clocked at 75 bpm, TENSION noon = dotted 1/8
$R skank_clocked.wav r08.wav --set springs=ECHO --set attitude=TAPE --set mix=0.5 --set decay=0.6 --set tone=0.5 --set tension=0.5 --set splash=0.3 --set drive=0.35 --set wobble=0.45 --auto a08.json --sidecar
# 09 VALVE Howl (Drowned Howl), DECAY pulled back at 10 s
$R snare_long.wav r09.wav --preset $P/drowned_howl.json --auto a09.json --sidecar
# 10 Hold: CLEAN, DECAY to the top, the chord stops at 8 s
$R pad_then_silence.wav r10.wav --set springs=2 --set attitude=CLEAN --set mix=0.6 --set tone=0.45 --set tension=0.5 --set splash=0.2 --set drive=0.25 --set wobble=0.43 --auto a10.json --sidecar
