# Owner tasks

Your running to-do list. Claude keeps it current: open items at the top in the suggested order, finished ones move to **Done** with the date. Tick boxes as you go, or just tell Claude.

**Last updated:** 30 Sep 2026 (reorganised: open items first, stale listening tasks retired)

| Milestone | State |
|---|---|
| M0 hardware · M3 CPU | **Done.** Run 12: 61 % average, 63 % peak (target 65 %) |
| Real firmware on the Versio | **Playing.** Keep `dist/resilio_versio_release_e618e12.bin` on it (knob layout, output fix, LED meters without flicker) |
| M8 sound | **In progress:** SPLASH round 4 and the "one smooth arc" tank are being built for you to hear |
| M2 Ableton check | After the next plugin install |
| M9 polish | LEDs done; manual and preset drafts written (`docs/manual.md`, `docs/presets.md`) |

**Plugin in Ableton:** still `ae844da` (29 Sep). The next install waits for the new SPLASH and tank sound, so you get them together. After an install: quit and reopen Ableton, rescan plug-ins (hold ⌥ and click Rescan), and replace any Resilio Versio in your set with a fresh one.

## Now (suggested order)

### 1. SPLASH round 4 · **ready** (≈15 min)
- [ ] Open `renders/splash_round4/index.html` from Finder (columns CLEAN / DRIVEN / KICKED, rows hits / skank, MIX half-way). **A** burst (today) and **E** DRIVE alone for reference, then **T** "hits bite harder" (grit on the hits) and **C** "hits clang the springs" (brighter drip on top), each **gentle / clear / dramatic**, plus **TC** both. Deliberately not level-matched: how much harder the hits hit is the point. Pick per panel, notes, **Copy results for Claude**
- [ ] **Decide with it:** should higher DRIVE make SPLASH bite sooner and harder (like your Wellspring's hot take C)? Today's findings say yes: DRIVE currently *reduces* splash
- [ ] **Decide with it:** when you push DRIVE, should the tail get **louder** (like a real INPUT knob or a desk send, so an envelope into DRIVE's CV becomes a dub throw), or stay level-matched as today? Background: [dub-spring-reference.md](dub-spring-reference.md) §8
- Heads-up: **T3, C2, C3 and TC** can touch the output limiter (they're loud on purpose); **ghost notes stay quiet** in every T/C version (A's burst fires on them). In CLEAN, T only gets louder (no grit to add); C still sparkles
- Why this is first: your pick unblocks the next build (DRIVE becomes the INPUT knob, SPLASH comes from your own hits), and the next plugin install

### 2. One smooth arc · listen when Claude says it's ready (≈15 min)
- [ ] Open `renders/proto_smooth_arc/index.html`. Built on A (bright tail): **B** one smooth arc per echo up to ~5.5 kHz (the Wellspring's high pitch bend, no kink, no plain clicks in the highs), **C** wider low mids in the tail, **D** light smear, **E** (option) repeats that don't stretch, **W** your Wellspring. Where do you stop?
- Why: last round the Wellspring won every panel. The pictures showed its echoes are smooth arcs and ours kink at ~3.3 kHz ([spectrograms](prototypes/spring-signature/))

### 3. Quick yeses (≈2 min)
- [ ] **OK to merge** three finished, tested pieces? (a) the tail-length measurement now copes with recording hiss (your Wellspring clicks read 3.4 s, not 14 s); (b) docs cleanup (glossary knob names, firmware notes, SPEC DECAY range, README); (c) the send-level study notes
- [ ] **Tight-tank ping (optional, ≈5 min):** `renders/proto_tight_ringing/index.html`. The ringing our test flagged is real but inaudible (75–85 dB under the tail). Fix C costs no CPU. Only question: do KICKED's loud hits, grit and tail ends sound **unchanged** with C?

### 4. Play the Versio
- [ ] Play it more thoroughly on the real panel: how does it feel? Anything surprising compared with the plugin?
- Tip while SPLASH is today's version: SPLASH **near max** and the SoundStage's **main Level** up until the loudest hits just touch **amber** on the input LEDs (today's SPLASH fades out on quiet sends; round 4 fixes that)
- [ ] Optional, when convenient: one OPTX take of `01_clicks` at DECAY **fully left** and one at **fully right** (rest as H2), to check the ~10 % shorter tails on the hardware come from DECAY's noon position, not the DSP

## After the next plugin install
- [ ] **M2 Ableton check (≈15 min):** [m2-ableton-check.md](m2-ableton-check.md), MIDI clip `test_audio/midi/kicks_16ths.mid`. Loads (AU + VST3), automatable, MIDI Kicks, null test at MIX 0, 44.1/96 kHz. A 10th control, **Bypass**, is normal
- [ ] **A fresh listening pass** in the plugin, answering the open design questions below where you have a view. (It replaces the old per-milestone listening pages, which judged builds that no longer exist)

## Design questions (answer whenever you have a view; the plugin is the best judge)
- [ ] **WOBBLE ceiling:** the Magneto's tape wobble tops out at ~8 cents; ours reaches ~36 cents on the first echo and ~50–55 in the tail at max, on purpose ("clearly out of tune", ADR 0008). Keep the wilder top, or bring it closer?
- [ ] **Kick with SPLASH at 0:** full crash anyway, or should SPLASH scale the Kick's crash too?
- [ ] **Kick with MIX fully down:** the Kick is part of the reverb, so at MIX 0 it's silent. OK?
- [ ] **Big hits in KICKED:** the pitch lurch goes one way on one spring and the other way on the other, briefly spreading hard hits in stereo. Keep, or lurch together?
- [ ] **ATTITUDE flip while Howling at max DECAY:** calm into the long (~9 s) tail, or fade within 1–2 s (ADR 0018)?
- [ ] **KICKED Howl on a tight tank** (TENSION up, DECAY max) leans toward one pitch, like a siren. Still a rough roar, or too tonal?
- [ ] **TENSION on a ringing tail** raises the pitch like tightening a string. Nice, or too much?
- [ ] **TONE fully right:** thin and splashy enough, too thin, or should the low cut start earlier? (The "Big Knob" idea below would change this side)
- [ ] **Bright vs dark material:** the reverb comes back a few dB louder on dark, rumbly material. OK, or even it out?
- [ ] **Hanging note at WOBBLE ~9 o'clock (optional page):** `renders/proto_wobble_hang/index.html`. B makes the springs drift together at low WOBBLE so a chord fades evenly. Still wanted once the new tank lands? (It predates the smooth-arc work)

## Later
- **Big Knob TONE:** TONE's right side becomes a King Tubby-style steeper low cut with a resonant bump; cheap on CPU. After SPLASH and the tank work ([dub-spring-reference.md](dub-spring-reference.md) §6B, §8)
- **Stereo in:** should the reverb follow where things are panned, or stay a classic mono-send dub tank? Options and costs: [SPEC §10](../SPEC.md). Decide once the new tank's CPU is known
- **CPU headroom:** 63 % peak against a 65 % target leaves ~1 point. If the new tank needs more, raising the target (say 72 %) is your call, with an ADR ([ADR 0030](adr/0030-fitting-the-versio-cpu.md) explains the 65 %)

## Waiting on Claude (no action needed)
- **Running:** one-smooth-arc prototype page
- **Found by SPLASH round 4:** without the noise burst, one tight, bright KICKED setting rings at 3.9 kHz (the burst was hiding it). The tight-tank fix (3, optional page) addresses exactly that corner; it would ship with the new SPLASH
- **First-hit level jump:** the level trim before the springs starts neutral and takes ~0.3 s to settle, so the first loud, bass-heavy hit after power-up peaks ~4 dB hot (your red output LEDs on the first skank stab). Fix: fast down (~20 ms), slow up
- **After your picks:** build SPLASH (DRIVE as INPUT) and the tank changes into the Core, with ADRs and tests re-tuned to the new sound; then a CPU run on the Versio and the plugin install
- **After the merge OK:** regenerate the reference report with the corrected tail lengths
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
