# Owner tasks

Your running to-do list. Claude keeps it current: new tasks are added, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 30 Sep 2026 · **Milestones:** M0 **passed** · M1: A/B vs the Wellspring ready (4b) · M2 needs your Ableton check · M3: run 11 **62 % average, 68 % peak** (target 65 %); run 12 coming (3c) · **Real firmware ready to play (3g)** · M8: SPLASH round 3 voicings to judge (3d) · Knob layout set to your panel (3b)

**Plugin installed in Ableton:** commit `ae844da`, installed 29 Sep 19:13. New: **TENSION turns up = tighter**, **SPLASH is the "heavier clang"** through the springs (no click at SPLASH 0), **TONE's bright side thins the lows**, and the **mid-DECAY resonances** are fixed (2 and 3 Springs are now slightly different from each other, like real springs).
**To load it:** quit and reopen Ableton, rescan plug-ins (hold ⌥ and click Rescan), and replace any Resilio Versio already in your set with a fresh one (a loaded copy keeps running the old build). ⚠ Sets saved with the old BOING knob open with TENSION at noon; BOING automation doesn't carry over.
**Next install would bring:** the earlier first echo (ADR 0029), the jolt fix, and the SPLASH choice once made. Not installed yet: `main` has moved past `ae844da`.
**Next:** SPLASH voicings page (3d), then run 11 on the Versio (3c).

## To do (suggested order)

### 1. Record the Wellspring · **recorded 29 Sep** · unblocks M1
- **Doc:** [recording-recipe.md](recording-recipe.md) · Ableton settings: [ableton-setup.md](ableton-setup.md)
- [x] Take **0** (TRS loopback) and take **A** (clicks) (29 Sep)
- [x] Core: **B, C** (hot INPUT), **D, E, E2** (29 Sep)
- [x] Optional: **F** (knocks), **G** + **G2** (ringing, parallel and ping-pong feedback), **A-L / A-R** (29 Sep)
- [x] Copied and renamed by Claude; analysed: [reference-report.md](reference-report.md) (tail ~5 s, echoes ~83 ms apart, highs 5.7 ms later; G/G2 flagged as Ringing, as intended)
- [x] Notes: take C's INPUT about 1:30–2 o'clock; E2's SPRINGS DRY/WET at noon (29 Sep)
- [x] Recordings stay **out of git** (29 Sep, your call; `.gitignore` + [ADR 0009](adr/0009-wellspring-reference-recordings.md) amended). They live only on your Mac
- Check every take: delay DRY/WET fully **dry**, MAGIC **zero**, SPRINGS fully **wet**

### 2. Record the Magneto · **recorded 29 Sep**
- **Doc:** [recording-recipe-magneto.md](recording-recipe-magneto.md) · why: [ADR 0020](adr/0020-magneto-benchmark.md)
- [x] Rack **off** → rear DIP **S2 = ON** (Dual Split), and back to **OFF** afterwards (29 Sep)
- Analysed: [reference-report.md](reference-report.md). Your ADAT hunch was right: that path is ~2 ms slower than the TRS jacks
- [ ] **Decide:** the Magneto's tape wobble tops out at about **8 cents** on one echo (WOW & FLUTTER 3 o'clock to fully CW; 3.5 cents at noon). Our WOBBLE's first echo swings up to about **36 cents** at max, on purpose ("clearly out of tune", ADR 0008). Keep our wilder top end, or bring it closer to the Magneto? Try WOBBLE on held chords in the plugin
- [x] **First, take 0 through the OPTX2 (ADAT loopback):** patch the OPTX2 output jack you'll use straight into the OPTX2 input jack you'll use, play `01_clicks` and record it as `magneto_0_adat_loopback.wav`. The ADAT path has its own latency and level, so the Wellspring's TRS loopback can't stand in for it. Note the ADAT channels in `NOTES.md` and keep them for every Magneto take
- [x] Spring (Right in/out): **MA, MB, ME**
- [x] Tape (Left in/out): **MW0–MW4** (WOW & FLUTTER × 5, `08_held_tones.wav`), **MD1–MD3** (REC LVL green/amber/red)
- [x] Save as `test_audio/reference/magneto_<take>_<desc>.wav`, with notes in the same `NOTES.md`

