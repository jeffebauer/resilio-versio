# TONE placement prototype: filter before, after, or split

> **Decided (owner, 4 Oct 2026, by ear): B, after the springs.** It is now
> the behaviour everywhere (firmware, plugin, Renderer); see ADR 0036
> "Placement: after the springs". Placement 0 (before) stays renderable with
> `tone_place_voicing=0`; the split (2) was dropped, so the sweeps here
> render 0 and 1 only. The numbers below are the prototype's, measured
> against the older main it was built on. Before / after renders on the
> shipped code: `presets/sweeps/tone_after_*.json`.

Owner's question (3 Oct 2026): "When you turn TONE right during a ringing
tail, what should thin out?" Source: `docs/research/dub-lens-critique.md`
§3.2, direction B ("Return Knob"), §8 answer 2 ("hear all three first").
Branch `proto/tone-place`. Renderer-only key `tone_place_voicing`, default 0 =
today, bit for bit. Not built into the firmware (`RV_FIXED_VOICINGS`).

## The three placements

The Big Knob is TONE's right half: an 18 dB/oct low cut, 20 Hz at noon up to
800 Hz fully right, with the coil bump on sharp hits (tone voicing 5, ADR 0036).
Only the low cut moves. The tilt, Loop damping, high path and tank voicing 7's
coil and pickup stay where they are. Left of noon, all three placements are
today's, bit for bit.

| Key | Page | Where | What you hear |
|---|---|---|---|
| 0 | A | before the Springs (today) | The next hit goes in thin. The tail already ringing barely changes. |
| 1 | B | on the wet (the return): after the Springs, pickups and shelf, before the limiter and MIX | The tail you hear goes thin and telephone-like at once, and gets its body back when you turn back. |
| 2 | C | split: the 1st-order section (6 dB/oct) before, the 2nd-order section (12 dB/oct, with most of the bump) on the wet | The tail thins at once, a little less than B. A new hit passes both, so its total is today's 18 dB/oct. |

**Why this split (C).** The Big Knob is already two sections multiplied
together: a 2nd-order high-pass and a 1st-order one. C puts one on each side of
the tank. A tank with no saturation is linear, so a new hit passes both and
gets exactly today's curve, with no double thinning. The steeper share goes on
the wet because the question is about the tail you're hearing. The tank is fed
only 6 dB/oct thinner, so its LoopSat keeps most of its lows. Thinning the
input is what once made a KICKED tank ring (ADR 0036 round 2), so this keeps
that risk lower than A.

**Level makeup on the wet (B, C).** ADR 0036's makeup reads the Springs' input,
so the post filter needs its own. It uses slow followers (`kExcSeconds`, 0.3 s)
of the wet's power (L + R, above ~90 Hz, so the Kick's sub thump doesn't count)
going into and out of the return filter. It gives back ¾ of what the filter
took (in dB, at most 12 dB), held while the wet is silent, and is ramped per
sample. Because it reads the wet itself, it follows a ringing tail too: a sweep
thins the tail at once, and the level comes back over ~0.3 s as a gentle swell,
not a jump. ADR 0036's two corrections:
- **Bump correction** (−1.2 dB × u on bumped voicings): moves to the wet.
- **Squash correction** (a driven tank squashing on the lows the pre cut fed
  it): dropped for B, since nothing in front of the tank changes. C keeps a
  third of it, for the 6 dB/oct it still takes off the input.

**Smoothing.** TONE is smoothed over 5 ms (ParamSpec). The return filter is
redesigned once per 32-sample control tick from that smoothed value, as the
pre filter is. Each section is crossfaded in from an exact pass-through
(depth and k ramped per sample), so crossing noon never jumps.

## Numbers

- **Bit for bit:** placement 0 = the pre-change binary on hits and skank,
  CLEAN / KICKED, TONE 0.3 / 0.5 / 0.85 / 1 and a TONE sweep: 20 of 20
  files identical, with and without the key set. Placements 1-2 at TONE
  0 / 0.3 / 0.5 match placement 0 bit for bit (`test_tone_place`).
- **Loudness, TONE 0.7 / 0.85 / 1 vs noon** (BS.1770, the owner's settings,
  CLEAN / DRIVEN / KICKED; limit ±3 dB):
  - A pre: worst +2.0 (KICKED hits at TONE 1).
  - B post: worst +1.3 (KICKED hits); skank −0.9.
  - C split: worst −0.7 (KICKED skank).
