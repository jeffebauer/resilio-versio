# Echo springs blend (prototype 6 Oct 2026)

Branch `proto/echo-springs-blend`. The owner's brief: "I'd like to hear a comparison of the echo mode with and without passing through the springs. The springs add diffusion which makes the individual repeats less distinct. Create a comparison page with the echo as it is today, the echo through the spring at 50%, 25%, and 0%." A follow-up the same day: the direct echo should be stereo, in two flavours, "wide but centred" and ping-pong.

Hidden Renderer key: `echo_springs_voicing` (0-6). The numbers are in `core/params/EchoVoicing.h` under "Springs blend", and the code is in `core/dsp/EchoDirect.h` plus the echo path in `core/dsp/Tank.cpp`. The firmware builds only voicing 0, with no blend code compiled in. Sizes are unchanged: release 125,492 B, profile 127,336 B, m0test 82,320 B.

Page: `tools/echo_springs_blend_page.sh` writes `renders/echo_springs_blend/index.html` (about 0.43 GB). It has CLEAN / DRIVEN / KICKED columns. The rows are:

- a rim at DECAY noon
- a rim at DECAY 0.8
- a rim with a fast echo (TENSION 0.75, about 0.18 s)
- the skank at DECAY noon
- a held C minor pad at DECAY noon

Every row is unclocked, with TENSION at noon (0.4 s) except the fast rim. WOBBLE is at noon (still), so the only difference between versions is the springs. TONE is noon, SPLASH 0.4, DRIVE 0.25 and MIX 0.6. Level matching is on.

## The versions

| Page | Name | Through the springs | The rest, heard directly |
|---|---|---|---|
| **A** (0) | today | all of it | none |
| **B** (1) | B-wide | half | half, wide |
| **C** (2) | C-wide | a quarter | three quarters, wide |
| **D** (3) | D-wide | none: the tape echo alone | all, wide |
| **E** (4) | B-pp | half | half, ping-pong |
| **F** (5) | C-pp | a quarter | three quarters, ping-pong |
| **G** (6) | D-pp | none: the tape echo alone | all, ping-pong |

