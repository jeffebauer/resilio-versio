# Ableton setup for stimulus playback and recording

For sending the test stimulus to the Wellspring, the Magneto and the Plugin.

## Once, in Settings

| Where | Setting |
|---|---|
| Audio | Sample rate **48 kHz** |
| Record, Warp & Launch | **Auto-Warp Long Samples: off**. **Create Fades on Clip Edges: off** |
| Plug-ins | **Use Audio Units v2: on**. **Use VST3 Plug-in System Folders: on**. Rescan after every plugin rebuild |

## Per set

| Item | Setting | Why |
|---|---|---|
| Stimulus clips | **Warp off**, **Loop off**, clip gain **0 dB**, start at **bar 1** | Warping resamples and smears clicks, the sweep and pitch. Each file already has 1 s of silence up front |
| Tempo | Any. `04_skank` is written at **75 bpm** (set that only if you want its chords on the grid). `kicks_16ths.mid` is written at **120 bpm** | With warp off, audio ignores tempo |
| Stimulus track | Volume **0 dB**, pan centre, **no devices**. **Audio To** → your interface output (e.g. Ext. Out 1, mono) | Levels are baked into the files |
| Master | **No limiter** or other processing | |
| Recording track | Stereo in (e.g. Ext. In 1/2), monitoring **off** | |
| Export (if bouncing) | 48 kHz, 24-bit WAV, **Dither: None**, normalise off. For Plugin bounces add ~10 s of tail | Keeps levels comparable |

Ableton's `.asd` files next to the WAVs are its analysis caches. Git ignores them; leave them.

You don't need the Plugin for reference renders: `rv_render` gives bit-identical output offline. Use the Plugin for the M2 check and for playing.
