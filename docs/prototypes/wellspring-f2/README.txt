F round 2 (branch proto/wellspring-f2; docs/m8-tuning-backlog.md "F round 2: noon low end and SPRINGS 3", ADR 0038 / 0037 "Round F2")

Owner on candidate F (3 Oct 2026): "with tone at 50%, it's just a tad too thin/high passed, and there isn't much of a
noticeable difference between 2 and 3 springs." Two hidden Renderer keys: f_lowcut_voicing (0 = F, 1-3 gentler low cut;
TankVoicing.h kFLowCutSteps) and springs3_voicing 11-14 (made on F; Springs3Voicing.h). Needs a Release build in build-r
and a Python with numpy, scipy, soundfile (none on this Mac: use a venv). Scratch renders go to build-r/f2_measure
(F2_OUT to move them).

page.py          the two picking pages renders/f2_lowend/ and renders/f2_springs3/ (main checkout), then make_review.py
                 (printed); `page.py verify` checks every column's voicing in its manifest and sidecar and re-renders one
                 sound per column bit for bit
f2m.py           shared measures: sweep response (tools/sweep_ir.py method) at 63-300 Hz, tail low-mid balance, wet RMS,
                 bass share, SPRINGS 3 character (T60, centroid, L/R corr, drip index, echo density, jump), the swell
explore_lc.py    low cut candidates at TONE noon (RV_TANKV_TUNE "name:lcHpHz=...,lcShelfDb=...")
tone_lc.py       each low cut step across TONE 0 / .25 / .5 / .75 / 1
s3cmp.py         SPRINGS 2 vs 3 (uncoupled, coupled) on tank voicings 0 and 7
s3tanks.py       the same along the tank voicings 0, 1, 2, 3, 5, 6, 7 (why coupled stands out less on F)
s3var.py         SPRINGS 3 voicings on F vs SPRINGS 2 (character and swell)
lvl.py           wet level vs SPRINGS 2 on hits / skank / pad / clicks
swmono.py        the swell's level vs SPRINGS 2, stereo and mono, DECAY x TENSION
m6_lowcut.sh     M6 Ringing + Howl grids per low cut step (summarise with ../wellspring-fit-3/m6_summary.py)
SPRINGS 3 M6 grid: python3 tools/springs3_m6_grid.py --render build-r/rv_render --voicings 11,12,13,14
As if shipped: cmake -S . -B build-lcN ... -DCMAKE_CXX_FLAGS=-DRV_F_LOWCUT_DEFAULT=N (or -DRV_SPRINGS3_DEFAULT_VOICING=N), ctest.
