# King Tubby's "Big Knob": what it was, and the numbers for Resilio

Research for the Big Knob TONE experiment (`docs/briefs/big-knob-tone.md`), 1 Oct 2026. It feeds ADR 0036 (Proposed) and `docs/m8-tuning-backlog.md` "Big Knob TONE".

**Tags**, as in `docs/dub-spring-reference.md`: **sourced** = two or more independent sources, or a primary one · **single source** · **inference** = our reasoning or circuit maths, not a document.

**Read this first: how the sources were checked.** This cloud session's network policy blocked opening web pages (the proxy refused soundonsound.com, wikipedia.org, mopop.emuseum.com, groupdiy.com, audiothing.net and others). Every web claim below comes from search-engine result text for the linked page, not from reading the full page. Two or more pages agreeing in their search text counts as "sourced". Re-check a claim in a browser before quoting it anywhere public. No original Altec datasheet turned up. The circuit maths (§2, §6) is ours and doesn't depend on the web: `tools/research/big_knob_circuit.py`.

## In plain words

- The Big Knob is a **passive high-pass filter**: two capacitors and one coil (the inductor), no power. Altec built it as a stepped broadcast and film "program equalizer" for cutting rumble. Its model number is the **9069B**. It sat in the MCI desk Tubby bought from Dynamic Sounds in 1972.
- It cuts lows at **18 dB per octave**, through 10 fixed steps from **70 Hz to 7.5 kHz**. Each click up the switch thins the sound further, until it sounds like a telephone, then a squeak.
- **Wired as Altec designed it, it has no bump.** It is a textbook flat ("Butterworth") filter, but only when it's fed and loaded at exactly 600 ohms. Wire it into a desk that feeds it from a low-impedance output and reads it with a high-impedance input, and it **peaks just above the cutoff**. That peak is the nasal, ringy part. How big it was on Tubby's desk isn't documented. The maths says somewhere between a few dB and +16 dB, depending on the wiring.
- **Coil saturation (the "grit") isn't documented.** Altec rated the unit up to +28 dBm, far above desk levels, so the original was probably clean. Emulations that add saturation say it's their own addition.

## 1. Origins

