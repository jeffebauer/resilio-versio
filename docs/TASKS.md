# Owner tasks

Your running to-do list. Claude keeps it current: open items at the top in the suggested order, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 3 Oct 2026, evening (Wellspring F merged with your picks: low end D, SPRINGS 3 wire gauges; one plugin in Ableton again; CPU run 15 and the new release ready for tomorrow)

| Milestone | State |
|---|---|
| M0 hardware · M3 CPU | M0 passed. Run 13 (SPLASH/DRIVE build): 63 % average, 66 % peak, target **70 %**. **Run 15 ready** (§0): measures everything since run 13 at once (run 14 skipped) |
| Real firmware on the Versio | **New release ready:** `dist/resilio_versio_release_340b542.bin` (the Wellspring F sound with your picks, SPRINGS 3 wire gauges, Big Knob TONE, SPLASH C; 96 % of flash). Run 15 first (§0), then flash this and the click check (§1) |
| M8 sound | **In progress:** SPLASH/DRIVE, bipolar WOBBLE, sustain trim, Big Knob TONE, SPLASH C, and **Wellspring F** (tank fitted to your Wellspring, gentler low cut D, SPRINGS 3 wire gauges) merged (1–3 Oct). Open: what 3 Springs should be (design questions), Wellspring round 5 (your notes) |
| M2 Ableton check | Ready: the plugin is installed |
| M9 polish | LED meters done (module and plugin); panel interface in the plugin; manual and starting points need a refresh for the Wellspring F sound |

**Plugin in Ableton:** `340b542`, version **1.3.29** (installed 3 Oct, evening): **Wellspring F with your picks** (low end D, SPRINGS 3 wire gauges), plus Big Knob TONE, SPLASH C and everything before; AU validated. "Resilio Versio F" removed. Your Whalesong set's devices now play this. **Rescan needed:** open Ableton, hold ⌥ and click Rescan.

**Share package:** [GitHub Release v2026.10.01-1d18fce](https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.01-1d18fce) (private, like the repo): universal plugin, Versio firmware, read-me. **For A/B:** [candidate F, v2026.10.02-cef6a77](https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.02-cef6a77-candidate-F) (pre-release, plugin only): installs as **"Resilio Versio F"** next to the 1 Oct "Resilio Versio"; it's F before your two picks. Claude makes the next one with `tools/make_release.sh --publish` (or `--candidate <ref> <label>`).

## Now (suggested order)

### 0. CPU run 15 on the module (≈10 min, USB only) · ready for tomorrow
- [ ] **Rack power unplugged**, Versio on USB. Flash `dist/resilio_versio_m3_profile_run15.bin` with NE Firmware Swap → Select Custom File. It ignores the knobs and makes its own test signal
- [ ] In Terminal: `screen /dev/tty.usbmodem* 115200`, let it run until the corner lines have gone round twice (a few minutes), then select all, copy and paste the output to Claude (quit screen: Ctrl-A, K, Y)
- Why: everything since run 13 is unmeasured on the chip (bipolar WOBBLE, sustain trim, Big Knob, SPLASH C, SPRINGS 3 wire gauges, the Wellspring F tank, and the flash savings), and knob moves are the CPU peaks: the lead for the red input LEDs. Also the first hardware run of the engine built at power-up
- Then flash the new release (§1)

