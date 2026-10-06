# Ableton setup for stimulus playback and recording

For sending the test stimulus to the Wellspring, the Magneto and the Plugin.

## Once, in Settings

| Where | Setting |
|---|---|
| Audio | Sample rate **48 kHz** |
| Record, Warp & Launch | **Auto-Warp Long Samples: off**. **Create Fades on Clip Edges: off** |
| Plug-ins | **Use Audio Units v2: on**. **Use VST3 Plug-in System Folders: on**. Rescan after every plugin rebuild |

## Input level: match the module

**Only for your own material through the Plugin.** Never level the stimulus files: their levels differ on purpose (e.g. `02_hits` plays the same snare at −6, −12 and −18 dBFS to test how SPLASH and DRIVE react to level). Stimulus clips stay at clip gain 0 dB, track 0 dB. For hardware recordings the level is set on the device (Wellspring INPUT, Magneto REC LVL), as each recipe says.

Resilio reacts to input level (DRIVE, SPLASH), and Eurorack signals are hot: a 10 Vpp modular signal is about **−4 dBFS**. Most DAW tracks sit lower. To hear the plugin as the module will sound:
- Put a **Utility before** Resilio Versio (or use clip gain) and raise the gain until the **loudest hits** peak around **−4 dBFS**. Read **True peak** on a meter such as Swiss Army Meter (its highest value over the loudest passage). Not LUFS or RMS: those measure loudness, not peaks. Max dB (sample peak) reads almost the same but can read slightly low.
- Unless BLEND is full, this also raises the dry signal. Add a second **Utility after** the plugin to bring the level back down, or judge the wet sound at BLEND full.
- Hotter than −4 dBFS drives the tank harder than the module normally gets. That's fine as an effect.

## Per set

| Item | Setting | Why |
|---|---|---|
| Stimulus clips | **Warp off**, **Loop off**, clip gain **0 dB**, start at **bar 1** | Warping resamples and smears clicks, the sweep and pitch. Each file already has 1 s of silence up front |
| Tempo | Any. `04_skank` is written at **75 bpm** (set that only if you want its chords on the grid) | With warp off, audio ignores tempo |
| Stimulus track | Volume **0 dB**, pan centre, **no devices**. **Audio To** → your interface output (e.g. Ext. Out 1, mono) | Levels are baked into the files |
| Master | **No limiter** or other processing | |
| Recording track | Stereo in (e.g. Ext. In 1/2), monitoring **off** | |
| Export (if bouncing) | 48 kHz, 24-bit WAV, **Dither: None**, normalise off. For Plugin bounces add ~10 s of tail | Keeps levels comparable |

Ableton's `.asd` files next to the WAVs are its analysis caches. Git ignores them; leave them.

You don't need the Plugin for reference renders: `rv_render` gives bit-identical output offline. Use the Plugin for the M2 check and for playing.
