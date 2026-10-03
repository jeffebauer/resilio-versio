# Resilio through a dub lens: blind spots, constraints, a unique take

3 Oct 2026, `main` at `96b6207`. Written by Claude at the owner's request. **These are ideas only:** nothing here changes the SPEC, the decision records or the code. Anything we take forward goes the usual way: a prototype, renders, a listening page, your pick, then a decision record.

This builds on [dub-spring-reference.md](../dub-spring-reference.md) (30 Sep), which compared dub techniques with what Resilio could do then. Since then a lot has landed: DRIVE became the INPUT, SPLASH comes from the hit, the Big Knob, bipolar WOBBLE, coupled SPRINGS 3, and the Wellspring F tank. So this document doesn't repeat that inventory. It asks harder questions: what we're missing, which early rules now get in the way, and whether our method is pointed at the right target.

**Tags.**
- **[D]** documented: the engineer's own words, a manual, or a period article.
- **[S]** secondhand: a credible historian or engineer reporting it.
- **[A]** anecdotal: forums, plugin marketing or tutorials.

Sources are listed at the end.

---

## 1. The verdict

The tank itself is in good shape, and its roots are honest. A mono send, a driven input coil, a high-pass before the springs, a kick you can hit, SPLASH that answers the hit, and feedback you can ride and that dies away naturally. All of these trace back to documented practice.

The blind spot is not the spring. **It's everything a dub engineer did with their hands around the spring.** In dub the spring was never a reverb you set and leave. It was one instrument inside a played mixing desk:
- open the send for one hit, then close it;
- cut the dry and let the tail carry;
- feed the return back into itself;
- chain it with tape echo.

Mad Professor calls feedback loops "the basic technique of dub mixing" [D]. Most of our last week went into making the tank's frozen spectrum match the Wellspring's. Almost none went into gestures.

The single biggest hidden constraint is not the CPU. **It's flash memory.** The release firmware is at 96 % of its 128 KB, and the profile build (the one that measures CPU) has 576 bytes left. That one number decides which ideas below can happen without reopening a decision first.

---

## 2. What's already authentic (keep it)

| What we have | Root | Confidence |
|---|---|---|
| Kick (thud and crash) | Tubby hit the spring unit for a "thunderclap"; Barrow's liner notes, repeated by Seddon | [S] |
| Mono tank, stereo out | Fisher, Grampian and Fender spring units are all mono. No source says a 1970s Jamaican spring return was stereo | [S] |
| DRIVE as the input, KICKED grit | Black Ark's Grampian 636 had a germanium input made to overdrive, and was used as a fuzz box (Boyle's reconstruction) | [S] |
| SPLASH aimed at snares and rims | Bovell (1977): "a lot of reverb on the snare… that clicking hollow wooden sound"; never on the kick, "it makes it sound muddy" | [D] |
| The Big Knob (Altec 9069B, 18 dB/oct) | Tubby's MCI desk filter; Scientist: the "phaser" sound at Tubby's was this filter, "No, no phasers. High-pass filter." | [D] filter, [S] desk |
| The Howl you ride, and that dies away naturally (ADR 0018) | Bovell: "I'm limiting the echo with my left hand so it doesn't distort." Feedback is ridden by hand, never cut | [D] |
| WOBBLE's Drift (tape wow and flutter) | Tape echo is everywhere in this lineage: Space Echo at Black Ark [S], for Pole [D] and for Sherwood [D] | [D]/[S] |
| TENSION bends the live tail's pitch | The same gesture as sweeping a tape echo's speed while it repeats | design analogy |

The lowered low end (155 Hz) and the warmer TONE at noon from round F2 also fit Bovell's rule: no bass in the spring.

---

## 3. Blind spots, ranked by musical impact

### 3.1 You can't close the send (the throw)
The most documented dub move is a **throw**: open the spring's send for one snare, one bar or one word, close it, and let the return ring out. Sherwood's first live mix had a spring with "a little tiny effect on the snare and on the lead vocal" [D]. In Bovell's step-by-step, the snare goes into "murderer reverb" [D].

On Resilio today, nothing lets you shut what feeds the springs while the tail keeps going:
- **MIX** fades the tail along with the input.
- **DRIVE at 0** still passes the full signal (0 dB). DRIVE only adds up to about +6 dB of tail across the knob (ADR 0033).
- The 30 Sep brief hoped DRIVE's CV could act as a throw. As built, DRIVE turns the send *up* a bit; it never turns it *off*.

