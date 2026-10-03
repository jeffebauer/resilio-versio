Wellspring fit round 4 (branch proto/wellspring-fit-4; docs/m8-tuning-backlog.md "Wellspring fit round 4", ADR 0038 round 4)

Tank voicings 5-7 (core/params/TankVoicing.h, Renderer key tank_voicing), on top of round 3's 0-4: 5 = 3 + transducers,
6 = 5 + wide again, 7 = 6 + gentler (low cut with makeup). Needs a Release build in build-r and a Python with numpy, scipy,
soundfile. The Wellspring takes (A clicks, B hits, E skank, D sweep) are read in place from renders/references/wellspring/
(never copied into git; ADR 0009). Outputs go to renders/wellspring_fit4/ (gitignored).

wf4.py              shared: round 3's wf3.py (render, DECAY fit, metrics) at the corrected closest settings (TENSION 0.5,
                    TONE 0.7), plus onset brightness, repeat darkening, the fit tool's tail balance score, tail width,
                    and the sweep (tools/sweep_ir.py) next to the Wellspring's take D
fit_transducers.py  voicing 5's transducers and high-frequency loss            -> fit_transducers.json
fit_lowcut.py       voicing 7's low cut, against the sweep's bass             -> fit_lowcut.json
probe.py            quick renders of voicing / tuning variants ('v=6;wideW=0.7')
measure.py          the table (DECAY re-fitted per voicing)                  -> measure.json
splash_tone.py      SPLASH audibility and Big Knob TONE per voicing           -> splash_tone.json
page.py             the compare page renders/wellspring_fit4/compare/ (then make_review.py, printed)
bench.cpp           desktop ns/sample and firmware pool per voicing
M6 grids: ../wellspring-fit-3/m6_grid.sh "5 6 7" renders/wellspring_fit4/m6, summarised by m6_summary.py.
As if voicing N shipped: cmake -S . -B build-vN ... -DCMAKE_CXX_FLAGS=-DRV_TANK_DEFAULT_VOICING=N, ctest (log to a file).
