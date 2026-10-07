---
title: "Manual: Resilio Versio controls, echo mode and the throw"
description: "The player's guide to Resilio Versio: what BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE and DRIVE do, the TANK and ATTITUDE switches, the THROW / TAP button, echo mode, the LEDs, CV and a quick start."
slug: "manual"
order: 2
---

# Manual

Resilio Versio is a dub spring reverb for the Noise Engineering Versio: a simulated spring tank you can drive, splash, throw into and push into feedback, with a tape echo in front of it on the TANK switch's third position. The plugin has the same panel and the same sound.

<p class="subhead">Terminology</p>

<dl class="terms">
<dt>Tank</dt><dd>The whole reverb, with one or two springs inside.</dd>
<dt>Tail</dt><dd>The sound ringing on after you stop.</dd>
<dt>Chirp</dt><dd>The springy up-sweep (the "boing") on each echo.</dd>
<dt>Throw</dt><dd>Opening the springs to the input for a moment (one snare, one stab) and letting the tail ring on.</dd>
<dt>Howl</dt><dd>The tank feeding back on itself.</dd>
<dt>Hold</dt><dd>The tail left hanging as a bed under what you play.</dd>
</dl>

## Panel map

Resilio runs behind your Versio's printed panel, so the printed labels don't match: use this map. Knobs are counted in reading order, top to bottom, left to right.

<!-- PANEL: assets/panel/resilio-versio-panel.svg -->

| Panel location | Resilio control |
|---|---|
| Top left knob | **BLEND** |
| Top right knob | **DECAY** |
| Centre knob, upper | **TONE** |
| Left knob, middle row | **SPLASH** |
| Right knob, middle row | **TENSION** |
| Centre knob, lower | **WOBBLE** |
| Right knob, lowest | **DRIVE** |
| Top switch | **TANK**: 1 · 2 · ECHO (up, centre, down) |
| Bottom switch | **ATTITUDE**: CLEAN · TAPE · VALVE (up, centre, down) |
| Button | **THROW / TAP** |
| Four LEDs, left to right | Input L, input R, output L, output R |
| Jacks, top two rows | CV for each knob, in the order drawn (BLEND, TONE, DECAY, DRIVE; SPLASH, WOBBLE, TENSION), and the THROW gate |
| Jacks, bottom row | In L, In R, Out L, Out R |

## Controls

Clock positions are approximate: fully left (7 o'clock) is the minimum, noon is the middle, fully right (5 o'clock) is the maximum.

### BLEND

Dry ↔ wet. Fully right is 100 % wet, for a send/return: the classic dub way, throwing the snare into it. Fully left is your dry signal, untouched, in every ATTITUDE.

### DECAY

How long the tail rings: from a quick slap (about 0.4 s) to a long wash (about 9 s). It always fades out, except at the very top (past about 4 o'clock), which depends on ATTITUDE:

- **CLEAN and TAPE: the hold.** The tail stretches out toward minutes and sits as a bed under what you play, never louder than what went in. New sound still joins it, a little quieter, and the whole bed dips out of the way of your kick and bass (not snares, hats or chords).
- **VALVE: the howl.** The tank feeds back on itself: a rough, moving roar that keeps going after you stop. Never a clean tone.

Pull DECAY back and either one falls into a normal tail and fades. In TANK ECHO, DECAY is the echo's feedback instead (see Echo mode).

### TONE

- **Left:** warm, dark dub. The drips and the boing are still there.
- **Noon:** neutral.
- **Right:** King Tubby's **“Big Knob”**, a steep low cut on the reverb's return, sweeping up to 800 Hz fully right: the further right, the more telephone-like and splashy.

Because it sits after the springs, turning TONE right thins the tail already ringing, at once, and turning back gives the body back. Sharp hits (snares, rimshots) also get a nasal "ring" just above the cut, like Tubby's desk filter; chords, pads and held sounds get the plain cut. The level stays about the same across the knob.

### SPLASH

How hard your hits hit the springs. A loud, sudden hit rings the springs with its own highs (a bright clang), and in TAPE and VALVE a drum hit also bites the input harder: grit, and a bigger tail. Chords get the clang, not the bite.

Nothing is added: it's your hit, hitting harder. It works the same at any DRIVE, so a quiet line-level send with DRIVE fully down still splashes. The last quarter (past about 3 o'clock) is much bigger: unmistakable on any hit. Ghost notes in a groove stay quiet. Fully down: no splash at all, only a faint pitch wobble on hard hits in TAPE and VALVE.

### TENSION

Which tank is fitted.

- **Right = tight:** a short tank, quick repeats, a small bright chirp.
- **Left = loose:** a long tank, slow repeats, a big dark boing.

