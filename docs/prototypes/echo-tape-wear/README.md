# Echo tape wear (prototype, 5 Oct 2026)

Branch `proto/echo-tape-wear`. The owner's brief: echo mode's BBD grit doesn't fit next to the springs, the tape echo and the saturation, because its aliasing adds new, off-key pitches that no tape machine makes. Make the repeats wear like tape instead: "maybe a crinkle effect like the magneto, or the tape-style saturation and roll off". WOBBLE already does wow and flutter, so none of these move the pitch.

Each version works inside the echo's feedback. The first repeat is the same in every version, and each later repeat has been through the wear once more, so the effect builds up.

Hidden Renderer key: `echo_wear_voicing` (5, 6, 7 are new; 0-4 are unchanged; the default is still 3, BBD grit). The numbers are in `core/params/EchoVoicing.h` under "Tape wear" and the code is in `core/dsp/EchoWear.h` (`TapeSat`). The firmware still builds only the default: release 125,172 B and profile 127,000 B, byte for byte the same size as before.

Page: `tools/echo_tape_wear_page.sh` writes `renders/echo_tape_wear/index.html`. It has CLEAN / DRIVEN / KICKED columns and versions A / B / C1 / C2. The rows are a rim at DECAY noon, at 0.8 and at the held top; the skank at noon and at the top; a held C minor pad at noon and at the top. WOBBLE is at noon (still) so you hear only the wear, and level matching is on.

## The versions

| | What you hear | How |
|---|---|---|
| **A, BBD grit** (3) | Today's default: grit and pumping. Its aliasing adds faint new, off-key pitches. | Unchanged. |
| **B, tape saturation + roll-off** (5) | A loud, bright repeat comes back thicker and duller, because the highs squash first. Each pass loses a little more treble and gets a slight warm lift around 70 Hz, so a build melts instead of fizzing. Quiet repeats pass unchanged. | Tape's record EQ: the highs are boosted going onto the tape (+8 dB shelf above ~1.6 kHz), saturated (soft clip, unity when quiet, drive 1.25) and cut back by the exact inverse on playback. Then a 1-pole low-pass at 6.5 kHz each pass, and a head bump of +1.5 dB at 70 Hz (peaking filter, Q 1). The level is made up so the mids (300 Hz and up) are at exactly 1 and never above it. |
| **C1, B + crinkle, subtle** (6) | Now and then a short, papery flutter: for a few milliseconds the tape loses the head, and the level and the highs dip together. The pitch is untouched. Older repeats crinkle more because they have passed more tape. | Crinkled patches (on average 2.5 per second, 20-80 ms long, depth 0.3-0.7). Inside each patch are fast, irregular flickers (on average 150 per second, 0.5-2.5 ms each, smooth-edged dips). In a flicker, everything above 1.2 kHz loses the full depth and the level drops by half of it (−6 dB at full depth). Seeded, so renders repeat exactly. |
| **C2, B + crinkle, obvious** (7) | The same, but longer and denser, so the older repeats clearly break up. | 3.5 patches per second, 30-150 ms long, depth 0.5-0.95, with flickers at 220 per second, 0.7-4 ms each. |

The crinkle is faster and rougher than the "worn tape" voicing's dropouts (1: 3-40 ms dips, 2 per second, plus wow and flutter). It has no wow or flutter at all.

## Measurements (`test_echo_mode tapewear`)

- **No new pitches.**
  - Folds: a steady 0.5 tone fed straight into B (no heads in front, the worst case). Its strongest fold is −134 dB at 1.7 kHz and −62.5 dB at 4.1 kHz. A BBD is −45 / −34 dB on the same probe.
  - A loud 1.7 kHz tone on the tape: inharmonic energy on repeats 2-4 is −37 dB re the tone. With no wear it is −38 dB; A is −34, −29 and −27 dB (rising each pass).
  - New narrow peaks in a rim's repeats: B 9.1 dB, none 7.5 dB, A 11.8 dB.
  - A hot 15 kHz tone: the echo's folds stay under −95 dBFS.
- **No added energy.**
  - B on its own stays at or under 0 dB from 300 Hz up and loses 0.9 dB at 3 kHz and 2.5 dB at 6 kHz. The bump peaks at +1.3 dB at 80 Hz, where the heads' high-pass is already losing ~5 dB a pass.
  - In the loop, the per-pass peak is −2.48 dB vs −2.37 dB without wear.
- **Level per repeat** (burst, 0.6 s, feedback 0.8): repeats 2-6 sit 0.4-0.9 dB under no wear and 1.1-1.6 dB over A. A loses more per pass to its compander.
- **Held top** (DECAY 1, a single rim, every ATTITUDE): bounded, the limiter never moves, and KICKED backed off to noon is ~80 dB down within 3 s. B and C hold like the bare tape:
  - They build for ~30 s before they sit. Over 10-30 s, 1 s windows spread 5.5-6 dB; A spreads 2.3-2.5 dB.
  - After that they creep up about 1 dB a minute. A 150 s probe gave +1.3 dB over 40-150 s, while A stays flat.
  - The held level is about A's (+2 to −0.5 dB re the hit).
  - Why: A's compander and its grit keep the held sound broad and its level pinned. B is clean, so the held repeats narrow towards the band where the loop is strongest (~500-700 Hz) and grow slowly into the tape's saturation. A compressor in B didn't change this (tried and removed).
- **Repeatable and clean:** deterministic, blocks 1 / 7 / 333 bit for bit, no clicks, and M6 flags no ringing or steady tone (24 cells each, same as no wear).
- **Cost** (desktop micro-bench, the wear alone per sample): A 11.3 ns, B 8.6 ns, C1 / C2 8.1 ns. Whole Tank, SPRINGS 3 worst case: about the same as A, within the measurement noise. B and C should cost a little less than today's BBD on the chip. Only a chip run (profile build) is trustworthy, though: desktop estimates have run ~2× low, and echo mode already peaks at 76 % of an 80 % ceiling.

## Listen for

- Is the BBD's faint off-key shimmer gone in B, and does a loud, bright build melt into warmth rather than fizz (rim at DECAY 0.8, skank at the top)?
- Is the 70 Hz lift a warmth you like, or does any of it read as boom (pad rows)?
- C1: is the crinkle audible at all? It is sparse, and on some repeats it doesn't land. C2: papery and broken-up, or too much?
- The held top: B and C build for ~30 s and keep growing a little, where A settles in ~20 s and sits. Does that feel alive or unsteady?
