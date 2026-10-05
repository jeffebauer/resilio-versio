Wellspring fit round 5 (branch fit/round5; docs/m8-tuning-backlog.md "Wellspring round 5", ADR 0038 "Round 5", proposed)

Tank voicings 8 (B: the front, the resonance, the stereo image), 9 (C: B + held sounds that settle flat) and 10 (D: half
way from 7 to C, the stereo image whole) on top of 7 as shipped (core/params/TankVoicing.h "Round 5", Renderer key
tank_voicing). Stdlib only (no numpy on this Mac): the measurements are a small C++ program. Needs a Release build in
build-r (cmake -S . -B build-r -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF) and the stimulus copied into
this checkout's test_audio/stimulus. The Wellspring takes are read in place from the main checkout's test_audio/reference
(never copied, never committed: ADR 0009).

r5an.cpp     the measurements (tone bursts, pink noise, envelopes, clicks, sweep distortion); build:
             clang++ -std=c++17 -O2 -I host/common docs/prototypes/wellspring-fit-5/r5an.cpp -o build-r/r5an
r5.py        render a voicing at the session-2 settings and measure it next to the Wellspring:
             r5.py ref | ours V | probe TAKES KEYS SPEC... | json V... (SPEC: '8', '8;tuneKey=val,...', '8|knob=val')
fit5.py      coordinate search of voicing 8's numbers (RV_TANKV_TUNE) -> fit5.json; runs used: 3 rounds, then 2 with the
             front weighted x2 and T60 x6, then 2 with the 1 kHz cut fixed by hand (--freeze=r5EqHz,r5EqDb,r5EqQ)
dist5.py     target 5: where CLEAN's distortion comes from (sweep, one source off at a time)
pitch5.py    C: WOBBLE's pitch movement and the held level when the Loops' wow moves to the pickups
tone5.py     TONE's character (tail brightness and level, TONE 0 / .25 / .5 / .75 / 1) per voicing
levels5.py   the page's levels re A
page5.py     the listening page (main checkout renders/fit_round5/), then the make_review.py line it prints
bitcheck.sh  the default voicing renders bit for bit as main (needs an rv_render built from main)
m6_d075.sh   M6 Ringing sweeps for one voicing with tuning overrides
gates.sh     M6 grids for 7-10 and the suite as if 8 / 9 / 10 shipped (build-v8..10)
fw_sizes.sh  firmware sizes as if 8 / 9 / 10 were the default, then the default again
bench.cpp    desktop ns/sample (SPRINGS 2 and echo mode worst cases, the session-2 settings) and the firmware pool
pool.cpp     the firmware pool per voicing
