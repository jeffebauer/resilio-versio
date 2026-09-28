# IR dispersion study: do BOING and DECAY cover real spring tanks?

28 Sep 2026. Tool: `tools/ir_dispersion.py`. Raw results: `renders/ir_library/dispersion_results.json` (gitignored, like the IRs). Background: ADR 0021.

## The short version

1. **Real tanks chirp the other way round from ours.** In every one of the 45 Ableton IRs where a chirp is visible, the **highs arrive later** than the lows. The delay grows sharply as you approach the transition frequency fC. Our Spring does the opposite: **lows later** (a falling "peeew"), because BOING maps the allpass coefficient `a` to *negative* values (−0.45 … −0.72). The reference paper's own calibration of a real tank (DAFx-11, Table 1: Leem Pro KA-1210) fits `a` = **+0.62 … +0.69**, and the real IRs fit `a` ≈ +0.22 … +0.40 (+0.8 for the Swissecho). So on the *direction* of the chirp, BOING's range covers no real tank at all.
2. **On size, the numbers are close.** Real tanks spread arrival times by about 2–35 ms per trip (median 15 ms; one outlier at 59 ms). Our BOING range spreads them by 5–48 ms. If the sign of `a` were flipped, keeping the same stage counts, BOING would give 5–28 ms. That covers every real tank except the four biggest chirps (30–59 ms).
3. **DECAY's repeat times fit well.** Clean tanks repeat every 53–117 ms (median 69 ms); our DECAY range is 33–103 ms. Only the longest tanks (Swissecho, 116 ms) sit past our long end. Our short end (33–45 ms) is shorter than any clean tank in the library, which is deliberate for dub slap (ADR 0006). fC in real tanks is 2.0–6.4 kHz, mostly 2.6–4.9 kHz; ours is 2.8–4.4 kHz. The data does **not** show DECAY's coupling (bigger tank = lower fC): some long tanks have a high fC and some short ones a low fC.

The recommendation is at the end. In short: flip the sign of `a` (a design decision, because SPEC §2.1 and ADR 0007 currently say "highs arrive first"). Everything else only needs small nudges.

## What "chirp shape" means here

A spring echo is not a click. Each trip along the spring delays each frequency by a different amount, so a click comes back smeared into a sweep: the chirp. Each round trip smears it a bit more. So the useful number is **how much longer the round trip is for one frequency than another**. Picture the chirp's curve on a spectrogram: flat for no chirp, strongly bent for a big boing.

Per IR we report:

| Column | Meaning |
|---|---|
| **repeat ms** | Round-trip time at 1 kHz: how often the boing comes back. |
| **200→2k ms** | Round trip at 200 Hz minus at 2 kHz. **Positive = lows later** (our model). Negative = highs later. This is the number the brief asked for, but real tanks put most of their chirp *above* 2 kHz, so it undersells them. The next two columns show the chirp in full. |
| **highs later ms** | How much later the top of the chirp band (just below fC) arrives than the fastest band. This is the real-tank chirp. |
| **lows later ms** | How much later the bottom (≈150–200 Hz) arrives than the fastest band. This is our model's chirp. Real tanks often show a small "hook" of 1–8 ms here. |
| **fC Hz** | Where the chirp band ends (dispersion stops or the chirps blur together). About ±10 % on the synthetic tests, ±15–20 % on our renders. |
| **fit a / M** | Our model's parameters that best reproduce the measured curve (DAFx-11 §3.2.2). Only shown where the fit is trustworthy (rising curve, error < 1 ms). |
| **quality** | 0–1 score: how clear, complete and smooth the tracked chirp is. Below 0.25 = don't trust the shape numbers. |
| **category** | clean tank / processed (the name or the signal shows cab, tape or digital delay, layering, diffusion or drive) / unclear. |

## Method (one paragraph)

Split the IR into narrow frequency bands: 1/6 octave up to 1.6 kHz, then 150 Hz-wide bands every 1/12 octave up to 7 kHz, because the curve gets steep near fC. In each band, take the loudness envelope, flatten its decay and **autocorrelate** it: slide it against itself and look for the lag where it lines up again. That lag is the band's own round-trip time P(f). Following that peak from band to band traces the chirp curve P(f) across frequency. This is the per-band autocorrelation idea from the reference paper (Gamper, Parker & Välimäki, DAFx-11, §3.1–3.3). Because it only measures the *spacing* between echoes, it ignores the loud broadband pulse at the start of the Ableton IRs, which defeated the first attempt in `tools/ir_analysis.py`. It also ignores filter delays and where the first echo lands. The pulse is gated out anyway (the first 30 % of a round trip). fC is found two ways: where the curve's periodicity fades to half its mid-band value, and by fitting our allpass-cascade formula to the curve (DAFx-11 §3.2.2). For stereo files, L, R and mono are each analysed, and the clearest channel that isn't a plain delay is kept. The whole thing is stdlib Python (about 4 minutes for the library on 10 cores).

