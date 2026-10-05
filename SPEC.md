# RESILIO VERSIO — Firmware Spec

**Name:** Resilio Versio (Latin *resilio*, "I leap back, rebound"). Firmware target name `resilio_versio`.
**Target:** Noise Engineering Versio platform (Electro-Smith Daisy Seed inside)
**Goal:** Dub-flavoured spring reverb. Priority sound = splashy, drippy tank ring-out on a single snare/rim hit, including "kicked tank" chaos.
**Status:** Spec **v1.0 (frozen)**, 27 Sep 2026. Vocabulary: `CONTEXT.md`. Decisions: `docs/adr/` (0001–0042). Changes after freeze: new ADR + changelog entry. Tuned numbers replace "starting guesses" as milestones confirm them.

### Changelog
- v1.0.36 — ADR 0042 amendment "Placement: wet only, before TONE" (owner, 5 Oct 2026, playing the release on the module: "In KICKED mode, the aliasing is too present applied to both the input and output … let's move it back so it only affects the wet signal, pre-tone so I can filter out the higher aliasing artifacts if desired"): the µ-law box (DRIVEN 12-bit, KICKED 10-bit) moves from after MIX (dry and wet) to the wet only, after the pickups and the output shelf, before TONE's return filter, then the limiter, the Hold's ducking and MIX. The dry is untouched again: MIX fully left is a clean passthrough in every ATTITUDE. TONE right of noon now thins the box's grit with the rest of the wet. The box's full scale is +8 dB over 0 dBFS (the wet before the limiter runs hotter than 0 dBFS). §3 MIX and ATTITUDE rows, §4.8, §7. (v1.0.35 is reserved for Wellspring round 5.)
- v1.0.34 — ADR 0041 and ADR 0042 amendments (owner, 5 Oct 2026, playing the Plugin): in SPRINGS 3 echo mode DECAY's top is the same in every ATTITUDE, persistent repeats at a roughly constant level held by the tape's saturation (feedback 1.16 at DECAY 1: what KICKED's DECAY 0.92 gave), never a runaway. KICKED's curve keeps its shape with the lower top (it used to rise to 1.25 and run away from DECAY ~0.87); CLEAN and DRIVEN rise to it from DECAY 0.85 (bit for bit as before below). Repeats stop fading from DECAY ~0.90 (KICKED) / ~0.94 (CLEAN, DRIVEN). KICKED's µ-law box is 24 kHz / 10-bit (was 8-bit: grain ~45 dB under the signal instead of ~33; DRIVEN's 12-bit ~57). §3 DECAY row, echo mode, ATTITUDE row, §4.8.
- v1.0.33 — ADR 0042 (owner, 4 Oct 2026, B on every panel of `renders/feat_output_mulaw`): in DRIVEN and KICKED the whole output, dry and wet, goes through a µ-law box after MIX: DRIVEN 24 kHz / 12-bit µ-law, KICKED 24 kHz / 8-bit µ-law, every SPRINGS position; CLEAN untouched. MIX fully left is a clean passthrough only in CLEAN. The box adds ~0.12 ms in DRIVEN/KICKED (an ATTITUDE flip crossfades it over 20 ms), ends fading tails in exact silence, and takes off the top octave above ~11 kHz. §3 MIX and ATTITUDE rows, §4.8, §7 M7.
- v1.0.32 — ADR 0030 amendment (owner, 4 Oct 2026): CPU target raised to **≤ 75 % peak** worst case, with an **80 % ceiling** for a release that passes its click check (run 15 measured 87.6 % peak and passed). §5. (v1.0.29–31 are reserved by the TONE-after, Throw/Hold and echo-mode branches, merging next.)
- v1.0.31 — ADR 0041 (owner, 4 Oct 2026): SPRINGS position 3 is **echo mode**: a tape echo into the springs (each repeat splashes into the tank, darker each pass). In position 3 DECAY is the echo's feedback (0 = one repeat; KICKED's top runs away and dies down with DECAY), TENSION its time (2 s → 80 ms, 0.4 s at noon) or, with a clock in the gate (one pulse = a quarter note), one of seven divisions 1/2 … 1/16 incl. dotted; time changes swoop like tape. The springs behind it are fixed (the noon tank, ~1.7 s tail). The gate kicks only in positions 1–2; the plugin follows the DAW's tempo. Spring C no longer runs (heard nowhere); the coupled Springs (ADR 0037) stay a Renderer reference. §3 SW0, P2, P5, Button + Gate; §4.3; §5.
- v1.0.30 — ADR 0039 (owner, dub-lens critique §8): the **gate throws** (in SPRINGS 1–2; in 3 it is the echo's clock, v1.0.31): with a gate patched, the Springs' send is open only while it is high (opens 2 ms, closes 15 ms; MIX and the ringing tail untouched; the Kick never gated); unpatched nothing changes (the throw switches on at the gate's first rise); **holding KICK 1 s leaves throw mode** (send open again, all four LEDs blink white once; the next rise switches it back on). The button stays KICK. Plugin: an automatable THROW switch. ADR 0040: the **Hold**: CLEAN and DRIVEN, top ~10 % of DECAY, the tail glides out toward minutes (~3 dB per 10 s at max, always under unity gain), new sound still goes in 6 dB down ("layer", the owner's pick; "freeze" stays a Renderer voicing), and the whole bed dips 12 dB under the input's kick and bass only (the ducking listens below ~120 Hz, so snares, hats and chords don't trigger it), a short dip with no swell back into the next beat. The Hold arms when DECAY enters its top range outside KICKED, so leaving the Howl by ATTITUDE fades as before (owner's pick). KICKED's Howl unchanged. In echo mode (SPRINGS 3, v1.0.31) DECAY is the echo's feedback and the springs run a fixed tank, so there is no Hold there. §3 DECAY, Button + Gate, §4.4, §4.6, §6.1, §6.3.
- v1.0.29 — ADR 0036 amendment "Placement: after the springs" (owner, 4 Oct 2026, by ear on the placement prototype): the Big Knob (TONE right of noon: the 18 dB/oct low cut, 20 Hz at noon → 800 Hz fully right, the coil bump on sharp hits) moves from in front of the Springs to the wet return, after the pickups and shelf, before the limiter and MIX (Black Ark's low cut on the return). Turning TONE right now thins the tail already ringing at once, and turning back gives its body back; a slow makeup of its own (¾ back, ≤ 12 dB, over ~0.3 s) keeps the level. The Kick's direct thump is thinned with the wet. Left of noon and noon unchanged. §3 P3, §4.2, §4.8.
- v1.0.28 — ADR 0038 / 0037 "Round F2" (owner, 3 Oct 2026): a little more low end at every TONE left of the Big Knob's own cut (the low cut in front of the Springs eased from 220 Hz / −3 dB to 155 Hz / −2 dB, level kept; held sounds trimmed a little sooner so they meet the limiter no harder), and SPRINGS position 3 is three coupled wire gauges (crisp short boing left, lower longer one right, high one in the centre, sharing energy every trip; repeat timing unchanged). §4.3, §4.8.
- v1.0.27 — ADR 0038 (owner, 2 Oct 2026, "F, plus gentler"): the tank is the Wellspring fit, voicing 7. A shared Sweep in front of the Springs (every echo the same smooth pew), short diffusers on each Loop's feedback (repeats blur into a wash), the transducers (a resonant low-pass where the coil drives the springs and where the pickups hear them, with the coil's even-order colour: gentle highs from the first moment, repeats that darken slowly), the stereo from decorrelated mid and Spring difference (wide, no left-right flicker), a low cut in front of the Springs with a level makeup. TONE re-mapped: fully left about as dark as before, noon the fitted sound, right of noon the Big Knob. DRIVE still grows the tail ~+6 dB; SPLASH at least as strong as before relative to SPLASH 0. §3 P3, §4.2, §4.3, §4.8.
- v1.0.26 — ADR 0037 (owner, 2 Oct 2026): SPRINGS position 3 is coupled. The three Springs share energy every round trip (an energy-preserving rotation of their Loop returns, let go in the Howl zone), so a hit's echoes multiply and bloom instead of dripping. Repeat timing, tail length and level as before; positions 1 and 2 unchanged. §3 SW0, §4.3.
- v1.0.25 — ADR 0032 amendment (owner, 2 Oct 2026): SPLASH stronger. The top quarter of SPLASH is much bigger (up to ×3 the Clang, held a little longer), and SPLASH no longer depends on DRIVE: every hit is judged as at DRIVE 0.8, so SPLASH works fully with DRIVE down (line-level sends). A ceiling keeps a big splash off the output limiter, never below the old splash. SPLASH 0 unchanged.
- v1.0.24 — ADR 0036 (owner, 2 Oct 2026): TONE's right side is King Tubby's Big Knob (Altec 9069B): an 18 dB/oct low cut sweeping 20 Hz at noon → 800 Hz fully right (was 12 dB/oct, → 300 Hz), with the coil's nasal bump above the cutoff on sharp hits only (chords, pads and held sounds get the plain cut); level kept within ±3 dB across TONE. Left of noon unchanged. §3 TONE row.
- v1.0.23 — ADR 0035 (owner, 1 Oct 2026): the Sustain trim, gentle voicing (round 3, owner's pick D). A safety net on held sounds (pads, drones, organs; never hits or stabs, which stay bit for bit as before): only when the tank's build-up would push the output limiter hard, the Springs' input is eased down (at most 5 dB, slowly), so held sounds stay within ~0–2 dB of their old level and the limiter takes only short pulls. The output limiter's envelope holds 30 ms before releasing (§4.8), so light limiting no longer rides each bass cycle (heard as drive).
- v1.0.22 — ADR 0034 (prototype, owner's idea): WOBBLE is bipolar. Noon still (±3 % dead zone); left = Drift, smooth random wow + flutter that never repeats; right = Warble, a sine LFO. Every knob step audible; Springs share the Drift at low amounts. Supersedes ADR 0008's one-way zones (§3 P6, §4.7). Round 2 (owner, 1 Oct): right side a vibrato (1.5 → 5.5 Hz), both end stops toned down (B −25 %, C −45 %; middles kept), a flutter tremolo (≤ 1 dB) on the left, the flutter's speed following the wow; A / B / C compared by ear, B the build default meanwhile.
- v1.0.21 — ADR 0032 (owner, SPLASH round 4): SPLASH comes from the hit (the Clang: its highs fed harder into the springs, every ATTITUDE; the Bite: short hits pushed harder into DriveIn, DRIVEN/KICKED), no noise burst on hits (the Clatter is the Kick's crash). ADR 0033: DRIVE is the INPUT (one gain, 0 → +24 dB, heard by the Splash first; +6 dB louder tail at DRIVE 1; no DRIVE push on the LoopSat). §3 SPLASH and DRIVE rows, §4.5, §4.9 updated. Tight-tank LoopSat quiet-tail fade (AntiRes, floor −30 dB). First-hit fix: the excitation trim starts turned down. Owner's first listen: gentler Bite (×(1 + 2.5e), three quarters taken back), DRIVEN driven less (midway between CLEAN and KICKED), KICKED's Clang ×12.
- v1.0.20 — ADR 0030 amendment (owner): CPU target raised from ≤ 65 % to **≤ 70 % peak** worst case, with an on-module click/dropout check for every release. Run 12 measured 63.3 % peak, so ~7 points are available for new sound (SPLASH/DRIVE build, tank changes).
- v1.0.19 — Docs only: §3 DECAY range now matches §4.4, ADR 0026 and the code (T60 0.4 → 9 s; §3 still said ~0.3–0.5 → ~8–10 s, the pre-tuning guess). M1's 0.3–0.5 s / 8–10 s acceptance windows are unchanged. The same note's "tight slap … TENSION low" predated ADR 0026's flip: now TENSION up.
- v1.0.18 — ADR 0031 (owner): the LEDs are level meters, as on NE's own Versio firmware. Left pair In L / In R, right pair Out L / Out R; brightness follows level (dB), green → amber when hot, red = input near clip / output limiter pulling down. Replaces the input-clip / tank-energy / mode-colour plan (§3, §7 M9).
- v1.0.17 — ADR 0030: how the Versio's CPU budget is met (DTCM pool, no fused multiply-add, pipelined Chirp sections, Springs redesigned in turn with the Jolt kept together, reciprocal saturators). Block 48 kept; idle Springs keep running. Worst case 83 → ~63 % average.
- v1.0.16 — ADR 0029 (owner): earlier first echo. Loop pickup 0.52 → 0.36 L, high path gets its own pickup at 0.70 L_hf; echo spacing unchanged. Loose tank's first sound 45 → 32 ms (Wellspring 32 ms).
- v1.0.15 — ADR 0028: knobs follow the owner's printed Versio panel (§3 now by pot P1–P7: MIX, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE); M0 found libDaisy's knob indexes aren't in panel order. Release output corrects the Versio's polarity flip and +1.2 dB (M0).
- v1.0.14 — ADR 0027: the Springs also differ in damping and tail length (§4.3); fixes mid-DECAY resonances between Springs after TENSION.
- v1.0.13 — ADR 0026 amended (owner, by ear): TENSION turns up = tighter (CW tight, CCW loose); §3 and §4.4 flipped.
- v1.0.12 — ADR 0026 implemented: K2 TENSION (§3), DECAY = T60 only (§3, §4.4 rewritten), M1/M4/M8/§5 criteria reworded from BOING to TENSION.
- v1.0.11 — ADR 0026: TENSION replaces BOING ("which tank"), DECAY becomes tail length only; supersedes ADR 0012; implementation staged.
- v1.0.10 — ADR 0025: CLEAN gets a real, gentler splash (light Clatter + tiny Jolt); §4.5 table updated.
- v1.0.9 — ADR 0024: chirp direction is highs-later (owner by ear + IR library); §2.1, §7 and CONTEXT wording corrected.
- v1.0.8 — §10: stereo-in recorded as an open question for after M3 (incl. plugin-only as a Core mode). Recipe: optional single-tank takes A-L / A-R.
- v1.0.7 — M6: ADR 0023 (Ringing judged by calibrated `ringing_db` < 15 dB, replacing the 12 dB peak test); ADR 0019 floor band 200 Hz–2 kHz; Micro-mod floor set to ±0.05 % of L at ~0.2 Hz.
- v1.0.6 — ADR 0022: DRIVE retune targets after owner listening (obvious from noon, cranked tape/tank at max, level constant); M5 criteria extended.
- v1.0.5 — ADR 0021: Ableton spring IRs as an IR library for range calibration; `tools/ir_analysis.py`.
- v1.0.4 — M4 mono-safe criterion made precise: fold-down energy vs stereo energy (the old wording conflicted with the width criterion).
- v1.0.3 — ADR 0020: Strymon Magneto recorded as a benchmark (not a target) + WOBBLE/DRIVEN calibration source. `08_held_tones` stimulus.
- v1.0.2 — Fact update: Wellspring manual read. It has no spring decay control (fixed T60, one reference point), stereo springs, INPUT = drive. ADR 0009 amended, recipe rewritten, M1/M5 reference checks clarified.
- v1.0.1 — Fact update only: toolchain verified (§8.1), firmware size watch item.
- v1.0 — Frozen. Milestone acceptance criteria from owner interview (§7). ADR 0019 (Howl may lean to pitch). LED_3 Kick flash dropped. §12 complete.
- v0.4 — Grill round 2: ADRs 0006–0018 (min DECAY slap, BOING always spring, WOBBLE zones, Wellspring reference recordings, AntiRes rescoped after Wellspring correction, NE-app flashing, DECAY bend, hold-rattle deferred, DRIVE onset, smoothing tiers, Kick character, TONE range, Howl exit). Tank-level vs Spring-level stages clarified (§4.2). Flashing research (§8). Toolchain facts.
- v0.3 — Grill round 1: ADRs 0001–0005 (DECAY fades, KICKED Howl, switch-change behaviour, plugin = test bench, fixed Kick strength). Added CONTEXT.md glossary.
- v0.2 — Added forum research + design principles (§2), TONE reworked as tilt "hero" control, multi-stage DRIVE voicing (§4.9), anti-resonance / anti-buildup system (§4.10), three-host architecture with shared parameter layer + JUCE plugin (§6), revised milestones (§7).
- v0.1 — Initial spec. Corrected knob count to 7.

---

## 1. Hardware facts (verified 27 Sep 2026)

| Item | Detail | Source |
|---|---|---|
| MCU | STM32H750 Cortex-M7, 480 MHz (boost), 400 MHz without | docs.daisy.audio/hardware/Seed, NIME 2021 Oopsy paper |
| Memory | 64 MB SDRAM, 8 MB QSPI flash | same |
| Knobs | **7** knobs (`KNOB_0`…`KNOB_6`), each paired with a CV jack | libDaisy `src/daisy_versio.h` |
| Switches | 2 × 3-position toggles (`SW_0`, `SW_1`, type `Switch3`) | same |
| Button | 1 momentary (`tap`) | same |
| Gate in | 1 (`gate`), triggers above ~+2 V | same; NE manuals |
| LEDs | 4 × RGB (`LED_0`…`LED_3`) | same |
| Audio | Stereo in / stereo out | NE Desmodus Versio manual |
| CV | 0–5 V; pots act as offsets summed with CV → firmware reads a single 0–1 value per knob | NE Ampla/Electus manuals |

HAL: `daisy::DaisyVersio` in libDaisy. DSP helpers: DaisySP.

---

## 2. Sound target

### 2.1 Core characteristics

1. **Chirp / "boing"** — dispersive low-frequency chirps repeating at the tank round-trip time. **High frequencies arrive after lows** (each echo sweeps up), as in every measured real tank. *(Corrected 29 Sep 2026, ADR 0024: v1.0 said the opposite, written from memory.)*
2. **Splash** — dense, bright, noisy wash on hard transients. Real-world cause: springs driven hard, clattering against each other and the housing.
3. **Drip / kick** — the dub move: physically hitting the tank → huge low thump + chaotic crash.
4. **Dark, dampened tail** — dub spring is rarely bright in the tail; HF rolls off fast.
5. **Warm, driven colour** — tape/transducer saturation, not clean digital.

Reference listening (for tuning, not sampling): King Tubby / Lee Perry-era dub mixes, Fender-style amp spring tanks, Roland RE-201 spring section, Basic Channel-style dub techno.

### 2.2 Community research (forums, Sep 2026)

What players value in classic dub springs:
- Descriptors: **"drippy," "liquid," "splashy."** Short tanks prized when drippy.
- Spring ≠ room reverb. It has its own colour; users want that colour, not realism.
- **Transient interaction is the magic** — sudden stabs bring out character in tails. Percussive hits + springs = core use.
- **Banging the tank for "thunder"** at musical moments is the most-cited dub technique ("instant King Tubbyism") → validates KICK as a core control.

Intellijel Springray / Springray² (real-tank Eurorack module) — community feedback used as design contrast:
- Often needs driving very hot (Drive ≥ 3 o'clock) before the spring is audible.
- Common complaint: reverb feels "not there," then tips into uncontrollable feedback → **narrow usable range**.
- Most-praised feature: **tilt / parametric EQ** — strongly affects how present the reverb sits.
- Voltage control of parameters valued over passive modules (e.g. Doepfer A-199).

Owner's own hardware (Wellspring, Teaching Machines: desktop stereo BBD delay + stereo spring reverb, two tanks each with a pair of 15" springs; the only spring control is SPRINGS dry/wet, with no decay control; INPUT drives it into distortion):
- *Correction (v0.4):* the sine-like **Ringing** the owner hears on the Wellspring comes from the **BBD delay's feedback network**, not the spring. The spring alone has not shown frequency buildup.
- Still relevant: each simulated Spring is itself a delay line with feedback, the same structure that rings in the delay. Digital loops don't have analog noise and drift to break modes up, so the risk is real but unproven for this model. → Designed out by construction and measured automatically (§4.10, ADR 0010).

### 2.3 Design principles (derived)

1. **Wide sweet spot.** Every knob position should sound usable. No dead zone, no cliff edge into runaway feedback.
2. **Characterful drive, not a fight.** Drive adds colour and splash with automatic level compensation; never needed just to make the reverb audible.
3. **TONE is a hero control.** Powerful tilt, not a subtle damping filter.
4. **No ringing single tones.** Tail stays spring-textured at all settings, without spending TONE to fix it.
5. **Transients are the instrument.** Hits should visibly change behaviour (splash, jolt, kick).
6. **Everything CV-able** (hardware gives this for free on all 7 knobs).

---

## 3. Panel map

### Knobs (all CV-able, 0–5 V + pot offset)

Pots P1–P7 in reading order (top to bottom, left to right; drawing: `docs/panel/versio_panel_current_mapping.svg`). The layout follows the owner's printed Versio panel until a custom panel exists (ADR 0028). The P → libDaisy knob index table is in `firmware/main.cpp` (`kPotKnob`).

| Pot | Name | Function | Notes |
|---|---|---|---|
| P1 | **MIX** | Dry/wet, equal-power | Full CW = 100% wet for send/return. Full CCW = dry only, a clean passthrough in every ATTITUDE (the µ-law box is on the wet only, ADR 0042 amendment) |
| P2 | **DECAY** | Tail length (feedback gain) only (§4.4, ADR 0026) | T60 0.4 s → 9 s (exponential, §4.4), always fades (ADR 0001, 0006; the tight slap is DECAY 0 with TENSION up, i.e. tight). KICKED: top ~10% enables Howl (ADR 0002), exits naturally (ADR 0018). CLEAN / DRIVEN: top ~10% is the Hold (ADR 0040): T60 glides out toward minutes, never self-oscillates, it ducks under the input's kick and bass; armed by DECAY entering the zone outside KICKED; not in SPRINGS 3 echo mode, where DECAY is the echo's feedback (ADR 0041; its top, in every ATTITUDE, is persistent repeats held by the tape, never a runaway: ADR 0041 amendment, v1.0.34). Doesn't change the tank or bend pitch (ADR 0026 supersedes 0012) |
| P3 | **TONE** | Bipolar tilt. CCW = dark dub (loop damping LPF down, a darker input coil, tilt toward lows: as dark as before ADR 0038); noon = the Wellspring-fit sound (ADR 0038); CW = bright/splashy and thinner: King Tubby's Big Knob, an 18 dB/oct low cut to 800 Hz with a nasal bump on sharp hits, on the wet return after the Springs so it thins the ringing tail at once (ADR 0036 and its amendment), the output pickup opening up | Hero control (§2.3.3). Tilt applied pre-tank (changes what excites springs) + damping in loop; the Big Knob on the wet (§4.8). CCW warm dub dark, CW splashy never harsh (ADR 0017) |
| P4 | **SPLASH** | How hard the hits hit: a loud, sudden hit's own highs fed harder into the springs (Clang), and in DRIVEN/KICKED a short hit pushed harder into the input transducer (Bite) (§4.5, ADR 0032) | Nothing is added on a hit; ghost notes in a groove stay quiet. SPLASH 0 = only the small Jolt floor (DRIVEN/KICKED) |
| P5 | **TENSION** | "Which tank": Loop delay L, transition fC, allpass `a` and stage count together (§4.4) | CW tight (short tank, small bright chirp, quick repeats; still a spring, ADR 0007), CCW loose (long tank, big darker chirp, slow repeats). More tension = tighter; turning it up raises the live tail's pitch, like tightening a string (ADR 0026) |
| P6 | **WOBBLE** | Bipolar pitch movement of the tank delay (§4.7, ADR 0034): noon still; left = random wow + flutter (tape-like, with a faint flutter tremolo); right = sine vibrato | Noon ±3 % dead zone. Left: Drift, never repeating, grows to fully left. Right: Warble, a vibrato getting faster and deeper. Both end stops clearly out of tune, toned down from round 1 (ADR 0034 round 2). Min floor always on (§4.10). CV adds to the pot: fully left + CV sweeps random → still → LFO |
| P7 | **DRIVE** | The INPUT: one gain, 0 → +24 dB, same in every ATTITUDE; the Splash hears the signal after it, before any saturation (§4.9, ADR 0033) | Level-compensated except a quarter of the gain: the tail grows ~+6 dB across the knob. Colour: clean-ish to ~9 o'clock, driven by ~3 o'clock (ADR 0014); intensity steps evenly CLEAN < DRIVEN < KICKED. Never shortens the tail or reduces the splash |

### Switches

| Switch | Left | Centre | Right |
|---|---|---|---|
| SW0 **SPRINGS** | 1 spring — sparse, most splashy | 2 springs — classic tank | Echo mode — a tape echo into the 2-spring tank: each repeat splashes into the springs (ADR 0041). DECAY = echo feedback, TENSION = echo time, gate = clock |
| SW1 **ATTITUDE** | CLEAN — linear tank, light transducer colour; wet untouched | DRIVEN — tape saturation, moderate clatter; the wet 24 kHz / 12-bit µ-law, before TONE (ADR 0042) | KICKED — hard drive in loop, full chaos, collisions, Howl allowed; the wet 24 kHz / 10-bit µ-law, before TONE (ADR 0042 and its amendments) |

Switch changes: ATTITUDE Morphs the live tail (all attitude params smoothed); SPRINGS crossfades ~20 ms (ADR 0003). Into echo mode the echo fades in over 80 ms on a fresh tape while the springs glide to their fixed tank; out of it the echo fades out over 80 ms, its last repeats ringing on in the springs (ADR 0041).

**Echo mode (SPRINGS 3, ADR 0041).** The panel changes meaning in position 3 only:
- **DECAY** = the echo's feedback, and every repeat is a step down from the hit, the first included (repeat n ≈ hit × gⁿ; owner, 4 Oct): 0 = one repeat about 10 dB down, noon = a few (−6 dB each), then longer builds; the very top, in every ATTITUDE, holds: persistent repeats at a roughly constant level, held by the tape's saturation, never a runaway (owner, 5 Oct; from DECAY ~0.90 in KICKED, ~0.94 in CLEAN and DRIVEN); they die away (≥ 30 dB in ~2.5 s) when DECAY comes back down.
- **TENSION** = the echo time. Unclocked: 2 s (CCW) → 0.4 s (noon) → 80 ms (CW), log. Clocked: seven zones CCW → CW, 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16 of the clock's beat (a time over 2 s plays at half).
- **Gate** = the clock: one pulse = a quarter note (30–300 bpm); lost after 2.25 beats without a pulse (back to free time).
- Every time change swoops like tape (~0.3 s, the repeats bend in pitch). WOBBLE moves the tape too. The springs behind the echo are fixed: the noon tank, T60 ~1.7 s.

CV/knob smoothing: snappy (~5 ms) for MIX, DRIVE, SPLASH, TONE; gliding (~50–100 ms) for DECAY, WOBBLE, TENSION (ADR 0015).

### Audio I/O

- Hardware normals In L → In R when R is unpatched (NE Versio manuals). Mono-in works with no firmware logic; libDaisy has no jack detection.
- Outputs not normalled.

### Button + Gate

- **Button = KICK.** Injects "tank kick" impulse (§4.6). Momentary. Fixed strength, scaled by ATTITUDE (ADR 0005). Tight thud + big crash (ADR 0016). Held 1 s it also leaves throw mode (ADR 0039, amending ADR 0013): the send opens again (2 ms glide) and all four LEDs blink white once (150 ms), only if throw mode was on; otherwise holding does nothing extra.
- **Gate in = THROW** in SPRINGS 1–2 (ADR 0039; was KICK until v1.0.30). While high, the Springs' send is open; low, closed (opens over 2 ms, closes over 15 ms). MIX and the tail already ringing are untouched, so a throw rings on. Unpatched the gate reads low, so the throw only switches on at the gate's first rising edge after power-up; until then the send is open, as before. Holding KICK 1 s switches throw mode off again (the send open; the next rising edge switches it back on). The Kick is never gated. The gate's role can depend on SPRINGS (position 3 is planned as the echo's clock). In SPRINGS 3 the gate is the echo's clock instead (one pulse = one beat, ADR 0041; the throw rests open there); its rising edges set the tempo in every position, so it is already known when SPRINGS reaches 3.

### LEDs

Level meters, like NE's own Versio firmware (ADR 0031). Panel LEDs left to right: **In L, In R, Out L, Out R**.
- Brightness follows the level on a dB scale: −48 dBFS and below is off, 0 dBFS full, so quiet signals still glow.
- Colour warms green → amber as the level gets hot (from −18 dBFS, fully amber at −6 dBFS). Level alone never makes red.
- **Red** is a warning, held 0.5 s: input LEDs when the input peaks at −1 dBFS or above (the ADC's full scale, i.e. the jack's clip point); output LEDs while the output safety limiter pulls the wet down by 0.5 dB or more (e.g. a loud Howl).
- Fast rise, ~0.3 s fall.
- No mode colours (the switches show their own position) and no Kick flash (owner choices, M9).
- Boot pattern: unique colour sequence confirming firmware loaded (NE convention), then metering.

---

## 4. DSP architecture

### 4.1 Basis

Parametric model from **Välimäki, Parker & Abel, "Parametric Spring Reverberation Effect," JAES 58(7/8), 2010**, with cost reductions from **Parker, "Efficient Dispersion Generation Structures for Spring Reverb Emulation," EURASIP JASP 2011** (multirate/multiband; reported ~⅓ original cost). Physical background: Parker & Bilbao, "Spring Reverberation: A Physical Perspective," DAFx-09.

Claude Code should read these papers before implementing. Välimäki structure: two parallel paths — low-frequency chirps + faster wideband echoes.

### 4.2 Per-spring structure

**Scope (v0.4):** DriveIn, Tilt and DriveOut are **Tank-level** stages, one copy each, shared by all Springs (input is summed mono first, §4.3). Only the Loop contents (allpass cascade, damping, AntiRes, delay, LoopSat) are per Spring. The diagram shows one Spring with the shared stages drawn around it.

```
                 ┌──────────────── LOW-CHIRP PATH (C_lf) ────────────────────────────────┐
in ─ DriveIn ─ Tilt ┤ + ─ DCblock ─ SpectralDelay(stretched AP × M) ─ LPF(tone) ─ AntiRes ─ Delay(L+mod) ─┐
                 │ ▲                                                                                 │
                 │ └──────── g_lf(decay) × LoopSat(attitude) ◄───────────────────────────────────────┘
                 │
                 └──────────────── HIGH PATH (C_hf) ─────────────────────────────┐
                   + ─ SpectralDelay(short AP chain) ─ HPF ─ Delay(L_hf+mod) ─ ┘ (feedback g_hf)

out_spring = DriveOut( C_lf + hf_level(tone) × C_hf )
```

- **Spectral delay filter:** cascade of M interpolated stretched allpass sections. Each = Schroeder-style allpass with embedded delay K−1 samples + first-order fractional-delay allpass. K sets chirp spacing; `a` sets chirp steepness.
- **DC blocker** in low-chirp loop (paper uses ~40 Hz).
- **DriveIn / DriveOut / LoopSat** = drive chain (§4.9). **AntiRes** = resonance suppressor (§4.10).
- **Tilt** = TONE's tilt (plus a fixed 20 Hz guard high-pass). TONE's right half, the Big Knob, is not here since v1.0.29: it filters the wet return after the Springs (§4.8, ADR 0036 amendment), so new input reaches the Springs with its lows at every TONE.
- **Tank voicing 7 (ADR 0038, the Wellspring fit)** around and inside that: after the Tilt a low cut (2nd-order high-pass 220 Hz + −3 dB shelf at 300 Hz, its level made up from the power it takes), then the **input coil** (the hit's Clang added first; a plain-square even-order term, then a resonant low-pass at 2.35 kHz, darker left of TONE noon, opening a little in KICKED with DRIVE), then the shared **Sweep** (≈40 "highs later" allpass sections at TENSION noon) into every Spring's Loop and high path; the Loops keep fewer sections, carry three short **diffusers** on their feedback (after the pickup) and lose less treble per trip (damping ×3.6 at noon and right of it, eased back toward ×1 at TONE fully left and ×1.5 at DECAY max); the high path is aligned on the Loop's first echo. On the way out the **pickups' treble loss** (a resonant low-pass at 4.5 kHz, opening toward 9 kHz at TONE fully right) comes before DriveOut, and the wet trim (−2.5 dB) after it.

### 4.3 Multiple springs (SW0)

- 1/2/3 instances of §4.2 in parallel, **detuned** L, K, `a` per spring (±3–8%, tune by ear), and (ADR 0027) each a step darker and shorter than the one before (damping × 1 / 0.85 / 0.72, T60 × 1 / 0.93 / 0.865), so modes that line up between Springs die at different rates instead of singing. Detuning = beating + density, and helps prevent shared resonances (§4.10).
- Position 3 (ADR 0041) is **echo mode**: a tape echo (one delay line up to 2 s + a margin, 379 KB in AXI SRAM on the Daisy) on the mono input before the Splash, feeding Springs A and B at position 2's mix and a fixed tank (TENSION noon, T60 1.7 s). The playback head darkens (2-pole LPF 3.5 kHz) and thins (HPF 140 Hz) every pass; the record head loses the top (LPF 6 kHz, no folding) and saturates; feedback stays on the tape. On the feedback, a bucket-brigade "wear" (owner's pick, 4 Oct): each pass sample-and-held at a low clock with gentle filters (aliasing grit that builds), a 2:1 compander that breathes, a faint clock whine (`EchoVoicing.h` kBbd; strength to be picked). Spring C is heard nowhere, so its audio doesn't run (its settings still follow, keeping positions 1–2 bit for bit). Numbers: `core/params/EchoVoicing.h`.
- Before ADR 0041, position 3 (ADR 0037) was three **coupled** Loops: each round trip their returns turned by a 40° rotation about the axis (1, 2, 3)/√14 (energy-preserving), let go across the Howl zone. Kept as a Renderer-only reference (`echo_mode=0`).
- Stereo (ADR 0038): no Spring is panned. L = mid + X, R = mid − X, where X is the bass-cut (150 Hz) sum of the mid through a decorrelator and the Springs' difference (A − B) through its own decorrelator: every echo reaches both ears at once (no left-right flicker), the fine detail differs (width), and mono is exactly the mid. (Before: Spring A → L, B → R, C centre; 1 Spring decorrelated R.)
- Input summed to mono before tank (real tanks are mono). Dry path stays stereo.
- ~20 ms crossfade on spring-count change.

### 4.4 TENSION picks the tank; DECAY sets the tail (ADR 0026)

*(Until v1.0.12 DECAY also sized the tank: L 30–100 ms and K grew with DECAY. ADR 0026 moved the tank to TENSION.)*

| TENSION | Loop delay L | fC (→ stretch K) | `a` | Stages | Effect |
|---|---|---|---|---|---|
| 0 loose | 110 ms | 2.7 kHz | 0.55 | 64 | long tank, big darker chirp (~37 ms), slow repeats (Swissecho, SNRA500) |
| 0.5 | 69 ms | 3.3 kHz | 0.47 | 40 | the IR library's median tank (~15 ms chirp) |
| 1 tight | 33 ms | 4.6 kHz | 0.40 | 24 | short tank, small bright chirp (~5 ms), quick repeats (Space Echo) |

Log-linear between anchors for L and fC, linear for `a` and stages; SPRINGS modes cap the stage count (3 Springs: 52). DECAY sets the feedback g for T60 0.4 → 9 s (exponential), designed from each Spring's own round trip, so tail length doesn't depend on TENSION. Fractional-delay interpolation + slew limiting on L.

**Hold (ADR 0040, CLEAN / DRIVEN, DECAY 0.9 → 1; not in SPRINGS 3 echo mode, where DECAY is the echo's feedback and the Springs glide to a fixed tank below the zone, so the Hold fades out with the echo's glide):** the T60 glides (log time, smoothstep) from the plain curve (6.6 s at 0.9) to 240 s at the design points; the Loop gain's cap moves with it so the peak per-trip gain is 0.998 (always < 1; heard: ~3 dB per 10 s at max). The high path keeps the plain DECAY's T60. From halfway in (DECAY 0.95) the bed is fully in: the send drops 6 dB ("layer", the default; "freeze" closes it, and an open Throw overrides that), and the wet ducks (a gain after the limiter) keyed on the dry input's 4th-order 120 Hz low-pass (peak follower 1 ms / 30 ms; −24 → −12 dBFS maps to 0 → 12 dB), falling in 3 ms, holding 20 ms, back on a 35 ms curve; the layer's send dips with it. The Hold arms when DECAY enters the zone outside KICKED; KICKED inside the zone disarms it until DECAY leaves the zone. KICKED's Howl zone is the same DECAY range and unchanged.

### 4.5 SPLASH / ATTITUDE nonlinear model

SPLASH comes from the hit itself (ADR 0032): nothing is added on a hit.

1. **Detectors,** on the mono input after the INPUT gain (DRIVE, §4.9), before any saturation, high-passed at 200 Hz. The Hit detector (fast − slow envelope, judged against the programme level) → `hit` 0–1 (control rate) for the Jolt. The hit envelope (round 4's detector) → e = SPLASH × sudden × loud, 0–1 per sample for the first ~10–25 ms of a loud hit, 0 on sustained sound; in a groove the loud reference rises with the programme level, so ghost notes stay quiet at any DRIVE.
2. **Clang** (every ATTITUDE): the springs' input gets its own highs (above 2 kHz) fed harder, ×(1 + 5e) at the owner's "clear" strength.
3. **Bite** (DRIVEN, KICKED): a short hit (its energy mostly above 2 kHz: drums, not chords) is pushed into DriveIn ×(1 + 4e) and half the push (in dB) taken back after: grit on the hit, a harder hit into the springs. Above DRIVE noon the Clang and the Bite grow with DRIVE (exactly as picked at 0.8), so DRIVE never reduces the splash.
4. **Coefficient jolt:** momentary modulation of `a` and L ∝ hit (decays ~50–200 ms) → chirp smear / pitch lurch.
5. **Loop saturation:** per §4.9. The Clatter (bandpassed knocks into every Spring's Loop and high path) is the Kick's crash only (§4.6, ADR 0016).

| Mode | Loop sat | Clang | Bite | Jolt |
|---|---|---|---|---|
| CLEAN | off | yes (ADR 0025, 0032) | no | tiny (ADR 0025) |
| DRIVEN | gentle tape | yes | short hits | small |
| KICKED | hard, asymmetric | yes | short hits | large + energy-dependent rattle |

### 4.6 KICK (button + gate)

Inject into tank input (post-drive):
- Low thump: decaying sine ~40–80 Hz, ~20–40 ms
- Broadband noise burst ~10 ms
- Forces maximal SPLASH jolt and the Clatter crash (the Kick is the only thing that still fires the Clatter, ADR 0032)
Level scales with ATTITUDE. Debounce button. (The gate was a second KICK until ADR 0039 made it the throw; the Plugin's MIDI notes still fire Kicks.)

### 4.7 WOBBLE

Modulation of each Spring's Loop delay L, plus one shared Transport generator on every pickup read (the first echoes waver too). Bipolar knob (ADR 0034, numbers in `core/params/WobbleVoicing.h`):
- **Noon** (±3 % dead zone): no WOBBLE, only the Micro-mod floor.
- **Left (Drift):** smooth random wow (Catmull-Rom random line, every segment its own random length: ~0.2–1.5 Hz) plus a smaller, faster random flutter (~5–12 Hz) whose speed follows the wow line (±20 %). Never repeats. A flutter tremolo on the wet (the Transport's flutter line, ≤ 1 dB peak fully left) makes it sound like a tape transport.
- **Right (Warble):** a sine vibrato, 1.5 → 5.5 Hz as it grows (most of the rise in the first half of the side), its rate drifting ±6 % (0 = pure sine).
- Depth in cents per pass (so WOBBLE sounds the same at every TENSION), curve s(a) = (e^{ka} − 1)/(e^k − 1) per side so every 0.1 of travel is a clear step; both end stops clearly out of tune (held tone through the whole Tank, DECAY noon: ~34 cents fully left, ~22 fully right; round 1 ~46–48). Round 2's three voicings (A = round 1, B, C: two strengths of toning down) are compared by ear; the Renderer picks one with the hidden key `wobble_voicing`, the firmware and plugin use B. Springs B and C follow Spring A (scaled by their L) at low amounts, independent from ~45 % of either side.
- **Minimum floor always active**, WOBBLE at noon included (§4.10).

### 4.8 Output stage

- Wet: the pickups' treble loss before DriveOut (§4.2, ADR 0038), the wet trim after it, gentle high-shelf cut, then the **Big Knob** (TONE right of noon, ADR 0036 and its amendment: 18 dB/oct low cut 20 Hz → 800 Hz, the coil bump blended in on sharp hits, stereo; its own slow makeup from the wet's power above ~90 Hz in / out, ¾ back, ≤ 12 dB, held in silence; a pass-through, skipped, at noon and left), then the limiter. The Kick's direct thump joins before the pickups, so it is thinned with the wet.
- MIX: equal-power.
- µ-law box (ADR 0042 and its amendments), on the stereo wet only, every SPRINGS position: after the pickups and the high-shelf cut, **before TONE's return filter** (so TONE right of noon thins its grit), then the limiter, the Hold's ducking and MIX (since v1.0.36; it sat after MIX on dry and wet before). CLEAN none (bit for bit), DRIVEN 24 kHz / 12-bit µ-law, KICKED 24 kHz / 10-bit µ-law (8-bit until v1.0.34). 48 → 24 → 48 kHz through polyphase IIR half-bands (flat to 10.6 kHz, ≥ 85 dB down from 13.4 kHz: nothing folds back); µ-law (µ 255) against a full scale of +8 dB (the wet before the limiter runs hotter than 0 dBFS; a clip at 24 kHz would fold), TPDF dither down to the last step, under half a step exactly 0 (silence in, silence out; tails end in grain, then silence). ~6 samples (0.12 ms) of delay on the wet in DRIVEN/KICKED only; ATTITUDE flips crossfade the box over 20 ms. The limiter and the red LEDs read the wet after it.
- Denormal protection (FTZ; tiny noise if needed).

### 4.9 DRIVE voicing — "characterful, not a fight"

Physical rationale: a real driven tank colours sound in three places — the **input transducer** (electromagnetic driver coil), the **springs/loop**, and the **output pickup transducer**. Dub rigs then often add **tape** (Space Echo / Echoplex lineage). Model the chain, not a distortion pedal in front of a reverb.

| Stage | Where | Character | Model (starting point) |
|---|---|---|---|
| **Input transducer** | DriveIn, pre-tank | Mid-forward, slightly gritty, soft magnetic saturation; LF + HF loss | Band-limit (HPF ~80–150 Hz, LPF ~5 kHz, tunable) → soft asymmetric saturator (e.g. biased tanh) |
| **Tape** | DriveIn, after transducer | Rounded peaks, gentle compression, HF smear increasing with drive | Pre-emphasis → soft saturator → de-emphasis; drive-dependent LPF; optional light hysteresis approximation |
| **Loop saturation** | Inside feedback | Keeps feedback bounded; adds thickness as tail builds | tanh-style; asymmetric in KICKED (adds even harmonics, "valve-ish") |
| **Output pickup** | DriveOut | Subtle second transducer colour | Light soft-clip + band-limit |

ATTITUDE sets which stages engage and how hard:

| ATTITUDE | Transducers | Tape | Loop sat |
|---|---|---|---|
| CLEAN | light | off | off |
| DRIVEN | medium | **on** (core dub colour) | gentle, symmetric |
| KICKED | hard | on, hot | hard, asymmetric |

Requirements:
- **DRIVE is the INPUT** (ADR 0033): one input gain G, 0 → +24 dB along the DRIVE curve, the same in every ATTITUDE. The Splash hears the input after G (§4.5); the saturators see G × the ATTITUDE's voicing offset (the pre-gain curve of ADR 0022). DRIVE drives the input (transducer, tape) and output (pickup) stages only: the Loop saturation keeps a fixed, gentle per-ATTITUDE hardness, so DRIVE never shortens the tail.
- **Automatic gain compensation** on DRIVE, except a quarter of G (in dB), added on the Springs' output: the tail grows ~+6 dB across the knob (a gentle throw), the same in every ATTITUDE; colour, grit and a few dB, never a volume knob. (Directly addresses Springray "must drive hot to hear it" complaint.)
- Reverb clearly audible at DRIVE = 0 with typical Eurorack levels.
- **Oversample nonlinear stages ×2 (min)** to limit aliasing. Include in CPU budget (§5).
- Optional research reference for tape modelling (verify before use): J. Chowdhury, "Real-time Physical Modelling for Analog Tape Machines," DAFx-19. Full hysteresis model likely too heavy for Daisy; use simplified version.

### 4.10 Anti-resonance / anti-buildup system

**Problem:** feedback loop with slightly excess gain at one frequency → that mode reinforces every pass → sine-like ringing tone dominates tail. Heard on the owner's Wellspring **delay** feedback (not its spring, §2.2), and structurally possible in any digital feedback loop. Must be prevented **without using TONE**.

**Scope (ADR 0010):** layers 1–3 and 5 are built by default (cheap, by design). Layer 4 (adaptive suppressor) is built **only if** the automated metric below fails at M6 after layers 1–3 are tuned.

Layered defence, in priority order:

1. **Even loop gain by design.** Loop gain per spring kept below target at *all* frequencies, not just on average. Tone/damping filters designed so no band peaks above others. Unit test measures loop magnitude response across the band.
2. **Always-on micro-modulation.** Tank delay L modulated continuously by slow smoothed random at a small floor depth (set at M6: ±0.05 % of L at ~0.2 Hz, ≤ 0.11 cents on held tones), even with WOBBLE at 0. Resonant frequencies keep moving → no mode can lock in. Depth below audible pitch wobble (confirm by ear).
3. **Spring detuning** (§4.3): springs don't share exact modes → no common reinforcement.
4. **Adaptive resonance suppressor (AntiRes block).** *Conditional (ADR 0010).* Safety net if a mode still pokes out:
   - Detector at control rate (not per sample): e.g. small FFT on wet tail in main loop, or bank of bandpass energy trackers. Flags narrowband peak exceeding broadband level by threshold.
   - Response: dynamic peaking-cut biquad **inside the loop** at detected frequency; depth ramps in (up to ~−6 to −12 dB), releases when peak subsides. Max 2–3 simultaneous notches.
   - Must be inaudible on normal material — only acts on runaway modes.
5. **Loop saturation** (§4.9) bounds energy as last resort.

Measurable criterion (starting thresholds — tune/confirm in interview):
- Impulse + noise-burst input, DECAY max, all SPRINGS × ATTITUDE combos, WOBBLE 0. KICKED Howl zone excluded from peak test but must still show no sustained pure sinusoid (ADR 0002).
- In tail from 1 s onward: ~~no narrowband peak > 12 dB above median of 1/3-octave-smoothed spectrum~~ **`ringing_db` < 15 dB** (ADR 0023; the old test failed real tanks).
- No sustained sinusoid (> 2 s) above −30 dBFS.
- Renderer (§6) reports this metric automatically.

---

## 5. Performance budget

- 48 kHz, block 48 initial. 480 MHz ÷ 48 kHz ≈ **10,000 cycles/sample**.
- Target **≤ 75% CPU peak** worst case, **80 % ceiling** for a release that passes its click check (ADR 0030 amendment, 4 Oct 2026; was 70 %) (since ADR 0041 no position runs 3 Springs: 2 Springs at the loosest TENSION (0), or echo mode at DECAY 1; KICKED, max DRIVE), measured by the M3 profile build; every release is also checked by ear on the module for clicks/dropouts at heavy settings (owner, 30 Sep 2026; was 65 %, ADR 0030).
- Main costs: allpass cascades, oversampled nonlinear stages. Mitigations:
  1. Delay lines + filter state in internal SRAM, not SDRAM.
  2. Decimated low-chirp path (×2/×4) per Parker 2011.
  3. Fewer stages per spring in 3-spring mode.
  4. Oversampling only on nonlinear blocks; cheap polyphase halfband filters.
  5. AntiRes detector at control rate in main loop, not audio callback.
  6. `-O3`, float only, CMSIS-DSP where useful.
- libDaisy `CpuLoadMeter` + serial logging during dev.
- **Unverified:** achievable M and oversampling factor. Decide after hardware profiling milestone.

---

## 6. Architecture: one DSP core, three hosts

Same DSP code, three wrappers. Hosts differ only in where audio + parameters come from.

```
                 ┌─────────────────────────────┐
                 │ core/                       │
                 │  params/ ParamSpec table    │  ← single source of truth
                 │  dsp/    Spring, Tank, ...  │  ← no platform deps
                 └──────┬──────────┬───────────┘
                        │          │           │
        ┌───────────────┘          │           └───────────────┐
  host/render (CLI)        plugin/ (JUCE AU+VST3)        firmware/ (Versio)
  WAV in → WAV out         live in Ableton               real knobs, CV, gate
  sweeps, overnight        automation, A/B               final feel + CPU
```

### 6.1 Shared parameter layer (`core/params/`)

- One table defines every parameter: id, display name, normalised range 0–1, mapping curve to internal units, default, smoothing time.
- **Every host passes normalised 0–1 values** (same as Versio knob + CV reading). Mapping curves live only here → a setting in the plugin sounds identical on the module.
- Switches = 3-state enums; THROW (ADR 0039) = an on/off `Toggle` standing in for the gate (not on the panel). KICK = trigger event.
- Presets/test settings stored as JSON of normalised values → portable across all hosts.

### 6.2 Host A — offline renderer (`host/render`)

- CLI: input WAV + JSON params → output WAV.
- **Automation:** parameter breakpoints over time (e.g. SPLASH ramp, KICK events at timestamps).
- **Sweep mode:** grid over params (e.g. DECAY × ATTITUDE × SPRINGS) → batch of WAVs + manifest (JSON/CSV) naming each file's settings. For overnight runs.
- **Metrics per render:** peak, RMS, estimated T60, resonance peak ratio (§4.10), NaN/Inf count, clip count.
- Deterministic: all randomness seeded → same input + params = bit-identical output.
- Test suite (`host/tests`): impulse chirp check, stability at max settings, NaN/denormal, loop magnitude response, resonance criterion.

### 6.3 Host B — JUCE plugin (`plugin/`)

- Formats: **Audio Unit + VST3** (Mac, Ableton).
- Parameters generated from ParamSpec table: 7 knobs as float params, 2 switches as 3-choice params, THROW as an on/off param (the gate, ADR 0039), KICK as button param.
- **KICK also triggered by MIDI note** (any note, velocity ignored — ADR 0005) → sequence kicks from Ableton clips. Throws are THROW automation.
- Test bench only in v1: generic parameter UI, no custom graphics (ADR 0004).
- Sample rate: core must be sample-rate-aware. Reference/validation rate = 48 kHz (matches Daisy). Tuning decisions checked at 48 kHz.
- Used for: live sound design, automation, A/B against other spring plugins/hardware.
- JUCE via CMake. **Check current JUCE licence terms at build time (unverified).**

### 6.4 Host C — Versio firmware (`firmware/`)

- libDaisy `DaisyVersio`; reads knobs/CV (0–1), switches, button, gate → ParamSpec → core.
- LEDs, boot pattern, CPU meter.
- Standard libDaisy Makefile.

### 6.5 Repo layout

```
resilio-versio/
  CMakeLists.txt            # desktop: core + render + tests + plugin
  core/
    params/ParamSpec.h      # parameter table + mapping curves
    dsp/
      StretchedAllpass.h
      SpectralDelay.h
      Spring.h/.cpp
      SpringTank.h/.cpp
      Splash.h/.cpp
      Kick.h
      Wobble.h
      Drive.h/.cpp          # transducer, tape, loop sat, output pickup (§4.9)
      AntiRes.h/.cpp        # resonance detector + dynamic notches (§4.10)
      Oversampler.h
  host/
    render/main.cpp
    tests/
  plugin/                   # JUCE AU/VST3
  firmware/
    Makefile
    main.cpp
    Controls.h/.cpp
    Leds.h/.cpp
  test_audio/               # impulse, snare, rimshot, dub skank, chord stab
  presets/                  # JSON param sets
```

**Rule:** `core/` has zero platform includes (no libDaisy, no JUCE).

---

## 7. Build milestones

Criteria come from the owner interview (27 Sep 2026). **[A]** = automated (Renderer/tests, must pass in CI-style runs). **[L]** = owner listening check. **[H]** = on hardware. Numbers marked *(start)* are starting thresholds, confirmed by ear at that milestone. Any change is noted in the changelog.

Shared definitions:
- **Stimulus set** = `tools/make_stimulus.py` output. **Reference set** = Wellspring recordings (ADR 0009).
- **Review page** = locally generated HTML per render batch: player + spectrogram + settings + metrics per file, next to the matching Reference file. WAVs also written to a folder for Ableton.
- **Click detector** = Renderer metric flagging sample-to-sample discontinuities above the signal's local high-frequency level *(start: 20 dB above)*.

### M0 — Toolchains
- [A] `cmake` builds empty Core + Renderer + test runner on macOS arm64. The Renderer copies a WAV through unchanged (bit-identical).
- [A] The Plugin target builds AU + VST3 (installing Xcode if Command Line Tools aren't enough). `auval` passes for the AU.
- [A] Firmware builds with `gcc-arm-embedded`. Binary ≤ 128 KB, reported by the build (ADR 0011).
- [H] Test firmware flashed via NE Firmware Swap (custom file). Boot LED pattern shows.
- [H] Passthrough indistinguishable from a patch cable: level within 0.5 dB per channel, no audible added hum/hiss with the gain up, both channels, mono-in on L comes out of both sides.
- [H] Every control verified two ways: LEDs react to each knob, switch, button and gate, and serial prints exact values. Knob+CV reads ≈0 at 0 V / knob CCW and ≈1 at 5 V or knob CW (±0.02). Switches report 3 states. Button and gate edges print once per press (no bounce).

### M1 — Core: one Spring, CLEAN + Renderer
- [A] Click render shows repeating dispersive chirps: spectrogram has chirps (highs later, ADR 0024) at a regular repeat time matching the configured L *(±5%)*.
- [A] Stable at every DECAY × TENSION corner (grid incl. extremes): no NaN/Inf, no growth, tail decays at max DECAY (ADR 0001). T60 at min DECAY 0.3–0.5 s (ADR 0006), at max 8–10 s.
- [A] TENSION 1 (tightest) still shows a chirp (ADR 0007).
- [A] Deterministic: same input + params → bit-identical output.
- [A] Metrics reported per render: peak, RMS, T60, resonance ratio, NaN/Inf count, clip count, click-detector hits.
- [A] Review page generated for the M1 grid, next to the Wellspring Reference set.
- [L] Owner A/B vs Wellspring clicks (take A), with DECAY set so T60 matches the Wellspring's measured T60 (the Wellspring has no decay control): "same family" (repeating boings, highs later than lows, dark tail). Thin/sparse is acceptable at this stage.

### M2 — JUCE Plugin shell
- [A] AU + VST3 load in Ableton. All ParamSpec params visible and automatable. Names/ranges generated from ParamSpec, no hand-written list.
- [A] Kick from any MIDI note on a routed MIDI track, velocity ignored, **sample-accurate** (offline test: note at sample N → Kick onset at N, with reported latency).
- [A] Plugin latency reported to the host. The dry path stays aligned with other tracks in Ableton (null test vs a duplicate track at MIX 0, every ATTITUDE: the µ-law box is on the wet only, ADR 0042 amendment).
- [A] Plugin render == Renderer output for the same stimulus/params at 48 kHz (bit-identical, or within float tolerance −120 dBFS).
- [A] Works at 44.1/48/96 kHz without crashing or detuning (T60 and chirp timing within 5% across rates).

### M3 — Hardware profiling
- [H] One Spring running on the Versio. CPU load logged over serial (average and peak) for each setting corner.
- [H] Decisions recorded as an ADR: M stages, decimation factor, oversampling factor, and the headroom plan to hit ≤ 65% worst case (§5).
- [H] CPU tradeoff order if short: simplify 3-spring mode first (fewer stages per Spring), keeping 1- and 2-spring modes at full detail.
- [H/L] Module vs Plugin: the same stimulus recorded from the module vs rendered on the Mac. The owner can't reliably pick which is which in a blind A/B (ABX: ≤ 12/16 correct). T60 within 5%, 1/3-octave spectrum within 1.5 dB (ignoring converter noise floor).

### M4 — Multi-spring, stereo, tank coupling
- [A] SPRINGS switch changes are click-free (click detector) in every combination, mid-tail.
- [A] SPRINGS levels matched: loudness of 1/2/3 within ±1.5 dB for the same input.
- [L] 1 → 2 → 3 sounds sparse/drippy → classic → dense/lush, clearly different in a blind test.
- [A] Stereo: clearly wide (inter-channel correlation of the wet tail < 0.5 *(start)*). Mono-safe: mono fold-down (L+R) energy no more than 1.5 dB below the stereo (L²+R²) energy, i.e. no phase cancellation (definition: `docs/m4-contracts.md`), no comb-filter notches > 6 dB in the 200 Hz–5 kHz band.
- [A] DECAY sweep min→max over 4 s on a held tail: no click-detector hits, no loudness jump > 3 dB in any 100 ms step. TENSION sweep likewise, with a smooth pitch bend (ADR 0026; was DECAY's, ADR 0012).
- [L] TENSION sweep audibly goes loose/boingy → tight/pingy; DECAY sweep goes short → long without retuning the echoes.

### M5 — Drive chain + TONE tilt
- [L] ATTITUDE at DRIVE noon on a snare: CLEAN hi-fi, DRIVEN warm tape dub, KICKED gritty/trashed. Owner picks all three correctly in a blind test.
- [A] ATTITUDE loudness within ±2 dB of each other at the same settings.
- [A] DRIVE audibility (ADR 0022): on `02_hits`, DRIVE 0 vs 0.5 difference ≥ −20 dB in DRIVEN/KICKED; DRIVE 0 vs 1 ≥ −6 dB in KICKED.
- [A] DRIVE sweep 0→max: loudness within ±2 dB (LUFS-style short-term). Clean-ish below ~25%, colour builds to ~85% (ADR 0014), measured as THD rising monotonically.
- [L] DRIVEN at high DRIVE vs Wellspring hot-INPUT hits (take C): comparable warmth/grit character (reference, not a clone).
- [A] Reverb clearly audible at DRIVE 0 with a 10 Vpp-equivalent input (wet within 6 dB of dry at MIX noon).
- [A] Aliasing: a 5–15 kHz sine sweep at max DRIVE/KICKED shows alias products ≤ −60 dB relative to the fundamental.
- [A/L] TONE: chirp still visible and audible at full CCW (ADR 0017). Full CW is splashy, not harsh (owner check on hats/cymbals; energy above 10 kHz capped *(start: ≤ +6 dB vs noon)*). TONE sweep loudness within ±3 dB.

### M6 — Anti-resonance
- [A] §4.10 criterion across the full sweep grid (DECAY max, all SPRINGS × ATTITUDE, WOBBLE 0): `ringing_db` < 15 dB (ADR 0023) and no steady tone > 2 s above −30 dBFS.
- [A] The metric **catches** the owner's Wellspring delay-Ringing recording (take G) if available, or a synthetic ringing loop. Proves the test isn't toothless.
- [A] Howl zone (KICKED, top ~10% DECAY): ADR 0019 criteria (rough, moving, may lean to a pitch, never a steady sine). Exiting the zone drops ≥ 30 dB within ~3 s (ADR 0018).
- [L] Micro-mod floor inaudible: with WOBBLE 0 the owner hears no pitch movement on a held chord stab.
- [A] If any grid cell fails after layers 1–3 are tuned → build layer 4 (ADR 0010) and re-run.

### M7 — SPLASH + KICK + WOBBLE + MIX
- [L] SPLASH max, KICKED, hard snare → big bright crash + pitch lurch that settles into the tail within ~1 s. Ghost notes (−18 dBFS hit in the stimulus) barely trigger it. The −6 dBFS hit clearly does.
- [A] Hit detector monotonic: hit value rises with input level. The −18 dB hit gives < 25% of the −6 dB hit's Clatter energy.
- [L] SPLASH 0 in DRIVEN still gives a faint natural splash on hard hits (not zero).
- [L] Kick = tight thud + big crash (ADR 0016). [A] Energy < 100 Hz down ≥ 20 dB within 300 ms.
- [H] Gate Kicks: every gate at up to 12/s (16ths at 180 bpm) gives exactly one Kick, onset within 1 ms of the gate edge. No double triggers.
- [L] WOBBLE: noon still, a touch left of noon (the default) keeps held chords in tune, every step away from noon is heard, both end stops clearly out of tune; left random (never same-same), right a steady sine (ADR 0034, was ADR 0008's one-way zones).
- [A] WOBBLE pitch deviation (cents, on `08_held_tones`) sits in the range measured from the Magneto's WOW & FLUTTER series (takes MW0–MW4): Drift ≈ the 9 o'clock–noon takes, Warble ≈ the 3 o'clock–fully CW takes.
- [A] MIX: CCW = dry only (null vs input, every ATTITUDE), CW = wet only (no dry leakage > −80 dB), noon = equal-power blend. Sweep loudness within ±1.5 dB.
- [A/H] Envelope on MIX CV (5 ms attack): throw lands with no audible lag (smoothing ≤ 5 ms, ADR 0015).
- [H] All 7 knobs respond to CV 0–5 V over their full range.

### M8 — Tuning pass
All four must hold:
- [L] Sweet-spot sweep: owner reviews a grid of renders stepping every knob. No dead zones, no cliffs, every position usable.
- [L] Dub record A/B: on the owner's own material it sits alongside King Tubby / Basic Channel references without sounding like a "digital reverb".
- [L] Wellspring A/B: same family as the spring Reference set, with less Ringing and more splash.
- [A] IR library (ADR 0021): DECAY, TENSION and TONE ranges cover the spread of real-tank T60, chirp spacing/dispersion (ridge method) and brightness.
- [L] Magneto A/B (ADR 0020): holds up next to the Magneto's spring on the same stimulus; the owner would reach for Resilio Versio for dub.
- [H/L] Live session on the module (patching, throws, Kicks): nothing surprises in a bad way.
- Final ranges/curves written back into ParamSpec, and SPEC starting guesses replaced with the tuned values.

### M9 — Polish
- [H] LEDs (ADR 0031): left pair meters In L / In R, right pair Out L / Out R; brightness follows level, green → amber when hot; input red near clip, output red while the limiter pulls down (a loud Howl). Boot pattern, then metering.
- One-page manual: panel map, controls, flashing via NE Firmware Swap, recovery to NE firmware.
- Panel overlay from NE's blank-panel/DXF template, labelled with Resilio Versio controls.
- Tagged release with `.bin` installable via NE Firmware Swap.
- Preset notes: a few documented starting points for classic dub sounds (as JSON presets + prose).

---

## 8. Flashing / distribution

Research 27 Sep 2026 (sources in §11). Status per item.

- **Connection (confirmed, NE Desmodus Versio manual):** power off, take the module out, **unplug the Eurorack power cable**, plug micro-USB into the Daisy Seed on the back of the module. The module runs on USB power alone. Never have rack power and USB connected at once.
- **Primary path: NE Firmware Swap web app with a custom .bin (verified by owner, who has flashed 1st- and 3rd-party firmwares with it many times):** noiseengineering.us/portal/firmware → "Select Custom File" → CONNECT → CHANGE FIRMWARE. The same app restores stock NE firmware, so recovery is known (ADR 0011).
- **Fallback: Daisy Web Programmer / dfu-util (generic Daisy method, not needed unless NE's app fails):** hold BOOT, tap RESET, release BOOT → STM32 system DFU (in ROM, can't be overwritten → very low brick risk). **Unverified for Versio:** whether the Seed's BOOT/RESET buttons are reachable when mounted, and whether NE's app enters DFU for you. → Check physically at M0 before any flash.
- **App size / bootloader:** internal flash is 128 KB. If the app exceeds it, build with the Daisy bootloader (`APP_TYPE = BOOT_SRAM`, up to 480 KB, runs from SRAM; RAM data then goes in DTCM, only 128 KB → delay lines must be placed explicitly). Install the bootloader with `make program-boot`, then flash apps with `make program-dfu` during the bootloader's LED-pulsing grace period. This is likely why some community Versio firmwares "need a different bootloader" (inferred, not stated by the index). The firmware index says bootloader-based firmwares "will not install through Noise Engineering's firmware updater". **Decision (ADR 0011):** plain internal-flash build (≤ 128 KB) so the NE app works. Binary size is checked on every firmware build.
- **Unverified:** whether restoring NE firmware through NE's app also removes the Daisy bootloader.
- **Flash procedure for M0:** first flash = passthrough + control-print test firmware via NE's app (custom file). Owner flashes; Claude produces the .bin.
- NE offers blank panel + DXF overlay templates (World of Versio). Overlay at M9.

### 8.1 Toolchain facts (27 Sep 2026)

- ARM compiler: `brew install --cask gcc-arm-embedded` (official Arm, native arm64). **Not** the Homebrew formula `arm-none-eabi-gcc` (no newlib → `nosys.specs` error). Daisy Toolchain installer is stale (2022, Intel-era).
- `brew install dfu-util cmake ninja`.
- libDaisy `DaisyVersio` confirmed on master. Quirks: knobs pre-inverted (`flip=true`); `ProcessAllControls()` only processes knobs, so `tap.Debounce()` must be called separately; `Gate()` already inverted; LEDs RGB order, inverted; default block 48, 48 kHz / 24-bit.
- Knob + CV sum clips at 0/1 in the analog stage (ADC rails), not in software. CV range 0–5 V (NE manual). Audio inputs clip ~16 Vpp.
- JUCE: now JUCE 9. Free for this use (Starter tier ≤ $20k/yr revenue, or AGPLv3). **Verified 28 Sep 2026:** AU + VST3 build with Command Line Tools only (CMake + Ninja, JUCE 9.0.2) and the AU passes `auval`. No Xcode needed.
- ARM toolchain in use: Arm GNU Toolchain 15.3.rel1 tarball in `~/.local/arm-gnu-toolchain` (sha256 verified). M0 test firmware = 94 KB of 128 KB. **Watch item for M3:** ~35 KB left for DSP code. Mitigations: drop USB logging in release builds, `-Os` on non-audio code.

---

## 9. Risks

| Risk | Mitigation |
|---|---|
| CPU too high (cascades + oversampling) | Multirate lf path; fewer stages in 3-spring mode; oversample only nonlinear blocks |
| Instability at max DECAY + KICKED | Loop sat, g clamp, limiter, NaN guard resets tank |
| Single-tone buildup | §4.10 layered defence + automated metric |
| Drive = volume jump, not colour | Gain compensation; test loudness across DRIVE sweep |
| Narrow sweet spot (Springray complaint) | Perceptual curves; sweep renders reviewed for dead zones/cliffs |
| Zipper noise | Smoothing on all params; slew-limit L |
| Sounds "digital reverb" | Chirp correctness first (M1); tune vs references |
| Clatter = added noise | Feed clatter through tank HF path |
| Plugin ≠ hardware sound | Shared ParamSpec; validate at 48 kHz; compare renders vs hardware recordings |
| Aliasing from saturation | Oversampling; test with high-freq sine sweeps |

---

## 10. Open questions (for grill pass)

- ~~Exact TONE tilt curve and pivot frequency?~~ Character decided (ADR 0017); numbers tuned at M5/M8.
- ~~AntiRes detector type?~~ Only needed if layer 4 is built (ADR 0010); decide then.
- ~~Hold-for-rattle?~~ Not v1 (ADR 0013).
- **Stereo in (open, explore after M3).** Should the Tank keep left/right placement from a stereo input instead of summing to mono (§4.3)? Options: per-Spring L/R blend (cheap, one drive stage); full dual input (two DriveIn/Tilt stages, ~+300–700 Daisy cycles/sample); or plugin-only if the Versio lacks CPU headroom. If plugin-only, implement it as a **Core mode** that the firmware doesn't enable, so both hosts still share one Core and mono settings stay identical across hosts (the ParamSpec parity principle, §6.1). A new ADR is needed before building. Data: Wellspring takes A-L / A-R (recipe).
- No other open design questions remain. Tuning numbers marked "starting guess" are confirmed by measurement or ear at their milestone.

---

## 11. References

1. V. Välimäki, J. Parker, J. S. Abel — "Parametric Spring Reverberation Effect," *JAES* 58(7/8), 547–562, 2010.
2. J. Parker — "Efficient Dispersion Generation Structures for Spring Reverb Emulation," *EURASIP J. Adv. Signal Process.*, 2011.
3. J. Parker, S. Bilbao — "Spring Reverberation: A Physical Perspective," DAFx-09, Como.
4. J. S. Abel, D. P. Berners, S. Costello, J. O. Smith — "Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure," AES 121st Conv., 2006.
5. "Automated Calibration of a Parametric Spring Reverb Model," DAFx-11 — https://www.dafx.de/paper-archive/2011/Papers/39_e.pdf
6. libDaisy Versio header — https://github.com/electro-smith/libDaisy/blob/master/src/daisy_versio.h
7. NE: Create your own Versio firmware. NE Firmware Swap: https://noiseengineering.us/portal/firmware/. NE Desmodus Versio manual: https://noiseengineering.us/manuals/desmodus-versio/. Daisy bootloader: libDaisy `doc/md/_a7_Getting-Started-Daisy-Bootloader.md`, https://github.com/electro-smith/DaisyBootloader.
   NE blog — https://noiseengineering.us/blogs/loquelic-literitas-the-blog/create-your-own-firmware-on-a-versio-module/
8. Versio firmware index — https://github.com/Maxhodges/noise-engineering-firmware-index
9. Forum research (dub spring character, Springray feedback): Gearspace dub/spring threads; ModWiggler "Which spring reverb should I get?", "Intellijel Springray 2?" threads.
10. (Verify before use) J. Chowdhury — "Real-time Physical Modelling for Analog Tape Machines," DAFx-19.

---

## 12. Pre-build steps (complete)

1. ~~Gap review~~: grill rounds 1–2, ADRs 0001–0019.
2. ~~Milestone-criteria interview~~: §7.
3. ~~Freeze spec v1.0~~: 27 Sep 2026. Building from M0.
