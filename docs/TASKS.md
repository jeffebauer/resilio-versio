# Owner tasks

Your running to-do list. Claude keeps it current: open items at the top in the suggested order, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 30 Sep 2026 (reorganised: open items first, stale listening tasks retired)

| Milestone | State |
|---|---|
| M0 hardware · M3 CPU | **Done.** Run 12: 61 % average, 63 % peak (target now **70 %**, your call 30 Sep: ~7 points for new sound) |
| Real firmware on the Versio | **Playing.** Keep `dist/resilio_versio_release_e618e12.bin` on it (knob layout, output fix, LED meters without flicker) |
| M8 sound | **In progress:** SPLASH round 4 and the "one smooth arc" tank are being built for you to hear |
| M2 Ableton check | After the next plugin install |
| M9 polish | LEDs done; manual and preset drafts written (`docs/manual.md`, `docs/presets.md`) |

**Plugin in Ableton:** still `ae844da` (29 Sep). The next install waits for the new SPLASH and tank sound, so you get them together. After an install: quit and reopen Ableton, rescan plug-ins (hold ⌥ and click Rescan), and replace any Resilio Versio in your set with a fresh one.

## Now (suggested order)

### 1. SPLASH + DRIVE build · being tuned from your notes
- [x] Listened (30 Sep): **the new build won every panel** (hits, skank, quiet send); DRIVE at half on the tail sweep. Remark: DRIVEN and KICKED hits and skank a bit hot/distorted; DRIVEN should sit between CLEAN and KICKED so intensity steps up evenly
- [x] Listened to the tuned build (30 Sep): **C everywhere** (more balanced) **except KICKED skank**, which sounded hotter and brighter (it had the "dramatic" clang). Fix: KICKED chords go back to your "clear" clang, drums keep the bigger one
- [ ] **Quick check (Claude says when ready):** version **D** on `renders/splash_drive_build2/index.html`, KICKED skank back to B's feel. Then Claude merges, builds the release firmware and the plugin
- Your round 4 picks (30 Sep), all at the "clear" strength: **every hit clangs the springs** (C2) in all ATTITUDEs; in DRIVEN and KICKED **short, sharp hits also bite** (T2); chords get the clang, not the bite
- Your DRIVE decisions (30 Sep): DRIVE becomes the **INPUT** knob; **partly louder** when pushed (the tail grows a few dB across the knob, so an envelope into DRIVE's CV makes a gentle throw); DRIVE drives only the **input and output** stages, so it no longer shortens DRIVEN/KICKED tails (measured: KICKED lost ~5 dB of tail at 0.6 s at DRIVE max)
- [x] **Run 13** (30 Sep, the build you heard): worst case 63.4 % average, **66.3 % peak** (highest anywhere 66.5 %), all under the 70 % target; the new SPLASH costs ~2.3 points. ~3.5 points left for the smooth-arc smear, the Wellspring-fit sweep and bipolar WOBBLE
- [ ] After the tuned version: release firmware + plugin install, then the click check

### 2. Fitted to your Wellspring · next round after the SPLASH/DRIVE merge
- [x] Listened (30 Sep): **B (fitted sweep)** in every panel; an improvement, but still far from the Wellspring: brighter (the old SPLASH burst was still in this prototype), and the pew on hits has a resonant quality in a different register
- Next round (local, one agent, after you OK the SPLASH/DRIVE build): keep B's sweep, drop the tone dip, make the echoes thin clean sweeps, add the Wellspring's fast highs-only echoes (every ~35 ms, a likely source of its resonant "zing"); compare at SPLASH 0 and with the new SPLASH

### 3. Play the Versio
- **With every new release firmware:** a quick click check on the module (≈2 min): 3 Springs, KICKED, DRIVE and DECAY up, move knobs fast and hit KICK. Any click or dropout → tell Claude (the CPU target is now 70 %, so this is the safety net)
- [ ] Play it more thoroughly on the real panel: how does it feel? Anything surprising compared with the plugin?
- Tip while SPLASH is today's version: SPLASH **near max** and the SoundStage's **main Level** up until the loudest hits just touch **amber** on the input LEDs (today's SPLASH fades out on quiet sends; round 4 fixes that)
- [ ] Optional, when convenient: one OPTX take of `01_clicks` at DECAY **fully left** and one at **fully right** (rest as H2), to check the ~10 % shorter tails on the hardware come from DECAY's noon position, not the DSP

## After the next plugin install
- [ ] **M2 Ableton check (≈15 min):** [m2-ableton-check.md](m2-ableton-check.md), MIDI clip `test_audio/midi/kicks_16ths.mid`. Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz. A 10th control, **Bypass**, is normal
- [ ] **A fresh listening pass** in the plugin, answering the open design questions below where you have a view. (It replaces the old per-milestone listening pages, which judged builds that no longer exist)

## Design questions (answer whenever you have a view; the plugin is the best judge)
- [ ] **Kick with SPLASH at 0:** full crash anyway, or should SPLASH scale the Kick's crash too?
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on one spring and the other way on the other, briefly spreading hard hits in stereo. Keep, or lurch together?
- [ ] **KICKED Howl on a tight tank** (TENSION up, DECAY max) leans toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] **TONE fully right:** thin and splashy enough, too thin, or should the low cut start earlier? (The "Big Knob" idea below would change this side)
- [ ] **Bright vs dark material:** the reverb comes back a few dB louder on dark, rumbly material. OK, or even it out?

## Later
- **Bipolar WOBBLE (your idea, 30 Sep; next prototype after the SPLASH/DRIVE build):** left of noon = smooth random wow + flutter (never repeats), noon = still (small dead zone for the hardware knob), right of noon = sine LFO strength up to today's wild top (you keep the extreme ceiling). Today the top end is 90 % one steady sine, which is why it can sound same-same. CPU ≈ +0.1–0.2 %. Touches ADR 0008, SPEC's WOBBLE row, WOBBLE tests; the random side includes your pick from the hanging-note page (springs drift together at low amounts), and each side must be clearly audible across its range (you found 9 o'clock and noon nearly the same today)
- **Big Knob TONE:** TONE's right side becomes a King Tubby-style steeper low cut with a resonant bump; cheap on CPU. After SPLASH and the tank work ([dub-spring-reference.md](dub-spring-reference.md) §6B, §8)
- **Stereo in:** should the reverb follow where things are panned, or stay a classic mono-send dub tank? Options and costs: [SPEC §10](../SPEC.md). Decide once the new tank's CPU is known

## Waiting on Claude (no action needed)
- **Running:** one-smooth-arc prototype page
- **Found by SPLASH round 4:** without the noise burst, one tight, bright KICKED setting rings at 3.9 kHz (the burst was hiding it). The tight-tank fix (your listen: subtle, keep it gentle) ships with the new SPLASH
- **First-hit level jump:** the level trim before the springs starts neutral and takes ~0.3 s to settle, so the first loud, bass-heavy hit after power-up peaks ~4 dB hot (your red output LEDs on the first skank stab). Fix: fast down (~20 ms), slow up
- **After your picks:** build SPLASH (DRIVE as INPUT) and the tank changes into the Core, with ADRs and tests re-tuned to the new sound; then a CPU run on the Versio and the plugin install
- **Renderer bug:** `rv_render --set attitude=KICKED` silently renders CLEAN (a separate session is fixing it; use `--preset` meanwhile)
- Small stale spots in docs and code comments (DRIVE-curve percentages, old SPEC sections): folded into the next code change that touches them

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
