# Prototype: mu-law on the whole output in DRIVEN / KICKED

**Shipped 4 Oct 2026 (ADR 0042):** the owner picked B on every panel. `output_bits_voicing` 1 is now the default everywhere, and 0 renders "before the box". The 8 suites below are adapted as proposed (see ADR 0042 "Tests adapted"). The rest of this page is the prototype record.

Branch `proto/output-mulaw` (from `main` f399902). Owner's decision, 4 Oct 2026: the echo's 8-bit mu-law "box" (`feat/echo-mode` 2792b14, `echo_bits_voicing` D) applied to the whole output, dry and wet, after MIX, in every SPRINGS position:

| ATTITUDE | Output |
|---|---|
| CLEAN | untouched, bit for bit (MIX 0 is still a clean passthrough) |
| DRIVEN | 24 kHz / 12-bit mu-law |
| KICKED | 24 kHz / 8-bit mu-law |

Hidden Renderer key `output_bits_voicing`: 0 = today (the default; firmware and plugin never set it), 1 = the table above. `rv_render in.wav out.wav --set output_bits_voicing=1`. Firmware as if picked: build with `-DRV_OUTPUT_BITS_DEFAULT=1` (`OutputBits::kBuilt`; with the default 0 the firmware compiles it out).

## What changed (small, self-contained output stage)

- `core/params/OutputVoicing.h` (new): depths per ATTITUDE, fade time, dither constants, `kOutputBitsDefault`.
- `core/dsp/OutputBits.h` (new): the box (`OutputBits::prepare / setVoicing / setTarget / reset / process`).
- `core/dsp/Tank.h`: `#include`, member `outBits_`, hooks `setOutputBitsVoicing()`, `outputBitsVoicing()`, `outputBits()`.
- `core/dsp/Tank.cpp`: `Tank::prepare` (`outBits_.prepare`), `Tank::reset` (`outBits_.reset`), `Tank::controlTick` (`outBits_.setTarget(att, snap)`, next to the ATTITUDE Morph), `Tank::process` (one call, `outBits_.process(outL + pos, outR + pos, n)`, after the per-sample MIX loop of each control chunk).
- `host/common/ParamsJson.{h,cpp}` (`kOutputBitsKey`, `applyHidden`), `host/render/main.cpp` (sweep manifests record the voicing).
- `host/tests/test_output_bits.cpp` (new), `tools/output_mulaw_page.sh` (new).

Merging with `feat/tone-after`, `feat/throw-hold`, `feat/echo-mode`: none of the touched lines is in the wet chain (TONE on the return, hold ducking before the limiter, the echo); the box only reads the finished `outL/outR` after MIX. Expected conflicts: the hidden-key lists in `ParamsJson.*` / `render/main.cpp` (one line each) and possibly the include list in `Tank.h`.

## Method

**Where.** After the wet's limiter and after MIX: the dry has to go through it, so it can only sit on the final sum. The limiter keeps the wet alone under −1 dBFS, so the wet never reaches the box's full scale by itself; dry + wet at mid MIX can, and is then clipped at full scale like a real converter. Full scale = Tank units = the Plugin's 0 dBFS. On the Versio the firmware's `kOutputTrim` (−1.17 dB and the polarity flip) comes after the Tank, so the box clips ~1.2 dB before the DAC would (unchanged otherwise).

