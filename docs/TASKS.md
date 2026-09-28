# Owner tasks

Your running to-do list. Claude keeps it current: new tasks are added, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 28 Sep 2026 (IR library added) · **Milestones:** M0 needs your hardware check · M1 needs a Wellspring take + your listen · M2 needs your Ableton check · M3 blocked on M0 · M4 in progress (stereo fix running)

## To do (suggested order)

### 1. Record the Wellspring (≈45 min) · unblocks M1
- **Doc:** [recording-recipe.md](recording-recipe.md) · Ableton settings: [ableton-setup.md](ableton-setup.md)
- [ ] Take **0** (loopback) and take **A** (clicks) first. A alone lets Claude finish M1
- [ ] Core: **B, C** (hot INPUT), **D, E, E2**
- [ ] Optional: **F** (knocks; skip if weak), **G** (ringing, delay on)
- [ ] Save as `test_audio/reference/wellspring_<take>_<desc>.wav`, one line per take in `test_audio/reference/NOTES.md`
- Check every take: delay DRY/WET fully **dry**, MAGIC **zero**, SPRINGS fully **wet**

### 2. Record the Magneto (≈30 min)
- **Doc:** [recording-recipe-magneto.md](recording-recipe-magneto.md) · why: [ADR 0020](adr/0020-magneto-benchmark.md)
- [ ] Rack **off** → rear DIP **S2 = ON** (Dual Split). Set it back to OFF afterwards
- [ ] Spring (Right in/out): **MA, MB, ME**
- [ ] Tape (Left in/out): **MW0–MW4** (WOW & FLUTTER × 5, `08_held_tones.wav`), **MD1–MD3** (REC LVL green/amber/red)
- [ ] Save as `test_audio/reference/magneto_<take>_<desc>.wav`, with notes in the same `NOTES.md`

### 3. M0 hardware check on the Versio (≈30 min) · unblocks M3
- **Doc:** [m0-hardware-check.md](m0-hardware-check.md)
- [ ] Flash **`dist/resilio_versio_m0_test.bin`** (NE Firmware Swap → Select Custom File). Not `firmware/build/…`, which changes with every build
- [ ] Session 1, **USB only**: boot pattern, serial values for knobs, switches and button. Note which switch direction reads 0
- [ ] Session 2, **rack power only**: LEDs, CV at 0 V/5 V (LED_3 green), gate, passthrough vs cable
- Never connect USB and rack power at the same time

### 4. M2 Ableton check (≈15 min)
- **Doc:** [m2-ableton-check.md](m2-ableton-check.md) · MIDI clip: `test_audio/midi/kicks_16ths.mid`
- [ ] **Rescan plug-ins first**: the plugin was rebuilt (the VST3 had ~2,000 junk "MIDI CC" parameters; now fixed)
- [ ] Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz
- A 10th control, **Bypass**, is normal (added by the plugin framework)

### 5. Listen: M1 renders (≈15 min)
- Open from Finder: `renders/m1_click_grid/index.html`, `renders/m1_hits_grid/index.html`
- [ ] Does it sound like a **spring**, even thin?
- [ ] High BOING: boings natural, or stretched?
- [ ] DECAY 0.75 (flagged for resonance): does one note stick out of the tail?
- [ ] Do the audio players play? (Claude couldn't test playback)
- [ ] **New:** the click-grid page now has 45 real spring tanks from Ableton in its Reference section ([ADR 0021](adr/0021-ableton-ir-library.md)). Pin one as A and a render as B. Does ours sit among real tanks, or sound clearly different? Worth trying: *Space Echo Spring*, *Classic Amp Spring*, *BRX100 Gentle*, *Short Spring*
- Ignore the stereo flags on the click grid for now: they're the 1-Spring mono dip being fixed (see "Waiting on Claude")

### 6. Listen: M4 renders (≈15 min) · after the stereo fix lands
- Open from Finder: `renders/m4_springs_hits/index.html`, `renders/m4_springs_skank/index.html`
- [ ] 1 → 2 → 3 Springs: clearly **sparse/drippy → classic → dense/lush**, at similar loudness?
- [ ] Wide on headphones, and still full when summed to mono?
- (The first renders are already there. The fix only changes stereo width, so you can start before it lands.)

## What to send Claude

- **Recordings:** just say they're done. Claude commits and analyses them.
- **Checks (M0, M2):** pass/fail per step. For anything odd, a line of description or a short recording in `test_audio/m0/` or `test_audio/m2/`.
- **Listening:** plain-words answers to the questions above.

## Waiting on Claude (no action needed)

- M4 stereo fix: the 1-Spring mono dip, and 2/3 Springs too narrow at short DECAY
- After take A: match DECAY to the Wellspring's T60, build the M1 A/B page
- After M0 passes: M3 profiling on the Versio

## Done

- 28 Sep 2026: Ableton spring impulse responses found and analysed (IR library, ADR 0021)

- 28 Sep 2026: GitHub push working again (it was a network routing problem on your connection, not GitHub)
