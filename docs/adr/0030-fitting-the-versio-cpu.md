# 0030 — Fitting the Versio's CPU (M3)

**Status:** Accepted, 29 Sep 2026 (owner chose the two trade-offs). Amends SPEC §5 (how the budget is met); block size stays 48.

**Context:** The first M3 run on the Versio (Daisy Seed 2, STM32H750 at 480 MHz, caches on) measured the worst case (3 Springs, loosest TENSION, KICKED, DRIVE max) at 83 % average and 100 % peak CPU against a 65 % target; the desktop estimate had been 65–70 %. Per-section timing (`core/dsp/ProfileHook.h`, profile firmware only) and on-chip micro-benchmarks (`firmware/m3_bench.cpp`) located the cost.

**Decision:** sound-neutral changes first, measured on the hardware one at a time (`firmware/README.md` "M3 results", runs 1–10):
- Delay pool in DTCM (was AXI SRAM behind a 16 KB cache): ~1 point.
- Firmware built with `-ffp-contract=off`: GCC's accumulate-form `vfma` on the M7 serialised every multiply-add (~9 cycles, even independent ones); plain `vmul` + `vadd` pipelines. ~5 points. Output differs from the fused build 84–95 dB down.
- Chirp sections pipelined (`Spring::processLow`: the next section's D{v} computed during this section's x chain; 21 → 14 cycles per section), bit-exact. Loop gain design caches its fixed-frequency cos and LoopSat latency, bit-exact. Together ~13 points.
- Springs take full coefficient redesigns in turn, one per control tick (**owner, over block 96**), so no audio block carries all three: the peak burst 13 → 6 %. The Splash Jolt's allpass coefficient still reaches every Spring on the same tick (the owner heard extra undulation on KICKED hits when it was staggered too). Static renders match the pre-stagger build 112–118 dB down.
- Hot-path saturators multiply by 1/k instead of dividing (14 divides per sample fewer), 95–113 dB down.

**Owner choices:** idle Springs keep running at SPRINGS 1–2 (seamless SPRINGS switching; the worst case is 3 Springs anyway). Block 96 (+1 ms latency) held in reserve, only if the sound-neutral work falls short.

**Result:** worst case 83 → **60.7 % average**, 100 → **63.3 % peak** (run 12, 30 Sep 2026); highest peak at any corner 64.0 %. Run 11 (all of the above) measured 62.1 / 67.8 %; run 12 then split each Spring's redesign into parts (one heavy step per audio block) and moved control-tick work to only when its inputs change: bit-identical with static settings, a TENSION change reaches a Spring within ~6 ms (was 1.3 ms). The ≤ 65 % target is met; block 96 stays in reserve.

**About the 65 % target (owner asked, 30 Sep):** SPEC §5 chose it as a safety margin at the start, not from a measurement. The margin covers what the profile run doesn't: the 24 corners are fixed settings with a synthetic signal (not every combination of Kicks, switch flips and fast CV), the release callback also reads the controls and feeds the LEDs, cache timing varies, and later features need room. Rules of thumb for embedded audio put a sensible peak ceiling around 65–80 %. Raising it (e.g. for the sweet-tank ingredients) is an owner decision with its own ADR.

**Rejected:** running the Chirp chain stage by stage over 32-sample runs (bit-exact but slower on the M7: 80 → 87.5 %), "split" and three-Springs-interleaved loop shapes (on-chip bench: no better than pipelining).
