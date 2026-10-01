RESILIO VERSIO {{VERSION}} — a dub spring reverb (test build)

Hi! This is a spring reverb I'm building as firmware for the Noise Engineering Versio Eurorack module. The plugin is my desktop test bench: it runs the exact same sound engine as the module, laid out like its panel. It's a work in progress, so I'd love your honest ears on it.

Think King Tubby / Lee Perry: a send effect you throw snares and skanks into, kick for crashes, and push into feedback.


INSTALL (Mac, macOS 12 or newer, Apple Silicon or Intel)

1. Unzip. You get two files: "Resilio Versio.vst3" (VST3) and "Resilio Versio.component" (Audio Unit, for Logic or Ableton). Install whichever your DAW uses, or both.
2. In Finder, Go > Go to Folder... and paste:
     ~/Library/Audio/Plug-Ins/
   Drag "Resilio Versio.vst3" into the VST3 folder and "Resilio Versio.component" into the Components folder (create the folder if it's missing).
3. It isn't signed by Apple (it's a test build), so macOS will block it at first. Open Terminal and paste these two lines (each one is a single line):
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Resilio Versio.vst3"
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Resilio Versio.component"
4. Restart your DAW and rescan plug-ins. In Ableton: Settings > Plug-Ins, turn on the VST3 / Audio Units system folders, then hold Option and click Rescan. It shows up as "Resilio Versio".

FIRMWARE FOR THE VERSIO (only if you have a Noise Engineering Versio)

The same sound runs on the module. File: {{FIRMWARE}}
1. Unplug the module's Eurorack power cable. Never have USB and rack power connected at the same time.
2. Connect the Versio to your computer with USB.
3. In Chrome, open Noise Engineering's Firmware Swap app, choose your Versio, then "Select Custom File" and pick {{FIRMWARE}}. Wait for it to finish.
4. Unplug USB, reconnect rack power.
To go back, use the same app to install any stock Noise Engineering firmware.
On the module: P1 MIX, P2 DECAY, P3 TONE, P4 SPLASH, P5 TENSION, P6 WOBBLE, P7 DRIVE; the top toggle is SPRINGS, the bottom ATTITUDE; the button and the gate input KICK the tank; the four LEDs are the meters described below.

Use the plugin on a return track (MIX fully up) or as an insert (MIX to taste).


THE CONTROLS (same places as on the module)

MIX — dry / wet. Fully up = reverb only (best on a send).
DECAY — how long the tail rings. Near the top it rings for ages; in KICKED it can feed back into a howl.
TONE — left is dark and warm; right thins the lows out for a splashier, brighter tank.
SPLASH — how hard each hit clangs the springs. Up = more "boing" on snares and rimshots.
TENSION — how tight the springs are. Down = loose and drippy; up = tight, with a higher-pitched ring.
WOBBLE — noon is still. Left = tape wow and flutter that never repeats. Right = vibrato, getting faster and deeper.
DRIVE — the input level, like the gain on an old tank's driver: from clean to gritty, and the tail gets a few dB louder as you push it.

SPRINGS (1 / 2 / 3) — how many springs are in the tank. More springs = a denser, smoother tail.
ATTITUDE (CLEAN / DRIVEN / KICKED) — how roughly the tank is treated: clean and polite, warmer and saturated, or cranked with a bigger crash and feedback.
KICK — kicks the tank for that classic crash. Any MIDI note into the plugin does the same, at the exact time of the note, so you can sequence kicks.

The four lights at the top are level meters, like on the module: input left / right, output left / right. Green to amber as it gets hot; red means the input is close to clipping, or the output is being limited.


THINGS TO TRY

- A snare or rimshot on a send, MIX up, SPLASH and DECAY around 2 o'clock. Ride the send level.
- A skank (offbeat chords) at DECAY noon, TENSION low, a touch of WOBBLE to the left.
- KICKED, DECAY near max, then hit KICK a few times. Sweep TONE and TENSION while it howls.


KNOWN ISSUES (already being fixed)

- Long, low pads and drones can make the reverb build up and push the output into its limiter (the output lights go red, and it can sound a bit driven). A fix is in progress.
- It's a test build: expect the sound to keep changing between versions.


WHAT I'D LOVE TO HEAR FROM YOU

- How it sounds next to spring reverbs you know (real tanks or plugins).
- Any knob that feels dead, too touchy, or doesn't do what its name suggests.
- Anything that clicks, glitches, or sounds broken, with the settings if you can.

Thanks!
