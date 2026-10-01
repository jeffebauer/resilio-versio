# Owner tasks

Your running to-do list. Claude keeps it current: open items at the top in the suggested order, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 1 Oct 2026, evening (session 5: SPLASH/DRIVE, bipolar WOBBLE, sustain trim, panel interface and the `--set` fix all merged; release published)

| Milestone | State |
|---|---|
| M0 hardware · M3 CPU | **Done.** Run 13 (SPLASH/DRIVE build): 63 % average, 66 % peak, target **70 %**. A new run is due for bipolar WOBBLE + the sustain trim (small costs) once the profile firmware fits again |
| Real firmware on the Versio | **New release ready:** `dist/resilio_versio_release_1d18fce.bin` (new SPLASH, DRIVE as INPUT, bipolar WOBBLE, sustain trim; 96.5 % of flash). Flash it, then the click check in §1 |
| M8 sound | **In progress:** SPLASH/DRIVE, bipolar WOBBLE and the sustain trim merged (1 Oct). Next: Big Knob TONE experiment; the Wellspring fit round |
| M2 Ableton check | Ready: the plugin is installed |
| M9 polish | LED meters done (module and plugin); panel interface in the plugin; manual and preset drafts written (`docs/manual.md`, `docs/presets.md`; need updating for the new SPLASH/DRIVE/WOBBLE) |

**Plugin in Ableton:** `1d18fce` (installed 1 Oct, evening: the gentle sustain trim + limiter hold, bipolar WOBBLE, the panel interface; AU validated). **Rescan needed:** open Ableton, rescan plug-ins (hold ⌥ and click Rescan), and replace any Resilio Versio in your set with a fresh one.