**Rate.** 48 → 24 → 48 kHz through the polyphase IIR half-band already in the Core (`dsp/Oversampler.h` `Halfband`, the Drive oversamplers': flat to 10.6 kHz, ≥ 85 dB down from 13.4 kHz), one down and one up per channel. Chosen over the echo's 63-tap FIR: same stop band, about a fifth of the work, and ~6 samples of delay instead of 62. Nothing above 12 kHz can fold back into the band; the top octave (above ~11 kHz) is gone in DRIVEN/KICKED, which is the 24 kHz sound.

**Latency.** 5.95 samples (0.124 ms) at 100 Hz–1 kHz, 6.4 at 5 kHz, 3.8 at 10 kHz (IIR: not constant). Only in DRIVEN/KICKED. Choice: CLEAN is never delayed (it stays bit for bit), and the box is crossfaded against it on a flip. At 0.12 ms a flip can't be heard as a time jump; during the 20 ms crossfade the two paths sum like a comb whose first notch is at ~4 kHz, for 20 ms (no click; test below). Dry and wet go through the box together, so they stay aligned with each other. The Plugin still reports latency 0 (0.12 ms in DRIVEN/KICKED is far under anything a DAW compensates audibly).

**Bits.** Mu-law (mu 255) against full scale, quantised in the compressed domain, signed (0 is a code). TPDF dither (±1 step) and nearest rounding down to the last step; the dither fades out between 1 and 0.5 steps of signal (1 ms envelope), and anything under half a step is exactly 0.

Two deliberate departures from the echo's code, both measured:
- The echo rounds toward zero, undithered, below 4 steps: in its feedback loop that guarantees repeats die. Out here there's no loop, and those undithered last steps made tails end in a pitched fizz (the tail's loudest mode, center-clipped): M6 ringing_db on a rim tail 7.7 dB DRIVEN / 10.3 KICKED vs today's 0.4 / 1.8, and a 10.3 / 8.5 dB narrow peak in the last second before silence. With dither to the last step: 0.2 / −0.6 dB and 4.8 / 4.6 dB (today's own level of structure).
- The echo's dither envelope releases over 40 ms: here that kept one step of hiss going 0.13–0.25 s after the input stopped. 1 ms: exact silence ~10 ms after.
- `log`/`exp` instead of `log1p`/`expm1` (the latter add 1.2 KB of newlib to the firmware; precision is far below a step).

**ATTITUDE (ADR 0003).** Weights (CLEAN, DRIVEN, KICKED) glide linearly to the switch over 20 ms, own per-sample glide: `out = wC·x + up(wD·mu12(y) + wK·mu8(y))`, `y = down(x)`. CLEAN ↔ DRIVEN fades the box in/out; DRIVEN ↔ KICKED crossfades the depths (one compression, one dither draw, both expansions during the fade). Back in CLEAN, the up filter rings out 2 ms, then the box is bypassed (bit for bit again); the down filter keeps running so a flip starts warm.

## Numbers (`build-proto/rv_test_output_bits`)

- Voicing 0 = untouched Tank, bit for bit (9 of 9). Voicing 1 CLEAN = today bit for bit (18 of 18: every SPRINGS, MIX 0 / 0.4 / 1, hits and skank); CLEAN MIX 0 = the input exactly.
- **Level** (K-weighted, 02_hits / 04_skank / 10_pad_cminor, every SPRINGS, worst over SPRINGS, MIX 0 / 0.4 / 1):

  | | hits | skank | pad |
  |---|---|---|---|
  | DRIVEN | −1.23 / −0.47 / 0.00 | 0.00 / 0.00 / 0.00 | 0.00 / 0.00 / 0.00 |
  | KICKED | −1.22 / −0.27 / +0.01 | +0.01 (all) | +0.01 (all) |

  The bits alone (vs today through the same two filters): ≤ 0.01 dB everywhere. The −1.2 dB on the dry drums is the top octave the 24 kHz rate removes (K-weighting lifts the highs, so it reads as quieter); white-noise snares lose 2.9 dB the same way. The wet has nothing up there (pickups, shelf): 0 dB at MIX 1. Checked ±0.5 dB everywhere but dry hits.
