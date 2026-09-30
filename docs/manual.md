# Resilio Versio — manual

*Draft for M9. Resilio Versio is a dub spring reverb for the Noise Engineering Versio: a simulated spring tank you can drive, splash, kick and push into feedback.*

**Words used here.** The **Tank** is the whole reverb, with 1–3 **Springs** inside. The **tail** is the sound ringing on after you stop. The **chirp** (or "boing") is the springy up-sweep on each echo. A **Kick** is a simulated knock on the tank. **Howl** is the tank feeding back on itself.

## Panel map

Resilio runs behind the stock Versio panel: the printed labels don't match yet, so use this map. Pots are named by position, top to bottom, left to right (P1–P7).

```
   P1 MIX        [LED LED  LED LED]        P2 DECAY
                  In L In R Out L Out R
                    P3 TONE
   P4 SPLASH                               P5 TENSION
                    P6 WOBBLE
   SW  SPRINGS  (top toggle)               P7 DRIVE
   SW  ATTITUDE (bottom toggle)   (button = KICK)

   [ 12 jacks: 7 CV ins, gate in, In L/R, Out L/R ]
```

Each pot has a CV input; the printed panel labels each CV jack with its pot, and here the CV follows the pot's Resilio function. The gate input and the audio jacks keep their printed labels. <!-- confirm jack positions against the owner's printed panel before release -->

## Controls

Clock positions are approximate: fully left (7 o'clock) is 0, noon is 0.5, fully right (5 o'clock) is 1.

| Control | What it does |
|---|---|
| **P1 MIX** | Dry ↔ wet. Fully right is 100 % wet, for a send/return (the classic dub way: throw the snare into it). |
| **P2 DECAY** | How long the tail rings: from a quick slap (~0.4 s) to a long wash (~9 s). It always fades out. In KICKED only, the last stretch (past ~4 o'clock) lets the tank Howl. Pull DECAY back and the Howl falls into a normal tail and fades within a second or two. |
| **P3 TONE** | Tilt. Left: warm, dark dub (drips and boing still there). Noon: neutral. Right: bright, sizzly splash, capped so it never turns harsh. It changes what hits the springs, not just an EQ after them. |
| **P4 SPLASH** | How hard the hits hit the springs. A loud, sudden hit rings the springs with its own highs (a bright clang), and in DRIVEN and KICKED a drum hit also bites the input harder (grit, and a bigger tail). Chords get the clang, not the bite. Nothing is added: it's your hit, hitting harder. Ghost notes in a groove stay quiet. Down = no splash at all, only a faint pitch wobble on hard hits in DRIVEN and KICKED. |
| **P5 TENSION** | Which tank is fitted. Right = tight: short tank, quick repeats, small bright chirp. Left = loose: long tank, slow repeats, big dark boing. Turning it while the tail rings bends the pitch, like tightening or slackening a string. |
| **P6 WOBBLE** | Pitch movement, both ways from noon. Noon: still. Left: tape-like drift, a smooth random wow with a faster flutter on top that never repeats; gentle just left of noon (held chords stay in tune), wild fully left. Right: a steady wobble (a sine whose speed drifts a touch), up to obvious worn-tape warble fully right. A tiny amount is always on, even at noon. CV adds to the knob: from fully left, CV sweeps drift → still → warble. |
| **P7 DRIVE** | The tank's INPUT: how hard your signal hits it. Up to +24 dB of gain before anything else, so a quiet mixer send (peaks around −18 to −24 dBFS) splashes like a hot one once DRIVE is up; turning it up never takes splash away. Colour and grit build as before (clean-ish to ~9 o'clock, driven from ~3 o'clock) and the tail gets a few dB louder (about +6 dB from 7 to 5 o'clock), never shorter. On hot material at full MIX the output limiter starts catching peaks from about 1 o'clock. |
| **SPRINGS** (top toggle) | Left: 1 Spring, sparse and the most splashy. Centre: 2 Springs, the classic tank. Right: 3 Springs, dense and smooth. Switching crossfades, so it's safe mid-tail. |
| **ATTITUDE** (bottom toggle) | Left: **CLEAN**, a polite, linear tank. Centre: **DRIVEN**, tape saturation, the core dub colour. Right: **KICKED**, hard drive inside the tank, full chaos, Howl allowed. Flipping it changes the tail already ringing. |
| **Button = KICK** | Knocks the tank: a short low thud, then a big crash ringing through the Springs. Same strength every time; ATTITUDE sets how hard. Holding does nothing extra. |
| **Gate in = KICK** | Same as the button, on each rising edge (above ~2 V). Patch a sequencer to hit the tank in time. |
| **CV ins** | 0–5 V, added to the pot's position (set the pot low to give CV room). MIX, TONE, SPLASH and DRIVE follow CV quickly; DECAY, TENSION and WOBBLE glide, so they don't zipper. |
| **In L / In R** | Stereo in. Patch In L only for mono: it feeds both sides. Inputs clip at about 16 V peak-to-peak. |
| **Out L / Out R** | Stereo out. A mono input still comes out wide. |

### LEDs

- **Left two: input level** (In L, In R). **Right two: output level** (Out L, Out R).
- Green → amber → red. **Input red:** you're near clipping at the jack, so turn the source down. **Output red:** the safety limiter is catching peaks (normal in a big Howl or Kick; if it's red all the time, lower DRIVE or DECAY).
- **At power-up** a short colour sweep across the four LEDs says the firmware has loaded.

## Quick start

1. Patch a snare or rim hit into **In L**; take **Out L** and **Out R** to your mixer.
2. Set everything to noon, SPRINGS centre, ATTITUDE centre (DRIVEN).
3. **MIX** to ~2 o'clock so you clearly hear the tank; bring it back down once you've found the sound.
4. **DECAY**: noon is a ~2 s tail. Turn it up for long dub throws.
5. **Hit the tank**: press the button. Then try TENSION left for a big boing, TONE right for splash, ATTITUDE right (KICKED) and DECAY past 4 o'clock for Howl.

## Installing (Noise Engineering Firmware Swap)

**Never connect USB and Eurorack power at the same time.** Power off, take the module out of the rack, and unplug the rack power cable first.

1. Plug a micro-USB cable into the Daisy Seed on the back of the module. It runs on USB power alone.
2. In Chrome, open **noiseengineering.us/portal/firmware**.
3. Choose **Select Custom File** and pick `resilio_versio.bin`.
4. **CONNECT**, then **CHANGE FIRMWARE**. Wait until it reports done.
5. Unplug USB, then reconnect rack power. The LEDs play the boot sweep.

## Going back to Noise Engineering's firmware

Same app, same steps (USB only, rack power unplugged): at step 3, pick the Noise Engineering firmware you want (for example your module's original one) from the app's list instead of a custom file, then **CONNECT** → **CHANGE FIRMWARE**. Resilio doesn't install a bootloader or change anything else, so this fully restores the module.
