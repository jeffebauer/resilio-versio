# Stereo in: what it would take (study, 6 Oct 2026)

**Status:** study only. No shipped code changes; no prototype code was written (the chip's own per-section timings from CPU runs 3–21 are better evidence than a desktop timing, see §3). Decision needed from the owner (§5), then an ADR before anything is built (SPEC §10 "Stereo in").

**The owner's starting point:** In R is for the right channel of a stereo input. Today the module sums In L and In R to mono before the Tank (SPEC §4.3); the dry path is already stereo.

---

## In short (plain language)

- **Today**, the module adds left and right together right at the input, so everything the springs hear is one mono signal. Your dry sound stays stereo, and the reverb comes back wide but centred. A guitar panned hard left gets a reverb in the middle.
- **A real stereo spring keeps sides apart.** Your Wellspring is two separate tanks, one per side. In the takes where only one side got signal (A-L, A-R, D-L, D-R), the other output is 33–65 dB quieter at every frequency, from the first echo to the end of the tail. A hard-left sound comes back hard left; it never blooms across.
- **Feeding the two springs different signals is not enough on its own.** Today's output mix deliberately does not put Spring A on the left and Spring B on the right (that "flicker", each echo jumping ear to ear, is what you heard as delay-like in ADR 0038). To keep a sound on its side, the output has to steer the springs toward the side the sound came from, and only as much as the input really is one-sided, so centred sounds keep today's flicker-free image.
- **The module can tell mono from stereo by listening.** There is no jack-detect switch the firmware can read. But when you patch In L only, the jack copies the signal to In R, so left and right are almost identical. The firmware can compare them: when they're identical, it's mono, and it runs exactly today's code, so mono sounds exactly as today.
- **CPU is the real limit.** Echo mode already peaks at 75.7 % and the worst moment (switching out of echo mode) at 78.1 %, against the 80 % ceiling. TANK 2 peaks at 70.4 %: it has room. A full second input chain (two drive stages, two tone stages) costs about 11–14 points: it doesn't fit anywhere. Splitting only the springs' feed costs about 1.5–3 points: it fits in TANK 2.
- **Recommendation:** stereo in for **TANK 2 only** (the classic two-spring tank: one spring per side, like the Wellspring), with one drive stage on the mono sum and the stereo part added to the springs' feed after it. TANK 1 and TANK ECHO stay mono-summed. Built first as a hidden Core mode for the plugin, judged by ear, then timed on the chip.

---

## 1. How it works today

All line references are `core/dsp/Tank.cpp`, `Tank::process`.

**Where L and R become one.** The first thing the input loop does is `x = 0.5 * (inL + inR)` ("Real tanks are mono: sum the input (SPEC §4.3)"). Echo mode's tape records the same sum (`xs[i] = ... 0.5f * (inL + inR)`). From there on everything is mono:

| Stage (in order) | Today | Stereo-aware already? |
|---|---|---|
| Throw / Hold send gain (`sendG`) | one gain on the mono sum | n/a (a gain: applies to both sides for free) |
| Hold's ducking key (input below 120 Hz) | on the mono sum | n/a (kick and bass are centred) |
| Excitation trim followers, low-cut makeup followers | on the mono sum | n/a (they measure a ratio, not a side) |
| Splash detectors (hit, hit envelope) → Clang, Bite, Jolt | on the mono sum, after the INPUT gain | no |
| **DriveIn** (input transducer → tape, ×2 oversampled, nonlinear) | one instance | no |
| TONE tilt, Big Knob makeup followers | one instance | no |
| Low cut (138 Hz HP + shelf), Clang (`x + c(x − lo)`), input coil (square term + resonant LP) | one instance | no |
| **Sweep** (≈40–50 "highs later" allpass sections, shared by every Spring) | one instance | no |
| Springs A, B (C doesn't run since echo mode) | **both get the same mono input** (Loop and high path) | no |
| Mid / side mix of the Springs, decorrelators (width) | stereo out | yes (output only) |
| Output pickups (DriveOut), high shelf | one per channel | yes |
| µ-law box | per channel | yes |
| TONE's return filter (Big Knob), limiter (stereo-linked), Hold ducking gain, BLEND | stereo | yes |
| Dry path | stereo, untouched | yes |
| Input LEDs | In L and In R metered separately (`firmware/main.cpp`) | yes |

So: **the dry path, the output half and the meters are already stereo. Nothing from the input to the Springs is.** No Spring is panned (ADR 0038): in TANK 2 the width is the Springs' difference (A − B) through its own decorrelator, side 0.55; mono is exactly the mid.

**Sustain trim** (ADR 0035) reads the wet's peaks (already max of L and R) and trims the Springs' input: one gain, so it can apply to a stereo feed unchanged.

**Normalling.** SPEC §3 "Audio I/O": the hardware normals In L → In R when In R is unpatched; libDaisy has no jack detection (`daisy_versio.h` exposes knobs, switches, button, gate and LEDs only). So:
- Mono into In L: both ADC channels read the same analog signal. Today's sum is that signal (`0.5 × (x + x) = x`). They differ only by the codec's channel mismatch and noise (not yet measured, §4).
- Mono into In R only: In L reads silence, so today the Tank gets the signal **6 dB down**. With stereo in it would come back on the right.
- Stereo: today the sum; the placement is lost in the wet.

**Rule for any option:** with a mono patch, the module must sound exactly as today. The simplest guarantee: when the input is judged mono, the stereo work is skipped entirely and the code path is today's, bit for bit (the same pattern as the Throw's `sendLive` branch).

---

## 2. Options

Cost units: the chip's budget is 480 MHz ÷ 48 kHz ≈ 10,000 cycles per sample, so **1 point of CPU = ~100 cycles/sample** at block 48. Section costs below come from the chip's own SPLIT lines (`firmware/README.md`, runs 16–21: `drvIn` 4.8, `splash` ~5.2, `tilt` 10.6 at a loose tank, of which the Sweep is ~5–6, each running Spring ~11–12, `out` 17.4). Estimates of new code use the in-order issue model's habit of reading low on the chip: ×1.3 for plain filter code (run 16: model 63–68 vs chip 68–73), up to ×2 for branchy code (run 17's µ-law box: estimate 3–4, chip 8–9). Treat every number here as a range to check on the chip, never as a result.

