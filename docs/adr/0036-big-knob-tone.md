# 0036 — TONE's right side as King Tubby's Big Knob

**Status:** Accepted, 2 Oct 2026 (owner: **voicing 5, the bump on hits only**, after two check pages). Amended 4 Oct 2026: the Big Knob sits after the Springs, on the wet return (see "Amendment" below). TONE's right half: an 18 dB/oct low cut from 20 Hz at noon to **800 Hz** fully right (the research's 1.2 kHz rang in one KICKED corner), plus the Altec's console-loading bump (+4.1 dB fully right, easing off over the top) blended in only while a sharp, cracking hit lasts (the Splash's hit reading, SPLASH-independent). Left of noon and noon unchanged. Default `kToneDefaultVoicing = 5`; voicings 0–4 stay as Renderer-only references (`tone_voicing`). Rounds: `docs/m8-tuning-backlog.md` "Big Knob TONE".

**Context:**
- King Tubby's "Big Knob" was the stepped high-pass filter on his MCI desk (an Altec 9069B: two capacitors and a coil, 18 dB/oct, 70 Hz–7.5 kHz). He swept it on the reverb and echo sends to thin a sound out, telephone-like, before it reached the spring.
- Today TONE's right half is a gentle tilt plus a 12 dB/oct low cut, 20 Hz at noon up to 300 Hz fully right. The owner wants the right half to be the Big Knob: a steeper low cut, reaching higher, with a nasal, "ringy" bump above the cutoff that grows as TONE turns. **No clicks or steps** (a smooth sweep, not the original's switch), and **explore the coil's tonal character** (owner, 30 Sep / 1 Oct 2026).
- Open question for the owner: "TONE fully right: thin and splashy enough, too thin, or should the low cut start earlier?"
- Research finding: the Altec is a textbook flat filter only when fed and loaded at 600 ohm. Wired into a desk with a low-impedance output and a higher-impedance input, it **peaks at ~1.3–1.4× the cutoff**, by a few dB up to +16 dB depending on the wiring. That is the likeliest source of the bump. How Tubby's desk was wired isn't documented. Coil saturation isn't documented either; the unit was rated far above desk levels.

**Decision (proposed): one of these four, Renderer key `tone_voicing`.** Left of noon and noon are today's, bit for bit, in every voicing.
- **0 = today** (reference): 12 dB/oct, 20 → 300 Hz.
- **1 = steep:** 18 dB/oct (a flat 3rd-order filter, the Altec matched at 600 ohm), 20 Hz at noon → ~170 Hz at TONE 0.7 → ~490 Hz at 0.85 → **1.2 kHz fully right**. It stops there because the Springs only respond from ~200 Hz to 4 kHz.
- **2 = steep + bump:** as 1, wired the way a desk would wire it: the circuit's own response, moving from "matched" at noon to "low-impedance source into ~1 kohm, coil Q ~10" fully right. That gives +2.2 dB at TONE 0.7, +4.1 at 0.85 and **+5.6 fully right**, at ~1.4× the cutoff.
- **3 = steep + bump + ringier when driven:** as 2, plus the lows pushed harder into DriveIn's existing coil saturator (up to +4 dB fully right). More DRIVE adds the lows' harmonics, which the Big Knob then lets through. No new nonlinear stage.
- Same place as today's low cut, before the Springs (thin what hits the tank), and it **replaces** it rather than stacking.
- **Level:** thinning takes energy out of the tank, and how much depends on the material (fully right, a skank lost 9 dB and a snare 2 dB). So the Tank measures what the low cut removed, above ~90 Hz where the tank hears, and gives back ¾ of it (at most 12 dB). It takes back a little for the bump, and for a driven tank, which squashes on its lows and so gets louder when they're removed. The Sustain trim may take that makeup back on held sounds, on top of its own 5 dB.