- **Noise floor** (dry 1 kHz tone, residual after fitting the tone, dBFS / SNR):

  | tone | −6 | −20 | −40 | −60 | −80 dBFS |
  |---|---|---|---|---|---|
  | DRIVEN 12-bit | −66 / 57 | −80 / 57 | −97 / 54 | −104 / 41 | −105 / 22 |
  | KICKED 8-bit | −42 / 33 | −56 / 33 | −73 / 30 | −80 / 17 | −84 / 1.5 |

  Mu-law: the grain follows the signal (constant SNR) until the bottom steps. Silence in: exactly 0 (18 of 18 cells, every SPRINGS × DRIVEN/KICKED × MIX). Dry only, after the input stops: exactly 0 after 10.8 ms (DRIVEN) / 8.1 ms (KICKED). A rim's tail at MIX 1 reaches exact silence 3.4 s / 9.5 s after the hit (DRIVEN, DECAY 0.5 / 0.85; today's tail is then at −121 / −118 dBFS) and 2.6 / 6.8 s (KICKED; today −97 / −93 dBFS).
- **No new pitches** (narrow peaks vs today, Welch 11.7 Hz bins, 100 Hz–10 kHz, both in the spectrum and relative to today's; limit 6 dB): DRIVEN rim tails 2.6, tone bursts (1 kHz / 1130 Hz, inharmonic) 3.1, quiet dry tone 2.1, last second before silence 4.8; KICKED 4.3 / 5.0 / 2.0 / 4.6. Positive control, the same bits without filters or dither: 28.2 / 27.4 dB (the check sees folding and granulation). M6: rim tails 30 s, no steady tone, ringing_db CLEAN 1.7 (= today), DRIVEN 0.2 (today 0.4), KICKED −0.6 (today 1.8); SPRINGS 3 grid 0 of 20 flagged (today 0), worst 4.6 (today 4.7).
- **Flips** every 0.6 s through every transition: 0 clicks on skank + pad mid-tail at MIX 0.4 (today 1, worst ratio 11.9 → 8.5) and on a dry 1 kHz tone at MIX 0 (worst ratio 4.1, limit 10). KICKED weight 0.50 at 10 ms, 1.00 at 22 ms.
- Deterministic, block 1 = 48 = 333.

## CPU and flash

- Desktop worst case (KICKED, 3 Springs, DECAY / TONE / DRIVE / SPLASH 1, TENSION 0, best of 7, four runs): 535–545 → 561–579 ns/sample, **+25 ns (+4.5 to +6 %)**.
- Chip estimate: per sample, both channels, 12 first-order allpass sections (~100 cycles), one `logf` + one `expf` (each at the half rate, two channels; ~150–250 cycles with newlib on the M7), dither, rounding and glide (~50): **~300–400 cycles, +3 to +4 points**. Scaling the desktop +25 ns by run 16's ratio (~12.7 cycles per desktop ns) gives the same ~+3.2 points. Run 16 ~68 % peak → **~71–72 %**, under the 75 % target. During a DRIVEN ↔ KICKED crossfade (20 ms) one more `expf` (~+0.5–1 point, briefly). If it's needed: a 128-entry expand table for 8-bit and a fast log would take most of the cost.
- Flash as if voicing 1 were the default (`-DRV_OUTPUT_BITS_DEFAULT=1`): **release 116,260 B (+3,608 over main's 112,652; 88.7 %)**, profile 118,208 B (+3,608 over 114,600). Of that, ~1.8 KB is GCC now inlining `DriveOut::process` into `Tank::process` (it was out of line); the box itself is ~1.6 KB. Default build: release 112,612 B (−40 B, layout only).

## Existing tests if voicing 1 became the default

`build-asif` (`-DCMAKE_CXX_FLAGS=-DRV_OUTPUT_BITS_DEFAULT=1`): 64 % of 22, 8 suites fail. The default ATTITUDE is DRIVEN, so every test that doesn't set ATTITUDE hears the box.

| Suite | What fails | Why | Adapt |
|---|---|---|---|
| test_mix | MIX 0 bit-identical dry (blocks 1/48/512), MIX 1→0 null, dry-gain read at noon, equal-power sum | MIX 0 is no longer a passthrough in DRIVEN (owner's choice) | run in CLEAN (they test MIX, not ATTITUDE); add a DRIVEN/KICKED MIX 0 check: "the box on the dry, level ±0.5 dB" |
| test_main | "Tank mix=0 gives bit-identical dry passthrough" | same | CLEAN |
| plugin_host_test | MIX 0 null vs the input; MIDI Kick onset at N + 3 samples | box on the dry; the box's ~6-sample delay moves the onset | CLEAN for both (the plugin still reports latency 0; or allow +6 samples in DRIVEN/KICKED) |
| test_tank, test_spring, test_springs3 (14 cells), test_drive stability | "peak < 1" (worst 1.02–1.11) | full-scale noise at mid MIX: dry + wet clip at the box's full scale at 24 kHz, and the up filter overshoots (Gibbs) by up to ~0.9 dB | the bar is about the Tank running away, not the output stage: measure the peak before the box (a test hook, `outputBits()` bypass) or run in CLEAN; on the Versio 1.107 × 0.874 = 0.97 still under the DAC's clip. (Or clamp after the up filter: a hard clip at 48 kHz, only on overload.) |
| test_drive stability, test_tank stability | "decaying / ends lower" on DECAY 0 impulses | the tail is exactly 0 by 5 s, so `power(5–6 s) < power(start)` is `0 < 0` | `<=`, or "both silent counts as decayed" |
| test_drive aliasing | DRIVEN / KICKED DRIVE 1, 5–15 kHz tones at 0 / −6 dBFS: an inharmonic product at −55 to −64 dB re the tone (limit −60) | mu-law's expansion of a dithered code is slightly nonlinear (convex), and that distortion is made at 24 kHz, so its highest products fold. 30 dB under the box's own grain (KICKED SNR 33 dB) | the test measures the drive chain's oversampling: run it on the wet before the box (hook), or keep it and widen to "−60 dB or under the box's noise floor" |
| test_output_bits | "default is 0", "voicing 0 = untouched" | by design | flip those two checks with the default |

Passed unchanged in the as-if run: test_antires, test_clicks, test_kick, test_kick_voice, test_led_meter (the LEDs read `limiterGain()`, untouched, and the per-block output peak, which now includes the box: correct per ADR 0031), test_m7_tank, test_metrics, test_pot_endstops, test_render, test_set_args, test_splash, test_sustain_trim, test_tank_voicing, test_wobble. (M4 mono/stereo metrics live in test_tank / test_springs3: only their stability bars failed.)

## Commands

```bash
cp -R ../../../test_audio/stimulus test_audio/          # main checkout's stimulus into the worktree
cmake -S . -B build-proto -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-proto
cd build-proto && ctest > ctest.log 2>&1; grep "tests passed" ctest.log   # 100% tests passed out of 22
build-proto/rv_test_output_bits [identity clean level latency noise silence artefacts flips deterministic cost]
tools/output_mulaw_page.sh build-proto/rv_render renders/feat_output_mulaw
cmake -S . -B build-asif -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-DRV_OUTPUT_BITS_DEFAULT=1   # as if picked
make -C firmware MODE=release MODE_DEFS="-DRV_MODE_RELEASE -DRV_OUTPUT_BITS_DEFAULT=1"        # clean firmware/build first
make -C firmware MODE=profile MODE_DEFS="-DRV_MODE_PROFILE -DRV_PROFILE_HOOKS -DRV_OUTPUT_BITS_DEFAULT=1"
```

Page: `renders/feat_output_mulaw/index.html` (main checkout). Columns CLEAN / DRIVEN / KICKED, A = today, B = mu-law, level matching on; rows: hits, skank, pad (MIX noon), sparse rim (−6/−12/−18 dBFS, MIX 1) at DECAY noon and 0.85, skank at MIX 0 and 0.4, hits at MIX 1. 2 Springs, TONE / TENSION noon, SPLASH 0.3, DRIVE 0.4. CLEAN's A and B are byte-identical files (checked).
