# Dub Spring Reverb: Reference & Exploration Brief

Resilio Versio · v2 · 30 Sep 2026 · checked against repo `main` @ `3e32524`

**For the Claude Code session:** this is a brief for discussion and alignment, not a build instruction. It gathers (1) how dub engineers actually used spring reverb, (2) how Resilio compares, checked against the code, and (3) four areas the owner may want to explore, each costed against the Versio's CPU budget. Nothing here changes the frozen SPEC. Anything taken forward goes through the usual path: prototype outside `core/`, renders, a listening page, an owner pick, then an ADR.

Suggested home in the repo: `docs/dub-spring-reference.md`.

Confidence tags: **[sourced]** = multiple or primary-ish sources · **[single source]** · **[lore]** = forum or anecdote · **[estimate]** = reasoning, not measured.

---

## 1. Core principle

Dub treats the mixing desk as an instrument played live. The reverb isn't a background room. It's thrown at single hits, ridden, filtered and muted, then left to ring on its own, and every mix pass comes out different. [sourced: Steve Barrow/Furious, RBMA, Orphiq]

Design implication: controls a dub engineer "rode" must be fast, playable, CV-able and musical across their whole range.

---

## 2. Key rigs

| Engineer / studio | Spring unit(s) | Around it | Confidence |
|---|---|---|---|
| King Tubby, Waterhouse, Kingston | Spring tank; one claimed to be a modified Fairchild (Alborosie collection, via AudioThing) | Ex-Dynamic Sounds MCI 12→4 console with built-in Altec 9069B high-pass filter ("Big Knob"); tape echo off an MCI 2-track | Console and filter [sourced]; exact spring model [single source], contested |
| Lee "Scratch" Perry, Black Ark | Grampian Type 636; Fisher SpaceXpander also cited | TEAC 4-track, small desk, Roland RE-201 Space Echo (which has its own spring), Mu-Tron Bi-Phase | Grampian, Space Echo and Bi-Phase [sourced]; Fisher [single source] |
| Scientist (Hopeton Brown), Tubby's then Channel One | Tubby's rig early on | Heavy EQ and filtering on effects | Career path [sourced]; specific EQ moves [lore] |

---

## 3. Techniques by signal-chain stage

### A. Before the tank (send side)
1. **High-pass filter, the "Big Knob".** Altec 9069B: a passive inductor (coil-based) high-pass, ~18 dB/oct, 10–11 stepped positions from 70 Hz to 7.5 kHz. Fitted to Tubby's MCI to clean rumble off mic channels; Tubby made it a trademark sweep. Often used on the reverb and echo sends. [sourced]
2. **Send "throws".** Burst the aux send on one channel (snare or rimshot, guitar skank, a vocal word) so only that hit goes into the spring or echo. The snare/rim had its own channel on Tubby's desk. [sourced]
3. **Drive into the unit.** Perry ran signals through the Space Echo's preamp with the delay off, just for grit; the Grampian 636 was famously overdriven as a fuzz. [sourced]
4. **Physical kick.** Tubby hit the spring unit with a stick for a thunderclap "clang", confirmed by Nick Manasseh. [sourced]

### B. The tank
5. Short, drippy, splashy springs are prized over long smooth washes; a spring has its own colour, not a room's. [forum consensus]
6. Perry used his spring "for the clash of the drum". [single source]

### C. After the tank (return side)
7. **Mute the source, keep the tail.** Throw a burst into the effect, mute the dry channel, and only the tail remains. [sourced]
8. **Feedback loops.** Effect returns fed back into themselves on the desk; echo swell ridden into runaway. [sourced as general practice]
9. **Spring and echo chained,** each on its own send and return, often into each other. [sourced]
10. **Filter on the return:** high-pass on the reverb [sourced]; heavy EQ on effects attributed to Scientist [lore].
11. **Phasing:** Bi-Phase on skanks (Perry); Big Knob blended against dry (Tubby). [sourced]

**Corrections to earlier voice-chat claims:**
- Tubby patching *spring output → spring input* to howl isn't documented. The runaway in sources is tape-echo regen and desk-return loops. Spring-only regen is a design choice, not history.
- "Boost highs on the return" is unverified. The documented move is a *high-pass* on the reverb, which thins it by removal rather than boosting.

