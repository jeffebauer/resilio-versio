# M7 integration plan: SPLASH, KICK, WOBBLE, MIX

Status: the components are built and tested **stand-alone** (M7 branch). They
are *not* wired into `core/dsp/Tank.*` / `core/dsp/Spring.*` yet, because M6
is rewriting those (Micro-mod floor of the Loop delay L, with a hook for
WOBBLE). This document is the exact plan for wiring them in after M6 lands.

Terms as in `CONTEXT.md`: Hit, Clatter, Jolt, Splash, Kick, Drift, Warble,
Micro-mod floor.

## Files

| File | What |
|---|---|
| `core/params/SplashVoicing.h` | Every SPLASH / KICK / WOBBLE number: per-ATTITUDE tables (`kVoice`, `kKick`), Hit threshold + curve, WOBBLE depth/rate curves with the pitch maths. Pure functions of Normalised values. |
| `core/dsp/Splash.h/.cpp` | `HitDetector`, `Clatter`, `Jolt`, and the `Splash` facade (one per Tank) with the stroke detector and impact sequencer. |
| `core/dsp/Kick.h/.cpp` | `KickVoice`: sample-accurate `trigger()`, thump + burst, two outputs (Loop feed, direct), `joltOffset()` for the forced Splash. |
| `core/dsp/Wobble.h/.cpp` | `Wobble`: one per Spring, Loop delay modulation in samples. |
| `core/dsp/Seed.h` | `mixSeed()`: murmur3 finaliser. Without it, small LCG seeds gave identical jitter and phases. |
| `host/tests/test_splash.cpp`, `test_kick_voice.cpp`, `test_wobble.cpp`, `test_mix.cpp` | Stand-alone tests (test_mix uses the Tank as-is). |

All components are real-time safe (no allocation after `prepare()`),
deterministic (seeded, `reset()` restores the seed) and sample-rate aware.
Their control-rate logic runs on their own 32-sample grid, counted from
`reset()`, so output is bit-identical for any block size (tested with blocks
of 1, 7, 32, 333 and 1024). `splash::kControlInterval == 32 ==
Tank::kControlInterval`. Add `static_assert(splash::kControlInterval ==
Tank::kControlInterval)` in Tank.cpp. Both grids start at `reset()` and
advance per sample, so they stay aligned.

## Signal flow after integration

```
in L,R ─ mono sum ─ DriveIn ─┬──────────────────────── Tilt ─ + ─┬─ Spring A ─┐
                             │                                 ▲  ├─ Spring B ─┼─ SPRINGS mix ─ mid ─ + ─ … DriveOut ─ shelf ─ limiter
                             └─ Splash (Hit detector)          │  └─ Spring C ─┘                       ▲
                                  │ Clatter ──► each Spring's high-path input                          │
                                  │ Jolt ────► each Spring: L offset (× kJoltSpringScale[i]), Δa       │
   Tank::kick(offset) ─ KickVoice ├─ loopOut (HP160⁴(thump + burst)) ───┘ (post-drive Tank input)      │
                                  ├─ directOut (thump) ────────────────────────────────────────────────┘ (wet mid, pre-DriveOut)
                                  └─ joltOffset() ─► Splash::strike(1, offset)
   Wobble[i] ─► Spring i: added to M6's Micro-mod floor offset on the Loop delay read
```

## Hooks, one by one

### Splash (Hit → Clatter + Jolt), one instance in the Tank

- **Detector input:** the mono signal **after DriveIn, before Tilt**
  (SPEC §4.5 "on driven input"). Taking it before Tilt keeps TONE from
  changing SPLASH sensitivity. DriveIn is gain-compensated, so Hit follows
  the player's level, not DRIVE.
- **Control tick** (`Tank::controlTick`): `splash_.set(attW_, smoothed_[Splash])`.
  It re-blends the voice and runs one `exp` only when something changed.
  Optional: `splash_.setTankLevel(x)` with a smoothed wet RMS 0..1 (the LED_1
  tank-energy meter can supply it) for KICKED's energy-dependent rattle.
