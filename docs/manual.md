# Resilio Versio — manual

*Draft for M9. Resilio Versio is a dub spring reverb for the Noise Engineering Versio: a simulated spring tank you can drive, splash, throw into and push into feedback, with a tape echo in front of it on the third switch position.*

**Words used here.** The **Tank** is the whole reverb, with 1–2 **Springs** inside. The **tail** is the sound ringing on after you stop. The **chirp** (or "boing") is the springy up-sweep on each echo. A **throw** opens the springs to the input for a moment (one snare, one stab) and lets the tail ring on. **Howl** is the tank feeding back on itself. The **Hold** is the tail left hanging as a bed under what you play.

**New names (v1.0.43).** BLEND was MIX; the TANK switch was SPRINGS, and its right position ECHO was 3; ATTITUDE's TAPE and VALVE were DRIVEN and KICKED; the button is THROW / TAP. Same sound, same positions.

## Panel map

Resilio runs behind the stock Versio panel: the printed labels don't match yet, so use this map. Pots are named by position, top to bottom, left to right (P1–P7).

```
   P1 BLEND      [LED LED  LED LED]        P2 DECAY
                  In L In R Out L Out R
                    P3 TONE
   P4 SPLASH                               P5 TENSION
                    P6 WOBBLE
   SW  TANK      1 · 2 · ECHO (top toggle)          P7 DRIVE
   SW  ATTITUDE  CLEAN · TAPE · VALVE (bottom)   (button = THROW / TAP)

   [ 12 jacks: 7 CV ins, gate in, In L/R, Out L/R ]
```

Each pot has a CV input; the printed panel labels each CV jack with its pot, and here the CV follows the pot's Resilio function. The gate input and the audio jacks keep their printed labels. <!-- confirm jack positions against the owner's printed panel before release -->

## Controls

Clock positions are approximate: fully left (7 o'clock) is 0, noon is 0.5, fully right (5 o'clock) is 1.