This matters more in your rig than in most. Your SoundStage II's FX send carries the whole mix, so you can't throw a single channel at the mixer.

In a full modular you could put a VCA in front of the input. That's a valid answer, but it uses a VCA and an envelope for the one move the genre is named after.

### 3.2 The filter sits before the springs, but half of the history filters after
Our Big Knob thins what *enters* the tank. Turn TONE right during a ringing tail and the tail changes slowly: the damping inside the Loop moves, but the new low cut only shapes the next hit. The history splits:
- **Tubby swept the filter on sends.** That claim is [A] (plugin copy). What is documented is that he used it on any channel.
- **At Black Ark, the low cut sat on the spring's return.** The springs were "blended back in, almost parallel-compression style", which gave the metallic drum sound [S, Boyle].
- **Dub techno filters the wet signal live.** Examples are a slow band-pass around 450 Hz [A], Springray 2's EQ with CV control [D], and the Wellspring's own 4-mode filter on its wet path [D].

So the move "sweep the Big Knob and the tail you're hearing goes thin and telephone-like" isn't on the panel.

### 3.3 Feedback lives only inside the springs, and only in KICKED
Feedback in dub came from the desk: a return patched back into its own send, ridden on a fader.
- Mad Professor did it with his tape machine's replay head through an aux [D].
- Boyle sends the phaser return to the reverbs and delays "and then back into itself" [S].
- Modular players expect the same thing from the Doepfer A-199's external feedback input and Springray 2's send and return [D]. Each puts a filter, VCA or phaser *inside* the loop.
- Your own Wellspring's "Magic Feedback" loop is this move [D].

Our Howl is a lovely sound, but it is locked to one switch position and the top 10 % of DECAY. You also can't put anything inside the loop.

### 3.4 No echo, in a genre where spring and tape echo travel together
Almost every rig in the research pairs the spring with tape echo:
- Tubby: a homemade tape delay [S].
- Perry: Space Echo [S].
- Sherwood: "a spring reverb, a Space Echo or something" [D].
- Pole: Space Echo is one of his core instruments [D].
- Echospace: Space Echo and Echoplex [A].
- Sylvan Morris built a tape-loop echo with a movable head, to set the delay to each tune's tempo [D].

The previous brief parked echo ("use a rack echo"). That still works. But the *interaction* is the sound: an echo feeding the spring, so each repeat lands in the tank with its own splash. A separate echo before the input only half-does it, because the echo's feedback never passes through the springs.

### 3.5 Long tails pile up under the kick (dub techno)
Dub techno's chord washes stay clear because the wet is **ducked under the kick**:
- Attack Magazine's recipe ducks the reverb bus from the kick [A].
- NE's own Desmodus Versio ducks its infinite tails from the input, and its manual includes a "Kick Ducker" patch [D].

We have a Sustain trim and a limiter, but nothing that makes the tail breathe in time with the music.

### 3.6 Nothing between a 9-second fade and the Howl (hold)
Dub techno breakdowns and siren beds hold a wash while new input is ducked or muted:
- Desmodus Versio has infinite and infinite-with-ducking [D].
- Beads has a freeze button and a gate input [D].

ADR 0001 ruled out freeze because "not a dub spring behaviour" and because of AntiRes. The first reason is true of roots dub and false of dub techno.

### 3.7 Silence vs noise floor (a taste split, not a gap)
- Scientist *avoided* Tubby's Fisher spring because "it was very noisy" [D].
- Echospace built *The Coldest Season* on recorded blizzard static [D], and Deepchord on AM radio static [D].
- The Wellspring has a flat hiss about 37 dB under its tail. That was measured on 30 Sep, and it's part of why it sounds "diffuse with top end".

Ours is digital silence. Either answer is defensible. Right now nobody has chosen; it simply never got built.

---

## 4. Early decisions worth reopening

