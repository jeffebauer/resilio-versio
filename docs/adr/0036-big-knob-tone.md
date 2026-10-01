# 0036 — TONE's right side as King Tubby's Big Knob

**Status:** Proposed (1 Oct 2026, branch `proto/big-knob-tone`, not merged). Four voicings on one listening page; the owner picks. The default stays voicing 0 (today) until then: the firmware and the plugin don't change. Research: `docs/research/big-knob.md`. Numbers: `core/params/DriveVoicing.h` "Big Knob TONE voicings". Measurements: `docs/m8-tuning-backlog.md` "Big Knob TONE".

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