| Control | What it does |
|---|---|
| **P1 BLEND** | Dry ↔ wet. Fully right is 100 % wet, for a send/return (the classic dub way: throw the snare into it). Fully left is your dry signal, untouched, in every ATTITUDE. |
| **P2 DECAY** | How long the tail rings: from a quick slap (~0.4 s) to a long wash (~9 s). It always fades out, except at the very top (past ~4 o'clock), which depends on ATTITUDE: in **CLEAN and TAPE** it is the **Hold**: the tail stretches out toward minutes and sits as a bed under what you play, never louder than what went in; new sound still joins it, a little quieter, and the whole bed dips out of the way of your kick and bass (not snares, hats or chords). In **VALVE** the top lets the tank **Howl**. Pull DECAY back and either one falls into a normal tail and fades. In TANK ECHO, DECAY is the echo's feedback instead (below). |
| **P3 TONE** | Left: warm, dark dub (drips and boing still there). Noon: neutral. Right: King Tubby's **Big Knob**, a steep low cut on the reverb's return, sweeping up to 800 Hz fully right: the further right, the more telephone-like and splashy. It sits after the springs, so turning it thins the tail already ringing at once, and turning back gives its body back. Sharp hits (snares, rimshots) also get a nasal "ring" just above the cut, like his desk filter; chords, pads and held sounds get the plain cut. Level stays about the same across the knob. |
| **P4 SPLASH** | How hard the hits hit the springs. A loud, sudden hit rings the springs with its own highs (a bright clang), and in TAPE and VALVE a drum hit also bites the input harder (grit, and a bigger tail). Chords get the clang, not the bite. Nothing is added: it's your hit, hitting harder. It works the same at any DRIVE, so a quiet line-level send with DRIVE fully down still splashes. The last quarter (past ~3 o'clock) is much bigger: unmistakable on any hit. Ghost notes in a groove stay quiet. Down = no splash at all, only a faint pitch wobble on hard hits in TAPE and VALVE. |
| **P5 TENSION** | Which tank is fitted. Right = tight: short tank, quick repeats, small bright chirp. Left = loose: long tank, slow repeats, big dark boing. Turning it while the tail rings bends the pitch, like tightening or slackening a string. In TANK ECHO, TENSION is the echo time instead (below). |
| **P6 WOBBLE** | Pitch movement, both ways from noon. Noon: still. Left: tape-like drift, a smooth random wow with a faster flutter on top that never repeats; gentle just left of noon (held chords stay in tune), wild fully left. Right: a steady wobble (a sine whose speed drifts a touch), up to obvious worn-tape warble fully right. A tiny amount is always on, even at noon. CV adds to the knob: from fully left, CV sweeps drift → still → warble. In TANK ECHO it moves the echo's tape too. |
| **P7 DRIVE** | The tank's INPUT: how hard your signal hits it. Up to +24 dB of gain before anything else, so a quiet mixer send (peaks around −18 to −24 dBFS) drives the tank like a hot one once DRIVE is up. SPLASH doesn't depend on it: hits splash the same at any DRIVE. Colour and grit build with it (clean-ish to ~9 o'clock, driven from ~3 o'clock), stepping up CLEAN < TAPE < VALVE, and the tail gets a few dB louder (about +6 dB from 7 to 5 o'clock), never shorter. On hot material at full BLEND the output limiter starts catching peaks from about 1 o'clock. |
| **TANK** (top toggle) | Up: **1**, one Spring, sparse and the most splashy. Centre: **2**, two Springs, the classic tank. Down: **ECHO**, echo mode: a tape echo into the two-Spring tank (below). Switching crossfades, so it's safe mid-tail; into ECHO the echo fades in on a fresh tape, and out of it its last repeats ring on in the springs. |
| **ATTITUDE** (bottom toggle) | Named for the saturation. Up: **CLEAN**, a polite, linear tank; the reverb isn't bit-reduced. Centre: **TAPE**, tape saturation, the core dub colour, and the reverb at **12-bit µ-law**: a fine grain. Down: **VALVE**, a spring in a cranked valve amp: hard, lopsided saturation inside the tank, the growl of an overdriven valve stage, rattle and lurch on hits, and Howl at DECAY's top; the reverb at **10-bit µ-law**: coarse, clearly gritty. **The grit:** in TAPE and VALVE the reverb runs through a 24 kHz µ-law converter, the companded grain of early digital delays and samplers: coarse steps on loud sounds, fine ones on quiet tails, so a fading tail ends in grain, then silence; the top octave (above ~11 kHz) is cut cleanly, with no aliasing. It sits before TONE, so TONE right of noon thins the grit with the tail. Flipping it changes the tail already ringing. The dry signal is never coloured or bit-reduced: BLEND fully left is a clean passthrough in every ATTITUDE. |
| **Button = THROW** (TANK 1–2) | A throw by hand: while you hold the button the springs hear the input; let go and they don't, and what is already ringing rings on. The first press switches **throw mode** on (until then the reverb works as usual), so from then on the springs only hear what you hold the button for. Tap it quickly or slowly, hold it for a bar: every press is a throw. With the gate patched too, the springs hear the input while either is on. **To go back to always-on: tap, then press again straight away and keep holding for 2 s.** The LEDs blink white once, and from then the reverb works as usual until the next press or gate. |
| **Button = TAP** (TANK ECHO) | Tap the echo's tempo: each tap is a beat (a quarter note); TENSION then picks the division, as with a clock in the gate. Three or four taps settle it; it stays when you stop tapping. A single tap on its own, then nothing for ~2 s, lets it go (back to TENSION's free time). If a clock is also in the gate, whichever you changed last wins. The LEDs flash purple on each tap, then pulse purple on the tapped beat for 4 s. A press in ECHO never throws. |
| **Gate in = THROW** (TANK 1–2; in ECHO the gate is the echo's clock) | Dub's throw, from a sequencer: while the gate is high (above ~2 V) the springs hear the input; while it is low they don't, and what is already ringing rings on. Nothing happens until the gate first goes high, so with nothing patched the reverb works as usual. To go back to always-on, tap, tap and hold the button 2 s; the next gate switches throw mode on again. |
| **CV ins** | 0–5 V, added to the pot's position (set the pot low to give CV room). BLEND, TONE, SPLASH and DRIVE follow CV quickly; DECAY, TENSION and WOBBLE glide, so they don't zipper. |
| **In L / In R** | Stereo in. Patch In L only for mono: it feeds both sides. Inputs clip at about 16 V peak-to-peak. |
| **Out L / Out R** | Stereo out. A mono input still comes out wide. |

### Echo mode (TANK ECHO)

A tape echo in front of the two-Spring tank: each repeat lands in the springs as a fresh hit and is heard directly too (most of what you hear is the repeats themselves, wide; a quarter goes through the springs, which are fixed at a medium tank). Three knobs change job in ECHO only:

- **DECAY** = feedback: how many repeats. Fully down, one repeat about 10 dB down; noon, a few; then longer builds. At the very top the repeats hold, held up by the tape's saturation, never running away; bring DECAY down and they die away.
- **TENSION** = echo time: 2 s fully left, ~0.4 s at noon, 80 ms fully right. With a clock (the gate, or tapped on the button) it picks a division of the beat instead, seven zones from left to right: 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16.
- **Gate** = the clock: one pulse per quarter note. The tempo holds when the pulses stop (stopping and starting a sequencer doesn't move the echo); a single pulse on its own lets it go.

Every change of echo time swoops like tape (the repeats bend in pitch for a moment), never a jump. WOBBLE moves the tape; TONE and ATTITUDE shape the repeats along with the springs. There is no Hold in ECHO, and DECAY's top holds the repeats rather than Howling.

### LEDs

- **Left two: input level** (In L, In R). **Right two: output level** (Out L, Out R).
- Green → amber → red. **Input red:** you're near clipping at the jack, so turn the source down. **Output red:** the safety limiter is catching peaks (normal in a big Howl; if it's red all the time, lower DRIVE or DECAY). Held sounds (pads, drones, organ) rarely get there: the tank notices a sound being held and gently turns down what goes into the springs, so the wet stays clear of the limiter. Hits and stabs are never touched, and the tail after you stop still rings its full length.
- **White blink** (all four, once): throw mode just went off.
- **Purple** (all four): tap tempo in TANK ECHO. A flash on each tap, then a pulse on each beat of the tapped tempo for 4 s, then the meters again.
- **At power-up** a short colour sweep across the four LEDs says the firmware has loaded.

## Quick start

1. Patch a snare or rim hit into **In L**; take **Out L** and **Out R** to your mixer.
2. Set everything to noon, TANK centre (2), ATTITUDE centre (TAPE).
3. **BLEND** to ~2 o'clock so you clearly hear the tank; bring it back down once you've found the sound.
4. **DECAY**: noon is a ~2 s tail. Turn it up for long dub throws.
5. **Throw**: hold the button while the snare plays, let go, and hear the tail ring on. (The first press switches throw mode on; tap, tap and hold 2 s to go back.) Then try TENSION left for a big boing, SPLASH up for a harder clang, TONE right to thin it out like Tubby's Big Knob, ATTITUDE right (VALVE) and DECAY past 4 o'clock for Howl.
6. **Echo**: TANK right (ECHO), tap the button on the beat a few times, set TENSION to the division you want and ride DECAY for longer builds.

## Installing (Noise Engineering Firmware Swap)

**Never connect USB and Eurorack power at the same time.** Power off, take the module out of the rack, and unplug the rack power cable first.

1. Plug a micro-USB cable into the Daisy Seed on the back of the module. It runs on USB power alone.
2. In Chrome, open **noiseengineering.us/portal/firmware**.
3. Choose **Select Custom File** and pick `resilio_versio.bin`.
4. **CONNECT**, then **CHANGE FIRMWARE**. Wait until it reports done.
5. Unplug USB, then reconnect rack power. The LEDs play the boot sweep.

## Going back to Noise Engineering's firmware

Same app, same steps (USB only, rack power unplugged): at step 3, pick the Noise Engineering firmware you want (for example your module's original one) from the app's list instead of a custom file, then **CONNECT** → **CHANGE FIRMWARE**. Resilio doesn't install a bootloader or change anything else, so this fully restores the module.
