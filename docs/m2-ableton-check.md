# M2 Ableton check

The automated tests prove the plugin matches the Renderer, MIDI notes (the gate) land on the exact sample, and state saves. This checklist covers what only a real Ableton session can show. Criteria: SPEC §7 M2.

*Written for M2 (28 Sep 2026), when the reverb was one CLEAN Spring and the Kick a placeholder. Steps updated 6 Oct 2026 to today's plugin (panel names of v1.0.43, ADR 0044); the Kick is gone (ADR 0043).*

## Setup

1. Install a build on purpose: `tools/install_plugin.sh <commit>` (dev builds don't install, `RV_INSTALL_PLUGIN` is off). It signs, verifies and records the version in `dist/installed_plugin.txt`.
2. Ableton **Settings → Plug-ins**: turn on **Use Audio Units v2** and **Use VST3 Plug-in System Folders**. Click **Rescan**.
3. Find it in the browser under **Plug-ins → Resilio → Resilio Versio** (both AU and VST3 versions appear).

## Checks

Do 1–5 with the **AU**, then repeat 1–2 with the **VST3**.

1. **Loads:** drop it on an audio track playing `test_audio/stimulus/02_hits.wav`. The panel shows the module: BLEND, DECAY, TONE, SPLASH, TENSION, WOBBLE, DRIVE, the TANK (1 / 2 / ECHO) and ATTITUDE (CLEAN / TAPE / VALVE) switches, the THROW button (TAP in ECHO), the GATE switch and four LEDs. (Ableton may also show a standard **Bypass** control, which the plugin framework adds.) You hear a spring on the hits.
2. **Automatable:** in the track's automation chooser, 10 parameters are listed: the seven knobs, TANK, ATTITUDE and THROW (the gate switch; the panel button itself is not a parameter). Draw a DECAY ramp over 4 bars: the tail grows smoothly, with no zips or clicks, and it doesn't bend the pitch (ADR 0026; it did under ADR 0012).
3. **MIDI Kicks** *(retired: the Kick was removed in ADR 0043, and `tools/make_kick_midi.py` with it; MIDI notes are now the gate, held = throw)*: create a MIDI track and drag in `test_audio/midi/kicks_16ths.mid` (4 bars of 1/16 notes, varied notes and velocities). Set its **MIDI To** to the audio track, then choose **Resilio Versio** in the second dropdown. Every note gives one click-into-the-tank at the same loudness (velocity ignored, ADR 0005), locked to the grid. Zoom into a recording to check it doesn't flam against a snare on the same beat.
4. **Dry alignment (null test):** duplicate the hits track. On one copy: Resilio Versio with **BLEND fully down**, then a Utility with **Invert (Ø)** on. Solo both: you should hear **silence**.
5. **Sample rates:** in Settings → Audio, switch to **44.1 kHz** and then **96 kHz**. The plugin keeps working and the tail length sounds the same.

## Report back

For each check: pass or fail, and anything odd (with a short recording if possible, saved in `test_audio/m2/`).