| Decision | What it costs musically now | Recommendation |
|---|---|---|
| **ADR 0011: firmware lives in the 128 KB internal flash** (to use NE's Firmware Swap app) | **The real ceiling.** Release 96 %, profile build 576 B free. Every idea in §3 needs code, and the profile build has to hold it too to be measured. The ADR itself named "-Os on non-audio code" and then "BOOT_SRAM" as the ways out. | **Reopen first, before choosing features.** Find out what's left after -Os and trimming, and whether a bootloader build can still go through Firmware Swap. It decides how big our next step can be. |
| **ADR 0009 → 0038: the Wellspring as the target** | The Wellspring is a modern boutique unit with a lovely tank, but we fit its *spring-only* path. Its own character comes from delay into springs, Magic Feedback and a filter on the wet. All three are missing from us (§3.2–3.4). Seven fitting rounds went into a frozen spectrum. The documented dub tanks were cheap and noisy: a modified Fisher, a germanium Grampian, a home-built spring "from Practical Electronics" (Mad Professor [D]). | **Keep it as the tank's timbre reference, and stop fitting after round 5.** Matching it closer has diminishing returns. Use the remaining effort on gestures (§5). |
| **Gate = Kick only (ADR 0005, 0013; the 30 Sep brief's "gate stays KICK")** | The only rhythmic input is spent on the one move that's [S] for Tubby and not documented as rhythmic anywhere (no source has anyone kicking the tank *in time*). The throw, which *is* documented (§3.1), has no input. | **Reopen.** See direction A. |
| **ADR 0002: Howl only in KICKED, top 10 % of DECAY** | Feedback is tied to "chaos", but in dub it's a mixing move at any level of grit (§3.3). | **Reopen as part of direction C,** not by itself. |
| **ADR 0001: DECAY always fades** | It loses dub techno's hold. The AntiRes worry was real for a loop gain of exactly 1. A hold that *ducks* the input and lets the loop's saturator bound it is the same mechanism our Howl already uses safely. | **Ask by ear** (§8). Low cost if we keep it CLEAN/DRIVEN-only and near-infinite rather than truly infinite. |
| **Input summed to mono (SPEC §4.3)** | Correct for roots dub. It's also why stereo in is a question at all (see §7). | **Keep the tank mono.** Spend the second input on a role. |
| **ADR 0005: Kick has a fixed strength** | Small. The gate is on/off, but at the moment of a Kick the firmware could read SPLASH's CV (or any CV) as a velocity. | **Optional,** if the Kick stays on the gate. |
| **ADR 0004: the plugin is only a test bench** | The plugin could become the place for what the Versio can't fit (MIDI-synced echo, sidechain). Not urgent. | Keep for v1. |
| **ADR 0030: the 70 % CPU budget** | Not the binding constraint yet (run 15 pending). The F tank used some of the 7 points. | Read run 15 before costing anything below. |

---

## 5. The method: fitting a frozen tank vs judging a played one

Our loop has been: record a reference, measure its spectrum, decay and stereo, fit, render, A/B by ear. It gave us a much better tank. Its blind side:
- **Every stimulus is a fixed input into fixed settings.** Dub's character is in what changes *during* a tail: a throw, a cut, a filter sweep, a feedback swell, a TENSION swoop.
- **The references are the unit alone.** In dub the spring is heard inside a mix, against a dry drum that's just dropped out. Forum complaints about digital springs agree on this. Algorithmic springs can sound fine solo but never "really sound like a spring once in a busy mix" [A].

**Proposed addition: gesture stimuli.** The Renderer already does parameter automation (`host/render/main.cpp`, `Automation.h`), so these need no new tooling:
1. **Throw.** Skank and rim loop; the send opens for beat 4 of bar 2 only, then closes. Until we have a send control, fake it in the stimulus by muting the input.
2. **Drop.** Full mix; the dry cuts at bar 3 and the tail carries alone for two bars.
3. **Sweep on a ringing tail.** One snare, then TONE swept from noon to fully right over 1.5 s, while the tail rings.
4. **Feedback ride.** DECAY automated into the Howl zone and back over 8 s, in KICKED.
5. **Swoop.** TENSION swept while a tail rings, at the speed of a hand on a Space Echo's rate knob.
6. **In the mix.** Every page also plays our wet *under* a dry drum loop at a dub-ish balance, not solo.

Judge these by ear against the records, not against the Wellspring. Bring a short playlist: a Tubby dub, a Scientist dub, a Black Ark side, a Rhythm & Sound track and a Deepchord track.

---

## 6. A unique take with real roots: four directions

Each direction has a one-sentence idea, its root, where it would live on the panel, its costs, and a cheap first test. CPU and flash costs are estimates.

### A. "The Send": the gate throws, the button kicks
- **Idea:** the gate opens the springs' input while high (and SPLASH still picks out the hits), so a sequencer or envelope decides which hits get drenched. The tail always rings on.
- **Root:** the throw; the most documented dub move (§3.1).
- **Panel:** the gate becomes THROW; the button stays KICK.
  - The 30 Sep brief's objection: unpatched, the gate reads "off", so the reverb would go silent. Fix: throw mode turns on at the gate's first rising edge and stays on until power-off (or a long button hold). Unpatched, it behaves as today.
  - Losing gate Kicks is the price. A long button press could be a "Kick while held" in their place (ADR 0013's idea).
- **CPU:** about free (one smoothed gain). **Flash:** small.
- **First test:** gesture stimulus 1 with a gated input, in the Renderer, against today's sound.

### B. "Return Knob": the Big Knob moves to the tank's output (or is split)
- **Idea:** turning TONE right thins the tail you're hearing right now, as at Black Ark and in dub techno. It keeps its pre-tank half so hits still go into the springs thin.
- **Root:** Boyle's low cut on the return [S]; dub techno's filter on the wet [A/D]; the Wellspring's wet filter [D].
- **Panel:** no new control. It changes where TONE's right half acts. Three voicings to compare by ear: pre (today), post, or split (pre for new hits, post for the tail).
- **CPU:** low (the same filter, now in stereo after the tank: ~2× a mono filter). **Flash:** small.
- **First test:** gesture stimulus 3 on all three voicings.
- **Risk:** ADR 0036's level makeup assumed the cut was before the tank; a post-tank cut needs its own.

### C. "The Desk Loop": In R becomes the spring's feedback return
- **Idea:** patch Out R through anything (a filter, a phaser, Magneto, a VCA on an envelope) and back into In R. That signal goes back *into the springs*. The tank becomes one block in a patchable dub desk, and feedback becomes something you play with external modules.
- **Root:** Mad Professor's desk feedback [D]; Boyle's "back into itself" [S]; the A-199's external feedback input and Springray 2's send/return [D]; the Wellspring's Magic Feedback [D].
- **Panel:** no knob. The return's level is set by the patch (an external VCA or attenuator), as on the A-199.
  - **Hard part:** the Versio's hardware copies In L to In R when R is unplugged, and the firmware can't see whether a jack is in. A detector that notices "R is just L" (their difference far below L) would treat it as unpatched. It must be robust.
  - A safety limiter inside the return path keeps runaway rideable, like the Howl.
- **CPU:** low (one more input, one limiter). **Flash:** small to moderate.
- **First test:** plugin only. A sidechain input as the return, patched in Ableton through Auto Filter and back, to hear whether it's thrilling before touching the firmware.
- **Why it's the unique take:** no digital spring we found offers this (Springray and A-199 do, with real tanks). Combined with Kick, SPLASH and TENSION, it would be a dub desk in 1U of thinking.

### D. "Echo Chamber": SPRINGS position 3 becomes tape echo into the springs
- **Idea:** position 3 is the open design question in TASKS ("what should make you reach for 3 Springs?"). Answer it with what every dub rig had: a tape echo feeding a 2-spring tank, whose repeats each splash into the springs and come back through WOBBLE's Drift.
- **Root:** §3.4: Tubby, Perry, Sherwood, Pole, Echospace; Morris's tempo-matched tape loop [D].
- **Panel:** the hard part is the echo time.
  - Option 1: TENSION sets the echo time in position 3 (it already means "length of the tank").
  - Option 2: the button taps tempo in position 3.
  - Echo feedback could come from DECAY's top half.
  - Both options overload controls. Be honest: this is the biggest panel change of the four.
- **CPU:** moderate. One long delay in SDRAM, a filter and a saturator, offset by running 2 Springs instead of 3. **Flash:** moderate; needs §4's flash question answered first.
- **First test:** pure Renderer. Put a simple tape echo in front of today's SPRINGS 2 in a prototype script, at three echo times, on skank and rim.

**My ranking** (musical impact against effort):
1. **A** (biggest gap, nearly free).
2. **B** (an ear test away, no new control).
3. **C** (the most distinctive; plugin test first).
4. **D** (the biggest payoff and the biggest change; depends on flash).

Ducking (§3.5) and hold (§3.6) fit inside A and C: a throw that closes can also duck, and a hold is a throw that closes with DECAY near the top.

---

## 7. Stereo in, through this lens

SPEC §10 asks: should the tank keep left/right placement from a stereo input? Through the dub lens: **probably not.**
- No source has a stereo spring return in classic dub. The tanks were mono, and so were the sends.
- Our tank's stereo already comes from inside (ADR 0038's decorrelated stereo).
- Spending In R on L/R placement buys a little realism for dub-techno pads and costs ~300–700 cycles per sample (the full dual-input option).

**The better use of In R is a second *role*:**
- **Feedback return** (direction C): the most distinctive.
- **Duck key:** patch the kick in and the wet breathes under it (§3.5). Cheap: one envelope follower.
- **Second send:** a separate source into the springs only, e.g. the snare channel only, while In L is the whole SoundStage send. That's a throw at the mixer without the mixer needing per-channel sends.

All three share the normalling problem (In R copies In L when unplugged). A "Return mode" that you switch on (e.g. at boot, by holding the button) is the simple, safe answer, and doesn't need a detector.

---

## 8. Questions for you (answer by ear where you can)

1. **The throw.** Should the gate decide *which hits go into the springs* (and lose sequenced Kicks), or stay the Kick? Would a long button press giving the Kick back be enough?
2. **Filter timing.** When you turn TONE right during a ringing tail, should the tail thin out *now* (filter on the return), or only the *next* hit (filter on the send, today), or both?
3. **Feedback.** Do you want to put things *inside* the spring's feedback (a filter, a phaser, Magneto) by patching Out R → something → In R? Or should feedback stay the KICKED Howl?
4. **The third switch position.** Would "tape echo into two springs" be what makes you reach for position 3? Or would you rather it stay springs, and run Magneto in front?
5. **Hold.** At the very top of DECAY in CLEAN/DRIVEN, should the tail *hold* (hiss away under new hits, ducked) for dub techno breakdowns, or always fade like a real tank?
6. **Hiss.** Should the tail fade into a faint tape hiss like your Wellspring's, or into silence like Scientist preferred?
7. **Method.** OK to stop chasing the Wellspring after round 5 and judge the next rounds with gesture renders played *inside a dry drum loop*, next to records?

---

### Owner's answers (3 Oct 2026)
1. **The throw:** the gate throws, the button kicks (direction A). Unpatched behaves as today.
2. **Filter timing:** hear all three first (pre / post / split on a sweep over a ringing tail).
3. **Feedback:** no. Keep the KICKED Howl; In R is not the feedback return (direction C parked).
4. **SPRINGS 3:** prototype tape echo into two springs in the Renderer, decide by ear.
5. **Hold:** yes. Near-infinite tail at the top of DECAY in CLEAN and DRIVEN, ducked under new hits (reopens ADR 0001 for those modes; KICKED keeps the Howl).
6. **Hiss:** hear a few levels first.
7. **Method:** Wellspring round 5 is the last fit; after it, rounds are judged with gesture renders inside a dry drum loop, next to records.

Still open: what In R is for, now that it isn't the feedback return (a duck key, a second send, or plain stereo).

### Listening picks (4 Oct 2026)
- **TONE placement: B, after the springs.** The Big Knob moves to the tank's return, so turning TONE right thins the ringing tail at once (branch `proto/tone-place`, `7dc535c`; `docs/prototypes/tone-place/`). Costs: ~+4 % desktop CPU, ~3 KB flash if it became the default. The Kick's thump is thinned with the wet too.
- **SPRINGS 3: B, tape echo into the springs** (each repeat splashes into the tank; branch `proto/echo-springs`, `c2fb5b3`). Owner, if it ships: more control over the delay. In position 3, **DECAY becomes the echo feedback, TENSION the echo time, and the gate a clock input**, with TENSION then picking straight and dotted divisions/multiplications of the clock.
  - **Echo mode design (owner, 4 Oct):** the gate is the echo's clock in position 3 only (positions 1–2 keep the throw; in echo mode the echo itself is the throw); unclocked, TENSION sets a free time. The springs behind the echo are fixed: today's noon tank with one classic medium tail (~1.5–2 s, like a Space Echo's built-in spring). A new division or time **swoops like tape** (~0.3 s, the repeats bend in pitch). Still to settle: the division set (which straight and dotted steps, and whether one gate pulse means a quarter note or the echo time itself), and the echo's longest time (memory: SDRAM).
- **Hiss: inaudible at every level.** Measured on the sparse-rims renders, the floor sits 84 / 70 / 63 dB under the hits' peaks (faint / Wellspring-like / audible). The Wellspring's "−37 dB" was a narrow-band figure (2–4 kHz floor against the tail's mids), not what the ear hears. So the hiss isn't what made the Wellspring sound diffuse, and a hiss at a realistic level isn't worth its flash (branch `worktree-agent-a294f20caa9a6d3da`, `30bbe94`, kept for reference).

## 9. Corrections to our own docs (found by this research)

- **ADR 0036, Context:** "He swept it on the reverb and echo sends" is only plugin and gear copy [A]. What's documented is that Tubby's filter worked on any channel. The Altec model and slope are well supported. Scientist adds that the "phaser" people hear at Tubby's was this filter [D]. The decision doesn't change; only the confidence of that sentence.
- **dub-spring-reference.md §3.A.4:** the tank kick is [S] (Barrow's liner notes), not first-person. *Rhythmic* tank-kicking is undocumented anywhere.
- **Tubby's spring:** Jammy says a modified Fisher [D, interview quote in Seddon]. Scientist says there was a Fisher but he avoided it as too noisy [D]. "Fairchild" is a plugin maker's claim [A].
- **The kick drum in the spring:** two schools. Bovell, never [D]. The Tubby "one-drop boom" through the spring is forum lore [A].

---

## Sources

Roots and UK:
- Scientist, Tape Op: https://tapeop.com/interviews/136/hopeton-overton-brown-scientist
- Scientist, Reggaeville: https://reggaeville.com/artist-details/king-tubby/news/view/interview-scientist-answers-back-part-i
- Prince Jammy, quoted in Seddon, "King Tubby's Reign": https://www.uvm.edu/~debate/dreadlibrary/seddon.html
- David Katz on King Tubby, The Quietus: https://thequietus.com/?p=270219
- Steve Barrow, *Dub Gone Crazy* notes: https://niceup.com/articles/dub_gone_crazy
- Daniel Boyle on Perry, Sound on Sound: https://www.soundonsound.com/people/lee-scratch-perry-daniel-boyle-recording-back-controls
- Grampian 636, Soundgas: https://soundgas.com/blog/grampian-type-636-history-technical-spec/
- Sylvan Morris, Reggaeville: https://www.reggaeville.com/artist-details/sylvan-morris/news/view/interview-sylvan-morris-at-studio-1/
- Adrian Sherwood, Tape Op: https://tapeop.com/interviews/136/adrian-sherwood
- Adrian Sherwood, The Tonearm: https://www.thetonearm.com/everything-is-everything-adrian-sherwood-dub-resistance/
- Mad Professor, Sound on Sound: https://www.soundonsound.com/people/mad-professor
- Dennis Bovell, *Sounds* 1977: https://insheepsclothinghifi.com/dennis-bovel-in-dub-guide/
- Altec 9069B / KTBK, Sound on Sound: https://www.soundonsound.com/news/audio-merge-ktbk-passive-filter

Dub techno and modular:
- Moritz von Oswald, RBMA lecture: https://www.redbullmusicacademy.com/lectures/moritz-von-oswald-early-morning-freestyles
- Pole, 15 Questions: https://15questions.net/interview/fifteen-questions-interview-pole-stefan-betke/
- Echospace, Headphone Commute: https://headphonecommute.com/2010/07/17/two-and-a-half-questions-with-echospace/
- Deepchord, Vice: https://vice.com/en/article/deepchord-interview-unsound-brooklyn
- Attack Magazine, Basic Channel-style dub techno [A]: https://www.attackmagazine.com/technique/beat-dissected/basic-channel-style-dub-techno/
- Desmodus Versio manual: https://noiseengineering.us/manuals/desmodus-versio/
- Doepfer A-199 manual: https://www.analoguehaven.com/doepfer/a-199/manual.pdf
- Intellijel Springray 2: https://intellijel.com/shop/eurorack/springray-2/
- Wellspring, Tape Op review: https://tapeop.com/reviews/gear/155/wellspring-stereo-spring-reverb
- Mutable Instruments Beads manual: https://pichenette.readthedocs.io/en/latest/modules/beads/manual/
- Channel One, Vice: https://www.vice.com/en/article/brooklyns-dub-stuy/
- Gearspace threads on digital springs [A]: https://gearspace.com/threads/virtual-spring-reverb-options.483475/

Repo: `SPEC.md` §3–5 and §10, ADRs 0001, 0002, 0004, 0005, 0009, 0011, 0013, 0030, 0033, 0036, 0037, 0038, `docs/m8-tuning-backlog.md` ("Sonic signature", "Send-level calibration"), `docs/handoff/HANDOFF.md`.