### 1. Flash the Versio and play (≈2 min + play)
- [ ] Flash `dist/resilio_versio_release_340b542.bin` (after run 15) and do the **click check**: 3 Springs, KICKED, DRIVE and DECAY up, move knobs fast and hit KICK. Any click or dropout → tell Claude (the CPU target is 70 %; this is the safety net with every release)
- [ ] Play your low-mid pad again at the old settings (CLEAN, DECAY noon, 2–3 Springs, TENSION past 3 o'clock): the output LEDs should mostly stay out of red, and any brief red should sound clean, not driven
- [ ] Play it more thoroughly on the real panel: how does it feel? Anything surprising compared with the plugin?
- [ ] Optional: one OPTX take of `01_clicks` at DECAY **fully left** and one at **fully right** (rest as H2), to check the ~10 % shorter tails on the hardware come from DECAY's noon position, not the DSP

- [ ] **Input LEDs flash red while moving knobs** (your note, 1 Oct; looped sample, LEDs otherwise green/amber, never red untouched). Next time it happens, please note: (1) is the red a brief flicker or held about half a second (a real warning is held 0.5 s)? (2) any click or dropout in the sound at the same moment? (3) which knob(s): DECAY, TENSION and TONE make the module work hardest; MIX, WOBBLE, DRIVE, SPLASH less. Claude's lead: knob moves are the CPU peaks; run 15 measures them

### 2. The plugin in Ableton (≈20 min)
- [ ] **Look at the panel interface:** does the layout read like your panel? Knobs comfortable to drag? Do the LEDs match the module? Does KICK fire? Tell Claude or send a screenshot of anything off
- [ ] **M2 Ableton check (≈15 min):** [m2-ableton-check.md](m2-ableton-check.md), MIDI clip `test_audio/midi/kicks_16ths.mid`. Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz. A 10th control, **Bypass**, is normal
- [ ] **A fresh listening pass**, answering the design questions below where you have a view
- [ ] Three checks on the new sound when you have a moment: **SPLASH in KICKED with DRIVE down** around 2 o'clock (a rim splashes ~2 dB less than before; fully up slightly more): strong enough? **TONE sweep on sharp clicks/rims**: does the level jump (left half ~+4 dB, fully right ~−4.5 dB on very sharp clicks; drum hits even)? **TONE fully left**: as warm and dark as you're used to?

### 3. Big Knob TONE · **merged** (2 Oct)
- TONE's right side becomes a King Tubby-style steeper low cut with a resonant bump, the "Big Knob" on Tubby's desk ([dub-spring-reference.md](dub-spring-reference.md) §6B, §8). Four versions on one page: today, steeper cut, steeper cut + nasal bump, and that plus "ringier when driven"
- [x] Researched and built in the cloud (1 Oct): `docs/research/big-knob.md` on the branch (the Altec 9069B, 18 dB/oct; its "bump" comes from how the desk was wired into it). Reference listen: *King Tubby Meets Rockers Uptown* (Augustus Pablo, 1976), the filter on the hi-hat
- [x] **Listened (1 Oct): v2** (steep + bump) nearly everywhere; v1 (no bump) won fully right on skank and KICKED hits; v3 only on pads at DRIVE 0.8. Your pick: **v2 with a gentler bump at the top** (+4.1 dB fully right instead of +5.6), built as v4
- [x] **Check page listened (1 Oct):** the bump won on every drum-hit panel, no bump won on pads, clicks and KICKED skank. Your pick: **the bump on hits only** (v5). (Note: that page's "v4" was really v3, a bug Claude fixed; your conclusion holds)
- [x] Top of the knob is now **800 Hz** (was 1.2 kHz): above ~850 Hz one KICKED setting (3 Springs, TENSION fully loose) rang. Every ringing/Howl test passes at 800 Hz. A deeper tank fix could reopen ~1.2 kHz later if you want it thinner
- [x] **Picked v5, the bump on hits only** (2 Oct). **Merged** (`c996a0a`, ADR 0036): TONE's right half is now the Big Knob. Not yet in Ableton or on the Versio: next plugin install / release

### 3b. SPRINGS 3 and SPLASH · experiments (cloud, can run alongside Big Knob)
- [x] Your note (1 Oct): 2 vs 3 Springs barely differ; SPLASH feels subtle. You want to hear every SPRINGS 3 idea, and SPLASH **stronger at the top** and **less tied to DRIVE**
- [x] SPRINGS 3 built in the cloud (1 Oct): five versions of position 3, no extra memory, all pass the ringing and Howl checks (the series version still flags one held-tone test: to fix before it could ship)
- [x] **Listened (2 Oct):** a different-sized tank muddles TENSION and DECAY (long, pan); liked the pan tank's character but couldn't tell whether it was the shorter tank or the brighter, higher-chirp sound; wide sounded close to today but lopsided, leaning to a higher harmonic on chords
- [x] Round 2 built (2 Oct): six versions, all at today's repeat timing (first echo and spacing within 0.5 ms), no extra memory, no ringing, the Howl unchanged
- [x] **Listen to SPRINGS 3 round 2 (≈15 min):** open `renders/springs3_palette2/index.html` (level-matched). **A** SPRINGS 2, **B** position 3 today, **C** pan brighter only (less bass, airier tail), **D** pan higher chirp only (higher, quicker, metallic boing), **E** mixed wire gauges (a small cluster of boings per hit, balanced L/R), **F** coupled (hits blur into a bloom instead of separate drips), **G** diffuse (smoother, softer echoes), **H** your cross-fed wide (bright left / today centre / dark right trading energy: roughly halves the lean, no upward pull on chords). C vs D answers "brightness or chirp?". Which makes position 3 its own thing?
- [x] SPLASH built in the cloud (1 Oct)
- [x] **SPLASH listened (2 Oct): C** (stronger top + works with DRIVE down) on every click and hit panel and most skank. **Merged** (SPEC v1.0.25). Not yet in Ableton or on the Versio: next install / release

### 3c. Today's sound next to your Wellspring (≈10 min)
- [x] Round 2 page (`wellspring_fit2`, deleted 2 Oct): listened, notes below
- Measured: echo spacing now matches (within 3–4 ms); the tail's tonal balance is ~3 dB off; our tails are shorter at both ends (lows 3.4 vs 4.7 s, 4 kHz 1.1 vs 1.9 s)
- [x] **Listened (2 Oct):** still quite different: the Wellspring is more diffuse; ours has more low end/mids (present, forward) where the Wellspring is further away and gentler; its repeats blur fast while ours flicker left/right like a delay. Measured, all three confirmed: tail washed-ness 0.97 vs ours 0.62–0.66; low-mid balance −6 vs −2 dB (defaults +1); L/R jumps 2.8 vs 9–10 dB per 10 ms
- [x] Wellspring fit round 3 built (2 Oct): no ringing in any version, Howl unchanged
- [x] **Round 3 listened (2 Oct):** still different: the Wellspring's echoes are further apart; it's more muted (less highs), wider, more diffuse; its repeats darken while ours sound metallic and bright. Measured: Claude's settings search had matched the Wellspring's fast high-only arcs (36 ms) instead of its main echoes (65 ms), so every comparison used TENSION far too tight: fixed, the closest is **TENSION noon**. And ours is ~17 dB brighter in the first 60 ms after a hit at every knob setting: the Wellspring's coil-and-magnet transducers filter the treble going in and coming out; ours barely do
- [x] Round 4 built (2 Oct), fitted to your sweep recording (take D): the transducers bring the onset brightness to your Wellspring's exactly (−25.7 dB), the treble falls steeply above ~3 kHz, the coil's warm even-order colour matches (2nd harmonic −25 dB); the tail as wide as yours with no flicker; the low end cut to your Wellspring's curve
- [x] **Listen (≈10 min):** open `renders/wellspring_fit4/compare/index.html`. One panel per sound, buttons switching in sync: **A your Wellspring**, **B** Resilio today, **C** round 3's best (sweep + stereo together + diffusion), **D** + transducers, **E** + wide, **F** + gentler low end. Level-matched, corrected settings (TENSION noon, TONE 2 o'clock). "My pick" per panel, then "Copy results for Claude". (Round 3's page `renders/wellspring_fit3/compare/` is now reference only)
- Known open after round 4: each echo's rise is sharper than the Wellspring's; the lowest octave rings shorter (3.3 vs 4.7 s); if picked, TONE needs re-mapping (its dark half gets much less dark), several tests need re-tuning, and the CPU-test firmware would be 1.3–3.1 KB over flash (needs trimming)
- Still different after round 3 (measured): the lowest octave rings shorter (3.8 vs 4.7 s; making it longer stretches DECAY past its range: a question for you later), the top octave too (1.2 vs 1.9 s), and the "pew" stops a bit lower than the Wellspring's

### 4. Fitted to your Wellspring · next round (local: your recordings stay on the Mac)
- [x] Listened (30 Sep): **B (fitted sweep)** in every panel; an improvement, but still far from the Wellspring: brighter (the old SPLASH burst was still in this prototype), and the pew on hits has a resonant quality in a different register
- Next round (one local agent, best after your weekly usage resets): keep B's sweep, drop the tone dip, make the echoes thin clean sweeps, add the Wellspring's fast highs-only echoes (every ~35 ms, a likely source of its resonant "zing"); compare at SPLASH 0 and with the new SPLASH

### 5. Wellspring recording session 2 (≈20 min, when convenient)
- [ ] Follow `docs/recording-recipe.md` §5b: same patch and base INPUT as session 1; takes H–N (a quiet and a hot sweep, the sweep into each tank alone, octave tone bursts, pink noise, held tones, the pad, 30 s of silence). First run `python3 tools/make_stimulus.py` and `python3 tools/make_sustain_stimulus.py` for the new files. Then tell Claude: it tells us the transducers' exact treble roll-off, how the input changes with level, the full stereo picture, per-octave darkening, and the hiss

### Friends' feedback
- [ ] Send your friend the candidate F zip (link above, in Share package). They compare "Resilio Versio F" with the 1 Oct "Resilio Versio" they already have
- When your friend replies about the plugin, paste it to Claude: it goes into the backlog next to your own notes

## Design questions (answer whenever you have a view; the plugin is the best judge)
- [ ] **What should make you reach for 3 Springs instead of 2?** Three rounds keeping today's repeat timing all came out subtle (wire gauges shipped). Options: a bigger, longer-ringing darker tank (rings longer than DECAY says); two tanks in a row (thicker, washed, a doubled boing); a brighter, splashier tank (sparse → classic → splashy); or leave it as "slightly denser"
- [ ] **Kick with SPLASH at 0:** full crash anyway, or should SPLASH scale the Kick's crash too?
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on one spring and the other way on the other, briefly spreading hard hits in stereo. Keep, or lurch together?
- [ ] **KICKED Howl on a tight tank** (TENSION up, DECAY max) leans toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] **TONE fully right:** thin and splashy enough, too thin, or should the low cut start earlier? (The Big Knob experiment, §3, will answer this one by ear)

## Later
- **Stereo in:** should the reverb follow where things are panned, or stay a classic mono-send dub tank? Options and costs: [SPEC §10](../SPEC.md). Decide once the new tank's CPU is known
- **A custom look for the plugin** (knob style, panel artwork), if you want one after living with the plain version

## Waiting on Claude (no action needed)
- **Docs:** refresh `docs/manual.md`, `docs/presets.md` and the share read-me for the Wellspring F sound; republish the friends' release once run 15 and the click check pass
- **Wellspring round 5** from your notes: a softer transient (each echo's front), the tail's resonance in a different place, which frequencies sit in the centre vs wide. Session 2 recordings (§5) would help measure the last two
- **After session 2 recordings:** ingest, the level series (how the input stage changes with level), the full stereo picture, per-octave darkening; feeds the next Wellspring round
- **Friends' share release** is still `1d18fce`: republish with `tools/make_release.sh --publish` when you want to send the new sound round
- **First chord after a run of drums** is still ~2.5 dB hot (the level trim's one-tick lag). Small; queued

## What to send Claude
- **Recordings:** say they're done and where (the Ableton project). Claude copies, renames and analyses them; the WAVs stay on your Mac, never in git
- **Listening:** paste the page's "Copy results for Claude", or plain-words answers
- **Hardware checks:** pass/fail per step; for anything odd, a line of description or a short recording

## Retired (30 Sep 2026: superseded, no action)
These judged builds that no longer exist, or were overtaken by newer work:
- M1 renders (5), M4 renders (6), M5 drive + TONE pages (8), SPLASH "heavier clang" re-listen (8b), M8 round 1 pairs (8c), M7 pages (9), TENSION/DECAY pages (10), M1 A/B listen (4b): the sound has moved on (TENSION, earlier first echo, spring EQ, SPLASH rework); their open questions moved to **Design questions**
- **Lows ring longer** page (`renders/proto_low_tail/`): the analysis showed the real difference is *how loud* our lows are (10–20 dB too much), not how long they ring
- **Optional re-listen** `renders/m3_stagger_abc/`: merged and measured (within 112–118 dB of the original)
- **Passthrough vs a cable:** done through the OPTX instead (H1: polarity right, −0.65 dB)
- **SPLASH round 3 pick** (`renders/splash_voicings/`): replaced by round 4, built from your notes

## Done
- 3 Oct 2026: **Wellspring F merged** with your picks (`340b542`, ADR 0038 / 0037 Round F2, SPEC v1.0.28): the tank fitted to your Wellspring (gentle highs from the first moment, repeats that darken, wider with no flicker), the low cut eased to D ("a little more" at TONE noon), SPRINGS 3 = coupled wire gauges, TONE's left half as dark as before. Installed in Ableton as the only Resilio; run 15 and the release built
- 3 Oct 2026: **BOING in Ableton fixed:** your Whalesong set's devices still held the pre-29 Sep BOING slot (sets save parameters by ID); remapped to TENSION, backup next to the set. Every install now also gets its own version number
- 2 Oct 2026: **SPRINGS 3 coupled merged** (your pick F "across the board", ADR 0037): position 3's three Springs share energy every round trip, so hits bloom instead of dripping; repeat timing and level as before. Firmware trimmed to fit (release 93 %, CPU-test 99.5 %). In Ableton (`2ed84f2`, 2 Oct); not yet on the Versio: next release together with Wellspring F
- 2 Oct 2026: **Wellspring round 4 listened:** F (+ gentler) on clicks, hits and skank. Still different: the Wellspring's transient is softer, the tail's resonance sits elsewhere, and different frequencies are centred vs wide
- 2 Oct 2026: **Manual, starting points and the friends' read-me refreshed** for the new sound (Big Knob TONE, SPLASH C, sustain trim, bipolar WOBBLE). The six starting points re-checked on it: none reaches the output limiter on hits, skank or a held pad. Changing a preset is still by ear: tell Claude what you'd move
- 1 Oct 2026: **Released** `1d18fce` as a GitHub Release (universal plugin, firmware, read-me) and installed it in Ableton; `tools/make_release.sh` makes the next one
- 1 Oct 2026: **Sustain trim merged** (ADR 0035, your pick D after three rounds): a gentle safety net on held sounds (pads, drones, organs) so they rarely reach the output limiter, plus a 30 ms limiter hold so light limiting sounds clean, not driven. Hits and skank unchanged. Found by you on the Versio: a low-mid pad lit the output red and sounded overdriven
- 1 Oct 2026: **Plugin panel interface** (knobs and switches at the Versio's positions, KICK button, LED meters) merged and installed; a universal share build sent to a friend
- 1 Oct 2026: **Bipolar WOBBLE merged** (ADR 0034, voicing D: B's middles, end stops halfway to C; right side a vibrato, left wow + flutter with a faint tape tremolo)
- 1 Oct 2026: **SPLASH/DRIVE build merged** (ADR 0032, 0033; your D + the DRIVE stretch "as far as the rule allows")
- 1 Oct 2026: **Renderer `--set` fix merged**: switch labels work (`attitude=KICKED`), wrong values are rejected
- 30 Sep 2026: **Design answers:** Kick at MIX fully down stays silent (it's part of the reverb); ATTITUDE flip while Howling at max DECAY calms into the long tail (already how it behaves; ADR 0018 wording corrected); TENSION bending the pitch of a ringing tail stays (you like it)
- 30 Sep 2026: **Smooth arc listened:** still way off the Wellspring (its pew on clicks, rounder, smoother). Measured the cause (a 2–5.5 kHz sweep we lack); started fitting our tank to your recording
- 30 Sep 2026: **CPU target raised to 70 % peak** (your call; ADR 0030 amendment, SPEC v1.0.20): room for the new sound, with a click check on the module for every release
- 30 Sep 2026: **Hanging note at WOBBLE ~9 o'clock:** B ("springs drift together at low WOBBLE") in every panel. Folded into the bipolar WOBBLE prototype. Also noted: WOBBLE 9 o'clock vs noon sounds nearly the same (today's lower half is only 0–3 cents)
- 30 Sep 2026: **WOBBLE ceiling:** keep the wilder top end (not the Magneto's ~8 cents). Asked how random it is: today it's sine + one smooth random line at a speed tied to the knob; at the top it's 90 % sine, hence "same-same"
- 30 Sep 2026: **SPLASH round 4 listened:** C2 on longer sounds and CLEAN hits, T2 on hits in DRIVEN/KICKED. Found by ear and measured: DRIVE shortens DRIVEN/KICKED tails (the saturator inside the loop). Decided: DRIVE = INPUT, partly louder, drives only in/out
- 30 Sep 2026: **Merged** the tail-length fix for noisy recordings, the docs cleanup and the send-level study (all 16 test suites pass); reference report regenerated (your Wellspring clicks: 3.42 s)
- 30 Sep 2026: **Tight-tank ping listened:** very subtle; C fine on hits, A slightly nicer on the skank (its faint inharmonic colour). Decided: keep the fix but gentler (fade starts at −30 dB, the most lenient setting that still passes the ringing check), shipped with the new SPLASH (without the burst, that corner rings audibly in the test)
- 30 Sep 2026: **Sweet tank listened:** the Wellspring won every panel (wider, a smoother high pitch bend on clicks; B/C/D added wavers, E's smear helped). Measured and pictured: its echoes are smooth arcs to ~5.5 kHz, width in the 250–500 Hz tail; B's different spring lengths caused the wavers. Next: one-smooth-arc prototype
- 30 Sep 2026: **SPLASH round 3 listened:** the burst isn't natural and B/C/D sound muted; E (DRIVE alone) sounds gritty and good. Next: round 4 at three strengths
- 30 Sep 2026: **LED flicker fixed** (the LEDs now dim through the chip's DMA; smooth, no clicks), LED order confirmed (In R only lights only the In R LED; In L lights both because the jacks copy L to R). Merged: `dist/resilio_versio_release_e618e12.bin`
- 30 Sep 2026: **Hardware recordings H0–H4** through the OPTX: the Versio plays the same reverb as the renders (tone within ~1 dB, same stereo, SPLASH the same); output polarity and level fix confirmed; tails ~10 % shorter (probably DECAY's noon). Found: the first skank stab after power-up peaks ~4 dB hot (fix queued)
- 30 Sep 2026: **M3 run 12: target met** (60.7 % average, 63.3 % peak). Merged. You asked whether 65 % is arbitrary: partly, a safety margin chosen at the start (ADR 0030)
- 30 Sep 2026: **First play on the real firmware:** working; output red on a KICKED Howl only; input red when the input's too hot (as designed). Your SoundStage send runs at line level, where today's SPLASH fades out; round 4 will make DRIVE the INPUT knob
- 30 Sep 2026: **Diffuse tank listened:** F (bright tail) the favourite; G (breath) dropped as added-on-top. Found: repeats too regular, highs lag growing per repeat, bass not centred
- 30 Sep 2026: **Low end vs real tanks:** ours had 10–20 dB too much below 160 Hz and centred at 300 Hz vs 1–2 kHz; the EQ preview's C (low cut + spring mids) was closer. **The Wellspring is the reference** (its soft warmth over the Magneto's thinness). Hiss decided: follows the tail only (later dropped)
- 30 Sep 2026: **DRIVE dampens SPLASH** (your Ableton note) measured and explained: SPLASH listens after the drive, which flattens hits
- 30 Sep 2026: **Dub signal-chain brief** saved with Claude's notes ([dub-spring-reference.md](dub-spring-reference.md)): gate stays KICK; outer feedback loop and Howl in DRIVEN parked
- 30 Sep 2026: **merged** the overnight work: tail-length measurement fix, LED meters (ADR 0031, SPEC v1.0.18), manual + dub preset drafts, new listening pages. Release firmware `3a790f7`
- 30 Sep 2026: **M3 run 11**: 62 % average, 68 % peak
- 29 Sep 2026: **Earlier first echo** (ADR 0029): loose tank's gap 45 → 32 ms, like your Wellspring; echo spacing unchanged
- 29 Sep 2026: **M3 CPU runs 1–10**: worst case 83 % → 63 % average, 100 % → ~67 % peak, sound unchanged
- 29 Sep 2026: **Knob layout** set to your printed Versio panel (ADR 0028): P1 MIX, P2 DECAY, P3 TONE, P4 SPLASH, P5 TENSION, P6 WOBBLE, P7 DRIVE
- 29 Sep 2026: **M0 passed** on the Versio (both sessions): controls, LEDs, CV, gate and audio work. The Versio flips polarity and adds 1.2 dB; the release firmware undoes both
- 29 Sep 2026: **Wellspring and Magneto recorded** and analysed (recordings stay on your Mac, ADR 0009); the ADAT path is ~2 ms slower than TRS (your hunch)
- 29 Sep 2026: **M1 A/B** against the Wellspring built: its tail is 3.5 s (the old "5 s" was a measuring error)
- 29 Sep 2026: **installed `ae844da`**: TENSION up = tighter, SPLASH "heavier clang", TONE's bright side thins the lows, mid-DECAY resonances fixed (ADR 0027)
- 29 Sep 2026: **TENSION direction flipped**: turning up = tighter (ADR 0026 amendment)
- 29 Sep 2026: SPLASH direction chosen: **"heavier clang"**; SPLASH-0 click traced and removed
- 29 Sep 2026: renders cleaned up (31 → 2.8 GB)
- 29 Sep 2026: **TENSION replaces BOING** and DECAY is tail length only (ADR 0026)
- 29 Sep 2026: CLEAN's gentle splash (ADR 0025)
- 29 Sep 2026: highs-later chirp (ADR 0024)
- 29 Sep 2026: session wrap/start skills added (`/resilio-wrap`, `/resilio-start`), plus a project `CLAUDE.md`
- 28 Sep 2026: M8 tuning round 1 merged and installed (flam gone, 1 Spring wider, early WOBBLE, audible SPLASH, DRIVE without dead zones, wet level spread 5.7–7.8 → 2.5–4.4 dB)
- 28 Sep 2026: **DRIVE at noon** too subtle: retuned, obvious from noon, level constant (ADR 0022)
- 28 Sep 2026: firmware flash trimmed (release 75 %, profile 83 %, was 98.5 %)
- 28 Sep 2026: held-note ticks fixed: the output limiter was hard-clipping peaks; now a smooth limiter + soft clip
- 28 Sep 2026: M7 built (SPLASH, real Kick, WOBBLE wired in)
- 28 Sep 2026: M6 anti-ringing ([write-up](m6-metric-calibration.md)): a Ringing test that passes real tanks, 0 of 270 grid cells ring
- 28 Sep 2026: plugin in Ableton pinned to milestone builds; dev builds no longer auto-install
- 28 Sep 2026: M5 drive chain + TONE built; IR dispersion study done ([report](ir-dispersion-study.md))
- 28 Sep 2026: M4 stereo fix (mono-safe, wide at every DECAY); M3 firmware variants ready
- 28 Sep 2026: Ableton spring impulse responses found and analysed (IR library, ADR 0021)
- 28 Sep 2026: GitHub push working again (a network routing problem on your connection, not GitHub)