---

## 4. What Resilio does today (from the code)

Panel (ADR 0028): P1 MIX · P2 DECAY · P3 TONE · P4 SPLASH · P5 TENSION · P6 WOBBLE · P7 DRIVE · SW0 SPRINGS 1/2/3 · SW1 ATTITUDE CLEAN/DRIVEN/KICKED · button + gate = KICK (plus MIDI in the Plugin only).

Signal path: in → DriveIn → Tilt (TONE) → Springs (each with its own Loop) → stereo mix → DriveOut → limiter → MIX.

- **DECAY:** each Spring's own Loop feedback gain (T60 ~0.3 s → ~8–10 s, always fades, ADR 0001). In KICKED, the top ~10% of the knob tips into **Howl**, self-sustaining and saturated, never a pure tone (ADR 0002). It fades out naturally when backed off (ADR 0018). CLEAN and DRIVEN never self-oscillate.
- **DRIVE:** three stages. DriveIn (transducer → tape) before the Springs, LoopSat inside each Loop, DriveOut (pickups) after, with auto makeup gain.
- **TONE:** smooth bipolar tilt before the Springs, plus a damping low-pass in each Loop, plus HF level at the output. Left = warm dark, right = bright splash capped below harsh (ADR 0017).
- **MIX:** equal-power dry/wet. There's no separate send level.
- **Gate:** digital on/off, triggers above ~+2 V, no velocity (SPEC §3, ADR 0005). Unpatched reads *off*.

| Dub technique | Resilio today | Status |
|---|---|---|
| Physical kick | KICK (button, gate, MIDI in Plugin), thud + crash (ADR 0016) | ✅ covered |
| Short drippy tank | DECAY low + TENSION + SPRINGS 1 | ✅ covered |
| Drive into unit | DriveIn + LoopSat + DriveOut | ✅ covered, matches history |
| Feedback to runaway | Howl, KICKED top ~10% only, inside each Spring | ✅ partly: no outer loop |
| Big Knob high-pass | TONE tilt, a gentle slope | ❌ gap (see §6B) |
| Send throws | none: MIX ≠ send; DRIVE is auto level-compensated | ❌ gap (see §6A) |
| Mute source, keep tail | MIX full wet only | ⚠️ partial |
| Echo chaining | none | out of scope: use a rack echo |
| Phaser | WOBBLE is spring modulation, not a phaser | leave |

---

## 5. CPU budget: the frame for every exploration

**Where we are** (`firmware/README.md` "M3 results", ADR 0030):
- Target: ≤ 65% of the audio-callback budget, worst case (3 Springs, loosest TENSION, KICKED, DRIVE max), block 48.
- Run 9: worst case **~63% average, ~67% peak**. Runs 10–11 not yet measured (run 11 is the final code).
- Reserve: block 96 (+1 ms latency), owner-approved only if the sound-neutral work falls short.
- Flash: release 83%; profile build 95% (benchmark code, to be trimmed).

**Reference costs per section** (run 3, before later optimisations, % of budget, worst case):

| Section | Cost | Reading |
|---|---|---|
| Each Spring | ~19.6 | Springs are the bulk (58 of 81 points in that run) |
| out (mix, decorrelator, pickups, shelf, limiter, MIX) | ~10.0 | — |
| drvIn (input drive + excitation followers) | ~7.4 | Nonlinear stage; the most useful anchor for any new saturation |
| splash (Kick + Splash) | ~2.4 | — |
| ctl (control tick) | ~2.0 (5.3–5.8 after a TENSION change) | — |
| tilt (TONE tilt + transport) | ~0.8 | The most useful anchor for any new pre-tank linear filter |

**Rough cost tiers for the explorations below** [estimate, all must be measured on a profile run]:

| Exploration | Tier | Why |
|---|---|---|
| Gate as KICK (no change) | none | Already built |
| Gate as CUT or THROW | ~free | One gain with a short ramp; control logic only |
| Inductor HPF, linear part (steep slope + bump) | low, same order as `tilt` | A few filter stages on the mono pre-tank signal, like today's tilt |
| Inductor saturation, folded into the existing DriveIn | low to moderate | Shares an existing nonlinear stage instead of adding one |
| Inductor saturation as a *new* nonlinear stage | moderate, risky | Could approach a slice of `drvIn` if it needs oversampling to stay clean |
| Outer feedback loop (output → DriveIn + Tilt) | low CPU, high risk | Cheap to compute, but touches AntiRes, Howl and ADRs 0001/0002/0018, so tuning and test cost is high |