- **Per chunk** (inside the Tank's `n <= 32` loop, after DriveIn):
  `splash_.process(driven, clatter, joltLoop, n)`.
- **Clatter → each Spring's high path.** Spring needs one new input: an
  extra signal added to the high-path input only (`processHigh(in + clatter)`).
  Suggested API: `Spring::process(const float* in, const float* highExtra,
  float* out, int n)`. (M7 also lifted CLEAN's high-path input on
  transients, "SPLASH = mild HF emphasis only"; it measured as no change
  and was replaced by a real, gentler splash in ADR 0025.)
  The same Clatter goes to all three Springs. Their own detuned high paths
  decorrelate it.
- **Jolt → L:** `joltLoop[k]` is a fraction of L, per sample. Spring i adds
  `joltLoop[k] × splash::kJoltSpringScale[i] × lCur_` samples to the same
  read offset that WOBBLE and the Micro-mod floor use (below). The scales
  are {1, −0.75, 0.9}: Spring B lurches the other way.
- **Jolt → a:** `splash_.allpassDelta()` (≤ 0, control rate) is added to each
  Spring's `allpassCoeff` in `controlTick`, after the detune multiply,
  clamped to `|a| ≤ splash::kMaxAllpassMagnitude` (0.85). The clamp only
  bites at BOING max on Spring C (−0.72 × 1.07 − 0.12 = −0.89 → −0.85). Spring's
  `setSettings` then re-designs g for the new `a` on that tick, as it already
  does for a BOING move (per-tick coefficient steps, the same zipper-free
  behaviour as BOING).
- **Slew:** Jolt's fastest slope is 0.066 samples/sample at L = 108 ms
  (KICKED, hit + Kick strike; `test_splash`). That is under
  `Spring::kLoopSlewPerSample` (0.08). If the modulation is added to the
  read position after the slew limiter, which is recommended (it is a
  modulation, not a DECAY move), the slew limiter never clips it.

### Kick (replaces the M2 placeholder)

- `Tank::kick(offset)` keeps its signature. `pendingKicks_` stays in the
  Tank. In the chunk loop, the code that now does `mono[at] +=
  kKickPlaceholder` instead calls `kick_.trigger(at)` for the chunk
  containing it. Then `kick_.process(kickLoop, kickDirect, n)`.
- **`kickLoop` is added after DriveIn and Tilt**, straight into the Springs'
  input (SPEC §4.6 "post-drive"). A knock on the tank bypasses the input
  transducer and the EQ. The Loop feed is high-passed at 160 Hz (120 before TENSION), 4th order
  (ADR 0016 "Kick-path high-pass on the part fed into the Loop").
- **`kickDirect`** (the full thump) is added to the wet **mid** before DriveOut,
  × `kWetGain`: the pickup hears the tank body move, and the pickup gives
  it its colour. It is centred and mono-safe.
- **Forced Splash:** `if (kick_.joltOffset() >= 0) splash_.strike(1.0f,
  kick_.joltOffset());` **before** `splash_.process()` for the same chunk. The
  crash's Clatter and Jolt then start on the Kick's sample, at Hit 1 /
  SPLASH 1 whatever the knob (SPEC §4.6 "forces maximal SPLASH jolt").
- `kick_.setAttitude(attW_)` on each control tick.
- Firmware: rising edge of Gate or button → `tank.kick(offset)`. Debounce
  stays in the Firmware. `KickVoice` also merges edges closer than 5 ms,
  as a second line of defence.

**Why the split matters (measured, `test_kick_voice`):** through one Spring at
DECAY max, KICKED, the < 100 Hz energy is down **29.5 dB** 300 ms after the
Kick (target ≥ 20 dB). Feeding the whole thump into the Loop, as the M2
placeholder's pre-DriveIn path does, gives only **6.5 dB**. That fails
ADR 0016.

### Wobble, one per Spring

- Instances `wobble_[i].prepare(fs, i, seed_i)`, seeds distinct from the
  Spring noise seeds. `setAmount(smoothed_[Wobble])` on each control tick
  (Gliding 80 ms, ADR 0015). It re-maps only on change.
- Per sample: `m = wobble_[i].next()` (samples). **M6 hook:** the Loop delay
  read becomes `readLow(lCur_ + microMod + m + jolt)`. Whatever M6 names its
  floor offset, WOBBLE is simply added to it. At WOBBLE 0, `m == 0` exactly,
  so only M6's floor remains (ADR 0008).
- The high path's delay (`lhCur_ = 0.43 L`): leave it unmodulated. Its
  echoes are short and bright, and wobbling them adds flutter, not Warble.
  Tune by ear at M8 if wanted.
- **Delay memory:** `lowDelaySize()` has a 2 % + 8 margin, about 112 samples
  at L = 108 ms, 48 kHz. After integration it must also hold WOBBLE's max
  (`splash::wobbleMaxDepthSamples(fs)`, 46 samples at 48 kHz), the Jolt
  (0.0125 × L, 65 samples) and M6's floor: about 116 samples, just above
  the margin. Size it as `ceil(kLongest·fs·(1.02 + 0.0125)) +
  wobbleMaxDepthSamples(fs) + floorMax + 8`. That is about 0.5 kB more pool
  at 48 kHz.

### MIX

Already in the Tank (equal-power sqrt law, Snappy 5 ms, per sample).
`test_mix` verifies it as-is: nothing to integrate.

## ParamSpec smoothing (unchanged, ADR 0015)

| Param | Tier | Where it acts |
|---|---|---|
| SPLASH | Snappy 5 ms | `Splash::set` per control tick (threshold, amounts) |
| WOBBLE | Gliding 80 ms | `Wobble::setAmount` per tick. The depth and rate glide, so a WOBBLE move never jumps pitch |
| ATTITUDE | Morph 30 ms | `attW_` into `Splash::set` / `KickVoice::setAttitude`: the Clatter/Jolt amounts morph with the drive stages. A ringing Kick keeps the voicing it started with |
| MIX | Snappy 5 ms | Tank's per-sample `Smoother`: measured 50 % lag 3.7 ms at block 1 and 4.2 ms at block 48 on a 5 ms envelope |
| Kick | none | trigger only (fixed strength, ADR 0005) |

## Test expectation changes when integrating

- **`test_kick`:** "Kick at N == input impulse at N, bit-identical" no longer
  holds (the Kick is no longer an input impulse). It becomes:
  1. "Kick onset sample-accurate: after silence, the first non-zero wet sample
     is at N + the fixed wet latency (DriveOut oversampler, about 2–3 samples),
     for blocks 1, 7, 32, 48, 128, 512, 1024";
  2. "Kick output bit-identical for every block size";
  3. offset clamping, kept.
  `Tank::kKickPlaceholder` goes away.
- **`test_tank` `kickReachesAllSprings`:** "== input impulse" becomes "heard on
  L and R in every SPRINGS mode, onset at N + latency". The Kick
  determinism/block-size tests stay as they are.
- **Plugin `PluginHostTest` Kick timing:** the same change as test_kick.
- **`test_drive` (M5):** SPLASH defaults to 0.3 and DRIVEN, so Clatter and Jolt
  now colour the snare-hit renders. The DRIVE-loudness (±2 dB) and ATTITUDE
  loudness checks may shift a little, and the "DRIVE 0 vs 0.5 difference"
  checks will read somewhat larger (the Hit depends on the post-DriveIn
  level). If any fails, pin SPLASH 0 and WOBBLE 0 in those tests: they are
  drive tests. SPLASH 0 in DRIVEN/KICKED still has the floor Clatter,
  −27 dB below SPLASH 1, so for a pure drive measurement use a hook
  or accept it.
- **M6 AntiRes metric:** re-run with M7 active. The noise-burst stimulus
  triggers Clatter and Jolt. The Jolt settles within 1 s (< 1 % of peak,
  < 0.05 cents: `test_splash`), and the metric only looks from 1 s on, so it
  should hold.
- **New Tank-level tests** to add at integration: ghost < 25 % Clatter
  energy on `02_hits` through the whole Tank; Kick low end ≥ 20 dB down in
  300 ms through the Tank at DECAY max, all ATTITUDEs; 12/s gate train
  → 36 onsets; WOBBLE cents on `08_held_tones` (the M7 [A] criterion,
  targets from the Magneto takes when recorded).

## Numbers (per ATTITUDE)

| | CLEAN | DRIVEN | KICKED |
|---|---|---|---|
| Clatter amount SPLASH 0 → 1 | 0 → 0.35 (ADR 0025; M7: 0) | 0.18 → 0.55 | 0.25 → 1.0 |
| Clatter burst decay (weak → Hit 1) | 4 → 10 ms | 6 → 18 ms | 8 → 30 ms |
| Rattle impacts after a Hit-1 stroke | 0 | 1 | 3 |
| Jolt amount SPLASH 0 → 1 (Kick = max) | 0 → 0.50 (ADR 0025; M7: 0) | 0.10 → 0.50 | 0.20 → 1.0 |
| Jolt decay / L depth / Δa at j = 1 | 60 ms / 0.2 % / 0.005 | 90 ms / 0.6 % / 0.05 | 180 ms / 1.1 % / 0.12 |
| Hard snare, SPLASH 1: Jolt peak, lurch at L = 55 ms | – | 0.15 % of L, −12 cents | 0.59 % of L, −43 cents per pass |
| Kick thump (Hz, 1/e) / burst | 80→55 Hz, 22 ms / 0.20 | 75→50 Hz, 28 ms / 0.35 | 70→45 Hz, 35 ms / 0.50 |
| Kick energy re KICKED | −8.0 dB | −3.9 dB | 0 |

Hit (snare at −6 / −12 / −18 dBFS): SPLASH 0 = 0.27 / 0.05 / 0.006, SPLASH 1 =
0.91 / 0.56 / 0.14. At SPLASH 1, ghost Clatter energy is −21 to −25 dB below
the hard hit (limit: −6 dB, i.e. 25 %). On `02_hits.wav`, KICKED SPLASH 1:
−12 dBFS −6 dB, −18 dBFS −18 dB. Clatter starts 1.3–4.6 ms after the hit
(seeded jitter).

## WOBBLE in the tail (the pitch maths)

Per pass: a delay read at `m[n]` plays at rate `1 − Δm`, so the pitch shift is
`c = 1200·log2(1 − Δm)`. For the sine alone, the peak is
`1200·log2(1 + 2π f D / fs)`. So depth is specified in **cents per pass** and
converted to samples: `D = (2^(c/1200) − 1)·fs / (wS·2πf + wR·2 f_r)`. That
keeps WOBBLE independent of DECAY. SPEC §4.7's "0.5–1 % of L" gives 1.7–3.4
cents per pass at 1 Hz and L = 55 ms, and 3.3–10.9 cents over L = 30–100 ms.

**The Loop multiplies it.** A held tone sits on the Loop's resonances, whose
phase slope is about `1/(1−g)` round trips. So the wet tail's pitch moves a
factor G more than one pass: G ≈ 1.5–1.8 at DECAY 0, 1–7 (typically 4–6) at
noon, 2–4.5 at max (measured in `test_wobble` with a held 1 kHz tone through
a modulated feedback Loop, 10-cycle averaged pitch). The zones are therefore
set per pass, *low*, so the **heard** tail lands in ADR 0008's zones:

| WOBBLE | Zone | per pass (peak) | tail p95, DECAY 0 / noon / max | rate |
|---|---|---|---|---|
| 0 | floor only | 0 | 0 (M6 floor only) | – |
| 0.25 | Drift | 0.18 cents | – | 0.22 Hz |
| 0.50 | Drift end | 0.82 cents | 1.4 / 0.7 / 3.6 cents | 0.41 Hz |
| 0.625 | transition | 1.6 cents | 2.8 / 8.5 / 5.5 cents | 0.56 Hz |
| 0.75 | Warble start | 3.2 cents | 5.3 / 22 / 7.4 cents | 0.76 Hz |
| 1.0 | Warble max | 12.3 cents | 19 / 52 / 43 cents | 1.40 Hz |

The curve is `c(w) = 12·(e^{5.42 w} − 1)/(e^{5.42} − 1)` cents per pass. At
WOBBLE 1 that is D ≈ 41 samples at 48 kHz: 2.8 % of L at DECAY 0, 1.5 % at
noon and 0.85 % at DECAY max. The single-tone tail numbers vary with where
the tone sits against the Loop's resonances; the real Spring's dispersion
will spread them. **Calibrate `kWobbleMaxCents` against the Magneto WOW &
FLUTTER takes on `08_held_tones` (ADR 0020) after integration.** The M7 [A]
criterion measures the output, which includes G.

## CPU and flash

Object sizes, `arm-none-eabi-g++ -O3 -mcpu=cortex-m7 -mfpu=fpv5-d16
-mfloat-abi=hard -std=gnu++17 -fno-exceptions -fno-rtti -ffunction-sections`
(`.text`, before link-time GC):

| Object | .text | RAM (object) |
|---|---|---|
| Splash.o | 4.1 kB | 360 B |
| Kick.o | 2.1 kB | 276 B |
| Wobble.o | 1.4 kB | 68 B each (×3) |
| **Total** | **7.6 kB** | ~0.8 kB (DTCM, in the Tank) |

For comparison, Drive.o is 3.7 kB. The only libm calls are `expf`, `sinf` and
`cosf`, which the Firmware already links (Filters.h). There is no `powf` and
no tables. With the Firmware at 108 kB of 128 kB, M7 leaves about 12 kB of
headroom. If flash gets tight, the first things to trim are the inlined
`prepare()` bodies (about 1.1 kB across the three).

CPU (host benchmark, scaled by the SPEC §5 estimate of ~800 cycles/sample
for one Spring measured in the same run): Splash about 115 cycles/sample
(detector always on; Clatter when active), Kick about 70 while ringing and
~5 idle (it skips its filters when idle), 3 × Wobble about 25 in total (one
`sinf` per 32 samples each). Worst case about **210 cycles/sample, ≈ 2 % of the
10 k cycles/sample budget**. Confirm with `CpuLoadMeter` at M3/M8.

## Open questions for the owner (musical)

1. The **Kick's direct thump** goes to the wet bus (the pickup hears the tank
   body). Should it also appear at MIX 0? (It currently would not.)
