# M4 work split and interface contracts

M4 = multiple Springs, stereo, DECAY coupling (SPEC §4.3, §4.4, §7 M4; ADRs 0003, 0012, 0015). Two parallel streams with disjoint files. Same rules as `docs/m1-contracts.md`: don't edit files owned by the other stream, don't edit the top-level `CMakeLists.txt`, don't commit, C++17, `core/` has no platform includes, CONTEXT.md vocabulary.

Build/test: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure`. All existing tests must keep passing, **including `plugin_host_test`** (Plugin == Tank bit-identical) and `test_kick`.

## Stream D — Multi-spring Tank (owns `core/**`, `host/tests/test_spring*.cpp`, new `host/tests/test_tank*.cpp`)

- SPRINGS switch (0 / 0.5 / 1 → 1 / 2 / 3 Springs). Tank API unchanged.
- **Detuning (§4.3):** each Spring gets its own L, K (via transition frequency) and `a` offsets, ±3–8%, fixed per Spring (not random per run). Chosen so Springs don't share modes (AntiRes layer 3, ADR 0010).
- **Stereo (§4.3):** Spring A → L, B → R, C centre with a small cross-feed. 1-Spring mode keeps the M1 decorrelator on R. 2-Spring: A → L, B → R, with light cross-feed so neither side is empty.
- **Levels:** loudness of 1/2/3 Springs within ±1.5 dB for the same input (normalise the Spring sum).
- **SPRINGS change:** ~20 ms crossfade from old to new structure, mid-tail, click-free (ADR 0003). Newly active Springs have been **running silently** (or are pre-filled by feeding them the same input) so they don't start empty. Pick one and justify it by sound and CPU.
- **CPU (SPEC §5, M3 tradeoff order):** 3-Spring mode may use fewer allpass stages per Spring. Put the per-mode stage caps in one table in `core/params/` so M3 profiling can retune them. Report estimated cycles/sample per mode (same method as M1).
- DECAY coupling as M1 (already built): make sure the full-range DECAY sweep is smooth with 1/2/3 Springs.
- Plugin == Renderer stays bit-identical (deterministic, block-size independent).
- **Tests** (`host/tests/test_tank.cpp`): SPRINGS switching click-free in all 6 transitions mid-tail; level match ±1.5 dB; stereo correlation of the wet tail < 0.5 for 2 and 3 Springs; mono_loss_db ≥ −1.5 dB (definition in Stream E: mono fold-down energy vs stereo energy); no comb notch > 6 dB in 200 Hz–5 kHz of the mono sum (1/3-octave smoothed, vs the stereo-average spectrum); DECAY 0→1 over 4 s with 1/2/3 Springs: no clicks, no > 3 dB loudness step between consecutive 100 ms windows; stability grid extended with SPRINGS; determinism; firmware still builds and fits (`make -C firmware size`, report the number). Keep M1 tests passing: update expectations only where M4 intentionally changes them, and say which.

## Stream E — Stereo metrics + review display (owns `host/common/Metrics*`, `host/common/Sidecar*`, `host/tests/test_metrics*.cpp`, `tools/review/**`)

Add to the sidecar `metrics` object (existing keys unchanged). The review page shows them, flagging failures in colour **and** text:

| Key | Definition | Flag when |
|---|---|---|
| `stereo_correlation` | Pearson correlation of L and R over the T60 segment (same segment as `t60_s`). `null` for mono files | > 0.5 (only if the sweep is a stereo wet render; mono refs show "mono") |
| `mono_loss_db` | 10·log10( power(L+R) / (power(L) + power(R)) ) over the whole file: how the energy of a mono fold-down compares to the stereo energy. Uncorrelated L/R → 0 dB, identical → +3 dB, out of phase → very negative. Negative = phase cancellation in mono | < −1.5 |
| `mono_notch_db` | Deepest dip in 200 Hz–5 kHz of the mono-sum power spectrum relative to the stereo-average power spectrum, both 1/3-octave smoothed (8192-point Hann average, T60 segment) | < −6 |
| `max_step_db_100ms` | Largest absolute change in RMS dB between consecutive 100 ms windows, ignoring windows below −60 dBFS and the first 100 ms after each event onset (event = as in t60) | > 3 |

- `--analyze` computes them for stereo files too (e.g. the Wellspring's stereo wet takes).
- Tests on synthetic signals: identical L/R → correlation 1, mono loss +3 dB, no notch; independent noise L/R → correlation ≈ 0, mono loss ≈ 0 dB; R = −L → mono loss < −40 dB; R = L delayed 1 ms → a deep notch near 500 Hz; a 6 dB step in level → max_step ≥ 5.
- Review page: show the four new metrics in each card, with flags. Add a "stereo" filter chip (flagged-stereo only). The `ignore_flags` manifest key (already supported) applies to these too.