## Validation: does the method measure the right thing?

### (a) Synthetic IRs with known answers (`--selftest`): PASS, 0 failures

Each test signal is a loud noise burst at t = 0 (8× the tank's peak, like the Ableton IRs). It is followed by a feedback loop with a known delay L and a known allpass cascade (a, M, fC), plus a separate fast high-frequency echo series and a −75 dB noise floor. The exact answer is L + M·τ(f).

| Case | repeat ms (meas / true) | 200→2k ms | highs / lows later ms | fC Hz |
|---|---|---|---|---|
| mid tank (a −0.60, M 44) | 56.5 / 56.5 | 20.1 / 20.5 | 0 / 21.0 vs 0 / 21.4 | 3412 / 3400 |
| short, soft (a −0.45, M 24) | 34.2 / 34.2 | 5.2 / 5.3 | 0 / 5.6 vs 0 / 6.3 | 4213 / 4200 |
| long, steep (a −0.72, M 64) | 96.0 / 96.0 | 46.1 / 46.4 | 0 / 54.8 vs 0 / 53.8 | 2671 / 2700 |
| Leem-like, falling (a −0.62, M 100) | 60.6 / 60.5 | 39.4 / 39.7 | 0 / 43.0 vs 0 / 44.2 | 4427 / 4300 |
| **Leem KA-1210 as in DAFx-11 (a +0.62, M 100)** | 53.2 / 53.1 | −1.6 / −2.0 | 27.7 / 0.1 vs 27.8 / 0 | 4276 / 4300 |
| rising, mid (a +0.50, M 60) | 58.8 / 58.7 | −4.0 / −4.3 | 23.3 / 0.1 vs 23.5 / 0 | 3298 / 3300 |
| U-shaped, like real tanks | 86.4 / 86.3 | −4.6 / −4.6 | 28.8 / 1.6 vs 29.2 / 1.4 | 3024 / 3000 |
| tiny dispersion (a −0.20, M 20) | 63.5 / 63.5 | 2.4 / 1.9 | 0 / 3.1 vs 0 / 2.4 | (no chirp) |

The fit also recovers the model parameters for rising chirps: a +0.60 / M 96 against a true +0.62 / 100, and a +0.50 / M 60 exactly. For falling chirps, fitting `a` and fC is poorly conditioned (the paper says the same), so the tool only uses that fit for rising curves.

### (b) Our own renders (`--renders renders/m1_click_grid`): PASS, 30 of 30 checks

The predictions are computed from `Mappings.h` plus the Spring A detune (1-Spring mode), and Spring.cpp's loop filters.

| DECAY | BOING | repeat ms meas / pred | 200→2k ms meas / pred |
|---|---|---|---|
| 0 | 0 / 0.5 / 1 | 33.2/33.2 · 36.4/36.1 · 37.7/36.6 | 5.2/5.4 · 16.7/16.3 · 40.3/37.9 |
| 0.25 | 0 / 0.5 / 1 | 43.5/43.4 · 46.2/46.0 · 46.8/46.3 | 6.2/6.3 · 18.8/18.3 · 42.6/40.7 |
| 0.5 | 0 / 0.5 / 1 | 57.1/57.2 · 59.7/59.5 · 59.7/59.5 | 7.1/7.1 · 19.9/20.3 · 43.2/43.4 |
| 0.75 | 0 / 0.5 / 1 | 75.9/75.7 · 78.0/77.8 · 77.4/77.6 | 8.0/8.1 · 22.1/22.3 · 45.6/45.6 |
| 1 | 0 / 0.5 / 1 | 100.9/100.8 · 102.8/102.6 · 102.3/102.3 | 8.9/9.1 · 24.6/24.3 · 48.2/47.5 |

Everything is within 6 %; the worst case is DECAY 0, BOING 1, where the ridge is steepest. Cross-check with the M1 test's band pair (316 Hz against 0.67·fC, DECAY 0.5, BOING 1): this tool measures 30.2 ms, the mapping predicts 30.9 ms, and M1 reported 28.2 measured / 30.5 predicted. fC on the renders reads within ~15 % at DECAY ≥ 0.25. It isn't found at DECAY 0, and there is one outlier (6.4 kHz at DECAY 0.5, BOING 1).

**Conclusion: the method recovers known dispersion in both directions, with the broadband pulse present. The IR numbers below can be trusted as far as their quality column says.**

## Results: all 45 IRs

"C:" = Convolution Reverb pack "09 Springs", "H:" = Hybrid Reverb "Springs". "Eye" = what the spectrogram shows (I checked all 45 by eye). "(" means the arcs curve to the right going up, i.e. highs later.

| IR | ch | repeat ms | 200→2k | highs later | lows later | fC Hz | fit a / M | quality | category | eye |
|---|---|---|---|---|---|---|---|---|---|---|
| C: Amazing Stereo Spring | L | 65.6 | 5.2 | 15.5 | 8.0 | 3596 | +0.27 / 80 | 0.38 | clean tank | "(" clear |
| C: Amp Spring Bright | mono | 52.9 | −5.3 | 14.6 | 2.9 | 2773 | +0.29 / 58 | 0.40 | clean tank | "(" clear |
| C: Amp Spring Dull | mono | 54.0 | −5.8 | 6.5 | 0.2 | 2021 | — | 0.50 | clean tank | "(" clear |
| C: Amp Spring High | mono | 52.8 | −7.7 | 14.1 | 0.0 | 2617 | +0.31 / 46 | 0.41 | clean tank | "(" clear |
| C: BRX100 Gentle | mono | 83.7 | 47.4 | 0.5 | 5.0 | 1039 | — | 0.14 | unclear | diffuse wash |
| C: BRX100 Loud | mono | 83.7 | — | 2.8 | 5.7 | — | — | 0.17 | unclear | diffuse wash |
| C: Cab and Spring | L | 194.5 | −32.2 | 17.3 | 17.4 | 2923 | — | 0.17 | unclear | dense, cab-coloured |
| C: DVRS23 Strange Spring | mono | 104.3 | −16.1 | 0.0 | 0.9 | 3876 | — | 0.36 | clean tank* | odd: late jagged arrival |
| C: Dull Tapedelay Spring | mono | 103.0 | 17.0 | 0.8 | 0.0 | 1051 | — | 0.25 | unclear | diffuse (tape) |
| C: Farfi Spring Dirtier Narrow L | L | 63.9 | — | 25.1 | 1.7 | 4800 | +0.28 / 260 | 0.29 | clean tank | "(" clear |
| C: Farfi Spring Dirtier Narrow R | R | 63.9 | — | 21.3 | 3.4 | — | — | 0.29 | clean tank | "(" clear |
| C: Farfi Spring Dirtier Wide L | L | 63.9 | — | 21.4 | 1.7 | 4941 | +0.22 / 306 | 0.29 | clean tank | "(" clear |
| C: Farfi Spring Dirtier Wide R | R | 63.9 | — | 34.6 | 1.6 | 4800 | +0.27 / 268 | 0.29 | clean tank | "(" clear |
| C: Farfi Spring Dirtier Wider L | L | 72.8 | — | 25.4 | 2.0 | 4402 | +0.22 / 298 | 0.29 | clean tank | "(" clear |
| C: Farfi Spring Dirtier Wider R | R | 72.8 | — | 30.3 | 1.7 | 4276 | +0.24 / 287 | 0.29 | clean tank | "(" clear |
| C: Farfi Spring Dirty Narrow | mono | 32.1 | — | 20.3 | 0.0 | 4941 | +0.23 / 178 | 0.20 | unclear | "(" clear |
| C: Farfi Spring Hiss Narrow | mono | 63.9 | — | 21.4 | 0.0 | 4800 | +0.25 / 287 | 0.33 | clean tank | "(" clear |
| C: HIC100L Crash | mono | 75.8 | — | 59.0 | 0.0 | 6407 | +0.37 / 485 | 0.63 | clean tank | "(" very clear |
| C: Moist Diffused Spring Echo 3 L | L | 14.9 | — | 0.6 | 0.0 | — | — | 0.44 | processed | pulse, gap, diffuse |
| C: Moist Diffused Spring Echo 3 R | R | 24.5 | — | 0.6 | 1.4 | — | — | 0.36 | processed | pulse, gap, diffuse |
| C: Moist Spring Mono | mono | 52.1 | −0.5 | 0.0 | 5.6 | 5388 | — | 0.21 | unclear | "(" faint |
| C: SNRA500 Plucky | mono | 58.0 | −10.4 | 31.3 | 0.9 | 3394 | +0.40 / 165 | 0.55 | clean tank | "(" textbook |
| C: Swissecho Layered | mono | 116.1 | −14.2 | 15.7 | 1.3 | 2470 | +0.84 / 144 | 0.43 | processed | "(" + HF delay stripes |
| C: Swissecho RevR DelayL 1 | L | 116.4 | 14.9 | 2.1 | 2.0 | 1022 | — | 0.32 | processed | R = plain delay |
| C: Swissecho RevR DelayL 2 | L | 116.4 | 6.2 | 1.9 | 2.0 | 1023 | — | 0.58 | processed | delay clicks + low arcs |
| C: Swissecho RevR DelayL 3 | L | 116.4 | 0.2 | 3.1 | 2.0 | — | +0.21 / 406 | 0.45 | processed | delay clicks + low arcs |
| C: Swissecho RevR DelayL 4 L | L | 116.4 | −10.9 | 14.2 | 2.1 | 2332 | +0.40 / 44 | 0.62 | processed | low "(" arcs |
| C: Swissecho RevR DelayL 4 R | L | 116.5 | — | 1.4 | 1.8 | — | — | 0.34 | processed | pure delay |
| C: Swissecho Spring 1.5s Wide | mono | 116.5 | −36.9 | 5.0 | 1.7 | 2773 | +0.77 / 141 | 0.32 | clean tank | low "(" arcs, HF diffuse |
| C: Swissecho Spring 3.5s Mono | mono | 116.4 | 7.2 | 1.3 | 1.1 | 1039 | — | 0.31 | clean tank* | low arcs, HF diffuse |
| C: Swissecho Spring 3.5s Wide | R | 116.1 | −1.5 | 2.1 | 0.0 | 1053 | — | 0.47 | clean tank | low arcs, HF diffuse |
| H: Awesome Stereo Spring | L | 65.6 | 5.2 | 15.5 | 8.0 | 3596 | +0.27 / 80 | 0.38 | clean tank | same file as C: Amazing |
| H: Berlin Spring | mono | 229.8 | −21.0 | 16.9 | 0.7 | 2694 | +0.30 / 68 | 0.21 | unclear | "(" clear, several springs |
| H: Classic Amp Spring | mono | 86.7 | −4.9 | 1.7 | 2.7 | 975 | — | 0.40 | clean tank* | U-shaped first echo, lows only |
| H: Clean Spring Slapback 4s | R | 81.5 | −3.0 | 1.7 | 0.0 | 1649 | — | 0.52 | processed | U-shaped first echo, lows only |
| H: Combo Spring 1s | mono | 81.2 | −3.2 | 5.6 | 0.0 | 1558 | — | 0.68 | clean tank | U-shaped first echo, lows only |
| H: Long Stereo Spring | R | 68.0 | 2.7 | 0.3 | 0.0 | 1231 | — | 0.17 | unclear | early "(", then diffuse |
| H: Oldschool Spring | mono | 98.6 | −4.5 | 3.6 | 17.9 | 1342 | — | 0.35 | clean tank* | "(" clear + HF stripes |
| H: Overdrive Spring 1s | R | 66.9 | 6.1 | 1.3 | 5.2 | 3072 | — | 0.53 | processed | U-shaped first echo, lows only |
| H: Short Spring | mono | 48.4 | 1.7 | 2.8 | 5.0 | 1714 | — | 0.47 | clean tank* | early "(", then stripes |
| H: Space Echo Spring | mono | 42.5 | — | 3.4 | 0.0 | 3596 | +0.78 / 123 | 0.13 | unclear | faint "(", mostly diffuse |
| H: Vintage Stereo Spring 2 sec L | R | 54.7 | 6.4 | 0.6 | 11.3 | 1968 | — | 0.17 | unclear | diffuse |
| H: Vintage Stereo Spring 2 sec R | R | 54.7 | 8.0 | 3.0 | 13.6 | 1974 | — | 0.17 | unclear | diffuse |
| H: Vintage Stereo Spring 5 sec L | R | 86.8 | −18.6 | 1.8 | 2.8 | 2265 | — | 0.23 | unclear | diffuse |
| H: Vintage Stereo Spring 5 sec R | R | 86.7 | −18.4 | 1.7 | 2.9 | 981 | — | 0.23 | unclear | diffuse |

Notes on the table:
- **\*** The tool calls these five "falling" or "no chirp", but that is an artefact. The chirp above ~1–1.7 kHz blurs too much to track, so only the small bottom hook is measured. By eye, none of them has our falling shape.
- "C: Amazing Stereo Spring" and "H: Awesome Stereo Spring" give identical numbers: probably the same IR shipped twice. Count them once.
- **Swissecho "RevR DelayL":** despite the name, the *plain delay* (clean repeats, no chirp at all) is on **R** and the spring is on **L**. The tool detects plain-delay channels and skips them. Their 83/121 ms echoes are the delay, not the tank.
- The 200→2k column is often "—" for the Farfi tanks because their ridge only becomes trackable above ~300 Hz.
- Long repeat times (Berlin 230, Cab 195) come from the pattern of several springs together repeating, not from one spring's round trip. Treat them as unclear.

## Real tanks against our mapping

Across the 22 non-unclear IRs with a rising (highs later) chirp:

| | real tanks: min · median · max | ours now (DECAY × BOING range) |
|---|---|---|
| repeat time | 53 · 69 · 117 ms | 33 – 103 ms |
| chirp direction | highs later, every one | **lows later**, everywhere |
| highs later (chirp size) | 2 · 15 · 59 ms (most 6–35) | 0 |
| lows later (bottom hook) | 0 · 1.7 · 8 ms (Oldschool 18) | 5 – 57 ms (our whole chirp) |
| fC | 1.0 · 3.1 · 6.4 kHz (most 2.0–4.9) | 2.8 – 4.4 kHz |
| fitted a / M | +0.22 … +0.40 (Swissecho ~+0.8) / 45–300+ stages | −0.45 … −0.72 / 24–64 stages |

Our model per grid point (Spring A, 1 Spring), with the same Spring and stage counts but `a` sign-flipped. "Highs later" is measured up to 0.88·fC, like the IRs:

| DECAY (fC) | BOING 0 | BOING 0.5 | BOING 1 |
|---|---|---|---|
| | *now: repeat / lows later → flipped: repeat / highs later* | | |
| 0 (4.4 kHz) | 33 / 6.6 → 30 / 5.0 | 36 / 18.4 → 30 / 11.9 | 37 / 42.9 → 30 / 18.1 |
| 0.25 (3.9 kHz) | 43 / 7.3 → 41 / 5.6 | 46 / 20.3 → 41 / 13.3 | 46 / 46.5 → 41 / 20.2 |
| 0.5 (3.5 kHz) | 57 / 8.1 → 54 / 6.3 | 60 / 22.4 → 55 / 14.8 | 60 / 50.2 → 55 / 22.6 |
| 0.75 (3.1 kHz) | 76 / 8.9 → 73 / 7.0 | 78 / 24.5 → 74 / 16.6 | 78 / 53.7 → 73 / 25.3 |
| 1 (2.8 kHz) | 101 / 9.8 → 99 / 7.8 | 103 / 26.8 → 99 / 18.5 | 102 / 57.1 → 99 / 28.2 |

How to read it: flipped, BOING 0 sounds like the gentlest real tanks (Amp Spring Dull, Combo, about 5–7 ms), BOING noon like the typical amp tanks (Amp Spring Bright/High, Amazing Stereo, about 14–16 ms), and BOING 1 like the Farfi tanks (about 21–30 ms). SNRA500 (31 ms) and Farfi Wide R (35 ms) sit just past the top, and HIC100L (59 ms) is far out. Our current falling chirp is also *bigger* than any real tank's bottom hook: 43–57 ms at BOING 1 against 8 ms at most in the clean tanks.

With `a` positive, raising |a| past ~0.7 doesn't grow the chirp further. It only piles the delay closer to fC. Extra size has to come from more stages. At DECAY 0.5: M 64 peaks at ~23 ms (a ≈ 0.7), and M 96 at ~35 ms. Real tanks use a low `a` (0.2–0.4) and many stages. With our ≤ 64-stage budget, a higher `a` (0.35–0.7) is the closest affordable match, as in the DAFx-11 "M capped at 100" calibration. The trade-off is that the bend is a little sharper near fC than in a real tank.

## Recommendation (for the lead: nothing in core was changed)

1. **Chirp direction (the big one).** Flip BOING's allpass coefficient to positive: `kBoingCoeffMin` ≈ **+0.35** and `kBoingCoeffMax` ≈ **+0.70**, keeping `kMinStages` 24 and `kMaxStages` 64. Expected highs later per trip: ~4–28 ms across the DECAY × BOING grid, which covers every real tank except the four biggest chirps (SNRA500, Farfi Wide R / Wider R, HIC100L: 30–59 ms). This contradicts SPEC §2.1 ("High frequencies arrive before lows") and ADR 0007, so it needs an owner decision and a listening test first. Real tanks, the reference paper's calibration (a = +0.62) and its remark that the delay is largest near fC all point the same way. If the owner prefers the falling sound by ear, keep it knowingly as a stylised choice, but then BOING 1 should come down to about **−0.55** (≈ 15–25 ms of lows later): today's 43–57 ms is 5–7× any real tank's lows-later hook. Knock-on work if flipped:
   - The low-frequency round trip gets shorter. At DC, K(1−a)/(1+a) per stage becomes small, so repeat times drop by ~3–6 ms. See 3.
   - The loop's delay is then largest just below fC, where the chirp low-pass sits. Check `Spring::updateCoefficients`' T60 design bands (`kDesignHz` stops at 1.2 kHz) and the Ringing / AntiRes behaviour near fC.
   - `test_spring.cpp` "Chirp: highs before lows" must be inverted. Mappings.h comments and CONTEXT.md wording change.
2. **Chirp size: more stages would reach the big tanks, but CPU says no.** SNRA500, Farfi Wide and HIC100L need M ≈ 96+. Only worth it if the multirate Loop (Spring.h note, Parker 2011) frees the cycles. Not needed to cover typical tanks.
3. **DECAY's L range: optional small stretch at the long end.** `kLoopDelayMaxSeconds` 0.100 → **~0.115 s** would reach the Swissecho-type long tanks (116 ms), especially once the flip shortens round trips by a few ms. Delay memory grows ~15 %. Keep the 30 ms short end: it is shorter than any clean tank here, but it is the dub slap (ADR 0006), not a realism target.
4. **fC range: fine as is.** 2.7–4.2 kHz sits in the middle of real tanks (2.0–4.9 kHz, with HIC100L at 6.4 kHz). The DECAY coupling (bigger tank = lower fC) is neither confirmed nor refuted by this data. Real tanks vary independently (HIC100L: 76 ms and 6.4 kHz; Amp Springs: 53 ms and 2.6–2.8 kHz), so it is a free design choice.

## Best "classic dub" references in the library

- **SNRA500 Plucky:** the clearest real chirp in the set (quality 0.55). 58 ms repeat, 31 ms highs later, fC ≈ 3.4 kHz, fit a +0.40 / M 165. The best single target for chirp *shape*.
- **Amp Spring Bright / High / Dull:** a typical amp-tank family. 53 ms repeat, 6–15 ms highs later, fC 2.0–2.8 kHz, fit a ≈ +0.3 / M 46–58: almost exactly our stage budget.
- **Classic Amp Spring:** 87 ms repeat, very clear in the lows (low bands correlate at 0.7–0.8). Above ~1 kHz it is a dense wash with no trackable chirp, so its shape numbers are a lower bound. Good for repeat time and tail, not for chirp shape.
- **Space Echo Spring:** 42.5 ms repeat (the only short classic). Faint rising arcs in the first ~100 ms, then diffuse, so quality is low (0.13). Use it by ear, not by these numbers.
- **HIC100L Crash, Farfi series:** very clear, but big chirps (21–59 ms) with high fC (4.3–6.4 kHz). They mark the upper end.

## Limits

- The impulse responses are linear snapshots (ADR 0021): no drive, Splash or Kick.
- Multi-spring tanks give one blended ridge. Where springs differ, the strongest one wins and quality drops.
- The top ~10 % of the chirp band, just below fC, is steep and blurs within a few echoes. So "highs later" is a slight **under**estimate for real tanks, typically by a few ms. The synthetic tests show < 2 % error when the ridge is clear.
- The fC calibration (`FC_EDGE_CAL`) was set on the synthetic tests. Treat fC as ±15 %.

## Rerun

```
python3 tools/ir_dispersion.py --selftest                              # synthetic validation, ~10 s
python3 tools/ir_dispersion.py --renders renders/m1_click_grid          # our renders vs Mappings.h
python3 tools/ir_dispersion.py renders/ir_library --json renders/ir_library/dispersion_results.json
```

The model prediction in the tool (`MAP`, `DETUNE_A`) mirrors `core/params/Mappings.h` and `SpringModes.h` by hand. If those change, update it; `--renders` on fresh renders will show any drift.