2. **SPLASH 0 in DRIVEN**: the natural splash sits 27 dB below SPLASH max on
   a hard hit. Is that "faint but there", or should it be louder?
3. **Jolt direction**: Spring B lurches the opposite way (a stereo pitch
   spread on big hits). Keep it, or make all Springs lurch together?
4. **WOBBLE top end**: about 45–50 cents in a long tail at WOBBLE max
   (worn-out tape). Is that the right ceiling before the Magneto numbers
   arrive?
5. Should a **Kick while SPLASH is 0** still give the full crash (the current
   design: "forces maximal SPLASH jolt"), or follow the knob a little?
6. **MIX sweep loudness** holds (±1.0 dB) on snare hits. The wet is +3.4 dB
   on pink noise and −4.4 dB on white noise, because the wet path is dark and
   builds up. Is that acceptable, or should the wet level adapt?

## Integrated (28 Sep 2026)

Wired into `core/dsp/Tank.*` and `core/dsp/Spring.*` as planned above, with
these deviations and findings:

- **Spring API.** `Spring::process(in, highIn, lFrac, lSamples, out, n)`
  (each extra may be null; the old 3-argument form still works). `highIn` =
  (Tilt out + Kick loop feed + Clatter) × CLEAN HF gain (ramped across each
  tick); `lFrac` = Jolt × `kJoltSpringScale[i]` (fraction of L); `lSamples` =
  WOBBLE (samples). Read = `lCur·(1 + floor + lFrac) + lSamples`, after the
  slew limiter, clamped to the delay memory. `lowDelaySize` now holds
  `L·(1.02 + 0.0125) + wobbleMaxDepthSamples + 8` (+~110 floats per Spring).