Flash: release 126,380 B (4.6 KB left), profile 127,520 B (**3.5 KB left**), limit 131,072 B (128 KB). A new inlined call site of an existing filter costs its code again; `Tank.cpp` notes "one call site: DriveIn inlines once".

Peaks today (run 21): TANK 1 68.8 %, **TANK 2 70.4 %**, **TANK ECHO 75.7 %**, switch S3 → S1 76.0 %, worst moment (lap wrap) **78.1 %**. Target ≤ 75 %, ceiling 80 % (ADR 0030 amendment).

### Option A: per-Spring L/R blend, one drive stage ("stereo into the springs' feed")

**What it does.** Everything up to the Sweep runs on the mono sum M = ½(L + R) exactly as today. The side S = ½(L − R) goes through a cheap **linear copy** of the shaping (DriveIn's small-signal gain as one number per control tick, the tilt, the low cut, the Clang's gain, the coil's low-pass; not the saturation, not the coil's square term), giving S′. Spring A hears M′ + w·S′, Spring B hears M′ − w·S′, so A carries the left, B the right. On the way out, a **steering term** p·k·(A − B) is added to L and taken from R before the pickups, with p following how one-sided the input is (0 for centred or mono, 1 for one side only), smoothed over a few hundred ms.

**To a player.** A hard-left guitar hits Spring A only and comes back mostly from the left, ringing on there while it fades, as on the Wellspring. A right-hand keys part rings in Spring B on the right. Two sources hard-panned keep their own tails on their own sides at the same time (each Spring holds its own memory). A centred sound sounds exactly as today (no flicker: p is 0). A wide stereo pad gets a wider, livelier tail. Grit: the side part of a sound skips the saturation, so in TAPE / VALVE a hard-panned sound gets about half the drive colour a centred one gets (one-sided at DRIVE high: the grit sits in the middle of the image, the side part is clean).