### 3. M0 hardware check on the Versio (≈30 min) · unblocks M3
- After M0, the M3 profile build will need one extra check: its serial output was rewritten to save flash, so confirm the `CORNER …` lines are readable in `screen` (details come with the M3 instructions)
- **Doc:** [m0-hardware-check.md](m0-hardware-check.md)
- [x] Flash **`dist/resilio_versio_m0_test.bin`** (29 Sep)
- [x] Session 1, **USB only** (29 Sep): boots, serial works, switches pass (**pointing left = 0**, matches the firmware), button pass (37 of 37). Knobs read 1000 on USB: expected, the knob circuit needs rack power
- [x] Session 2, **rack power** (29 Sep): all knobs fade their LEDs smoothly, LED_3 green with knobs down, CV and gate pass. Passthrough (recordings in `test_audio/m0/`): both channels, In L → both outs, no hum, no audible hiss (you listened). Found: the Versio **flips polarity** and is **1.2 dB louder** than a cable; the release firmware now undoes both

### 3b. Control remap (≈10 min)
- **Drawing:** [panel/versio_panel_current_mapping.svg](panel/versio_panel_current_mapping.svg) shows today's mapping with an id per control (P1–P7, SW1–SW2, BTN, L1–L4, J1–J12)
- [x] The first drawing didn't match your LED test (the Versio's knob wiring isn't in reading order); fixed from your notes (29 Sep)
- [x] Your layout (29 Sep, [ADR 0028](adr/0028-knobs-follow-the-printed-versio-panel.md)): **P1 MIX, P2 DECAY, P3 TONE, P4 SPLASH, P5 TENSION, P6 WOBBLE, P7 DRIVE**. Built into the release firmware; the drawing shows it. You'll feel it the first time you flash the real firmware
- Never connect USB and rack power at the same time

