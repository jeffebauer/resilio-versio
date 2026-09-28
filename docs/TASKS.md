# Owner tasks

Your running to-do list. Claude keeps it current: new tasks are added, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 28 Sep 2026 (IR library added) · **Milestones:** M0 needs your hardware check · M1 needs a Wellspring take + your listen · M2 needs your Ableton check · M3 blocked on M0 · M4 + M5 built, need your listen · chirp-direction decision needed

**Plugin installed in Ableton:** M5 build (commit `d73d818`), installed 28 Sep 14:00. It only changes when Claude installs a milestone and notes it here. **Rescan plug-ins** after any change.

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
- [ ] **Rescan plug-ins first**: the installed plugin is now the M5 build (see the top of this page)
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

### 6. Listen: M4 renders (≈15 min)
- Open from Finder: `renders/m4_springs_hits/index.html`, `renders/m4_springs_skank/index.html`
- [ ] 1 → 2 → 3 Springs: clearly **sparse/drippy → classic → dense/lush**, at similar loudness?
- [ ] Wide on headphones, and still full when summed to mono?
- The stereo fix has landed and the pages are re-rendered: 0 flags

### 7. Decide: which way should the boing chirp? (≈15 min) · blocks retuning BOING
- **Why:** the IR study ([ir-dispersion-study.md](ir-dispersion-study.md)) found that **every real tank** has its **highs arriving later** than its lows. Ours does the opposite (lows later), because the spec said "highs before lows". The spec's wording came from planning, not measurement, so this is a genuine conflict. It's your call by ear.
- Open from Finder: `renders/chirp_ab/current/index.html`. The renders are **ours now** (lows later). The references include **flipped** (highs later, *untuned*: treat BOING 1 as a direction test only) and six real tanks: *SNRA500 Plucky* (the clearest real chirp), *Amp Spring Bright/Dull*, *Classic Amp Spring*, *Space Echo Spring*, *Short Spring*
- [ ] Which sounds more like a spring to you: **current** or **flipped**? Or does it depend on BOING?

### 8. Listen: M5 drive + TONE (≈15 min)
- Open from Finder: `renders/m5_attitude_drive/index.html` (ATTITUDE × DRIVE on hits), `renders/m5_tone/index.html` (TONE sweep), and `renders/m5_howl/m5_kicked_howl_decay1_drive0.8_pull6s.wav` (KICKED Howl, DECAY pulled back at 6 s)
- [ ] CLEAN / DRIVEN / KICKED: clearly **hi-fi / warm tape dub / trashed**?
- [x] **DRIVE at noon:** too subtle, even at max. Decided: obvious from noon, cranked tape/tank at max, level constant ([ADR 0022](adr/0022-drive-retune.md)). Retune queued after M6
- [ ] TONE: full left warm and dark with the boing still there? Full right splashy but not harsh?
- [ ] Howl: a rideable rough roar that dies away when DECAY comes down?
- [ ] **Decide:** if you flip ATTITUDE away from KICKED *while Howling at max DECAY*, should it calm into the normal long (~9 s) tail, or fade within 1–2 s (as ADR 0018 says)? Both can't hold at max DECAY

## What to send Claude

- **Recordings:** just say they're done. Claude commits and analyses them.
- **Checks (M0, M2):** pass/fail per step. For anything odd, a line of description or a short recording in `test_audio/m0/` or `test_audio/m2/`.
- **Listening:** plain-words answers to the questions above.

## Waiting on Claude (no action needed)

- M6 anti-ringing (running): first recalibrating the Ringing metric (it currently "fails" 24 of 45 real tanks), then the Micro-mod floor
- M7 components (running, separate worktree): Splash (Hit, Clatter, Jolt), real Kick, WOBBLE, MIX checks
- After M6 lands: DRIVE retune ([ADR 0022](adr/0022-drive-retune.md)) + wiring the M7 components into the Tank, then a new plugin build for you
- BOING retune: after your chirp-direction decision (task 7)
- M3 profiling: firmware is ready ([firmware/README.md](../firmware/README.md)); flashing waits for your M0 check
- M6 anti-ringing: after M5
- After take A: match DECAY to the Wellspring's T60, build the M1 A/B page
- After M0 passes: M3 profiling on the Versio

## Done

- 28 Sep 2026: plugin in Ableton pinned to the M5 build; dev builds no longer auto-install

- 28 Sep 2026: M5 drive chain + TONE built; IR dispersion study done ([report](ir-dispersion-study.md))

- 28 Sep 2026: M4 stereo fix (mono-safe, wide at every DECAY); M3 firmware variants ready; README added

- 28 Sep 2026: Ableton spring impulse responses found and analysed (IR library, ADR 0021)

- 28 Sep 2026: GitHub push working again (it was a network routing problem on your connection, not GitHub)