(The page's switch needs single letters in order, so B-pp to D-pp appear as E to G.)

The blend is a crossfade: wet = s × (the springs' output) + (1 − s) × (the direct repeats). s is 1, ½, ¼ or 0.

**Wide but centred.** This works like two playback heads. The left channel gets the repeats as the tape plays them. The right channel gets them 8 ms later and a touch darker (a gentle 6 kHz roll-off). 8 ms is in the Haas range: you hear one wide repeat, not two. The delay and darkening apply only above 250 Hz, so the bass is identical in both channels and stays centred.

**Ping-pong.** This runs inside the tape's feedback. There are two tapes, cross-fed: the input goes onto the left tape, the left tape's playback (× DECAY's feedback) goes onto the right tape, and the right tape's playback goes back onto the left. So repeat 1 is left, 2 is right, 3 is left, and so on. The alternation follows the echo time and DECAY exactly. Each repeat passes through one tape's heads, wear (tape saturation) and WOBBLE once per pass, as today. Both tapes share one transport (same WOBBLE). In the B and C ping-pong versions, the springs hear both tapes summed. That is still every repeat once, as with today's single tape.

## Where the direct echo joins, and why

"Direct" means only the tape's **repeats**. The dry hit already reaches the output through MIX, so it isn't doubled.

The repeats join the wet at the springs' output: just after the springs' stereo mix, and just before the output pickups (DriveOut). From there they go through everything after the tank, exactly as the springs' output does:

1. the output pickups (DriveOut, DRIVE / ATTITUDE)
2. the high shelf
3. the wet µ-law box (DRIVEN / KICKED)
4. TONE's return
5. the limiter
6. the Hold's ducking
7. MIX

They are scaled like the springs' output too: by DRIVE's heard gain, the echo-mode trim (`kTrim`) and the echo's glide in and out.

They skip what comes before the springs: the Splash, the Clang and the Bite, DriveIn and TONE's tilt into the tank. Those exist to excite the springs, and on the direct path they would change the repeats' sound rather than the springs' share.

The springs themselves still hear the input plus the tape's repeats, as today. In B to D only their share of the output changes.

## Raw level differences (before the page's matching)

Whole-file RMS, dB relative to A, for CLEAN / DRIVEN / KICKED. The order within each group is B-wide, C-wide, D-wide, B-pp, C-pp, D-pp.

**At MIX 1 (the wet alone):**

| Row | CLEAN | DRIVEN | KICKED |
|---|---|---|---|
| rim, DECAY noon | −6.1 −12.0 −22.4 · −6.1 −12.0 −24.7 | −6.0 −11.9 −23.3 · −6.0 −12.0 −25.6 | −4.4 −10.4 −25.5 · −4.4 −10.4 −27.7 |
| rim, DECAY 0.8 | −6.0 −11.7 −18.4 · −6.1 −11.8 −20.7 | −6.0 −11.7 −19.3 · −6.0 −11.8 −21.6 | −4.8 −10.7 −21.2 · −4.8 −10.8 −23.5 |
| rim, fast | −6.2 −12.1 −21.9 · −6.1 −12.2 −24.0 | −6.1 −12.0 −22.8 · −6.1 −12.1 −24.9 | −4.2 −10.3 −24.9 · −4.3 −10.3 −27.0 |
| skank | −5.7 −10.2 −12.6 · −5.9 −11.1 −15.5 | −5.7 −10.1 −12.6 · −5.9 −11.0 −15.5 | −5.7 −10.0 −12.4 · −5.9 −11.0 −15.2 |
| pad | −5.8 −9.6 −9.8 · −5.8 −10.3 −12.7 | −5.9 −9.7 −9.7 · −5.8 −10.4 −12.7 | −5.9 −9.4 −9.2 · −5.8 −10.2 −12.1 |

**At MIX 0.6 (the page):**

| Row | CLEAN | DRIVEN | KICKED |
|---|---|---|---|
| rim, DECAY noon | −5.6 −10.0 −13.6 · −5.6 −10.1 −13.8 | −5.6 −10.3 −14.5 · −5.7 −10.3 −14.7 | −4.2 −9.6 −16.7 · −4.2 −9.7 −16.9 |
| skank | −3.6 −5.2 −5.6 · −3.7 −5.4 −6.1 | (same within 0.1) | −3.5 −5.0 −5.5 · −3.6 −5.3 −5.9 |
| pad | −2.8 −3.7 −3.8 · −2.6 −3.6 −3.9 | (same within 0.1) | −2.6 −3.4 −3.4 · −2.5 −3.3 −3.5 |

The page's level matching moves each version towards the panel's median, so C sits at about 0. On the rims, A is turned down about 10 dB, B / E about 4.5-5.5 dB, and D / G turned up 3.5-7 dB. On the skank and pad the moves are under 5 dB.

**What the gap is, and what it isn't.** It is mostly not the repeats. Take a rim in CLEAN: the first three repeats peak at −13.6 / −25.7 / −34.2 dBFS in A, and at −17.1 / −27.0 / −33.9 dBFS in D. In KICKED they peak at −9.1 / −22.0 / −33.5 (A) against −16.5 / −26.9 / −33.7 (D). So the repeats themselves come through within about 3.5 dB in CLEAN, and within about 7 dB on KICKED's first repeat (its pickups push the springs' Clang).

The big gap at MIX 1 is two other things:

- the springs' splash of the hit itself, which the springs hear and the direct path doesn't
- the springs' 1.7 s ring between and after the repeats

**What a real voicing would need.** Give the direct repeats about +3 dB (KICKED +5 to +7 dB on the first repeat) to meet A's repeats. Then decide whether the missing splash of the hit (B / C still have half or a quarter of it) should be made up at all. That is a musical question: in D the hit has no spring at all.

The level matching is anchored on MIX 0.6 because the dry hit is in every version there. At MIX 1, D / G would need 20-28 dB of make-up on the rim, past the page's 12 dB cap, and their repeats would come out much louder than A's.

## Mono (L + R)

These are K-weighted loudness numbers for the wet alone (MIX 1, hits, DECAY noon): the mono fold-down (L + R)/2 against the stereo, from `test_echo_mode blend`.

| Version | Mono vs stereo |
|---|---|
| A | −3.0 dB |
| B-wide | −2.9 dB |
| C-wide | −2.8 dB |
| D-wide | −2.2 dB |
| B-pp | −2.9 dB |
| C-pp | −2.9 dB |
| D-pp | −3.0 dB |

For reference, −3 dB is what two unrelated channels give and 0 dB is a centred mono signal.

- **Wide:** the bass folds to mono untouched. Above 250 Hz, L and R are 8 ms apart, so in mono the repeats' highs get a fine comb (notches every 125 Hz, shallower above ~6 kHz where R is darker). Expect a slightly phasey, hollow repeat in mono, at about the same level. Because L leads by 8 ms and is a touch brighter, the image leans a little left (L is ~1.6 dB louder than R in D-wide on a rim).
- **Ping-pong:** each repeat is wholly in one channel, separated by more than 200 dB in the test. So the mono fold-down is exactly the mono echo, with no comb and no colour change, each repeat at −6 dB per channel (−3 dB in loudness). On a single hit the left side is louder overall (~8 dB on a rim in D-pp), because it gets the first, loudest repeat.

## Cost

Desktop, `test_echo_mode blend`, KICKED, DRIVE 1, DECAY 1, TENSION 0. A is 453 ns/sample.

| Flavour | Desktop cost | Renderer wall clock, 143 s skank | Rough chip estimate |
|---|---|---|---|
| Wide | 453 ns/sample (unmeasurable: one 8 ms line and two one-pole filters) | +0.2-0.3 % | +0.1-0.2 points |
| Ping-pong | 482 ns/sample (+6.3 %) | +3.5-3.9 % | +2.7-4.8 points, so 78.5-80.6 % |

Echo mode peaks at 75.8 % on the chip against an 80 % ceiling. Ping-pong costs a second tape: a second read, record, wear and WOBBLE. On the chip that puts echo mode at or over the 80 % ceiling.

Ping-pong also needs a second 2 s tape: 97,024 floats, 388 KB. Today's tape lives in AXI SRAM, which already holds 429 KB of its 512 KB, so a second tape doesn't fit there. It would have to go in SDRAM (slower random access) or halve the longest echo time.

## The first, mono version (dropped)

Before the stereo follow-up, a mono, centred direct echo was built (the same join and scaling). It wasn't put on the page.

## What to listen for

- **The repeats' edges.** On the rim at noon and at 0.8, does each repeat stay a distinct hit as the springs come out (A → B → C → D)? Is ¼ springs (C / F) already enough of a spring to sound like the unit, or does it read as a plain delay?
- **The fast echo (TENSION 0.75).** Repeats 0.18 s apart smear together most through the springs. This is where B / C should help most.
- **The hit itself.** With less spring, the hit's own splash is smaller too (D / G: none). Is that a loss, or cleaner?
- **Wide against ping-pong.** Does wide sound like one wide repeat in the middle (like the springs' spread today, only clearer), or like a doubled echo? Does ping-pong's bouncing suit dub, or pull focus from the skank?
- **Skank and pad.** In a groove, does B (half and half) keep the tank's character while letting the offbeat repeats speak?
