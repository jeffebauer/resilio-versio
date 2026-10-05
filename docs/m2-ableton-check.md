# M2 Ableton check

The automated tests prove the plugin matches the Renderer, Kicks land on the exact sample, and state saves. This checklist covers what only a real Ableton session can show. Criteria: SPEC §7 M2.

The reverb is still M1: one Spring, CLEAN. **Kick is a placeholder**: a single click into the Tank, not the real thud + crash (M7). SPRINGS, ATTITUDE, SPLASH, DRIVE and WOBBLE are visible but don't change the sound yet.

## Setup

1. Build: `cmake --build build`. The AU and VST3 are copied to `~/Library/Audio/Plug-Ins/` automatically.
2. Ableton **Settings → Plug-ins**: turn on **Use Audio Units v2** and **Use VST3 Plug-in System Folders**. Click **Rescan**.
3. Find it in the browser under **Plug-ins → Resilio → Resilio Versio** (both AU and VST3 versions appear).

## Checks

Do 1–5 with the **AU**, then repeat 1–2 with the **VST3**.

1. **Loads:** drop it on an audio track playing `test_audio/stimulus/02_hits.wav`. The generic panel shows 9 controls: DECAY, TONE, TENSION, SPLASH, DRIVE, WOBBLE, MIX (sliders) and SPRINGS (1/2/3), ATTITUDE (CLEAN/DRIVEN/KICKED). (Ableton may also show a standard **Bypass** control, which the plugin framework adds.) You hear a spring on the hits.
2. **Automatable:** in the track's automation chooser, all 9 parameters are listed. Draw a DECAY ramp over 4 bars: the tail grows smoothly, with no zips or clicks, and the pitch bends a little as it goes (ADR 0012).
3. **MIDI Kicks** *(retired: the Kick was removed in ADR 0043, and `tools/make_kick_midi.py` with it; MIDI notes are now the gate, held = throw)*: create a MIDI track and drag in `test_audio/midi/kicks_16ths.mid` (4 bars of 1/16 notes, varied notes and velocities). Set its **MIDI To** to the audio track, then choose **Resilio Versio** in the second dropdown. Every note gives one click-into-the-tank at the same loudness (velocity ignored, ADR 0005), locked to the grid. Zoom into a recording to check it doesn't flam against a snare on the same beat.
4. **Dry alignment (null test):** duplicate the hits track. On one copy: Resilio Versio with **MIX fully down**, then a Utility with **Invert (Ø)** on. Solo both: you should hear **silence**.
5. **Sample rates:** in Settings → Audio, switch to **44.1 kHz** and then **96 kHz**. The plugin keeps working and the tail length sounds the same.

## Report back

For each check: pass or fail, and anything odd (with a short recording if possible, saved in `test_audio/m2/`).