**Testable:** `test_drive` "bigknob" checks the slope (≥ 16 dB/oct, measured 18), the bump's size and place, hits and skank bit for bit at TONE ≤ 0.5, and loudness within ±3 dB on hits, skank and held tones at TONE 0.7 / 0.85 / 1, at every ATTITUDE and DRIVE 0.25 / 0.8. It also checks the Chirp at full CW in every voicing. `test_sustain_trim` checks the held pad, drone and organ at TONE 0.7 / 0.85 / 1, voicings 1–3, against the same limiter limits as the default. The M6 grid runs at TONE 0.7 / 0.85 / 1 (`presets/sweeps/proto_big_knob_m6_*.json`).

**Consequences:**
- Costs: release firmware **+1,228 B** with every voicing compiled in (main 124,728 → 125,956 B with this container's GCC 13.2; the owner's toolchain measured 126.5 KB on `main`, so ~127.7 KB of 128). A single picked voicing would drop some of that. CPU: the Tilt goes from 8.3 to 8.7 ns/sample on the desktop, plus four one-pole followers.
- The makeup is adaptive (two slow followers). It trims only new input, never a ringing tail, and holds in silence, like the Excitation trim.
- Held bass pads at TONE 0.7–1 reach the limiter a little more than today: worst moment 2.3 dB vs 1.3, red LED in 8–9 of 27 cells vs 5. That is within the Sustain trim's limits.
- **The M6 grid doesn't fully pass yet.** One noise-burst cell flags Ringing in every voicing 1–3: KICKED, DECAY 0.75, 3 Springs, TENSION 0, TONE 1, 19–20 dB at 3.1 kHz (limit 15; today 6.4). It isn't the bump or the makeup. It looks like KICKED's Loop saturation letting a ~3.1 kHz mode ring once the lows stop driving it. This must be fixed (in the Loop, not by limiting TONE) before any pick ships. The `steady_tone` flags at DECAY 1 / TENSION 1 / TONE ≥ 0.7 also happen with today's voicing (6 cells), so they predate this work.
- If the owner picks 1–3, `kToneDefaultVoicing` changes, test_drive's TONE checks are re-read (its low-cut check gets steeper numbers), and the firmware carries only that voicing.

## Amendment: Placement: after the springs (owner, 4 Oct 2026)

**Question** (`docs/research/dub-lens-critique.md` §3.2, direction B): "When you turn TONE right during a ringing tail, what should thin out?" The prototype (`docs/prototypes/tone-place/`, branch `proto/tone-place`) rendered the Big Knob before the Springs (A, as shipped), on the wet return (B) and split (C). **The owner picked B by ear** (`renders/proto_tone_place`): Black Ark's low cut on the return; turning TONE right thins the ringing tail at once, and turning back gives its body back.

