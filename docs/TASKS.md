# Owner tasks

Your running to-do list. Claude keeps it current: new tasks are added, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 29 Sep 2026 (session 2: CLEAN splash + TENSION built and installed) · **Milestones:** M0 needs your hardware check · M1 needs a Wellspring take + your listen · M2 needs your Ableton check · M3 blocked on M0 · M4–M7 built; listening tasks 5–9 ready · M8: highs-later chirp, CLEAN splash and TENSION in; your listen (task 10) feeds round 2

**Plugin installed in Ableton:** CLEAN's gentle splash + **TENSION** (commit `df8e5a3`), installed 29 Sep 11:49. **Rescan plug-ins** (hold ⌥ and click Rescan). New since the last build: the second knob is now **TENSION** (which tank: tight/pingy ↔ loose/boingy), **DECAY only sets the tail length** (no pitch bend), and SPLASH now works in CLEAN. ⚠ Saved Live sets that used the old BOING knob will open with TENSION at its default (noon), and any BOING automation won't carry over. Listening pages: `renders/tension/` and `renders/clean_splash/`.

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
- **Why:** the IR study ([ir-dispersion-study.md](ir-dispersion-study.md)) found that **every real tank** sends its **highs later** than its lows. Ours does the opposite (lows later) because the spec said "highs before lows", which was written from memory, not measurement. It's your call by ear.
- **Now properly tuned both ways** (one switch in the code). *(These A/B pages were deleted on 29 Sep after the decision, to free disk space.)* Open from Finder:
  - `renders/chirp_ab2/lows_later/index.html` (today's sound)
  - `renders/chirp_ab2/highs_later/index.html` (like the real tanks)
  - Each page has BOING 0 → 1 on a click and on hits, with the other direction and six real tanks as references (*SNRA500 Plucky*, *Amp Spring Bright/Dull/High*, *Classic Amp Spring*, *Space Echo Spring*).
- **Listen for:** highs-later makes each echo sweep **up**, with the sizzle arriving last, instead of today's falling "peeew". Lows stay tight and punchy, and repeats are a little quicker. Compare BOING 0–0.5 with the *Amp Spring* tanks and BOING 1 with *SNRA500*. Highs-later plays about 0.6 dB quieter; tick "Level-match".
- **Worth knowing:** highs-later keeps the **tail length the same at every BOING** (your "BOING shortens decay" issue goes away); today's direction loses up to 12%.
- [x] Which sounds more like a spring to you? **Highs later** (29 Sep, [ADR 0024](adr/0024-chirp-highs-later.md)). Being switched on and re-tuned now; the next plugin build will have it
- Related: the **TENSION** idea in "Later"; the chosen direction would carry into it.

### 8. Listen: M5 drive + TONE (≈15 min)
- Open from Finder: `renders/m5_attitude_drive/index.html` (ATTITUDE × DRIVE on hits), `renders/m5_tone/index.html` (TONE sweep), and `renders/m5_howl/m5_kicked_howl_decay1_drive0.8_pull6s.wav` (KICKED Howl, DECAY pulled back at 6 s)
- [ ] CLEAN / DRIVEN / KICKED: clearly **hi-fi / warm tape dub / trashed**?
- [x] **DRIVE at noon:** too subtle, even at max. Decided: obvious from noon, cranked tape/tank at max, level constant ([ADR 0022](adr/0022-drive-retune.md)). Retune queued after M6
- [ ] TONE: full left warm and dark with the boing still there? Full right splashy but not harsh?
- [ ] Howl: a rideable rough roar that dies away when DECAY comes down? (M6 makes it wander more, about ±7 cents like a siren, so it's never a steady tone. You'll hear that in the next plugin build)
- [ ] **Decide:** if you flip ATTITUDE away from KICKED *while Howling at max DECAY*, should it calm into the normal long (~9 s) tail, or fade within 1–2 s (as ADR 0018 says)? Both can't hold at max DECAY

### 8b. Decide: SPLASH in CLEAN
- The sweeps show SPLASH does **nothing** in CLEAN (by the original spec: "mild HF emphasis only"). It's the only knob with a fully dead range.
- [x] Decided 29 Sep: **a real, gentler splash**: light clatter + a tiny pitch lurch; CLEAN gentle / DRIVEN clear / KICKED unmistakable ([ADR 0025](adr/0025-clean-gentle-splash.md)). **Built 29 Sep** (not in Ableton yet; it goes in together with TENSION)
- [ ] **Listen (≈10 min):** open `renders/clean_splash/index.html` from Finder. Rows are ATTITUDE (CLEAN, DRIVEN, KICKED), columns SPLASH 0 / 0.5 / 1, on the snare and rim hits. CLEAN at SPLASH 0 is exactly what CLEAN used to be at any SPLASH. **Is CLEAN at SPLASH 1 a gentle, bright shimmer on the hard hits, clearly smaller than DRIVEN's crash?** Is the tiny pitch lurch noticeable, and is that OK in CLEAN?

### 8c. Listen: M8 tuning round 1 (≈20 min) · the build is installed
- Before/after WAV pairs (open in Ableton or Finder):
  - `renders/m8_round1/space_motion/springs/`: hits and held chord at SPRINGS 1/2/3. **No flam in 3?** **2 still as spacious** (its width now comes from diffusion, not timing)? **1 wider but not phasey?**
  - `renders/m8_round1/space_motion/wobble/`: WOBBLE 0/0.5/0.75/1 on hits and held tones. **Do snare echoes waver at 0.75+?** **Is WOBBLE 1 too much** on held notes (~45–65 cents)?
  - `renders/m8_round1/dynamics_colour/before|after/`: rimshot and hits at −9 and −4 dBFS, SPLASH 0/0.5/1 and DRIVE 0/0.5/1 per ATTITUDE. **A bright crash on top at SPLASH 1, ghost notes quiet?** **KICKED DRIVE 1 gritting the tail?** **CLEAN DRIVE a gentle tint?**
- [ ] Held chords: notes can now lean slightly left or right (side effect of the new width). OK or distracting?
- [ ] With highs-later: does **max DRIVE still grit the top of the tail**? Held notes may also vary a little more in level from note to note
- [ ] Or just play the plugin (use a Utility before it to reach about −4 dBFS peaks, see [ableton-setup.md](ableton-setup.md))

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

### 10. Listen: TENSION + DECAY (≈20 min) · the build is installed
- Open from Finder: `renders/tension/tension_clean/index.html` (and `tension_driven`, `tension_kicked`): TENSION 0 → 1 on hits, DECAY noon. *(These pages were made before the flip: on them TENSION 0 is tight. In the plugin now, turning TENSION **up** is tighter.)* Then `renders/tension/decay_clean/index.html` (and `_driven`, `_kicked`): DECAY 0 → 1 at TENSION noon. All at MIX 0.5, SPRINGS 2. Or just play the plugin
- [ ] Does each TENSION position sound like **one real tank**, a proper dub tank (not cartoonish) fully down and tight and pingy fully up?
- [ ] DECAY: does **only the length** change, with the echoes staying put?
- [x] Direction: **turning up = tighter** (29 Sep, your note; flipped)
- [ ] Turn TENSION on a ringing tail: turning up raises the pitch like tightening a string. Nice, or too much?
- [ ] **Tight tank, short-to-medium tail** (TENSION fully up, DECAY 9–12 o'clock): does one note **ping** out of the end of the tail? The meters put it right at the edge of "Ringing"; your ears decide whether it needs fixing
- [ ] **KICKED Howl on a tight tank** (TENSION fully up, DECAY max): it leans more toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] Anything in the old BOING range you miss (e.g. a short tank with a huge chirp)?

## Later (no action now)

### Stereo in: explore down the line
- **Question:** should Resilio's reverb **follow where things are panned** (e.g. a hi-hat panned left splashes mostly in the left spring), or stay a classic dub send where a mono aux goes into the tank and the springs make the stereo? Today it sums the input to mono before the Springs (SPEC §4.3).
- **Options:** a cheap "blend" (each Spring gets mostly L or mostly R, one shared drive stage); full dual input (two drive stages, ~+300–700 CPU cycles/sample); or **stereo-in in the plugin only**, if the Versio is short on CPU.
- **When:** decide after M3 profiling shows the Versio's real CPU headroom. Background: [SPEC §10](../SPEC.md)
- **Optional data to help:** Wellspring takes **A-L / A-R** (each tank alone) in the [recipe](recording-recipe.md)

### TENSION instead of BOING (adopted and built, see task 10)
- **Idea (yours, 28 Sep):** real tanks have no BOING and usually no decay knob; their character comes from which tank is fitted. Replace BOING with **TENSION** (which tank: echo spacing + chirp + brightness together, tight/pingy ↔ loose/boingy) and make **DECAY feedback only** (how long it rings, no pitch bend).
- **Trade-offs:** clearer knob jobs and closer to real springs (it also absorbs "BOING shortens decay"); less direct control of the cartoon boing; supersedes ADRs 0006/0007/0012 with a new one. Middle option: rename and widen BOING into TENSION, keep DECAY's size link.
- **Prototype ready to hear (29 Sep; pages deleted after the decision, the built version is in task 10):** open `renders/tension_proto/index.html` from Finder. Guide: `renders/tension_proto/GUIDE.md`. The code is on branch `proto/tension`, not merged
  - [ ] Does each TENSION position sound like **one real tank**? Is the tight end still clearly a spring, and the loose end a proper dub tank rather than cartoonish?
  - [ ] DECAY page: does **only the length** change?
  - [ ] Turning page: TENSION bends pitch, DECAY doesn't. Do you miss DECAY's bend?
  - [x] Overall: **adopt TENSION** (29 Sep, [ADR 0026](adr/0026-tension-replaces-boing.md)). Staged after the highs-later switch-on and CLEAN's splash

## What to send Claude

- **Recordings:** just say they're done. Claude commits and analyses them.
- **Checks (M0, M2):** pass/fail per step. For anything odd, a line of description or a short recording in `test_audio/m0/` or `test_audio/m2/`.
- **Listening:** plain-words answers to the questions above.

## Waiting on Claude (no action needed)

- Round 2 after your round-1 listen (8c) and the TENSION listen. Known: worst-case CPU is estimated at ~65–66% vs a 65% target (TENSION adds ~2%); confirm on hardware in M3 before trimming
- M8 gain staging: loud held chords can push the wet up to the safety limiter (smooth, but the wet runs hot on resonant material; ties to M7 question "bright vs dark material")
- M3 profiling: firmware is ready ([firmware/README.md](../firmware/README.md)); flashing waits for your M0 check
- After take A: match DECAY to the Wellspring's T60, build the M1 A/B page

## Done

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
