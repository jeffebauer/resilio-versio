# Owner tasks

Your running to-do list. Claude keeps it current: open items at the top in the suggested order, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 4 Oct 2026, evening (three features built and picked, waiting on your last listen + OK to merge; run 16 ready; Wellspring session 2 recorded, ingest running)

| Milestone | State |
|---|---|
| M3 CPU | Run 18: 2 Springs 66.0 / 72.0 %, echo mode 70.2 / 76.2 %, switch 77.3 %, worst moment 79.9 %. Under the 80 % ceiling (target 75 %; echo mode's 76 % accepted) |
| Real firmware on the Versio | `d74ddc5` (Wellspring F sound + knob end stops). **New release ready:** `843c5fc` with all of today's features (§2) |
| M8 sound | **Merged:** TONE after the springs, Throw + Hold, Echo mode (SPRINGS 3). µ-law on the DRIVEN/KICKED output picked, merging next. Wellspring round 5 (last fit) being built |
| M2 Ableton check | Ready: the plugin is installed |
| M9 polish | Manual, starting points and share read-me are stale; refreshed after the merge |

**Plugin in Ableton:** `843c5fc` (installed 5 Oct, AU validated): everything merged: CPU savings, TONE after the springs, echo mode (SPRINGS 3, follows Ableton's tempo), Throw + Hold (THROW is a latching button next to KICK), µ-law in DRIVEN/KICKED. **Rescan needed:** hold ⌥, Rescan, then insert a fresh instance

**Share package:** [GitHub Release v2026.10.01-1d18fce](https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.01-1d18fce) (private): universal plugin, firmware, read-me. A/B candidate: [v2026.10.02-cef6a77-candidate-F](https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.02-cef6a77-candidate-F). Both predate this week's features; the next release (after the merge) replaces them.

## Now (in this order)

### 1. Plugin install with µ-law · done 5 Oct
- [x] `843c5fc` installed and the AU validated. Rescan in Ableton (hold ⌥, Rescan) and insert a fresh instance

### 2. CPU run 17 + the new release on the module (≈15 min, USB first)
- [x] **Run 17 done (5 Oct):** 2 Springs 68.9 % avg / 74.6 % peak, echo mode 73.3 / 79.1, but a **82.4 % spike when switching from echo mode back to the Springs** (over the 80 % ceiling). The µ-law box costs ~8–9 points (twice its estimate). Claude is finding sound-neutral savings (the µ-law box, the switch) for run 18
- [x] **Run 18 (5 Oct):** 2 Springs 66.0 / 72.0 %, echo mode 70.2 / **76.2 %**, switch 3 → 1 77.3 %, the lap wrap 79.9 %: everything under the 80 % ceiling. Your call: merge it, and 76 % for echo mode is fine (the click check decides)
- [ ] **Then the release** `dist/resilio_versio_release_843c5fc.bin` and the **click check**: KICKED, DRIVE and DECAY up, move knobs fast, hit KICK, patch a gate in, and **flip SPRINGS between 3 and 1/2 while it plays hard** (that's the 82 % moment). Any click or dropout → tell Claude

### 3. Merged today (5 Oct), for reference
- [x] CPU savings, TONE after the springs, echo mode, Throw + Hold, µ-law (DRIVEN 12-bit / KICKED 8-bit). `main` passes all 25 test suites; release 94 % of flash, CPU-test 95 % (5.4 KB left: the next features need a trim first)
- Behaviour changes to know: the gate **throws** in SPRINGS 1–2 and **clocks the echo** in 3 (it no longer kicks); the button kicks everywhere, hold it 1 s to leave throw mode. MIX fully left in DRIVEN/KICKED is no longer a clean passthrough (µ-law); CLEAN still is

### 3b′. Echo tuning · picked B everywhere, merged (5 Oct, `c7dbf75`)
- [x] Echo mode's DECAY top = steady held repeats in every ATTITUDE; KICKED µ-law 10-bit. In the plugin after the next install

### 3c. Wellspring round 5, the last fit · being built (started 5 Oct)
- From your session 2 recordings: a softer echo front (the Wellspring swells in over ~0.5 s; ours hits and drops in ~35 ms), the longest ring moved from 1 kHz to ~500 Hz with the presence peak up to ~1.4 kHz, lows centred (front included) and the mids no longer left-heavy, a wider 1 and 4 kHz tail, and held sounds that settle flat like the real tank (less work for the sustain trim). Retunes existing stages only (the module's CPU and flash are tight). Page `renders/fit_round5/` with your Wellspring as the reference; your pick, then merge

### 4. Play it (plugin and module)
- [ ] **KICKED, TONE fully right, hit KICK:** with µ-law the grain from the ringing highs fills the lows after a Kick (the low end falls 10 dB in 300 ms instead of 24). Gritty longer thud, or mud?
- [ ] **Echo mode feel**, on the plugin or module: is TENSION noon's **0.4 s** a good resting echo time (range 2 s → 80 ms)? Does **DECAY** feel right from one repeat to a long build? Is the **KICKED runaway** in the right place (top ~13 % of DECAY)? Is the **swoop** (~0.3 s) the right speed?
- [ ] **Throw on the module:** gate into the gate jack opens the springs' send; hold KICK 1 s to leave throw mode (all four LEDs blink white)
- [ ] Your low-mid pad again (CLEAN, DECAY noon, 2–3 Springs, TENSION past 3 o'clock): the output LEDs should mostly stay out of red, and any brief red should sound clean, not driven
- [ ] Play it thoroughly on the real panel: how does it feel? Anything surprising compared with the plugin?

### 5. The plugin in Ableton (≈20 min, after the next install)
- [ ] **Panel interface:** does the layout read like your panel? Knobs comfortable? LEDs match the module? KICK and THROW work?
- [ ] **M2 Ableton check (≈15 min):** [m2-ableton-check.md](m2-ableton-check.md), MIDI clip `test_audio/midi/kicks_16ths.mid`. Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0 (in CLEAN), 44.1/96 kHz. A **Bypass** control is normal

### Optional
- [ ] Friends' feedback: worth waiting for the next release (after the merge) rather than sending candidate F, which is now several picks behind. Paste any reply to Claude

## Design questions (answer whenever you have a view; the plugin is the best judge)
- [ ] **What is In R for?** A duck key (patch your kick in so the hold ducks to it), a second send (e.g. just the snare into the springs), or plain stereo in? (SPEC §10)
- [ ] **Kick with SPLASH at 0:** full crash anyway, or should SPLASH scale the Kick's crash too?
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on one spring and the other way on the other, briefly spreading hard hits in stereo. Keep, or lurch together?
- [ ] **KICKED Howl on a tight tank** (TENSION up, DECAY max) leans toward one pitch, like a siren. Still a rough roar, or too tonal? (At TONE right you said the pitch lean is fine)

## Later
- **Stereo in:** see "What is In R for?" above; options and costs in [SPEC §10](../SPEC.md)
- **A custom look for the plugin** (knob style, panel artwork), if you want one after living with the plain version

## Waiting on Claude (no action needed)
- **Merging** the three features once you've said OK (§1), then the plugin install, release and CPU-test firmware
- **Docs after the merge:** `docs/manual.md`, `docs/presets.md`, the share read-me; then a new friends' release with `tools/make_release.sh --publish`
- **CPU, if run 16 comes in above 75 %:** more sound-neutral savings first (the tilt loop, the springs' per-sample work); a bigger audio block (+1 ms) stays the reserve
- **First chord after a run of drums** is still ~2.5 dB hot (the level trim's one-tick lag). Small; queued

## What to send Claude
- **Recordings:** say they're done and where (the Ableton project). Claude copies, renames and analyses them; the WAVs stay on your Mac, never in git
- **Listening:** paste the page's "Copy results for Claude", or plain-words answers
- **Hardware checks:** pass/fail per step; for anything odd, a line of description or a short recording

## Retired (superseded, no action)
- 4 Oct 2026: **"What should make you reach for 3 Springs?"**: answered by echo mode. **Wellspring fit "next round" (old §4)**: superseded by round 5. **Three checks on the F sound** (SPLASH with DRIVE down, TONE sweep level on clicks, TONE fully left): the sound moves again with TONE after the springs; check by ear after the merge (§4). **TONE fully right thin enough?**: settled by the Big Knob picks and TONE after the springs. Listening pages already judged were deleted (prototypes, BBD, diffuse, wear)
These judged builds that no longer exist, or were overtaken by newer work:
- M1 renders (5), M4 renders (6), M5 drive + TONE pages (8), SPLASH "heavier clang" re-listen (8b), M8 round 1 pairs (8c), M7 pages (9), TENSION/DECAY pages (10), M1 A/B listen (4b): the sound has moved on (TENSION, earlier first echo, spring EQ, SPLASH rework); their open questions moved to **Design questions**
- **Lows ring longer** page (`renders/proto_low_tail/`): the analysis showed the real difference is *how loud* our lows are (10–20 dB too much), not how long they ring
- **Optional re-listen** `renders/m3_stagger_abc/`: merged and measured (within 112–118 dB of the original)
- **Passthrough vs a cable:** done through the OPTX instead (H1: polarity right, −0.65 dB)
- **SPLASH round 3 pick** (`renders/splash_voicings/`): replaced by round 4, built from your notes

## Done
- 4 Oct 2026: **Listening picks:** TONE after the springs (B everywhere, Kick and Howl at TONE right fine); throws "thrown" everywhere, leave throw mode by holding KICK 1 s; hold = layer, a Howl flipped out of KICKED fades as before; echo mode (B) with even steps down from the hit, BBD grit A (stronger aliasing added pitched chirps); repeats' bit depth too subtle alone, so µ-law on the whole DRIVEN/KICKED output instead
- 4 Oct 2026: **Wellspring session 2 recorded** (takes H, I, D2 at a lowered OUTPUT, D-L, D-R, J, K, L, M, N)
- 4 Oct 2026: **CPU budget** now ≤ 75 % peak target, 80 % ceiling with a click check (your call; ADR 0030, SPEC v1.0.32)
- 4 Oct 2026: **Run 15** on the chip: 82 % average / 86 % peak (the Wellspring F tank's Sweep ~16 %, coupled SPRINGS 3 ~40 % more). Run 16 (sound-neutral savings, bit-identical) built
- 4 Oct 2026: **Click check passed** on `327af86`; **knob end stops** merged and confirmed (`d74ddc5`: MIX fully right is fully wet; every knob reaches exact 0 / 1)
- 4 Oct 2026: **Red input LEDs:** real input peaks (the shaker loop peaks at 0 dBFS); gone with the clip 6 dB lower. Not a firmware fault
- 4 Oct 2026: **Hardware DECAY end takes:** the module's tails match the desktop at both ends (the pot reads 0.00 / ~0.99); the old 10 % noon shortfall is the knob's mid-travel, not the DSP
- 4 Oct 2026: **Flash study merged** (`327af86`): 18.4 KB free in the release, 16.5 KB in the CPU-test build (were 4.8 KB / 576 B), by not setting up unused hardware; firmware stays one-click Firmware Swap (ADR 0011)
- 3–4 Oct 2026: **Dub-lens critique** ([research/dub-lens-critique.md](research/dub-lens-critique.md)) and your answers: gate = throw, hold at DECAY's top, Howl stays KICKED-only, no feedback return, no hiss (inaudible at real levels), round 5 is the last Wellspring fit
- 2 Oct 2026: **Big Knob TONE merged** (v5, the bump on hits only, top 800 Hz; ADR 0036). **SPLASH C merged** (stronger top, works with DRIVE down). **SPRINGS 3 rounds 1–2** listened (wire gauges shipped with Wellspring F)
- 2 Oct 2026: **Wellspring fit rounds 2–4** listened; round 4's F became the Wellspring F tank
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