| Claim | Tag | Source |
|---|---|---|
| In 1972 Byron Lee moved Dynamic Sounds to 16-track. Tubby bought its older MCI console, a 12-input, 4-output desk from the mid-1960s. | sourced | [RBMA: The Roots of Dub](https://daily.redbullmusicacademy.com/2018/08/the-roots-of-dub/) · [MoPOP collection record](https://mopop.emuseum.com/objects/95703/mci-mixing-console-formerly-owned-and-operated-by-king-tubby) · [Dread Editions: the MCI and the Big Knob](https://www.dreadeditions.com/single-post/2020/06/11/uk-tubbys-essentials-the-mci-the-big-knob) |
| One page says 16 channels instead of 12. | single source | (outlier; MoPOP and RBMA both say 12) |
| It was Dynamic's Studio B desk. Bunny Lee advised the purchase, and it went to 18 Dromilly Avenue, Waterhouse. | single source | Dread Editions |
| The console is now at MoPOP in Seattle, with the knob still fitted. | sourced | MoPOP · Dread Editions |
| A large red knob at the top right, labelled "Hi Pass Filter", which Tubby called "the Big Knob". | sourced | MoPOP · [Gearnews: AudioThing Dub Filter](https://www.gearnews.com/audiothing-dub-filter/) |
| The filter was already on the desk when Tubby bought it. What he added was how he used it. | single source | MoPOP. RBMA only says it "seems" to have been a custom addition to cut lows from mic channels. |
| A forum post says it was an "Altec-Lansing HP filter tied to the patchbay" on a "MCI pre-400 series, custom unit". | single source (forum) | [interruptor.ch dub board](http://www.interruptor.ch/php5/dubboard/viewtopic.php?t=1040) |
| **Who fitted it** (Dynamic's engineers, MCI to order, or someone else) isn't documented anywhere we found. | gap | — |
| The filter is the **Altec 9069B** (also sold as the 9069A). It is the high-pass half of the 9067A/B high- and low-pass filter set; the low-pass half is the 9068. Sales listings call it a "600Ω Program Equalizer". Made from the 1960s into the mid-1970s. | sourced | [Reverb: Altec 9067B listing](https://reverb.com/item/3644813-altec-9067b-filter) · [WorthPoint: Altec 9069/9068 listing](https://www.worthpoint.com/worthopedia/altec-lansing-9069-9068-600-program-1885318658) · [KMR Audio: Audio Merge KTBK-1B](https://kmraudio.com/products/audio-merge-ktbk-1b-king-tubby-s-big-knob) |
| Designed by Arthur Davis at Altec. | single source in practice (KMR and a groupDIY thread, which may both trace back to Audio Merge) | KMR · [groupDIY: 9069B multitap inductor](https://groupdiy.com/threads/altec-9069b-multitap-inductor.74452/) |
| Gearnews and AudioThing call Tubby's studio "Federal". That is very likely wrong: Federal was a different Kingston studio. Not repeated here. | inference | Gearnews |

**Lore vs documents.** Stories that Tubby built or rewired the filter himself aren't supported by anything we found. Neither is the occasional "parametric EQ" label: it is a stepped high-pass.

## 2. The circuit

| Claim | Tag | Source |
|---|---|---|
| A passive **constant-k "T" section**: a capacitor in series, a coil to ground, another capacitor in series. **3rd order, 18 dB/oct, 600 ohm.** The coil is a toroid with one tap per step. | sourced | [groupDIY: 9069B multitap inductor](https://groupdiy.com/threads/altec-9069b-multitap-inductor.74452/) · [groupDIY: 9069 B high pass filter](https://groupdiy.com/threads/altec-9069-b-high-pass-filter.73747/) · [Soundgas: KTBK](https://soundgas.com/products/audio-merge-king-tubbys-big-knob-ktbk) · [Sound On Sound: Audio Merge KTBK](https://www.soundonsound.com/news/audio-merge-ktbk-passive-filter) |
| **Steps: 70, 100, 150, 250, 500 Hz, 1, 2, 3, 5, 7.5 kHz.** | sourced | Sound On Sound · Soundgas · [KVR: Melda TubFilter](https://www.kvraudio.com/forum/viewtopic.php?t=573265) · MoPOP ("ten settings") |
| Some sources say 11 steps over 165° of rotation; a clone uses an 11-position switch. Ten frequencies on eleven positions suggests one position is flat (bypass). | sourced claim; the flat position is inference | Dread Editions · [Westfinga](https://www.westfinga.com/gears) |
| A cloner's part values: coil taps 698 mH (70 Hz) down to 6.3 mH (7.5 kHz); series capacitors 3.77 µF down to 35.2 nF. | single source | groupDIY 74452 |
| **Check:** a 600 ohm T section needs a coil of 600/(4π·fc) and capacitors of 2/(4π·fc·600). That gives 682 mH and 3.79 µF at 70 Hz, and 6.37 mH and 35.4 nF at 7.5 kHz. Every listed value is within ~5 %, so the values are credible. | inference (calc) | — |
| Altec's catalog text, as quoted in a listing: the slope stays "constant at 18 dB per octave" at every cutoff. "Zero insertion loss" lets it go anywhere in a line "having a level from -70 to +28 dbm" without a power supply. | single source (closest thing to a primary source we found) | Reverb 3644813 |
| "Zero insertion loss" (Altec) and the KTBK clone's "6 dB insertion loss" in its 600 mode describe the same behaviour. Telephone practice measures loss against a 600 ohm source wired straight to a 600 ohm load, which already loses 6 dB. | inference | Reverb 3644813 · KMR |
| No make-up gain: it is passive. | sourced (by construction) | Reverb 3644813 |

### Where the bump comes from (inference: circuit maths)

`tools/research/big_knob_circuit.py` models the T section at its 600 ohm values with different source and load impedances, plus the coil's winding loss (coil Q ≈ 10 at the cutoff). Results are the same at every step because the shape scales with the cutoff.

| How it's wired | Peak | Where | Notes |
|---|---|---|---|
| 600 ohm in, 600 ohm out (as designed) | none | — | Exactly a 3rd-order Butterworth: −3 dB at the step, flat above, 18 dB/oct below. Real coil loss softens the knee slightly. |
| 600 ohm in, bridging (10 kohm) out | none | — | The knee softens: −3 dB moves to ~1.4× the step, and the slope near it falls towards 12 dB/oct. |
| Low-impedance in (~0 ohm), 600 ohm out | +2.3 dB | 1.3× step | Mild. |
| Low-impedance in, ~900 ohm–1.2 kohm out | +5 to +7 dB | 1.4× step | Medium. **This is the size the bump voicing uses fully right** (§6). |
| Low-impedance in, 2.4 kohm out | +12 dB | 1.4× step | Strong. |
| Low-impedance in, bridging (10 kohm) out | +16 to +19 dB | 1.4× step | A sharp ring, coil Q 10 vs ideal. |

So **the bump needs a low source impedance and a high load together.** That fits a 1960s–70s transistor desk. A transistor line output is low-impedance, and a bridging input is high-impedance. **How the filter was actually wired in the MCI, and so the real size of Tubby's peak, isn't documented.** King Jammy's "squawky sounds" (§4) fit a peak, but switch clicks would explain them too. Emulators treat the peak as part of the sound. AudioThing's Impedance control "acts as a resonance", and the KTBK's BELL mode has a six-step resonance switch (§5).

**Inference.** A mis-terminated filter's three poles can be written exactly as a 1st-order corner times a 2nd-order resonant pair. That is how the Renderer models it, so the digital filter is the circuit's own shape, not an approximation. Example: low-impedance in, 900 ohm out gives the pair at 1.28× the step with Q 2.07, and the corner at 0.82× the step.

**Measurements** of an original 9069 or of the KTBK: none found.

## 3. What the coil adds

- **No measurements of saturation, hysteresis, winding resistance or Q** for an original Altec toroid, or for any clone. (gap)
- The catalog's input rating of up to +28 dBm (~19.5 V RMS into 600 ohm) suggests the cores stayed linear at desk levels. So **the original was probably clean**. (inference from a single source)
- A coil saturates on magnetic flux, which is the integral of the voltage. Loud **lows** saturate it first. If it ever saturated, it would get softer and grittier on loud bass first. (inference, physics; the same idea is already in DriveIn's input transducer, `DriveVoicing.h` `kFluxHz`)
- A clone seller says good inductors "avoid the 'ringy' effect associated with bad inductors". That's marketing copy, but it hints that low-quality coils ring more. (single source: [Reverb: 9069 clone with Cinemag inductors](https://reverb.com/item/63646630-altec-9069-clone-with-cinemag-inductors))
- AudioThing's Magnetism, Character and Dynamics controls (a level-dependent inductance, harmonics, an envelope-like movement) are described as their own additions, not as measured traits. (sourced as product copy; [Sonicstate 2024](https://sonicstate.com/news/2024/03/28/king-tubbys-big-knob-emulated/), Gearnews)

## 4. How Tubby used it

| Claim | Tag | Source |
|---|---|---|
| Switched through its steps on the reverb and echo sends and returns. Stepping through the bands on reverb gave an "other-worldly" effect. | sourced | [Erik Söderberg: Vintage Filter Suite](https://blog.eriksoderberg.se/post/620102437176541184/recent-work-vintage-filter-suite-king-tubbys) · [Reason Studios: Vintage Filter Suite](https://www.reasonstudios.com/shop/bundle/vintage-filter-suite/) · Gearnews |
| The sound's traits: **audible steps, clicks and "crunches" when switching, and a phasing effect** when filtered and dry are blended. | sourced (AudioThing copy repeated by several sites) | Gearnews · Dread Editions |
| King Jammy: a high-pass filter "that made some squawky sounds when you change the frequency". | single source (seen quoted; the original interview wasn't located) | search text around [KVR: recreating the Tubby filter](https://www.kvraudio.com/forum/viewtopic.php?t=217939) |
| Scientist: "sometimes you can hear like the frequency changin'". | single source (forum excerpt) | interruptor.ch |
| The phasing: a 3rd-order filter shifts phase ~225° at the cutoff. Blending dry with filtered gives a dip of about −11 dB just below the cutoff, and the dip moves with each step. | inference (calc) | — |
| Tubby's spring was a modified Fairchild; his echo was an MCI 2-track tape machine (AudioThing's research). | sourced (one origin, AudioThing) | [Sonicstate 2021](https://sonicstate.com/news/2021/02/03/king-tubbys-dub-fx-in-a-plug-in/) · [MusicRadar](https://www.musicradar.com/news/audiothing-models-king-tubbys-studio-gear-in-the-alborosie-dub-station-plugin) |
| Academic study: Sean Williams on Tubby repurposing the desk's high-pass and the 4-track ("ten notched frequency steps"). | single source (academic) | [Williams, "Tubby's dub style" (Edinburgh)](https://www.research.ed.ac.uk/en/publications/tubbys-dub-style-the-live-art-of-record-production/) · [Open Research Online](https://oro.open.ac.uk/48773) |

**Where to listen.** One source (Dubmatix, via [Bass Culture](https://bassculture.substack.com/p/the-heart-of-dub-filter-sweeps-and)) points to *King Tubby Meets Rockers Uptown* (Augustus Pablo, 1976), with the filter on the hi-hat throughout. That's a single source, and we found no timestamped track list. Listen for a hi-hat or snare that suddenly goes thin and nasal, then steps back, and for reverb returns that go telephone-thin. The steps are audible as jumps. Ours is smooth on purpose (owner).

## 5. Existing emulations: what their makers decided matters

| Product | What it exposes | Source |
|---|---|---|
| **AudioThing Dub Filter** (2024, plugin) | Stepped frequency (70 Hz–7.5 kHz, 18 dB/oct); **Impedance** (acts as resonance); **Magnetism**, **Character**, **Dynamics** (a non-linear inductor model, their addition); **Artefacts** (switch-click level). | Gearnews · Sonicstate 2024 · [AudioThing](https://www.audiothing.net/effects/dub-filter/) |
| **AudioThing Alborosie Dub Station** (2021, plugin) | "Filter Man" (the Big Knob), "Spring Bling" (Tubby's spring), "Echowuk" (the MCI tape echo). | Sonicstate 2021 · MusicRadar |
| **Audio Merge KTBK / KTBK-1B** (hardware, hand-wound multitap coils) | The 10 steps. Modes: **600** (the original response, 6 dB loss), **BELL** (a resonant peak above the cutoff, no loss, **6-step resonance switch**), **NOTCH** (their own addition). | Sound On Sound · KMR · Soundgas |
| **MeldaProduction TubFilter** | The 10 steps plus key tuning, resonance. Its designer suggests it around a spring return. | KVR 573265 |
| **Reason Vintage Filter Suite** (HighPass 1012) | "Inspired by": a 2-pole filter with stepped cutoffs, peak, drive. | Reason Studios · Söderberg |
| **Westfinga WF Filter V2**, a Ken Howard Eurorack 9069 | Passive hardware clones. | Westfinga · [ModularGrid](https://modulargrid.net/e/other-unknown-9069-high-pass-filter) |

**What they agree on:** the steps, the 18 dB/oct slope, and a **resonance or impedance control** (AudioThing, KTBK, Melda). Every one of them treats the peak as adjustable rather than one true value. Only AudioThing models saturation, and it calls that its own addition.

## 6. Numbers for Resilio

| Ingredient | Original | Resilio (voicings in `DriveVoicing.h`) | How close |
|---|---|---|---|
| Slope | 18 dB/oct, 3rd order (sourced) | 18 dB/oct, 3rd order (measured 17.9–18.1 dB/oct, test_drive "bigknob") | Exact. |
| Cutoff | 10 steps, 70 Hz–7.5 kHz (sourced) | Smooth, no steps (owner): 20 Hz at noon (today's, unchanged) → ~170 Hz at TONE 0.7 → ~490 Hz at 0.85 → **1.2 kHz fully right** | **Departs on purpose.** Smooth (owner's decision). Capped at 1.2 kHz because the Springs only respond ~200 Hz–4 kHz, so above ~2 kHz there's nothing left to excite them. That covers the Altec's 70 Hz–1 kHz steps; the Altec's 2–7.5 kHz steps would just mute the tank. |
| Shape when matched | Flat 3rd-order Butterworth (inference, calc) | Voicing 1: same | Exact (digital: −3 dB lands within 1–6 % of the nominal cutoff, worst at 1.2 kHz). |
| Bump | Depends on the desk's wiring: 0 to +16 dB at ~1.3–1.4× the step (inference, calc); Tubby's real size undocumented | Voicings 2–3: the circuit's own poles, moving from "matched" at noon to "low-impedance source into ~1 kohm, coil Q ~10" fully right: +2.2 dB at TONE 0.7, +4.1 at 0.85, **+5.6 fully right**, at 1.36–1.39× the cutoff | The shape is the circuit's exactly. The size is a choice: the "medium" wiring, between the KTBK-style mild bump and a bridging input's +16 dB. It grows as TONE turns (owner's brief), where the real one was fixed by the wiring. Picked by measurement (backlog). |
| Level | Passive: 6 dB loss when matched, ~0 dB when peaking | Makes up what the thinning takes out of the tank, so loudness stays within ±3 dB (test_drive) | Departs (a guarantee, ADR 0017). |
| Coil saturation | Undocumented, probably little (+28 dBm rating) | Voicing 3 only: loud lows pushed harder into DriveIn's existing coil-flux saturator, more as DRIVE rises | **A guess.** The weakest-sourced ingredient; offered because the owner asked to explore it. |
| Phasing against dry | Real on Tubby's blends (sourced as a trait) | Not modelled: the tail isn't the same sound as the dry, so blending with MIX won't phase much | Left out (`dub-spring-reference.md` §6B.4). |
| Switch clicks / steps | Part of the original's sound (sourced) | Left out (owner: no clicks or steps) | Departs on purpose. |

## Sources

All seen through search-engine text only (see the note at the top).

- [Sound On Sound: Audio Merge KTBK passive filter](https://www.soundonsound.com/news/audio-merge-ktbk-passive-filter)
- [Gearnews: AudioThing Dub Filter](https://www.gearnews.com/audiothing-dub-filter/)
- [Gearnews: King Tubby Big Knob hardware replica](https://www.gearnews.com/king-tubby-big-knob-a-new-hardware-replica-of-the-altec-9069b-filter/)
- [Sonicstate: King Tubby's Big Knob emulated (2024)](https://sonicstate.com/news/2024/03/28/king-tubbys-big-knob-emulated/)
- [Sonicstate: King Tubby's dub FX in a plug-in (2021)](https://sonicstate.com/news/2021/02/03/king-tubbys-dub-fx-in-a-plug-in/)
- [MusicRadar: Alborosie Dub Station](https://www.musicradar.com/news/audiothing-models-king-tubbys-studio-gear-in-the-alborosie-dub-station-plugin)
- [AudioThing: Dub Filter](https://www.audiothing.net/effects/dub-filter/)
- [KMR Audio: Audio Merge KTBK-1B](https://kmraudio.com/products/audio-merge-ktbk-1b-king-tubby-s-big-knob)
- [Soundgas: Audio Merge KTBK](https://soundgas.com/products/audio-merge-king-tubbys-big-knob-ktbk)
- [MoPOP: MCI console formerly owned by King Tubby](https://mopop.emuseum.com/objects/95703/mci-mixing-console-formerly-owned-and-operated-by-king-tubby)
- [Red Bull Music Academy: The Roots of Dub](https://daily.redbullmusicacademy.com/2018/08/the-roots-of-dub/)
- [Dread Editions: the MCI and the Big Knob](https://www.dreadeditions.com/single-post/2020/06/11/uk-tubbys-essentials-the-mci-the-big-knob)
- [interruptor.ch dub board thread](http://www.interruptor.ch/php5/dubboard/viewtopic.php?t=1040)
- [groupDIY: Altec 9069B multitap inductor](https://groupdiy.com/threads/altec-9069b-multitap-inductor.74452/)
- [groupDIY: Altec 9069 B high pass filter](https://groupdiy.com/threads/altec-9069-b-high-pass-filter.73747/)
- [Reverb: Altec 9067B filter (catalog text)](https://reverb.com/item/3644813-altec-9067b-filter)
- [Reverb: Altec 9069 clone with Cinemag inductors](https://reverb.com/item/63646630-altec-9069-clone-with-cinemag-inductors)
- [WorthPoint: Altec 9069/9068 600Ω Program Equalizer](https://www.worthpoint.com/worthopedia/altec-lansing-9069-9068-600-program-1885318658)
- [KVR: Melda TubFilter](https://www.kvraudio.com/forum/viewtopic.php?t=573265)
- [KVR: recreating the Tubby filter](https://www.kvraudio.com/forum/viewtopic.php?t=217939)
- [Reason Studios: Vintage Filter Suite](https://www.reasonstudios.com/shop/bundle/vintage-filter-suite/)
- [Erik Söderberg: Vintage Filter Suite / King Tubby's](https://blog.eriksoderberg.se/post/620102437176541184/recent-work-vintage-filter-suite-king-tubbys)
- [Westfinga gear](https://www.westfinga.com/gears)
- [ModularGrid: 9069 High Pass Filter](https://modulargrid.net/e/other-unknown-9069-high-pass-filter)
- [Bass Culture: filter sweeps](https://bassculture.substack.com/p/the-heart-of-dub-filter-sweeps-and)
- [Williams: Tubby's dub style (Edinburgh research)](https://www.research.ed.ac.uk/en/publications/tubbys-dub-style-the-live-art-of-record-production/) · [Open Research Online](https://oro.open.ac.uk/48773)
- Circuit maths: `tools/research/big_knob_circuit.py` (this repo)