### 3c. M3 CPU test on the Versio (≈10 min) · checks the reverb fits
- [ ] Flash **`dist/resilio_versio_m3_profile.bin`** (NE Firmware Swap → Select Custom File), **USB only**, nothing patched
- [ ] Open the serial stream in Claude's Terminal (`screen /dev/tty.usbmodem* 115200`) and let it run ~2 minutes (24 settings, ~3 s each), then tell Claude. Claude reads the `CORNER …` lines: the number that matters is `max`, target ≤ 65%
- [x] Run 1 (29 Sep): **over budget**, worst case 83 % average with peaks at 100 % (would click). Details: [firmware/README.md](../firmware/README.md) "M3 results"
- [x] Run 2 (29 Sep): faster memory, only ~1 point better. The cost is in the calculation itself
- [x] Run 3 (29 Sep): the three springs are 58 of the 81 points; their "boing" stages were stalling the chip
- [x] Run 4 (29 Sep): the reordering was slower on the chip; reverted
- [x] Run 5 (29 Sep): chip at full speed, caches on; the compiler's choice of multiply-add instruction was stalling every calculation
- [x] Run 6 (29 Sep): worst case 83 → 76 % average, 100 → 93 % peak. Still over the 65 % target; more work on Claude's side, then another run
- [x] Decided (29 Sep): idle springs keep running (seamless SPRINGS switching); the 2 ms audio slice only if still needed
- [x] Run 7 (29 Sep): the peaks come from the springs recalculating their settings; a faster spring loop found (a third cheaper)
- [x] Run 8 (29 Sep): worst case **62.9 % average (under the 65 % target)**, peaks 75 %. The peaks are the springs recalculating all at once when a knob moves or a hit jolts them
- [x] Run 9 (29 Sep): springs take changes in turn (your pick). Peaks 75 → ~67 %; close to the target
- [x] Listened (29 Sep): a bit more undulation on KICKED hits with the stagger. Cause: SPLASH's jolt reached springs B and C a tick or two late. Fixed: the jolt now reaches all springs together; only the heavy recalculation takes turns (now within 112–118 dB of the original sound)
- [ ] **Optional re-listen:** `renders/m3_stagger_abc/` at moderate DRIVE (KICKED hits, DRIVEN hits, KICKED skank). A = before, B = the version you heard, C = the fix. C should sound like A
- [x] **Run 11** (30 Sep): worst case **62 % average, 68 % peak** (target peak 65 %). The steady load is comfortably under; the peaks are occasional moments when a spring recalculates its settings in the same instant as other housekeeping. About 3 points still to find (Claude's side), or accept it (your call, below)

- [ ] **Run 12 (≈10 min, when convenient):** flash **`dist/resilio_versio_m3_profile_run12.bin`** (USB only), stream in the Terminal panel like run 11 (`screen /dev/tty.usbmodem* 115200`), ~2 minutes, then tell Claude. Target: worst-case peak ≤ 65 % (run 11: 68 %). Sound unchanged with knobs still; a TENSION move reaches the springs within ~6 ms (was 1.3 ms). Afterwards reflash the release firmware to keep playing

### 3d. Slapback gap and SPLASH (29 Sep)
- [x] The gap between dry and reverb felt like slapback at low MIX. Decided: **earlier first echo** at every TENSION, same echo spacing ([ADR 0029](adr/0029-earlier-first-echo.md)). Loose tank now 32 ms, like your Wellspring (was 45). Listened: `renders/predelay_ab/`, most noticeable at higher TENSION
- [x] The snare-like layer is SPLASH's noise burst (gone at SPLASH 0). Direction: **derive the splash from the input**, like a real tank driven hard, not an added impulse. Claude builds voicings to A/B
- [ ] **Listen (≈15 min):** open `renders/splash_voicings/index.html` from Finder. Columns = ATTITUDE, rows = hits / skank; the five voicings switch in sync. Mark **My pick** per panel, add notes per column, then **Copy results for Claude** and paste them into the chat. Per ATTITUDE folder, hits and skank: **A** today's noise burst, **B** hits push the transducer (grit), **C** hits feed their own highs into the springs (brighter drip), **D** both by ATTITUDE, **E** DRIVE alone. Level-matched. Which is closest to a real tank being hit hard? Anything to change?
- [ ] **Then decide:** should higher DRIVE make SPLASH bite sooner and harder (realistic, matched to your Wellspring takes B vs C), or keep them independent?
- Afterwards: Claude hands you the real firmware (your knob layout, output fix) to flash and play: **ready now, see 3g**

### 3g. Play the real firmware on the Versio (≈20 min, 30 Sep)
- Flash **`dist/resilio_versio_release_3a790f7.bin`** (NE Firmware Swap → Select Custom File, **USB only**). Then unplug USB and play on **rack power** (never both at once)
- It's the real instrument: your panel's knob layout (ADR 0028), the output polarity/level fix, the new LED meters (ADR 0031). SPLASH is still today's version (round 3 not built yet)
- [ ] **LED order:** patch something into **In L only**. The **leftmost** LED should light (and the right pair shows the output). If a different one lights, tell Claude which (a one-line fix)
- [ ] **Input red:** turn your source up until it starts to distort at the jack: does the input LED go red about there?
- [ ] **Look:** does amber read as amber, and does the dimmest glow show without flicker?
- [x] **Output red:** lights on a KICKED Howl, off in normal playing (30 Sep)
- [ ] **Passthrough vs a cable (once):** MIX fully left, record the output next to the same source through a plain cable. Same level and not flipped? (Claude checks the recording if you save it in `test_audio/m0/`)
- [x] First play (30 Sep): **working!** Drum hits (Squid Salmple + Rample → Worng SoundStage II FX send → Resilio → return). Controls audible as expected. The input LEDs never left green (peaks below about −18 dB of full scale, line level rather than Eurorack level: the SoundStage runs quiet), so SPLASH was barely audible
- [ ] **Try:** SPLASH **near max** (it doubles as sensitivity today) and raise the SoundStage's **main Level** (its FX send follows it) until the loudest hits just touch **amber** on the input LEDs, then play with SPLASH again. Why: today's SPLASH falls off a cliff below about −18 dB of full scale, and DRIVE can't help yet (study in [m8-tuning-backlog.md](m8-tuning-backlog.md) on branch, "Send-level calibration study")
- Round 3 plan (from the study): DRIVE becomes a real INPUT gain (0 → +24 dB), SPLASH listens right after it, so DRIVE up = more splash and quiet sends get their splash back. Your SoundStage II manual recommends exactly this: set the effect 100 % wet and use its input gain
- [ ] Then play it more thoroughly: how does it feel on the real panel?

### 3e. Two prototypes to listen to (≈10 min each, from the overnight agents, 30 Sep)
- [ ] **Hanging note at WOBBLE ~9 o'clock:** open `renders/proto_wobble_hang/index.html` from Finder. **A** today, **B** "Springs drift together at low WOBBLE" (Claude's pick: the hang drops to the WOBBLE-0 level), **C** halfway. Listen for: does the chord now fade evenly? Does B lose any life at 9 o'clock, or sound narrower at noon? Pick, notes, **Copy results for Claude**
- [ ] **Lows ring longer, like your Wellspring:** open `renders/proto_low_tail/index.html`. **A** today, **B** "lows linger" (snare tail 3.5 → 3.8 s, rims unchanged), **C** "lows linger, top fades sooner" (closer to the tank's rims; one ringing check slightly over its limit), **W** your Wellspring (DECAY 2 o'clock rows). Question: more like a real tank, or just muddier? Note: the Wellspring's lows ring long but quietly; ours are louder, so the same stretch risks mud