**Share package:** [GitHub Release v2026.10.01-1d18fce](https://github.com/jeffebauer/resilio-versio/releases/tag/v2026.10.01-1d18fce) (private, like the repo): universal plugin (Apple Silicon + Intel, macOS 12+), Versio firmware, read-me with install steps. Download the zip there to send to friends. Claude makes the next one with `tools/make_release.sh --publish`.

## Now (suggested order)

### 0. CPU run 14 on the module (≈10 min, USB only) · ready
- [ ] **Rack power unplugged**, Versio on USB. Flash `dist/resilio_versio_m3_profile_run14.bin` with NE Firmware Swap → Select Custom File. It ignores the knobs and makes its own test signal
- [ ] In Terminal: `screen /dev/tty.usbmodem* 115200`, let it run until the corner lines have gone round twice (a few minutes), then select all, copy and paste the output to Claude (quit screen: Ctrl-A, K, Y)
- Why: WOBBLE and the sustain trim have never been measured on the chip, and knob moves are the CPU peaks: the lead for the red input LEDs. This build is also the first with the engine built at power-up (frees flash): the run checks that on the hardware too
- Then flash the release back (`dist/resilio_versio_release_1d18fce.bin`) and do §1

### 1. Flash the Versio and play (≈2 min + play)
- [ ] Flash `dist/resilio_versio_release_1d18fce.bin` and do the **click check**: 3 Springs, KICKED, DRIVE and DECAY up, move knobs fast and hit KICK. Any click or dropout → tell Claude (the CPU target is 70 %; this is the safety net with every release)
- [ ] Play your low-mid pad again at the old settings (CLEAN, DECAY noon, 2–3 Springs, TENSION past 3 o'clock): the output LEDs should mostly stay out of red, and any brief red should sound clean, not driven
- [ ] Play it more thoroughly on the real panel: how does it feel? Anything surprising compared with the plugin?
- [ ] Optional: one OPTX take of `01_clicks` at DECAY **fully left** and one at **fully right** (rest as H2), to check the ~10 % shorter tails on the hardware come from DECAY's noon position, not the DSP

- [ ] **Input LEDs flash red while moving knobs** (your note, 1 Oct; looped sample, LEDs otherwise green/amber, never red untouched). Next time it happens, please note: (1) is the red a brief flicker or held about half a second (a real warning is held 0.5 s)? (2) any click or dropout in the sound at the same moment? (3) which knob(s): DECAY, TENSION and TONE make the module work hardest; MIX, WOBBLE, DRIVE, SPLASH less. Claude's lead: knob moves are the CPU peaks, and this release adds WOBBLE and the sustain trim without a CPU run on the module (the CPU-test firmware is over flash). Next: trim it so it fits, then CPU run 14

### 2. The plugin in Ableton (≈20 min)
- [ ] **Look at the panel interface:** does the layout read like your panel? Knobs comfortable to drag? Do the LEDs match the module? Does KICK fire? Tell Claude or send a screenshot of anything off
- [ ] **M2 Ableton check (≈15 min):** [m2-ableton-check.md](m2-ableton-check.md), MIDI clip `test_audio/midi/kicks_16ths.mid`. Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz. A 10th control, **Bypass**, is normal
- [ ] **A fresh listening pass**, answering the design questions below where you have a view

### 3. Big Knob TONE · experiment (next)
- TONE's right side becomes a King Tubby-style steeper low cut with a resonant bump, the "Big Knob" on Tubby's desk ([dub-spring-reference.md](dub-spring-reference.md) §6B, §8). Four versions on one page: today, steeper cut, steeper cut + nasal bump, and that plus "ringier when driven"
- [x] Researched and built in the cloud (1 Oct): `docs/research/big-knob.md` on the branch (the Altec 9069B, 18 dB/oct; its "bump" comes from how the desk was wired into it). Reference listen: *King Tubby Meets Rockers Uptown* (Augustus Pablo, 1976), the filter on the hi-hat
- [x] **Listened (1 Oct): v2** (steep + bump) nearly everywhere; v1 (no bump) won fully right on skank and KICKED hits; v3 only on pads at DRIVE 0.8. Your pick: **v2 with a gentler bump at the top** (+4.1 dB fully right instead of +5.6), built as v4
- [x] **Check page listened (1 Oct):** the bump won on every drum-hit panel, no bump won on pads, clicks and KICKED skank. Your pick: **the bump on hits only** (v5). (Note: that page's "v4" was really v3, a bug Claude fixed; your conclusion holds)
- [x] Top of the knob is now **800 Hz** (was 1.2 kHz): above ~850 Hz one KICKED setting (3 Springs, TENSION fully loose) rang. Every ringing/Howl test passes at 800 Hz. A deeper tank fix could reopen ~1.2 kHz later if you want it thinner
- [ ] **Quick check (≈5 min):** open `renders/proto_big_knob3/index.html`: **v1** no bump, **v4** gentle bump everywhere, **v5 bump on hits only**, all with the 800 Hz top. If v5 sounds right (hits ringy, pads and chords clean), Claude merges it

### 3b. SPRINGS 3 and SPLASH · experiments (cloud, can run alongside Big Knob)
- [x] Your note (1 Oct): 2 vs 3 Springs barely differ; SPLASH feels subtle. You want to hear every SPRINGS 3 idea, and SPLASH **stronger at the top** and **less tied to DRIVE**
- [ ] **Start the SPRINGS 3 session:** "Follow docs/briefs/springs3-palette.md on main. Work on branch proto/springs3-palette and push it; don't merge to main." Five versions of position 3: today, a long big tank, tanks in series, wide stereo spread, a different tank type
- [x] SPLASH built in the cloud (1 Oct)
- [ ] **Listen to SPLASH (≈10 min):** open `renders/proto_splash_stronger/index.html` (not level-matched: the splash's size is the point). **A** today, **B** a much bigger top quarter (about 3× the clang at full), **C** B + independent of DRIVE (with DRIVE down, SPLASH works as if DRIVE were at 0.8), **D** C, bolder (the clang reaches lower into each hit's body and rings longer). Rows: hits, skank, clicks × DRIVE 0 / 0.8 × SPLASH steps; columns CLEAN / KICKED. A ceiling keeps a big splash off the output limiter, never below today's splash

### 4. Fitted to your Wellspring · next round (local: your recordings stay on the Mac)
- [x] Listened (30 Sep): **B (fitted sweep)** in every panel; an improvement, but still far from the Wellspring: brighter (the old SPLASH burst was still in this prototype), and the pew on hits has a resonant quality in a different register
- Next round (one local agent, best after your weekly usage resets): keep B's sweep, drop the tone dip, make the echoes thin clean sweeps, add the Wellspring's fast highs-only echoes (every ~35 ms, a likely source of its resonant "zing"); compare at SPLASH 0 and with the new SPLASH

### Friends' feedback
- When your friend replies about the plugin, paste it to Claude: it goes into the backlog next to your own notes

## Design questions (answer whenever you have a view; the plugin is the best judge)
- [ ] **Kick with SPLASH at 0:** full crash anyway, or should SPLASH scale the Kick's crash too?
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on one spring and the other way on the other, briefly spreading hard hits in stereo. Keep, or lurch together?
- [ ] **KICKED Howl on a tight tank** (TENSION up, DECAY max) leans toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] **TONE fully right:** thin and splashy enough, too thin, or should the low cut start earlier? (The Big Knob experiment, §3, will answer this one by ear)

## Later
- **Stereo in:** should the reverb follow where things are panned, or stay a classic mono-send dub tank? Options and costs: [SPEC §10](../SPEC.md). Decide once the new tank's CPU is known
- **A custom look for the plugin** (knob style, panel artwork), if you want one after living with the plain version

## Waiting on Claude (no action needed)
- **Big Knob:** merge v4 after your quick check. **SPRINGS 3** cloud session still running
- **CPU run 14** ready for you (§0). Flash freed: the engine is now built at power-up (release 93 %, was 96.5 %; the CPU-test firmware fits again).
- **First chord after a run of drums** is still ~2.5 dB hot (the level trim's one-tick lag). Small; queued
- **Docs to refresh:** `docs/manual.md` and `docs/presets.md` for the new SPLASH, DRIVE, WOBBLE and sustain trim; small stale spots in code comments and old SPEC sections

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
