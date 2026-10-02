Wellspring fit round 3 (branch proto/wellspring-fit-3; docs/m8-tuning-backlog.md "Wellspring fit round 3", ADR 0038 Proposed)

Tank voicings 0-4 (core/params/TankVoicing.h, Renderer key tank_voicing): 0 today, 1 Sweep, 2 + stereo together,
3 + diffusion, 4 + gentler. Needs a Release build in build-r (cmake -S . -B build-r -G Ninja -DCMAKE_BUILD_TYPE=Release
-DRV_BUILD_PLUGIN=OFF) and a Python with numpy, scipy, soundfile. The Wellspring takes are read in place from
renders/references/wellspring/ (never copied into git; ADR 0009). Outputs go to renders/wellspring_fit3/ (gitignored).

wf3.py            shared: render a voicing at the closest settings (RV_TANKV_TUNE overrides TankVoicing.h for fitting),
                  measure it next to the Wellspring (tools/wellspring_character.py, tools/wellspring_settings_fit.py,
                  wfmetrics.py), DECAY bisection to the Wellspring's broadband T60
wfmetrics.py      first-arc / arrival / rise metrics, copied from proto/wellspring-fit
fit_sweep.py      voicing 1's Sweep re-fitted on today's tank          -> fit_sweep.json
fit_diffusion.py  voicing 3's Loop diffusers                           -> fit_diffusion.json
fit_gentle.py     voicing 4's low cut, damping, high path T60          -> fit_gentle.json (fit_gentle_round1.json: with the
                  dropped "lows last longer" Loop shelf)
measure.py        the table: python3 measure.py --fit-decay             -> measure_s2.json
page.py           the listening page (A = the Wellspring, B-F = voicings 0-4)
m6_grid.sh        M6 Ringing + Howl grids per voicing; m6_summary.py summarises them
bench.cpp         desktop ns/sample per voicing and the firmware pool per voicing

Running the whole test suite as if voicing N shipped: a scratch build with
  cmake -S . -B build-vN -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF -DCMAKE_CXX_FLAGS=-DRV_TANK_DEFAULT_VOICING=N
then ctest --test-dir build-vN (log to a file, read the "tests passed" line).