Headroom is ~2 points against the 65% target *if* run 11 lands where run 9 did. Anything above "low" probably needs an offsetting saving or the block-96 reserve.

---

## 6. Exploration areas

### A. Gate: KICK, THROW or CUT?

Hardware facts: gate is on/off only, unpatched reads off; MIDI kicks exist only in the Plugin, so on the Versio KICK = button + gate.

**Keep KICK on the gate (today)**
- Pros:
  - Nothing else in the rack can do it: a Kick is an internal tank impulse, not reachable from outside.
  - Sequenced "thunder" (on the 4, on fills, random) is a performance move no hardware spring offers.
  - Built, tuned, no CPU cost.
- Cons:
  - Fixed strength, no velocity: sequenced kicks risk sounding like a retriggered sample.
  - Historically the clang was an occasional accent, so kicking on a pattern can tip into gimmick.

**Gate = THROW (gate on → input opens into the Springs)**
- Pros:
  - The most historic dub move.
  - A sequencer can drench only chosen hits.
- Cons:
  - **Bad default:** unpatched = off = silent reverb. It would need a workaround (e.g. throw mode only after the first gate edge), which adds complexity and surprises.
  - On/off only, so no partial throws.
  - Loses gate kicks.
  - **The rack can already do it:** mult the source, VCA + envelope into Resilio at MIX full wet, mix dry back externally.

**Gate = CUT (gate on → input into the Springs closes, tail rings on)**
- Pros:
  - The "mute source, keep tail" move (§3.7).
  - **Safe default:** unpatched = off = reverb works normally.
  - A gate pattern chops what feeds the tank while tails ring through.
- Cons: loses gate kicks.

Current lean (Claude, not yet an owner decision): **keep KICK on the gate.** Throws are easy with an external VCA; kicks can't be done any other way. If on-module throws are wanted, CUT beats THROW on defaults.

### B. Inductor-voiced TONE (the "Big Knob" character)

**Owner decisions (30 Sep 2026):**
- **No clicks or jumps.** TONE stays a smooth sweep; the stepped positions and switch clicks are out.
- **Explore the tonal character of the inductor filter.**

**Ingredients to model**, most to least certain:
1. **Steep low cut, ~18 dB/oct.** Bass disappears fast below cutoff; at high settings it's thin and telephone-like. The current tilt can't do this. [sourced]
2. **Bump just above cutoff:** nasal, "ringy". The KTBK hardware copy has a resonance control, which suggests the peak matters; its size on Tubby's unit is unverified. [single source / inference]
3. **Level-dependent saturation:** loud lows saturate the coil, adding harmonics and softness, so the filter gets grittier when hit harder. AudioThing's emulation exposes "Character / Non-linearities / Inductance". [lore, plausible]
4. **Phase shift near cutoff.** This is what made Tubby's blend phasing work, but it only phases against an unfiltered copy of the *same* sound. The reverb tail isn't the same sound as the dry, so expect little phasing in Resilio. Low priority. [estimate]

**Proposed shape** (for discussion):
- **Placement: before the Springs,** where the tilt already sits. Matches dub practice (thin what hits the tank) and keeps low boom out of the Loops.
- **Share TONE:**
  - Left of noon: unchanged warm dark tilt.
  - Noon: neutral.
  - Right of noon: the tilt morphs into the inductor high-pass, with cutoff climbing and the bump rising.
  - Open question: does this replace today's bright-splash right end (ADR 0017) or blend with it?
- **Couple saturation to DRIVE** ("ringy when driven"), ideally folded into DriveIn rather than a new nonlinear stage, for CPU (see §5).
- **Keep TONE's existing guarantees** (ADR 0017, SPEC §2.3.4): the Chirp stays audible at every setting, loudness stays within ±3 dB across the sweep, TONE is never needed to fight Ringing, and AntiRes still passes the M6 grid. A steep filter with a bump is exactly where a narrow peak could feed Ringing, so re-run the grid.

