RESILIO VERSIO {{VERSION}} — a dub spring reverb (test build)

Hi! This is a spring reverb I'm building as firmware for the Noise Engineering Versio Eurorack module. The plugin is my desktop test bench: it runs the exact same sound engine as the module, laid out like its panel. It's a work in progress, so I'd love your honest ears on it.

Think King Tubby / Lee Perry: a send effect you throw snares and skanks into, feed from a tape echo, and push into feedback.
{{NOTES}}


INSTALL (Mac, macOS 12 or newer, Apple Silicon or Intel)

1. Unzip. You get two files: "{{NAME}}.vst3" (VST3) and "{{NAME}}.component" (Audio Unit, for Logic or Ableton). Install whichever your DAW uses, or both.
2. In Finder, Go > Go to Folder... and paste:
     ~/Library/Audio/Plug-Ins/
   Drag "{{NAME}}.vst3" into the VST3 folder and "{{NAME}}.component" into the Components folder (create the folder if it's missing).
3. It isn't signed by Apple (it's a test build), so macOS will block it at first. Open Terminal and paste these two lines (each one is a single line):
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"{{NAME}}.vst3"
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"{{NAME}}.component"
4. Restart your DAW and rescan plug-ins. In Ableton: Settings > Plug-Ins, turn on the VST3 / Audio Units system folders, then hold Option and click Rescan. It shows up as "{{NAME}}".

{{FIRMWARE_START}}
FIRMWARE FOR THE VERSIO (only if you have a Noise Engineering Versio)

The same sound runs on the module. File: {{FIRMWARE}}
1. Unplug the module's Eurorack power cable. Never have USB and rack power connected at the same time.
2. Connect the Versio to your computer with USB.
3. In Chrome, open Noise Engineering's Firmware Swap app, choose your Versio, then "Select Custom File" and pick {{FIRMWARE}}. Wait for it to finish.
4. Unplug USB, reconnect rack power.
To go back, use the same app to install any stock Noise Engineering firmware.
On the module: P1 BLEND, P2 DECAY, P3 TONE, P4 SPLASH, P5 TENSION, P6 WOBBLE, P7 DRIVE; the top toggle is TANK, the bottom ATTITUDE; the button is THROW / TAP and the gate input throws (or clocks the echo in TANK ECHO); the four LEDs are the meters described below.
{{FIRMWARE_END}}

Use the plugin on a return track (BLEND fully up) or as an insert (BLEND to taste).


THE CONTROLS (same places as on the module)

BLEND — dry / wet. Fully up = reverb only (best on a send).
DECAY — how long the tail rings. At the very top, in CLEAN and TAPE the tail holds as a bed that ducks under kick and bass; in VALVE it feeds back into a howl.
TONE — left is dark and warm; right is King Tubby's "Big Knob": a steep low cut on the reverb's return that thins the tail as you turn it, more telephone-like the further you go, with a nasal ring on snares and rimshots.
SPLASH — how hard each hit clangs the springs. Up = a brighter clang on snares and rimshots; the last quarter is much bigger. It works the same at any DRIVE, so it splashes on quiet sends too.
TENSION — how tight the springs are. Down = loose and drippy; up = tight, with a higher-pitched ring.
WOBBLE — noon is still. Left = tape wow and flutter that never repeats. Right = vibrato, getting faster and deeper.
DRIVE — the input level, like the gain on an old tank's driver: from clean to gritty, and the tail gets a few dB louder as you push it.

TANK (1 / 2 / ECHO) — one spring (sparse, splashy), two (the classic tank), or ECHO: a tape echo feeding the two springs. In ECHO, DECAY is the echo's feedback and TENSION the echo time (following the DAW's tempo in the plugin).
ATTITUDE (CLEAN / TAPE / VALVE) — the saturation: clean and polite; tape saturation, the dub colour with a fine grain; or a cranked valve stage inside the tank, with rattle, coarse grit and a howl at the top of DECAY.
THROW / TAP — in TANK 1–2, hold it and the springs hear the input only while it's held, and the tail rings on (double-tap and hold 2 s to go back to always-on). In ECHO it taps the tempo (in a DAW its tempo wins). Held MIDI notes into the plugin act like the gate: a clip can sequence throws to the sample. The GATE switch next to it is the gate jack.

The four lights at the top are level meters, like on the module: input left / right, output left / right. Green to amber as it gets hot; red means the input is close to clipping, or the output is being limited. White once when throw mode goes off; purple on tap tempo.


THINGS TO TRY

- A snare or rimshot on a send, BLEND up, SPLASH and DECAY around 2 o'clock. Hold THROW on the hits you want drenched.
- A skank (offbeat chords) at DECAY noon, TENSION low, a touch of WOBBLE to the left. Then TANK to ECHO, TENSION on a dotted 1/8.
- VALVE, DECAY near max, feed it a few hits. Sweep TONE and TENSION while it howls.


KNOWN ISSUES (already being fixed)

- Held sounds (low pads, drones) are gently turned down going into the springs so they stay clean; with DRIVE well up on hot material the output limiter can still catch them (the output light goes red).
- It's a test build: expect the sound to keep changing between versions.


WHAT I'D LOVE TO HEAR FROM YOU

- How it sounds next to spring reverbs you know (real tanks or plugins).
- Any knob that feels dead, too touchy, or doesn't do what its name suggests.
- Anything that clicks, glitches, or sounds broken, with the settings if you can.

Thanks!
