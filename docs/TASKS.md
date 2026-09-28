# Owner tasks

Your running to-do list. Claude keeps it current: new tasks are added, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 28 Sep 2026 (M8 prep started) (IR library added) · **Milestones:** M0 needs your hardware check · M1 needs a Wellspring take + your listen · M2 needs your Ableton check · M3 blocked on M0 · M4–M7 built; listening tasks 5–9 ready · chirp-direction decision needed

**Plugin installed in Ableton:** M7 + limiter fix (commit `81f8124`): SPLASH, the real Kick and WOBBLE work; the held-note ticks around DECAY noon are fixed. Installed 28 Sep. **Rescan plug-ins** (hold ⌥ and click Rescan).

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
- [ ] Howl: a rideable rough roar that dies away when DECAY comes down? (M6 makes it wander more, about ±7 cents like a siren, so it's never a steady tone. You'll hear that in the next plugin build)
- [ ] **Decide:** if you flip ATTITUDE away from KICKED *while Howling at max DECAY*, should it calm into the normal long (~9 s) tail, or fade within 1–2 s (as ADR 0018 says)? Both can't hold at max DECAY

### 9. Listen + M7 questions (≈20 min) · the build is installed
- Open from Finder: `renders/m7_splash/index.html` (ATTITUDE × SPLASH on hits), `renders/m7_wobble/index.html` (WOBBLE on held tones), and the Kick files in `renders/m7_kick/` (each ATTITUDE: singles, a pair, a 12-per-second train). Or play the plugin. Background: [m7-integration.md](m7-integration.md)
- [ ] KICKED crash big enough? DRIVEN moderate? Kick a tight thud + crash, clean on the fast train?
- [ ] WOBBLE 0.75 already clearly out of tune? Max (about 40 cents in the tail) the right ceiling?
- [ ] **SPLASH at 0 in DRIVEN:** hard hits keep a faint natural splash (about 27 dB below max). Faint enough, or too faint?
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on the left spring and the other way on the right, so hard hits briefly spread in stereo. Keep it, or lurch together?
- [ ] **WOBBLE at max:** about 50 cents of wobble in the tail (clearly seasick). Right ceiling, more, or less? (The Magneto WOW & FLUTTER takes will help set this)
- [ ] **Kick with SPLASH at 0:** should a Kick still give the full crash, or should SPLASH scale the Kick's crash too?
- [ ] **Kick with MIX fully down:** the Kick is part of the reverb, so at MIX 0 (dry only) it's silent. OK?
- [ ] **Bright vs dark material:** the reverb comes back a few dB louder on dark, rumbly material than on bright, hissy material. OK, or should it even out?

## Later (no action now)

### Stereo in: explore down the line
- **Question:** should Resilio's reverb **follow where things are panned** (e.g. a hi-hat panned left splashes mostly in the left spring), or stay a classic dub send where a mono aux goes into the tank and the springs make the stereo? Today it sums the input to mono before the Springs (SPEC §4.3).
- **Options:** a cheap "blend" (each Spring gets mostly L or mostly R, one shared drive stage); full dual input (two drive stages, ~+300–700 CPU cycles/sample); or **stereo-in in the plugin only**, if the Versio is short on CPU.
- **When:** decide after M3 profiling shows the Versio's real CPU headroom. Background: [SPEC §10](../SPEC.md)
- **Optional data to help:** Wellspring takes **A-L / A-R** (each tank alone) in the [recipe](recording-recipe.md)

## What to send Claude

- **Recordings:** just say they're done. Claude commits and analyses them.
- **Checks (M0, M2):** pass/fail per step. For anything odd, a line of description or a short recording in `test_audio/m0/` or `test_audio/m2/`.
- **Listening:** plain-words answers to the questions above.

## Waiting on Claude (no action needed)

- M7 components (running, separate worktree): Splash (Hit, Clatter, Jolt), real Kick, WOBBLE, MIX checks
- M8 prep (running, 4 agents): sweet-spot sweeps (dead zones and cliffs per knob) + wet-level report; reference ingest (one command turns your recordings into aligned A/B pages) + pitch tracker for WOBBLE; chirp direction behind one switch with a tuned highs-later version and a new A/B page; firmware flash trim (profile build at 98%)
- M8 gain staging: loud held chords can push the wet up to the safety limiter (now smooth, but it means the wet runs hot on resonant material; ties to M7 question "bright vs dark material")
- BOING retune: after your chirp-direction decision (task 7)
- M3 profiling: firmware is ready ([firmware/README.md](../firmware/README.md)); flashing waits for your M0 check
- M6 anti-ringing: after M5
- After take A: match DECAY to the Wellspring's T60, build the M1 A/B page
- After M0 passes: M3 profiling on the Versio

## Done

- 28 Sep 2026: held-note ticks fixed: the output limiter was hard-clipping peaks; now a smooth limiter + soft clip, with a regression test

- 28 Sep 2026: M7 built (SPLASH, real Kick, WOBBLE wired in; all 14 test suites pass). Installed in Ableton

- 28 Sep 2026: DRIVE retune ([ADR 0022](adr/0022-drive-retune.md)): DRIVE clearly audible from noon, level constant. Installed in Ableton

- 28 Sep 2026: M6 anti-ringing ([write-up](m6-metric-calibration.md)): a Ringing test that passes real tanks, the Micro-mod floor (inaudible, ≤ 0.11 cents), 0 of 270 grid cells ring

- 28 Sep 2026: plugin in Ableton pinned to the M5 build; dev builds no longer auto-install

- 28 Sep 2026: M5 drive chain + TONE built; IR dispersion study done ([report](ir-dispersion-study.md))

- 28 Sep 2026: M4 stereo fix (mono-safe, wide at every DECAY); M3 firmware variants ready; README added

- 28 Sep 2026: Ableton spring impulse responses found and analysed (IR library, ADR 0021)

- 28 Sep 2026: GitHub push working again (it was a network routing problem on your connection, not GitHub)