**Decision:**
- The Big Knob (this ADR's low cut: slope, cutoff curve, the bump on hits, voicing 5) runs on the **stereo wet, after the pickups and the high shelf, before the limiter and MIX** (`dsp::ToneReturn`, `DriveVoicing.h` "TONE placement"). Firmware, plugin and Renderer all play it (`kTonePlaceDefault = kTonePlacePost`). Left of noon and noon are unchanged, bit for bit.
- The Tilt keeps the tilt and noon's 20 Hz guard; its own Big Knob sections are skipped (they are pass-throughs there).
- **Level:** the wet's own makeup: slow followers (0.3 s) of the wet's power above ~90 Hz into and out of the return filter, ¾ back (in dB), at most 12 dB, held in silence, ramped per sample; the bump correction (−1.2 dB × u) moves with it. The pre makeup's squash correction is dropped (nothing in front of the tank changes). A sweep thins the tail at once and the level swells back over ~0.3 s.
- The Kick's direct thump joins the wet before the pickups, so it is thinned with the wet.
- Renderer key `tone_place_voicing`: 0 = before the Springs (reference: main `c6df657`'s sound bit for bit, 25 of 25 renders: hits and skank, every ATTITUDE, TONE 0.2 / 0.5 / 0.85 / 1 at DRIVE 0.8, and a TONE sweep on a ringing tail), 1 = after (default). The split (2) was dropped. The firmware builds only placement 1 (`RV_FIXED_VOICINGS`).
- CPU: at noon and left of it (once its crossfades sit at 0 and its makeup at 1) the return stage is an exact pass-through and is skipped; only its into-follower runs.

**Numbers** (branch `feat/tone-after`, desktop unless stated):
- Slope / bump: unchanged filter (`test_drive` bigknob: 18.0 dB/oct; voicing 2 bump +5.8 dB at 1.39 × cutoff, voicing 4 +4.3 dB at 1.35 ×).
- Thinning a tail already ringing (TONE noon → 1 in one step, lows < 250 Hz 0.25 s later vs no move): **−22.7 / −22.6 / −20.1 dB** CLEAN / DRIVEN / KICKED (before the Springs: −0.0). New gate in `test_tone_place`: ≤ −10 dB.
- Loudness vs noon at TONE 0.7 / 0.85 / 1, hits / skank / held chords, every ATTITUDE, DRIVE 0.25 and 0.8 (limit ±3 dB): worst **+1.3 dB** (KICKED / DRIVEN hits at TONE 1); skank −0.8 to −1.2; held within ±0.7. Before the Springs on the same grid: worst −2.2 (KICKED held, DRIVE 0.8).
- Clicks: fast TONE noon → 1 → noon on a ringing tail, every ATTITUDE: 0 (worst ratio 4.2, limit 10).
- Chirp at TONE 1 (voicings 0–3 and the default 5): highs still after lows (TENSION 0: 74.3 vs 46.3 ms; TENSION 1: 22.3 vs 14.6 ms).
- Sustain trim (ADR 0035), held pad / drone / organ at TONE 0.7 / 0.85 / 1, default voicing 5 (now checked too): limiter pulls at most **1.59 / 0.00 / 1.73 dB** (before the Springs: 2.10 / 0.00 / 1.40; limit 3.0), 0 s past 2.5 dB.
- **Kick (ADR 0016)**, < 100 Hz down ≥ 20 dB within 300 ms, DECAY max (KICKED also 0.88), now also at TONE 0.85 and 1: worst **24.2 dB** (KICKED DECAY 1, TONE 1; before the Springs 38.2). At TONE 1 the thump is gone (its < 100 Hz energy ~50 dB lower than before); what is left is the knock and the crash, brighter and with a sharper peak (CLEAN −3.8 vs −9.0 dBFS).
- M6 grid (`presets/sweeps/proto_big_knob_m6_*`, 270 Ringing-grid cells, TONE 0.7 / 0.85 / 1): **0 Ringing, 0 steady_tone**; worst `ringing_db` 7.9 at TONE 0.85 / 1 and 8.4 at 0.7 (before: 7.4 / 8.2; limit 15). The Howl-zone sweeps (DECAY 1, where a Howl is meant to happen) read a more prominent Howl at TONE ≥ 0.85 (worst 67.5 vs 40.6 dB), since its lows are now cut on the wet; they are not part of the Ringing gate.
- test_tank stereo / mono margins unchanged (the return filter is the same on L and R, and its makeup is one gain).
- CPU, whole Tank, KICKED, 2 Springs, hits: TONE noon −1.6 %, TONE 1 +0.4 % vs before the Springs; across 36 corners (SPRINGS 1–3, TENSION 0 / 1, TONE 0.5 / 0.85 / 1, CLEAN / KICKED, DECAY 1) vs main: worst corner −1.1 %, TONE-right corners up to +2.5 % (busy machine; ±2 % noise).
- Flash: release 114,868 B (main 112,628: +2,240), profile 116,800 B (main 114,600: +2,200); 128 KB limit. No `vfma` in our objects (`-ffp-contract=off` holds).
