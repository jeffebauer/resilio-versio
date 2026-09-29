# Owner tasks

Your running to-do list. Claude keeps it current: new tasks are added, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 29 Sep 2026 (session 2, before your studio session) · **Milestones:** M0 needs your hardware check · M1 needs a Wellspring take + your listen · M2 needs your Ableton check · M3 blocked on M0 · M4–M7 built · M8: TENSION, CLEAN splash and TONE low cut built; SPLASH "heavier clang" and the mid-DECAY resonance fix being finished, then one install

**Plugin installed in Ableton:** TENSION + CLEAN's gentle splash (commit `df8e5a3`), installed 29 Sep 11:49. In this build TENSION still runs the old way (up = looser) and SPLASH is the old "hi-hat on top" crash.
**Next install (Claude installs it when it's finished, today):** TENSION flipped (up = tighter), TONE's bright side thins the lows, SPLASH "heavier clang" (through the springs, no click at SPLASH 0), and the mid-DECAY resonance fix. The top of this page will say when it's in. **Then:** quit and reopen Ableton, rescan plug-ins (hold ⌥ and click Rescan), and replace any Resilio Versio already in your set with a fresh one (a loaded copy keeps running the old build). ⚠ Sets saved with the old BOING knob open with TENSION at noon; BOING automation doesn't carry over.
**Studio order:** the recordings and hardware (tasks 1–3) need no new build. Leave the listening (tasks 5–10) until the next install is in, since several questions depend on it.

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
- After M0, the M3 profile build will need one extra check: its serial output was rewritten to save flash, so confirm the `CORNER …` lines are readable in `screen` (details come with the M3 instructions)
- **Doc:** [m0-hardware-check.md](m0-hardware-check.md)
- [ ] Flash **`dist/resilio_versio_m0_test.bin`** (NE Firmware Swap → Select Custom File). Not `firmware/build/…`, which changes with every build
- [ ] Session 1, **USB only**: boot pattern, serial values for knobs, switches and button. Note which switch direction reads 0
- [ ] Session 2, **rack power only**: LEDs, CV at 0 V/5 V (LED_3 green), gate, passthrough vs cable
- Never connect USB and rack power at the same time

### 4. M2 Ableton check (≈15 min)
- **Doc:** [m2-ableton-check.md](m2-ableton-check.md) · MIDI clip: `test_audio/midi/kicks_16ths.mid`
- [ ] **Rescan plug-ins first** and use a fresh instance (see the top of this page). The second knob is now TENSION, not BOING
- [ ] Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz
- A 10th control, **Bypass**, is normal (added by the plugin framework)

### 5. Listen: M1 renders (≈15 min)
- Open from Finder: `renders/m1_click_grid/index.html`, `renders/m1_hits_grid/index.html`. These are from the very first build (BOING era), so judge the spring character, not the details
- [ ] Does it sound like a **spring**, even thin?
- [ ] Do the audio players play? (Claude couldn't test playback)
- [ ] **New:** the click-grid page now has 45 real spring tanks from Ableton in its Reference section ([ADR 0021](adr/0021-ableton-ir-library.md)). Pin one as A and a render as B. Does ours sit among real tanks, or sound clearly different? Worth trying: *Space Echo Spring*, *Classic Amp Spring*, *BRX100 Gentle*, *Short Spring*
- Ignore the stereo flags on the click grid: that 1-Spring mono dip was fixed in M4

### 6. Listen: M4 renders (≈15 min)
- Open from Finder: `renders/m4_springs_hits/index.html`, `renders/m4_springs_skank/index.html`
- [ ] 1 → 2 → 3 Springs: clearly **sparse/drippy → classic → dense/lush**, at similar loudness?
- [ ] Wide on headphones, and still full when summed to mono?
- The stereo fix has landed and the pages are re-rendered: 0 flags

### 8. Listen: M5 drive + TONE (≈15 min)
- Open from Finder: `renders/m5_attitude_drive/index.html` (ATTITUDE × DRIVE on hits) and `renders/m5_howl/m5_kicked_howl_decay1_drive0.8_pull6s.wav` (KICKED Howl, DECAY pulled back at 6 s). These are from the M5 build; DRIVE has been retuned since, so the plugin is the better judge for DRIVE. For TONE, use the new before/after: `renders/tone_ab/before/index.html` and `renders/tone_ab/after/index.html` (hits at TONE noon, 3 o'clock, full right)
- [ ] CLEAN / DRIVEN / KICKED: clearly **hi-fi / warm tape dub / trashed**?
- [x] **DRIVE at noon:** too subtle, even at max. Decided and built: obvious from noon, cranked tape/tank at max, level constant ([ADR 0022](adr/0022-drive-retune.md))
- [x] TONE: dark side good; bright side should thin the lows (29 Sep, your note). Built, in the next install
- [ ] TONE after the next install: is full right now thin and splashy enough, too thin, or should the low cut start earlier?
- [ ] Howl: a rideable rough roar that dies away when DECAY comes down? (Since M6 it wanders about ±7 cents like a siren, so it's never a steady tone; that's in the plugin)
- [ ] **Decide:** if you flip ATTITUDE away from KICKED *while Howling at max DECAY*, should it calm into the normal long (~9 s) tail, or fade within 1–2 s (as ADR 0018 says)? Both can't hold at max DECAY

### 8b. SPLASH: in CLEAN, and "heavier clang"
- [x] SPLASH in CLEAN: **a real, gentler splash** (29 Sep, [ADR 0025](adr/0025-clean-gentle-splash.md)). Built
- [x] SPLASH sounded like an open hi-hat on top: from `renders/splash_ab/` you picked **C, "heavier clang"** (29 Sep), a lower, metallic crash that goes through the springs. Being finished, in the next install
- [x] The click at SPLASH 0: DRIVEN/KICKED's built-in faint splash. Removed, in the next install
- [ ] **After the next install (≈10 min):** open `renders/splash_ab/C/hits/index.html` and `renders/splash_ab/C/rim/index.html` (re-rendered from the finished code), or play the plugin. Does SPLASH feel part of the reverb now? Is CLEAN gentle, DRIVEN clear and KICKED unmistakable? Anything metallic that bothers you?

### 8c. Listen: M8 tuning round 1 (≈20 min)
- Before/after WAV pairs (open in Ableton or Finder):
  - `renders/m8_round1/space_motion/springs/`: hits and held chord at SPRINGS 1/2/3. **No flam in 3?** **2 still as spacious** (its width now comes from diffusion, not timing)? **1 wider but not phasey?**
  - `renders/m8_round1/space_motion/wobble/`: WOBBLE 0/0.5/0.75/1 on hits and held tones. **Do snare echoes waver at 0.75+?** **Is WOBBLE 1 too much** on held notes (~45–65 cents)?
  - `renders/m8_round1/dynamics_colour/before|after/`: rimshot and hits at −9 and −4 dBFS, DRIVE 0/0.5/1 per ATTITUDE. **KICKED DRIVE 1 gritting the tail?** **CLEAN DRIVE a gentle tint?** (Skip the SPLASH files: SPLASH has been redone, see 8b)
- [ ] Held chords: notes can now lean slightly left or right (side effect of the new width). OK or distracting?
- [ ] With highs-later: does **max DRIVE still grit the top of the tail**? Held notes may also vary a little more in level from note to note
- [ ] Or just play the plugin (use a Utility before it to reach about −4 dBFS peaks, see [ableton-setup.md](ableton-setup.md))

### 9. Listen + M7 questions (≈20 min)
- Open from Finder: `renders/m7_splash/index.html` (ATTITUDE × SPLASH on hits), `renders/m7_wobble/index.html` (WOBBLE on held tones), and the Kick files in `renders/m7_kick/` (each ATTITUDE: singles, a pair, a 12-per-second train). These are from the M7 build, so for SPLASH and WOBBLE the plugin (after the next install) is the better judge. Background: [m7-integration.md](m7-integration.md)
- [ ] KICKED crash big enough? DRIVEN moderate? Kick a tight thud + crash, clean on the fast train?
- [ ] WOBBLE 0.75 already clearly out of tune?
- [x] **SPLASH at 0 in DRIVEN:** the faint natural splash came across as a click. Removed (29 Sep)
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on the left spring and the other way on the right, so hard hits briefly spread in stereo. Keep it, or lurch together?
- [ ] **WOBBLE at max:** about 50–55 cents of wobble in the tail (clearly seasick). Right ceiling, more, or less? (The Magneto WOW & FLUTTER takes will help set this)
- [ ] **Kick with SPLASH at 0:** should a Kick still give the full crash, or should SPLASH scale the Kick's crash too? (Answer after the next install: the Kick uses the new "clang" crash)
- [ ] **Kick with MIX fully down:** the Kick is part of the reverb, so at MIX 0 (dry only) it's silent. OK?
- [ ] **Bright vs dark material:** the reverb comes back a few dB louder on dark, rumbly material than on bright, hissy material. OK, or should it even out?

### 10. Listen: TENSION + DECAY (≈20 min) · after the next install
- Open from Finder: `renders/tension/tension_clean/index.html` (and `tension_driven`, `tension_kicked`): TENSION 0 → 1 on hits, DECAY noon. *(These pages were made before the flip: on them TENSION 0 is tight. In the plugin now, turning TENSION **up** is tighter.)* Then `renders/tension/decay_clean/index.html` (and `_driven`, `_kicked`): DECAY 0 → 1 at TENSION noon. All at MIX 0.5, SPRINGS 2. Or just play the plugin
- [ ] Does each TENSION position sound like **one real tank**, a proper dub tank (not cartoonish) fully down and tight and pingy fully up?
- [ ] DECAY: does **only the length** change, with the echoes staying put?
- [x] Direction: **turning up = tighter** (29 Sep, your note; flipped)
- [ ] Turn TENSION on a ringing tail: turning up raises the pitch like tightening a string. Nice, or too much?
- [x] Resonances creeping in at mid DECAY (29 Sep, your note). Being fixed, in the next install
- [ ] After the next install: any note still **ringing** out of the tail, especially on a tight tank (TENSION fully up) at DECAY 9–12 o'clock?
- [ ] **KICKED Howl on a tight tank** (TENSION fully up, DECAY max): it leans more toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] Anything in the old BOING range you miss (e.g. a short tank with a huge chirp)?

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

- **Next install, today:** finish SPLASH "heavier clang" (C) and the mid-DECAY resonance fix, merge them with the TENSION flip and TONE low cut, re-run every test and the ringing checks, install, and update the top of this page
- Round 2 after your round-1 listen (8c) and the TENSION listen. Known: worst-case CPU is estimated at ~65–66% vs a 65% target (TENSION adds ~2%); confirm on hardware in M3 before trimming
- M8 gain staging: loud held chords can push the wet up to the safety limiter (smooth, but the wet runs hot on resonant material; ties to M7 question "bright vs dark material")
- M3 profiling: firmware is ready ([firmware/README.md](../firmware/README.md)); flashing waits for your M0 check
- After take A: match DECAY to the Wellspring's T60, build the M1 A/B page

## Done

- 29 Sep 2026: **TONE's bright side thins the lows**: a low cut sweeps up the right half, ~105 Hz at 3 o'clock to 300 Hz fully right; noon and the dark side unchanged (your note). Next install
- 29 Sep 2026: **TENSION direction flipped**: turning up = tighter (your note, [ADR 0026](adr/0026-tension-replaces-boing.md) amendment). Next install
- 29 Sep 2026: SPLASH direction chosen: **"heavier clang"** (C); SPLASH-0 click traced and removed. Next install
- 29 Sep 2026: renders cleaned up (31 → 2.8 GB): decided A/B pages and old machine-check renders deleted

- 29 Sep 2026: **TENSION replaces BOING** and DECAY is tail length only ([ADR 0026](adr/0026-tension-replaces-boing.md)); installed with CLEAN's splash (`df8e5a3`). Along the way: loose tank + short tail stays full in mono, cleaner max DRIVE in DRIVEN, no dead patch at the bottom of CLEAN's DRIVE, a tighter Kick thud at long DECAY, held chords in tune at WOBBLE noon, no Ringing on tight tanks at max DECAY

- 29 Sep 2026: CLEAN's gentle splash built ([ADR 0025](adr/0025-clean-gentle-splash.md)): SPLASH now works in CLEAN (no dead zone), smaller than DRIVEN's at every hit level; ghost notes still ignored

- 29 Sep 2026: highs-later chirp switched on and installed (`8f0a09c`); round 1 re-checked in the new direction

- 29 Sep 2026: TENSION adopted to replace BOING ([ADR 0026](adr/0026-tension-replaces-boing.md))
- 29 Sep 2026: session wrap/start skills added (`/resilio-wrap`, `/resilio-start`), plus a project `CLAUDE.md`

- 29 Sep 2026: SPLASH in CLEAN decided: real, gentler splash ([ADR 0025](adr/0025-clean-gentle-splash.md))

- 29 Sep 2026: chirp direction decided: highs later ([ADR 0024](adr/0024-chirp-highs-later.md))

- 28 Sep 2026: M8 tuning round 1 merged and installed (flam gone, 1 Spring wider, early WOBBLE, audible SPLASH, DRIVE without dead zones, wet level spread 5.7–7.8 → 2.5–4.4 dB)

- 28 Sep 2026: firmware flash trimmed (release 75%, profile 83%, was 98.5%)

- 28 Sep 2026: held-note ticks fixed: the output limiter was hard-clipping peaks; now a smooth limiter + soft clip, with a regression test

- 28 Sep 2026: M7 built (SPLASH, real Kick, WOBBLE wired in; all 14 test suites pass). Installed in Ableton

- 28 Sep 2026: DRIVE retune ([ADR 0022](adr/0022-drive-retune.md)): DRIVE clearly audible from noon, level constant. Installed in Ableton

- 28 Sep 2026: M6 anti-ringing ([write-up](m6-metric-calibration.md)): a Ringing test that passes real tanks, the Micro-mod floor (inaudible, ≤ 0.11 cents), 0 of 270 grid cells ring

- 28 Sep 2026: plugin in Ableton pinned to the M5 build; dev builds no longer auto-install

- 28 Sep 2026: M5 drive chain + TONE built; IR dispersion study done ([report](ir-dispersion-study.md))

- 28 Sep 2026: M4 stereo fix (mono-safe, wide at every DECAY); M3 firmware variants ready; README added

- 28 Sep 2026: Ableton spring impulse responses found and analysed (IR library, ADR 0021)

- 28 Sep 2026: GitHub push working again (it was a network routing problem on your connection, not GitHub)