Turning it while the tail rings bends the pitch, like tightening or slackening a string. In TANK ECHO, TENSION is the echo time instead (see Echo mode).

### WOBBLE

Pitch movement, both ways from noon.

- **Noon:** still. (A tiny amount of movement is always on, even here.)
- **Left: drift.** Tape-like: a smooth random wow with a faster flutter on top, never repeating. Gentle just left of noon (held chords stay in tune), wild fully left.
- **Right: warble.** A steady wobble whose speed drifts a touch, up to obvious worn-tape warble fully right.

CV adds to the knob: from fully left, a rising CV sweeps drift → still → warble. In TANK ECHO it moves the echo's tape too.

### DRIVE

The tank's input: how hard your signal hits it. Up to +24 dB of gain before anything else, so a quiet mixer send (peaks around −18 to −24 dBFS) drives the tank like a hot one once DRIVE is up.

Colour and grit build as you turn it up (clean-ish to about 9 o'clock, driven from about 3 o'clock), stepping up CLEAN < TAPE < VALVE. The tail gets a few dB louder across the knob (about +6 dB from fully left to fully right), never shorter. SPLASH doesn't depend on DRIVE. On hot material at full BLEND the output limiter starts catching peaks from about 1 o'clock.

### TANK (top switch)

- **1** (left): one spring. Sparse and the most splashy.
- **2** (centre): two springs. The classic tank.
- **ECHO** (right): echo mode, a tape echo into the two-spring tank (below).

Switching crossfades, so it's safe mid-tail. Into ECHO the echo fades in on a fresh tape; out of it, its last repeats ring on in the springs.

### ATTITUDE (bottom switch)

Named for the saturation.

- **CLEAN** (left): a polite, linear tank. The reverb isn't bit-reduced.
- **TAPE** (centre): tape saturation, the core dub colour, and the reverb at **12-bit µ-law**: a fine grain.
- **VALVE** (right): a spring in a cranked valve amp. Hard, lopsided saturation inside the tank, the growl of an overdriven valve stage, rattle and lurch on hits, and the howl at the top of DECAY. The reverb is at **10-bit µ-law**: coarse, clearly gritty.

**The grit.** In TAPE and VALVE the reverb runs through a 24 kHz µ-law converter, the companded grain of early digital delays and samplers. µ-law steps are coarse on loud sounds and fine on quiet ones, so hits get the most grit and a fading tail ends in grain, then silence. At 24 kHz the top octave (above about 11 kHz) is cut cleanly, with no aliasing. The converter sits before TONE, so TONE right of noon thins the grit along with the tail.

Flipping ATTITUDE changes the tail already ringing. The dry signal is never coloured or bit-reduced, so BLEND fully left is a clean passthrough in every position.

### THROW / TAP button

**In TANK 1 and 2 it throws.** While you hold the button, the springs hear the input; let go and they don't, and what's already ringing rings on.

- The **first press switches throw mode on**. Until then the reverb works as usual (the springs hear everything). From then on the springs only hear what you hold the button for.
- Tap it quickly or slowly, hold it for a bar: every press is a throw.
- With the gate patched too, the springs hear the input while either one is on.
- **To go back to always-on:** tap, then press again straight away and keep holding for 2 seconds. All four LEDs blink white once, and the reverb works as usual until the next press or gate.