- **Fast TONE move** (noon → 1 → noon in one step each, 5 ms smoothing only,
  on a ringing snare tail, every ATTITUDE): 0 clicks in all three placements
  (worst ratio 4.5 / 4.2 / 4.3, limit 10).
- **The tail's lows (< 250 Hz) 0.25 s after that move**, vs no move:
  - A: −0.0 dB.
  - B: −22.7 / −22.6 / −20.1 dB (CLEAN / DRIVEN / KICKED).
  - C: −19.0 / −19.1 / −17.0 dB.
- **Gesture 1 (one snare, swept over 1.5 s), at TONE 1:** the lows' share of
  the tail is about −9 dB in A, −25.6 dB in B and −23.8 dB in C (CLEAN). The
  tail is 3.5-4.5 dB quieter in B and 1.5-2.5 dB quieter in C than in A while
  it's thin (the makeup gives back ¾, ~0.3 s behind). Back at noon, B and C match A again.
- **M6 Ringing** (the Big Knob round's sweeps: clicks and bursts, DECAY 0.75
  and 1, every SPRINGS and TENSION, CLEAN / DRIVEN / KICKED in the bursts,
  TONE 0.7 / 0.85 / 1; 270 cells per placement): 0 Ringing and 0
  steady_tone in all three. Worst `ringing_db`:
  - TONE 0.85 / 1: A 7.4, B 7.9, C 8.0.
  - TONE 0.7: A 8.2, B 8.4, C 8.3.
  - Every placement's worst cell is the same one: bursts, CLEAN, 3 Springs,
    TENSION 0, DECAY 0.75, 5.1 kHz. Limit 15.
- **CPU, desktop, whole Tank** (KICKED, 2 Springs, TONE 1, hits; best of 3):
  - pre 510.7 ns/sample.
  - post 529.7 (+3.7 %).
  - split 530.1 (+3.8 %).
  - The return stage runs two paths (plain and bumped) × 2 channels, plus
    four weighting one-poles for its makeup. Without the hits-only bump path
    it would be about half that.
- **Flash, if B or C were the firmware default:** ~2.8 KB of Cortex-M7 code
  at -O3 (a probe file with the return stage, its tick and its makeup,
  measured with arm-none-eabi-size), plus a little Tank glue: ~3 KB. The
  release had ~8 KB headroom (119,944 B of 128 KB at the last check).

## Commands

```bash
cmake -S . -B build-proto -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-proto
ctest --test-dir build-proto > ctest.log 2>&1; grep "tests passed" ctest.log
bash docs/prototypes/tone-place/render.sh build-proto/rv_render renders/proto_tone_place   # renders + page
python3 docs/prototypes/tone-place/analyze.py renders/proto_tone_place                     # gesture numbers
bash docs/prototypes/tone-place/m6.sh build-proto/rv_render renders/proto_tone_place_m6     # M6 spot-check
python3 docs/prototypes/tone-place/m6_summary.py renders/proto_tone_place_m6
```

Sweeps live in `presets/sweeps/proto_tone_place_*.json`. The gesture
automation is in `proto_tone_place_auto_tail.json` and
`proto_tone_place_auto_skank.json`. A sweep JSON can now name an automation
file (`"auto": "path.json"`, applied to every render), so gestures render as
ordinary sweeps. `make_one_snare.py` writes
`test_audio/stimulus/proto_one_snare.wav` (the first 6.5 s of 02_hits: one
−6 dBFS snare at 1.0 s).

## Files

- `core/params/DriveVoicing.h`: "TONE placement": the constants,
  `bigKnobPre` / `bigKnobPost`, and the per-placement makeup trims.
- `core/dsp/Drive.h`: `Tilt::setPlace` and the `ToneReturn` stage.
  `core/dsp/Drive.cpp`: the Tilt uses `bigKnobPre`.
- `core/dsp/Tank.h`, `core/dsp/Tank.cpp`: `setTonePlaceVoicing`, the return
  stage after the shelf, and its makeup in `controlTick`. All inside
  `#ifndef RV_FIXED_VOICINGS`.
- `host/common/ParamsJson.*`: the `tone_place_voicing` key.
- `host/render/main.cpp`, `host/common/Sweep.*`: the key in manifests, and
  the sweep `"auto"` field.
- `host/tests/test_tone_place.cpp`: identity, loudness, clicks, thinning, CPU.