### 3f. Low end vs real tanks (≈10 min, 30 Sep)
- Your ear was right: next to your Wellspring, the Magneto and 45 Ableton spring IRs, ours has **10–20 dB more below 160 Hz**, and centres near 300 Hz where real tanks centre at 1–2 kHz. Details: [m8-tuning-backlog.md](m8-tuning-backlog.md) "Sonic signature vs real springs"
- [ ] **Listen:** `renders/eq_preview/index.html`. **A** today, **B** a tank-like low cut, **C** low cut + the honky spring midrange, **W** Wellspring, **M** Magneto. Offline EQ only: it previews the balance before Claude changes the reverb. Which is closest to a real tank? Too thin anywhere?
- This probably changes what "lows ring longer" (3e) should be: real tanks' lows ring long but quietly
- [x] Listened (30 Sep): C closer; still less diffuse than the Wellspring, stereo flickers ear to ear, mids drop between skank hits. Measured: all three come from ours being too sparse (separate echoes instead of a wash). **The Wellspring is the reference** (its soft warmth over the Magneto's thinness). Decided: a faint hiss that **follows the tail only** (never at rest)
- [ ] **Listen (ready):** `renders/proto_diffuse_tank/index.html`, step by step, each adding to the last: **A** today, **B** spring EQ, **C** more pickups, **D** smear, **E** both sides, **F** brighter tail + darker attack, **G** tank breath, **W** your Wellspring. Which step is the right place to stop? Things to listen for: does C–E's wash and steady stereo sound closer to the Wellspring? Does the tight tank (TENSION up) lose too much boing from C on? Is G's breath nice or distracting?
  - Cost on the Versio (rough): B, E, F, G almost free; C about +2 points; D about +4 points (the chip is already at its limit, so D would need more trimming or a cheaper version)
  - Only B passes every existing test as is; C–G would need tests re-tuned to the new sound (nothing loosened blindly)

### 3h. Tight-tank ping (≈5 min, optional, 30 Sep)
- The "ringing" our test flagged on a tight, bright KICKED tank is real but **inaudible** (75–85 dB under the tail): the drive inside the tank makes a faint overtone that lines up with one of the tank's own high notes. Fix found, **no CPU cost**: that drive fades out once the tail is too quiet to be bent by it
- [ ] **Listen:** `renders/proto_tight_ringing/index.html`: **A** today, **B** level fade, **C** bend fade (Claude's pick). Only question: do KICKED's loud hits, grit and tail ends sound **unchanged** with C?

### 4. M2 Ableton check (≈15 min)
- **Doc:** [m2-ableton-check.md](m2-ableton-check.md) · MIDI clip: `test_audio/midi/kicks_16ths.mid`
- [ ] **Rescan plug-ins first** and use a fresh instance (see the top of this page). The second knob is now TENSION, not BOING
- [ ] Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz
- A 10th control, **Bypass**, is normal (added by the plugin framework)

### 4b. Listen: Resilio vs the Wellspring (≈15 min) · M1 A/B
- Open from Finder: `renders/references/wellspring/ab/index.html`. The Wellspring's own takes are in its Reference section; ours are rendered at the DECAY that gives the **same tail length (3.5 s, DECAY about 2 o'clock)**, CLEAN and DRIVEN, on the same clicks, hits and skank. Pin a Wellspring take as A (A = clicks, B = hits, E = skank) and ours on the same material as B
- [ ] Same **length** of tail by ear? (The "~5 s" we quoted before was a measuring error; it's 3.5 s)
- [ ] On hits and skank: the Wellspring's **low drums ring longer** (4.3 s) than its higher ones (3.3 s); ours is an even 3.5 s on every hit. Does ours sound too even, or is it fine?
- [ ] Anything else that gives ours away: brightness, the echo spacing, the "boing"?

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
- [x] TONE: dark side good; bright side should thin the lows (29 Sep, your note). Installed
- [ ] TONE now: is full right now thin and splashy enough, too thin, or should the low cut start earlier?
- [ ] Howl: a rideable rough roar that dies away when DECAY comes down? (Since M6 it wanders about ±7 cents like a siren, so it's never a steady tone; that's in the plugin)
- [ ] **Decide:** if you flip ATTITUDE away from KICKED *while Howling at max DECAY*, should it calm into the normal long (~9 s) tail, or fade within 1–2 s (as ADR 0018 says)? Both can't hold at max DECAY

### 8b. SPLASH: in CLEAN, and "heavier clang"
- [x] SPLASH in CLEAN: **a real, gentler splash** (29 Sep, [ADR 0025](adr/0025-clean-gentle-splash.md)). Built
- [x] SPLASH sounded like an open hi-hat on top: from `renders/splash_ab/` you picked **C, "heavier clang"** (29 Sep), a lower, metallic crash that goes through the springs. Installed
- [x] The click at SPLASH 0: DRIVEN/KICKED's built-in faint splash. Removed, installed
- [ ] **Listen (≈10 min):** open `renders/splash_ab/C/hits/index.html` and `renders/splash_ab/C/rim/index.html` (re-rendered from the finished code), or play the plugin. Does SPLASH feel part of the reverb now? Is CLEAN gentle, DRIVEN clear and KICKED unmistakable? Anything metallic that bothers you?

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
- [ ] **Kick with SPLASH at 0:** should a Kick still give the full crash, or should SPLASH scale the Kick's crash too? (The Kick now uses the new "clang" crash)
- [ ] **Kick with MIX fully down:** the Kick is part of the reverb, so at MIX 0 (dry only) it's silent. OK?
- [ ] **Bright vs dark material:** the reverb comes back a few dB louder on dark, rumbly material than on bright, hissy material. OK, or should it even out?

### 10. Listen: TENSION + DECAY (≈20 min)
- Open from Finder: `renders/tension/tension_clean/index.html` (and `tension_driven`, `tension_kicked`): TENSION 0 → 1 on hits, DECAY noon. *(These pages were made before the flip: on them TENSION 0 is tight. In the plugin now, turning TENSION **up** is tighter.)* Then `renders/tension/decay_clean/index.html` (and `_driven`, `_kicked`): DECAY 0 → 1 at TENSION noon. All at MIX 0.5, SPRINGS 2. Or just play the plugin
- [ ] Does each TENSION position sound like **one real tank**, a proper dub tank (not cartoonish) fully down and tight and pingy fully up?
- [ ] DECAY: does **only the length** change, with the echoes staying put?
- [x] Direction: **turning up = tighter** (29 Sep, your note; flipped)
- [ ] Turn TENSION on a ringing tail: turning up raises the pitch like tightening a string. Nice, or too much?
- [x] Resonances creeping in at mid DECAY (29 Sep, your note). Fixed and installed ([ADR 0027](adr/0027-springs-differ-in-damping-and-decay.md))
- [ ] Any note still **ringing** out of the tail? Especially: a tight tank (TENSION fully up) at DECAY 9–12 o'clock; **KICKED, 1 Spring, TENSION fully up, TONE fully right, DECAY ~3 o'clock** (the one corner the meters put near the line); and chords with WOBBLE around 9 o'clock on a tight tank
- [ ] **KICKED Howl on a tight tank** (TENSION fully up, DECAY max): it leans more toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] Anything in the old BOING range you miss (e.g. a short tank with a huge chirp)?

## Later (no action now)

### Stereo in: explore down the line
- **Question:** should Resilio's reverb **follow where things are panned** (e.g. a hi-hat panned left splashes mostly in the left spring), or stay a classic dub send where a mono aux goes into the tank and the springs make the stereo? Today it sums the input to mono before the Springs (SPEC §4.3).
- **Options:** a cheap "blend" (each Spring gets mostly L or mostly R, one shared drive stage); full dual input (two drive stages, ~+300–700 CPU cycles/sample); or **stereo-in in the plugin only**, if the Versio is short on CPU.
- **When:** decide after M3 profiling shows the Versio's real CPU headroom. Background: [SPEC §10](../SPEC.md)
- **Optional data to help:** Wellspring takes **A-L / A-R** (each tank alone) in the [recipe](recording-recipe.md)

## What to send Claude

- **Recordings:** just say they're done (and where the Ableton project is). Claude copies, renames and analyses them. The WAVs stay on your Mac, never in git
- **Checks (M0, M2):** pass/fail per step. For anything odd, a line of description or a short recording in `test_audio/m0/` or `test_audio/m2/`.
- **Listening:** plain-words answers to the questions above.

## Waiting on Claude (no action needed)

- **M3 run 12**: built and tested (all 16 suites), waiting for your run (3c). Not merged until the chip confirms it
- **Overnight 29–30 Sep (agents, nothing merged until Claude reviews):** tail-length measurement fix; M9 LEDs as level meters (your call: left pair = In L/R, right pair = Out L/R, green → amber → red, input red = near clipping, output red = limiter working, no mode colours, boot pattern kept); M9 one-page manual + dub preset notes (drafts); review pages get the SPLASH-page layout; two prototypes to listen to: the hanging partial at WOBBLE ~9 o'clock (`renders/proto_wobble_hang/`) and lows ringing longer like your Wellspring (`renders/proto_low_tail/`)

- **M3:** read run 11. If the peak is still over 65 %: trim more (next candidates in [firmware/README.md](../firmware/README.md) "M3 results"), block 96 only if you agree. Then the real firmware for you to flash and play
- **SPLASH round 3:** build your pick from `renders/splash_voicings/` into the Core, with an ADR, tests and the Kick keeping its crash
- **Metrics.cpp T60:** reads long on repeated stimuli (tuning backlog, M1 section); fix and re-check the gates that pin a T60
- **Possible follow-up:** on the skank with WOBBLE around 9 o'clock, one partial on a tight tank can still hang on after the chord (ADR 0027 "Open")
- M8 gain staging: loud held chords can push the wet up to the safety limiter (ties to the M7 question "bright vs dark material")

## Done

- 30 Sep 2026: **merged** the overnight work: tail-length measurement fix, LED meters (ADR 0031, SPEC v1.0.18), manual + dub preset drafts (`docs/manual.md`, `docs/presets.md`), new listening pages. All 16 test suites pass. **Release firmware built** (`dist/resilio_versio_release_3a790f7.bin`, 84 % of flash)
- 30 Sep 2026: **M3 run 11**: 62 % average, 68 % peak (target peak 65 %)

- 29 Sep 2026: **Earlier first echo** (ADR 0029): loose tank's gap 45 → 32 ms, like your Wellspring; echo spacing unchanged
- 29 Sep 2026: **M3 CPU**: worst case 83 % → 63 % average, 100 % → ~67 % peak, sound unchanged (faster memory for the delay lines, no fused multiply-add, pipelined spring stages, cached design maths, springs recalculating in turn with the jolt kept together, fewer divisions)
- 29 Sep 2026: **Knob layout** set to your printed Versio panel (ADR 0028)
- 29 Sep 2026: **M1 A/B** against the Wellspring built (tail 3.5 s; the old "5 s" was a measuring error)

- 29 Sep 2026: **M0 passed** on the Versio: controls, LEDs, CV, gate and audio all work. The Versio's own circuit flips the signal and adds 1.2 dB; the release firmware now undoes both, so MIX 0 matches a cable

- 29 Sep 2026: **M0 Session 1 passed** (USB): switches (left = 0) and button work; knobs move to Session 2 (they need rack power)

- 29 Sep 2026: **M1 A/B page built** against the Wellspring: its tail is 3.5 s (not ~5 s; the old number was a measuring error), matched by DECAY about 2 o'clock (task 4b)

- 29 Sep 2026: **installed `ae844da`**: TENSION up = tighter, SPLASH "heavier clang", TONE low cut, mid-DECAY resonances fixed
- 29 Sep 2026: **Wellspring and Magneto recorded** and analysed (recordings stay on your Mac)

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