**Prototype path:** same as SPLASH round 3.
1. A patch outside `core/` with 2–3 voicings (e.g. slope only · slope + bump · slope + bump + DRIVE-coupled saturation).
2. Renders: snare/rim, skank, held chords, at TONE 0.5 / 0.7 / 0.85 / 1.0 across ATTITUDE.
3. Listening page, then the owner picks.
4. Measure the CPU of the pick on the next profile run before any ADR.

### C. Outer feedback loop (Space Echo-style regen)

Today's Howl lives inside each Spring's Loop. An outer loop would send the output back through DriveIn + Tilt, so each pass gets grittier and thinner, like riding a Space Echo's intensity.
- Cheap CPU, but high design risk: it interacts with AntiRes, Howl and ADRs 0001, 0002 and 0018. It would need a new ADR since the SPEC is frozen.
- No control is free, so it would have to share one (e.g. become the DECAY top zone, or tie to ATTITUDE).
- **Status:** idea only. The owner hasn't decided whether it's wanted.

### D. Howl outside KICKED?

Today only KICKED can Howl (ADR 0002). Should DRIVEN reach a gentler runaway? Zero CPU; it's a design and tuning question. **Status:** open.

---

## 7. For the Claude Code session to align on

**Owner questions still open:**
1. Gate: keep KICK, or switch to CUT / THROW?
2. Inductor TONE: should the right side go fully Big Knob (thin, nasal), or blend with today's bright splash?
3. When to prototype inductor TONE: now, or after the SPLASH round 3 picks (next in the handoff)?
4. Outer feedback loop and Howl-in-DRIVEN: explore, or park?

**Asks for Claude Code:**
- Sanity-check the §5 cost tiers against the code and the latest profile numbers (run 11 once measured).
- Say whether ~2 points of headroom realistically fits the inductor HPF, and whether DRIVE-coupled saturation can share DriveIn's oversampling.
- Flag any ADR or test that each exploration would touch.
- Suggest the order of work alongside the current backlog (SPLASH round 3, M3 run 11, plugin install, Metrics T60 fix).

---

## Sources