- **WOBBLE does not use `SpringSettings::modDepth/lfoDepth`** (the M6 hook
  comment). The Wobble generator specifies depth in cents per pass, so it is
  a per-sample offset; modDepth/lfoDepth stay floor + Howl (AntiRes.h updated).
- **Clatter gain ×5 (`kClatterGain` 0.6 → 3.0).** Through the Tank the Clatter
  lands in the high path (HPF at 0.8 fC, level 0.225 at TONE noon), so at 0.6
  KICKED SPLASH 1 added +0.2 dB of 1–6 kHz to a hard snare (Clatter 16 dB
  under the hit's own bright part): no crash. Now −2 dB (KICKED) / −8 dB
  (DRIVEN) re the hit. Ratios (ghosts) unchanged.
- **DRIVEN `joltAllpass` 0.05 → 0.025.** The Δa is common to all Springs; at
  0.05 (default SPLASH 0.3) it broke M4: mono notch −6.4 dB (limit −6) on
  chord stabs, 2 Springs, DECAY 0 BOING 1, and correlation 0.49 on hits
  (margin 0.47). Per-Spring Δa scales traded one failure for the other;
  halving passes all. KICKED keeps 0.12.
- **Tank level → rattle:** RMS of the wet mid (50 ms) × smoothed SPLASH
  (`kTankLevelSmoothMs`), so SPLASH 0 has no energy rattle.
- **Kick onset = N + 0** (the DriveOut oversampler's first tap answers at
  once), every block size. `Tank::kKickPlaceholder` removed.
- Test hooks: `Tank::setSplashEnabled / setSplashParts` (Clatter, Jolt) and
  read-only `splash()`, `kickVoice()`, `wobble(i)`.
- Same Clatter to all Springs kept: per-Spring Clatter was tried and changed
  no metric (the M4 notch came from Δa).

**Test expectation changes:** test_kick rewritten (onset N + 0 all blocks,
bit-identical, clamp, low end, 12/s); test_tank `kickReachesAllSprings` (heard
on L/R, onset ≤ N + 48 vs a no-Kick render instead of "== input impulse");
test_drive Tank-wet aliasing renders pin SPLASH 0 / WOBBLE 0 (WOBBLE/rattle
sidebands 10–20 Hz from the tone, −41 dB, are not aliasing); PluginHostTest
adds "MIDI Kick onset at N + 0". New `test_m7_tank`.

**Results** (test_m7_tank, test_kick; 02_hits, DECAY 0.5, DRIVE 0.5, 2 Springs):

| | Clatter re hit (−6 / −12 / −18 dBFS snare) | Splash energy −18 re −6 (snare / rim) |
|---|---|---|
| DRIVEN SPLASH 0 / 1 | −38.5 / −139 / – ; −8.0 / −16.5 / −19.1 dB | −142 / −113 ; −29 / −17 dB |
| KICKED SPLASH 1 | −2.0 / −9.2 / −17.1 dB | −17.7 / −15.3 dB |
| CLEAN SPLASH 1 | HF lift +0.17 dB, no Jolt, 99.9 % of the change above 800 Hz | – |

KICKED SPLASH 1: Jolt at +1 s 0.6 % of peak, 1–6 kHz within +0.3 dB of the
no-Splash render (settled). DRIVEN SPLASH 0 sits 30.5 dB under SPLASH 1
(plan: 27): open question 2 stands, it is very faint.
Kick < 100 Hz down 28.4 / 30.1 / 30.5 dB in 300 ms (CLEAN / DRIVEN DECAY 1 /
KICKED 0.88), 24.4 dB KICKED DECAY 1 (Howl zone). 12/s: 36/36 Kicks on their
exact sample, 36 forced strokes, all ATTITUDEs.

WOBBLE, 08_held_tones 1 kHz held, DRIVEN (default DRIVE), 1 Spring, p95 (peak) cents:

| DECAY | 0 | 0.25 | 0.5 | 0.75 | 1 |
|---|---|---|---|---|---|
| 0 | 0.0 | 0.1 | 0.8 | 3.4 (3.8) | 12.1 (16.7) |
| 0.5 | 0.0 | 0.0 | 0.2 | 8.3 (52) | 42.6 (96) |
| 1 | 0.3 | 0.3 | 0.3 | 15.7 (88) | 33.6 (75) |

At DRIVE 0.5 the Loop multiplies less (DECAY 0.5: 3.0 / 13.3 cents at 0.75 / 1).
After the tone stops the tail is several Loop modes beating, so pitch is
measured on the held wet. Magneto calibration (ADR 0020) still pending.

**M6 re-check** (renders/m6_*_m7 = files as-is, SPLASH 0.3 WOBBLE 0; `_sw0`
= 0 / 0; `_sw05` = 0.5 / 0.5): all 540 ringing cells pass, max `ringing_db`
12.3 dB, no steady tone. Howl: 0 failures as-is and at 0.5; at SPLASH 0 /
WOBBLE 0 one bursts cell (1 Spring, BOING 1, TONE 0) moves 0.47 % (limit
0.5 %). test_antires (loop gain, evenness, Howl exit) passes.

**CPU** (test_m7_tank, same ×15–25 estimate): 3 Springs KICKED BOING/TONE/DRIVE
1 with M7 busy (hits + Kicks 12/s, SPLASH 1, WOBBLE 1) 3870–6450 cycles/sample,
39–64 %; M7 share 180–300 cycles. **Flash:** release 117,420, m0test 94,304,
profile 128,676 of 131,072 B (profile 2.4 kB left). .text: Splash.o 4,128,
Kick.o 2,120, Wobble.o 1,368, Tank.o 11,227, Spring.o 5,952 B.

**Listening:** renders/m7_splash (ATTITUDE × SPLASH), renders/m7_wobble,
renders/m7_kick/*.wav (`presets/sweeps/m7_kick.json` is an --auto file:
singles, a pair, a 12/s train; each ATTITUDE, DECAY 0.5 and 1).