**In TANK ECHO it taps the tempo.** Each tap is a beat (a quarter note); TENSION then picks the division, as it does with a clock at the gate. Three or four taps settle it, and it stays when you stop tapping. A single tap on its own, then nothing for about 2 seconds, lets it go (back to TENSION's free time). If a clock is also patched into the gate, whichever you changed last wins. The LEDs flash purple on each tap, then pulse purple on the tapped beat for 4 seconds. A press in ECHO never throws.

### Gate input

**In TANK 1 and 2 it throws**, from a sequencer: while the gate is high (above about 2 V) the springs hear the input; while it's low they don't, and what's already ringing rings on. Nothing happens until the gate first goes high, so with nothing patched the reverb works as usual. To go back to always-on, use the button's tap-then-hold (above); the next gate switches throw mode on again.

**In TANK ECHO it's the echo's clock** (one pulse per quarter note).

### CV inputs

0–5 V, added to the knob's position (turn the knob down to give CV room). BLEND, TONE, SPLASH and DRIVE follow CV quickly; DECAY, TENSION and WOBBLE glide, so they don't zipper.

### Audio in and out

- **In L / In R:** stereo in. Patch In L only for mono: it feeds both sides. Your dry signal stays stereo; the springs hear left and right summed to mono, like a real tank. The inputs clip at about 16 V peak to peak.
- **Out L / Out R:** stereo out. A mono input still comes out wide.

## Echo mode (TANK ECHO)

A worn tape echo in front of the two-spring tank. A quarter of each repeat goes through the springs and the rest comes straight off the tape, wide like two playback heads, so every repeat stays a distinct hit with a spring halo. The repeats wear like old tape: loud, bright ones come back thicker and duller, each pass a little darker, with no digital fizz. The springs behind the echo are fixed at a medium tank.

Three controls change job in ECHO:

- **DECAY = feedback**, how many repeats. Fully down: one repeat, about 10 dB down. Noon: a few. Then longer builds. At the very top the repeats hold, kept up by the tape's saturation, never running away; bring DECAY down and they die away.
- **TENSION = echo time.** Free: 2 s fully left, about 0.4 s at noon, 80 ms fully right. With a clock (the gate, or tapped on the button) it picks a division of the beat instead, in seven zones from left to right: **1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16.**
- **Gate = the clock**: one pulse per quarter note (30–300 bpm). The tempo holds when the pulses stop, so stopping and starting your sequencer doesn't move the echo; a single pulse on its own lets it go.

Every change of echo time swoops like tape (the repeats bend in pitch for a moment), never a jump: moving TENSION bends the repeats like a Space Echo's rate knob. WOBBLE moves the tape. TONE and ATTITUDE shape the repeats along with the springs. There's no Hold in ECHO, and DECAY's top holds the repeats rather than howling.

## LEDs

- **Left two: input level** (In L, In R). **Right two: output level** (Out L, Out R).
- Green → amber → red as the level gets hot.
  - **Input red:** you're near clipping at the jack. Turn the source down.
  - **Output red:** the safety limiter is catching peaks. Normal in a big howl; if it's red all the time, lower DRIVE or DECAY.
- Held sounds (pads, drones, organ) rarely get there: the tank notices a sound being held and gently turns down what goes into the springs, so the wet stays clear of the limiter. Hits and stabs are never touched, and the tail after you stop still rings its full length.
- **White blink** (all four, once): throw mode just went off.
- **Purple** (all four): tap tempo in TANK ECHO. A flash on each tap, then a pulse on each beat for 4 seconds, then back to the meters.
- **At power-up** a short colour sweep across the four LEDs says the firmware has loaded.

## Quick start

1. Patch a snare or rim hit into **In L**. Take **Out L** and **Out R** to your mixer.
2. Set every knob to noon, TANK to the centre (2), ATTITUDE to the centre (TAPE).
3. **BLEND** to about 2 o'clock so you clearly hear the tank. Bring it back once you've found the sound.
4. **DECAY**: noon is about a 2-second tail. Turn it up for long dub throws.
5. **Throw**: hold the button while the snare plays, let go, and hear the tail ring on. (The first press switches throw mode on; tap, then press and hold for 2 s to go back.)
6. Then try **TENSION** left for a big boing, **SPLASH** up for a harder clang, **TONE** right to thin it out like Tubby's “Big Knob”, **ATTITUDE** right (VALVE) with DECAY past 4 o'clock for the howl.
7. **Echo**: TANK right (ECHO), tap the button on the beat a few times, set TENSION to the division you want, and ride DECAY for longer builds.

## A few dub moves

- **Throw the snare.** TANK 2, TAPE, BLEND fully right on a send. Hold the button on the beats you want drenched; let go and the tail rings on.
- **“Big Knob” sweep.** Ride TONE from noon to fully right as a tail rings: it thins to a telephone splash. Back down for the warmth.
- **Skank into echo.** TANK ECHO, clocked from your sequencer, TENSION on dotted 1/8 (just about noon). DECAY around 2 o'clock for a trail of repeats, each one splashing into the springs.
- **Hold the bed.** CLEAN, DECAY fully up. Play a chord, let it bloom into a held wash, and keep playing: the bed ducks under your kick and bass.
- **Kick the tank.** VALVE, DRIVE past 3 o'clock, DECAY into the top: ride the howl, then pull DECAY back and let it fall away.

## In the plugin

The plugin (AU and VST3, macOS) has the same panel, the same sound and the same LEDs. A few things work the way a DAW needs:

- **THROW** is the panel button (hold it with the mouse). It reads **TAP** in TANK ECHO.
- **GATE** (the switch next to it) stands in for the gate jack: on = gate high. In your DAW's parameter list it's called THROW, so you can automate throws.
- **MIDI notes** into the plugin act as the gate: held note = gate high, to the sample. A MIDI clip can sequence throws in TANK 1–2.
- **In TANK ECHO the DAW's tempo wins**, and TENSION's readout shows the note value.
- **SIZE** (1x, 1.5x, 2x) scales the panel.
- Use it on a return track with BLEND fully right, or as an insert with BLEND to taste.

<!-- SCREENSHOT: assets/screenshots/plugin-panel.png -->