- [AudioThing Dub Filter, Plugin Boutique](https://www.pluginboutique.com/product/2-Effects/19-Filter/12115-Dub-Filter)
- [Gearnews: AudioThing Dub Filter / Big Knob](https://www.gearnews.com/audiothing-dub-filter/)
- [Red Bull Music Academy: The Roots of Dub](https://daily.redbullmusicacademy.com/2018/08/the-roots-of-dub/)
- [Sound On Sound: Audio Merge KTBK](https://www.soundonsound.com/news/audio-merge-ktbk-passive-filter)
- [Soundgas: KTBK product notes](https://soundgas.com/products/audio-merge-king-tubbys-big-knob-ktbk)
- [KVR: Alborosie Dub Station (Tubby's Fairchild spring claim)](https://www.kvraudio.com/forum/viewtopic.php?t=560219)
- [KVR: recreating the Tubby filter (inductor ringing, phase)](https://www.kvraudio.com/forum/viewtopic.php?t=217939)
- [Wikipedia: King Tubby](https://en.wikipedia.org/wiki/King_Tubby)
- [Steve Barrow: King Tubby tribute (Furious.com)](https://www.furious.com/perfect/kingtubby2.html)
- [King Tubby's Studio case study (Williams; Prince Fatty)](https://notempo205853180.wordpress.com/2021/12/29/analysing-popular-music-through-the-lens-of-music-technology-the-case-of-king-tubbys-studio/)
- [Wikipedia: Scientist](https://en.wikipedia.org/wiki/Scientist_(musician))
- [Orphiq: What Is Dub Music?](https://orphiq.com/resources/what-is-dub-music)
- [Equipboard: Lee Perry gear](https://equipboard.com/pros/lee-scratch-perry)
- [Gearspace: Lee Perry & Black Ark](https://gearspace.com/board/featured-content/1444057-lee-scratch-perry-amp-his-black-ark-studio-kingston-jamaica.html)
- [Sound On Sound: Perry & Daniel Boyle](https://www.soundonsound.com/people/lee-scratch-perry-daniel-boyle-recording-back-controls)
- [Vintage King: Lee Perry in the studio](https://vintageking.com/blog/2021/09/lee-perry)
- [Ableton: Studio as Instrument pt 3](https://www.ableton.com/en/blog/studio-as-an-instrument-part-3/)
- [Pressure Sounds: Black Art From The Black Ark](https://pressuresounds.bandcamp.com/album/black-art-from-the-black-ark)
- [Wikipedia: Roland Space Echo](https://en.wikipedia.org/wiki/Roland_Space_Echo)
- Repo: `SPEC.md` §3–5, `CONTEXT.md`, ADRs 0001, 0002, 0005, 0016, 0017, 0018, 0028, 0030, `firmware/README.md` "M3 results", `docs/handoff/HANDOFF.md`

---

## 8. Claude Code's notes (30 Sep 2026, against `main` @ `f0d8b29`)

**Out of date in §4–5:**
- **CPU headroom is negative, not ~2 points.** Run 11 (final code at the time): worst case **62.1 % avg / 67.8 % max**, highest max 68.6 % (`firmware/README.md` "Run 11"). Run 12 (redesign split over ticks, no sound change) is built and waiting for the owner's hardware run; it aims to bring the peak under 65 %. The diffuse/sweet-tank ingredients the owner likes also want CPU (diffuse-tank backlog: more pickups ≈ +2 points, smear ≈ +4). Anything above "~free" competes with that sound work.
- **TONE's right half is not only a gentle tilt:** since 29 Sep it also sweeps a 2nd-order (12 dB/oct) high-pass before the Springs, 20 Hz at noon → ~105 Hz at 3 o'clock → 300 Hz fully CW (`DriveVoicing.h` `toneLowCutHz`). The diffuse-tank prototype's step B adds a fixed tank-like low cut (200 Hz, 12 dB/oct) and a broad ~1.6 kHz lift before the Springs (a real driver coil's colour, from the Wellspring comparison). The Big Knob would extend what's there.
- DECAY's range is 0.4 → 9 s (SPEC §3 being corrected); the Metrics T60 fix landed.

**Agree:**
- **Gate stays KICK** (§6A). CUT beats THROW on defaults if on-module throws are ever wanted.
- **Big Knob on TONE's right side (§6B):** extend today's low cut to ~18 dB/oct, reaching higher (towards ~1–2 kHz for "telephone"), with a bump above cutoff that rises with it. Cost: a couple of filter stages on the mono pre-tank signal, same order as `tilt`. Cautions: cap well below the Altec's 7.5 kHz (above ~2 kHz there's little left to excite the Springs, whose band is ~200 Hz–4 kHz); the bump can feed tank modes, so re-run the M6 grid; keep ADR 0017's guarantees.
- **DRIVE-coupled "ringier when driven" saturation** belongs inside the existing DriveIn (it already has a flux-style HF cut into its saturator and 2× oversampling), not a new stage. It pairs with SPLASH round 3's DRIVE-as-INPUT plan (`m8-tuning-backlog.md` "Send-level calibration study"): the filter would then see the real driven level.
- **Park** the outer feedback loop (§6C) and Howl in DRIVEN (§6D): high risk, and the owner's Magneto does tape regen for real; chain it.

**Missing, and the most useful link: throws via DRIVE.** SPLASH round 3 makes DRIVE a real input gain ("how hard the signal hits the tank", like the Wellspring's INPUT). The open question: should the tail get **louder** when DRIVE rises (little or no output compensation, as on a real INPUT knob or a desk send), or stay level-matched as today? If louder, an envelope or sequencer into DRIVE's CV is a **throw**: no new control, gate stays KICK. This matters for the owner's rig: the Worng SoundStage II's FX send carries the whole mix, not single channels, so per-hit throws are hard at the mixer. Decide in SPLASH round 3.

**Order of work (proposed):** (1) SPLASH round 3 with DRIVE as INPUT, including the throw/level question; (2) sweet-tank prototype (running) → fold the owner's pick into the Core with ADRs and re-tuned tests; (3) run 12 on hardware to know the CPU room; (4) Big Knob TONE prototype, 2–3 voicings (after 1–2: it shares the pre-tank spot with the spring EQ and today's low cut); (5) parked: outer loop, Howl in DRIVEN; gate stays KICK.