**Core changes.** `Tank::process`: the side signal, the linear shadow shaping (~5 small filters), per-Spring input buffers (today `springs_[s].process(lin, mono, …)` takes one shared buffer; it would take A's and B's own), the steering term in the output loop, the mono/stereo detector (§4). `Spring` unchanged. A new hidden setting (`stereo_in`, Renderer key) and an ADR. ParamSpec: no new knob; possibly a hidden on/off for A/B.

**Where it applies.** TANK 2 only. TANK 1 has one audible Spring (B runs silent), so there's nothing to split; TANK ECHO keeps its mono tape (§2 "Echo mode" below).

**CPU.** Side + detector ~12 cycles, shadow shaping ~60–80, per-Spring inputs ~6, steering ~6: **~90–110 cycles model, ~1.2–2.5 points on the chip.** TANK 2 peak 70.4 → **~72–73 %**, under the 75 % target. Echo mode and the switch moments unchanged if the stereo path stays off there (and engages only after a TANK switch's 20 ms fade has finished).
Variant **A+**: the side through its own Sweep as well (so its first echo is smeared "highs later" like the mid's): +5–6.5 points, TANK 2 → **~77–79 %**, over the target, at the ceiling. A shorter Sweep on the side (e.g. 12 sections) sits in between (~+1.5).

**Flash.** ~0.8–1.5 KB (inlined filters in the per-sample loop, the detector, the steering; set-up at -Os). Fits both builds, but uses a quarter to almost half of the profile build's last 3.5 KB.

**Risk.** Low to medium. Bit-exact for mono by construction (the branch is skipped). Unknowns: how much the side skipping the saturation and the Sweep is audible (A vs A+ by ear); the steering's smoothing (a left tail drifting to the centre when a centred sound follows); the extra level of a one-sided source (§5 Q3).

### Option B: full dual input

**What it does.** Two complete input chains: DriveIn, tilt, low cut, Clang, coil and Sweep, one per side; Spring A fed by the left chain, B by the right; the output steered as in A.

**To a player.** Like A, plus each side gets the full drive colour and the full "highs later" smear: a hard-left sound in VALVE is as gritty as a centred one. The closest to two physical tanks.

**Core changes.** Second instances of `DriveIn`, `Tilt`, the low cut, the coil and `Sweep` (Sweep memory goes in the pool: one more set of section rings, a few hundred floats, the pool is at ~29,800 of 30,000 floats per ADR 0038, so the pool grows); a second call site of each (code copied again); everything else as in A.

**What can stay mono:** the Splash detectors (a hit is a hit; the Clang / Bite gains then apply to both chains), the Excitation and low-cut makeup followers, the Big Knob followers, the Hold's ducking key, Throw / Hold send (a gain), the Sustain trim (a gain), the transport. Making the Splash stereo too (a left hit splashing only Spring A) adds another ~5 points.

**CPU.** DriveIn ~2.5–3 points, shaping ~3–4, Sweep ~5–6.5, routing and steering ~0.5: **~11–14 points (~1,100–1,400 cycles/sample)**, SPEC §10's earlier guess (300–700 cycles) predates the shared Sweep (ADR 0038). TANK 2 → **~82–84 %**, over the ceiling. Echo mode → ~87–90 %. Doesn't fit without a new round of CPU work or block 96 (ADR 0030's reserve, +1 ms latency, gain unmeasured).

**Flash.** ~3–5 KB (DriveIn and the shaping inlined twice). **Over the profile build's 3.5 KB**; the release build at best just fits. Would need flash reclaimed first.

**Risk.** High: over both budgets.

### Option C (middle ground I'd consider): "follow the balance"

**What it does.** The springs stay mono, exactly as today. Two or three slow band followers on In L and In R (e.g. below 300 Hz, 300 Hz–2 kHz, above 2 kHz) measure where the sound is; the wet's L/R balance leans toward it (constant power), per band, smoothed over ~100–300 ms. Mono → balance centred → today's output, bit for bit.

**To a player.** A hard-left guitar's reverb leans left. Works in every TANK position, echo mode included. But it's one balance for the whole tail: two sources on opposite sides get a centred reverb, and when a right-hand sound follows a left one, the whole tail (the old left part too) swings right. It "follows" rather than "remembers".

**Core changes.** Followers on the raw L/R input, a balance gain pair per band on the wet (needs a band split of the wet: two one-pole crossovers per channel) before the limiter. No change to the springs.

**CPU.** ~30–50 cycles: **~0.4–0.8 points** anywhere. Echo mode 75.7 → ~76.5 %, worst moment ~78.9 %: still under the ceiling, a thin margin. Broadband only (no band split): ~0.2–0.4 points.

**Flash.** ~0.4–0.8 KB.

**Risk.** Low technically; the musical risk is the swinging tail.

### Option D: plugin-only Core mode

**What it does.** A, B or C built in the Core behind a switch that only the Renderer and the plugin can turn on; the firmware compiles it out (`#ifndef RV_FIXED_VOICINGS`, like the other hidden voicings). SPEC §10 asks for exactly this if the module can't afford it: one Core, and every mono setting identical across hosts.

**To a player.** Stereo in Ableton, mono-summed on the module. A stereo source sounds different on the two; a mono one identical.

**Cost on the chip.** None (0 cycles, 0 bytes in the firmware).

**Risk.** It doesn't do what you decided In R is for. Its real use is as the **first step of A**: build A as a hidden Core mode, listen in the plugin (sound work on desktop first), and only then pay its CPU and flash on the module.

### Echo mode, Throw / Hold, and the rest: what has to change

| Part | Option A | Option B | Option C |
|---|---|---|---|
| Echo mode tape | **stays mono** (TANK ECHO is mono-summed) | same | stays mono; the wet's balance follows |
| Throw / Hold send | the same gain on M and S | same gain on both chains | untouched |
| Hold's ducking (key: the input's lows) | mono sum (kick and bass are centred) | same | same |
| Sustain trim | one gain, also on S′ | one gain on both | untouched |
| Splash detectors (Clang, Bite, Jolt) | mono sum; Clang's gain also on S′; the Bite (inside DriveIn) only on M | mono sum, gains on both chains (or stereo, +5 points) | untouched |
| Jolt | every Spring, as today (a left hit lurches B too) | same, unless the Splash goes stereo | untouched |
| µ-law box | already per channel: unchanged | same | same |
| Limiter | already stereo-linked: a loud left hit also pulls the right down; unchanged | same | same |
| Input LEDs | already per channel | same | same |

**Why echo mode stays mono.** Its tape is 379 KB of AXI SRAM; about 105 KB of the 512 KB is left, so a second tape doesn't fit there (SDRAM has room, but its cost on the chip is unmeasured). Echo mode is also the CPU peak (75.7 %). The repeats are already wide (two playback heads). A stereo echo exists in the Renderer (the ping-pong voicing, `EchoDirect.h`, a second tape): a possible later plugin-only step.

---

## 3. CPU headroom: what to measure on the chip

**Reality.** Echo mode 75.7 % peak, worst moment 78.1 %, ceiling 80 % (ADR 0030). Desktop and model estimates have read 4–5 points low (run 16) and up to 2× low on new code (run 17). Only chip runs count.

**Why the current profile build would see nothing.** Its test signal is mono (`bufL[i] = bufR[i] = x`, `firmware/main.cpp`), so any option gated on a stereo input would sit idle and measure 0. The plan:

1. **Build A as a hidden Core mode** (Renderer + plugin), with a Renderer flag to force the stereo path on regardless of the detector (the worst case for timing). Desktop: relative cost against `main` on the same stereo renders (clearly labelled relative); mono renders `cmp`-identical to `main`.
2. **Profile build, stereo corners:** a stereo test signal for new corners (clicks alternating L-only / R-only, plus decorrelated low-level noise on each side), at TANK 2's existing worst corners (D1 TN0 TO0.5 / TO1, VALVE, DRIVE max). Add `SWITCH S3>S2` with the stereo signal (the switch into the stereo position) and the lap wrap. Report a new SPLIT section `stIn` (side shaping + detector + steering) so its own share is visible.
3. **Pass:** TANK 2 stereo peak ≤ 75 %, every switch moment ≤ 80 %, no change to TANK 1 / ECHO corners (± noise). Then the owner's click check (TANK 2, VALVE, DRIVE and DECAY up, fast knob moves, throws, a stereo source).
4. **Flash:** `make -C firmware all-variants`; profile must stay ≤ 128 KB with the extra corners (the switch corners alone were +288 B).
5. A+ (second Sweep) only if A's side without the smear is heard as a problem; it needs its own chip run and likely sound-neutral savings first.

---

## 4. Telling mono from stereo

**No jack detect.** libDaisy's `DaisyVersio` has no input-sense pin, and NE's manuals describe a plain normal (In L's signal copied into In R's jack). The firmware can't see whether a cable is in In R.

**Listen instead.** With In L only, L and R are the same analog signal into two codec channels: the side S = ½(L − R) is only the channels' gain mismatch and noise (expected ~30–45 dB below the mid, to be measured). A detector at control rate:
- Two power followers (mid and side, ~50 ms), compared each tick; silence (mid below ~−60 dBFS) keeps the last verdict.
- **Stereo** when the side rises above ~−24 dB re the mid for ~100 ms; **mono** again after it stays under ~−36 dB for ~2 s (hysteresis, so a stereo source with a centred kick doesn't flip).
- The verdict glides w and p over ~100 ms (no click); at w = p = 0 the stereo path is skipped and the code is today's, bit for bit.
- A stereo source playing mono content is treated as mono: correct, because it then *is* mono, and it sounds the same either way.
- In R only: the side equals minus the mid, clearly stereo: it comes back on the right (today: centred, 6 dB down). That level change is a decision (§5 Q4).

**Measure before building the detector.** The mismatch floor sets the thresholds. Cheapest way without USB on rack power: a one-off test firmware (like the M0 check) that outputs Out L = In L, Out R = (In L − In R) × 100 (+40 dB), recorded in Ableton with a mono source in In L only, then with a stereo source. The Out R level is the floor directly. (Or a Renderer-side analysis of any stereo Ableton recording of the module's own inputs, if one exists.)

**Plugin.** JUCE gives a real mono bus when the track is mono (`isBusesLayoutSupported` accepts mono); the plugin can copy L to R before the Core, so it behaves like the normal. The detector then decides as on the module.

---

## 5. What the owner would need to decide

1. **Should a hard-left guitar come back hard left from the springs, or lean left and bloom across?** (Wellspring: hard left, the other side 33–65 dB quieter. Option A can do either: the steering amount sets it.)
2. **When a left phrase is followed by a centred one, should the left tail stay left while it fades, or drift to the middle?** (A: each Spring remembers its side; the steering can hold on for seconds or let go quickly. C: the whole tail swings.)
3. **Should a sound panned to one side come back as loud as the same sound in the middle?** Fed to one Spring only, it puts 3 dB more into the springs than today's sum; we can trim it to match or let one-sided sounds splash a little harder.
4. **Patched into In R only: should it come back on the right (stereo), or in the middle as today?** (Today it's also 6 dB quieter than In L only.)
5. **Is stereo for TANK 2 only OK, with TANK 1 and TANK ECHO staying mono?** TANK 2 is the only position with CPU room; TANK 1 has one audible Spring; echo mode is at the CPU peak and its tape doesn't fit twice.
6. **In TAPE and VALVE, is it OK if the panned part of a sound gets less grit than the centred part?** (Option A's price. B fixes it, but doesn't fit on the chip.)
7. **Should the stereo part get the same "highs later" smear on its first echo?** (A vs A+: hear it in the plugin before paying ~5 points.)

**Recommendation: Option A, TANK 2 only, built first as a hidden plugin mode (D), then timed on the chip.** One line: it gives the Wellspring's "each side its own tank" where the module has CPU room (~1.5–3 points against TANK 2's ~5 to the target), and a mono patch runs today's code untouched.

---

## 6. Useful data: the Wellspring's one-sided takes

Takes A-L, A-R (clicks) and D-L, D-R (sweep) are in `test_audio/reference/` on the Mac (never committed). How they were made: `docs/recording-recipe.md` (A-L / D-L: a dummy plug in R, left tank only; A-R / D-R: R only). Session 2 findings (`docs/m8-tuning-backlog.md`, "Wellspring session 2"): each tank feeds only its own output; the tanks are alike within ~2 dB (right 2–2.6 dB darker at 2–4 kHz; T60 3.9 vs 3.5 s).

Re-measured for this study (a throw-away pure-Python script in the scratchpad, not committed; `tools/` has no ready analysis for one-sided takes): the far side's level against the near side's, dB.

| Take | Broadband | 125 Hz | 250 | 500 | 1 k | 2 k | 4 k | first 0.1 s | 0.1–1 s | 1–3 s |
|---|---|---|---|---|---|---|---|---|---|---|
| D-L (sweep) | −37 | −46 | −44 | −41 | −37 | −34 | −33 | −34 | −32 | −29 |
| D-R (sweep) | −59 | −63 | −65 | −65 | −62 | −58 | −54 | −60 | −58 | −48 |
| A-L (clicks) | −21 | −9* | −12* | −23 | −27 | −27 | −23 | −31 | −25 | −5* |
| A-R (clicks) | −27 | −30 | −36 | −37 | −37 | −33 | −26 | −38 | −30 | −11* |

\* The click takes peak near −38 dBFS, so the far side's quiet bands and late windows read the recording's noise floor, not the tank. The sweeps (peaks −8 dBFS) are the clean reading.

**Reading.** A real stereo spring keeps one-sided input on its side at every frequency and for the whole tail: no bloom. That is Option A at full steering (Q1 "hard left"). Option C can't do it for two sources at once.

**What `tools/` could add.** `tools/ingest_references.py` already computes stereo metrics per take (correlation, balance) for the normalled takes; a small extension (near/far level per octave and over time, for takes named `-L` / `-R`) would make this table repeatable and let a prototype's renders (hard-left clicks and sweep through Option A) be compared with D-L / D-R on the same scale.

---

## Sources

`SPEC.md` §1, §3 "Audio I/O", §4.2–4.3, §4.8, §5, §6.1, §10; `CONTEXT.md`; ADR 0030 (CPU budget), 0038 (stereo without flicker), 0041 (echo mode), 0035 (Sustain trim), 0039 / 0040 (Throw, Hold), 0042 (µ-law box); `firmware/README.md` (M3 runs 3–21, flash techniques, memory); `core/dsp/Tank.cpp` (`Tank::process`); `firmware/main.cpp` (input feed, profile test signal); `libs/libDaisy/src/daisy_versio.h`; `docs/reference-report.md`; `docs/recording-recipe.md`; `docs/handoff/HANDOFF.md` (flash sizes).
