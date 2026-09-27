# Wellspring reference recording recipe

Why: gives Resilio Versio a measurable target (chirp spacing, T60, spectrum, Kick character). See ADR 0009.

The Wellspring is a **BBD delay + spring reverb**. Every take below is of the **spring only**: the delay section must be fully out of the signal path (delay mix/level at zero, feedback at zero, or bypassed). The Ringing the owner has heard comes from the delay's feedback network, not the spring.

## 1. Make the stimulus

```bash
python3 tools/make_stimulus.py
```

Writes six files to `test_audio/stimulus/` (48 kHz, 24-bit, mono, deterministic):

| File | What it is | What we learn from it |
|---|---|---|
| `01_clicks.wav` | 6 clicks, 8 s apart | Chirp spacing, dispersion, T60 |
| `02_hits.wav` | Snare ×3 levels, rim ×3 levels, 6 s apart | Priority sound; how level changes Splash |
| `03_sweep.wav` | 10 s sine sweep 20 Hz–20 kHz | Precise impulse response + frequency response |
| `04_skank.wav` | 4 bars of offbeat chord stabs, then tail | Musical check, A/B material |
| `05_silence_for_kicks.wav` | 40 s of silence | Bed to record physical Kicks over |
| `06_noise_bursts.wav` | Short and long noise bursts, 10 s apart | Optional: example of delay-feedback Ringing for AntiRes detector tests |

## 2. Patch

```
Ableton out ─► [your interface → rack] ─► MULT ─┬─► Wellspring IN ─► Wellspring OUT ─► interface IN 2 (WET)
                                                  └──────────────────────────────────► interface IN 1 (DRY)
```

Recording dry and wet on the same stereo take keeps them sample-aligned, so latency and level can be measured exactly.

## 3. Ableton setup

- Project sample rate **48 kHz**, record **24-bit WAV**.
- One audio track plays the stimulus file. One stereo audio track records IN 1 + IN 2 (dry = L, wet = R). Monitoring off on the recording track (avoids feedback loops).
- Record from bar 1 so the file starts with the stimulus's 1 s of silence.

## 4. Wellspring settings (all takes unless stated)

- **Delay section:** off / zero / bypassed. Double-check before every take.
- **Mix:** 100% wet (spring only).
- **Tone / filter:** neutral (flat / noon). We want to hear the raw tank.
- **Drive / input gain:** lowest setting where the reverb is clearly audible and nothing clips. Note the setting.
- **Decay:** see each take.
- Levels: dry channel peaking around −6 dBFS in Ableton. Wet channel never above −1 dBFS. Adjust the interface input gain, never the Wellspring, and keep it fixed for the whole session.

## 5. Takes

| Take | Stimulus | Decay | Notes |
|---|---|---|---|
| A | `01_clicks` | Noon | |
| B | `01_clicks` | Max usable (just before it starts running away) | |
| C | `02_hits` | Noon | |
| D | `03_sweep` | Noon | Don't touch anything during the take |
| E | `04_skank` | Where it sounds best to you for dub | Note the setting |
| F | `05_silence_for_kicks` | Noon | Knock the tank ~6 times, ~6 s apart: 2 soft, 2 medium, 2 hard. Let each ring out fully. |
| G | `06_noise_bursts` | — | Optional, and the **only take with the delay on**. Push delay feedback until the single tone appears; keep any take where it shows up. Used only to test that AntiRes's detector recognises Ringing. Not a spring reference. |

A–E is the core spring set. F is the Kick reference. G is optional and delay-only.

## 6. Naming and notes

Export each take as `test_audio/reference/wellspring_<take>_<short-desc>.wav`, e.g. `wellspring_B_clicks_decaymax.wav`.

Add a line per take to `test_audio/reference/NOTES.md`: take, knob positions (clock face is fine, e.g. "decay 2 o'clock"), and anything you heard (e.g. "ringing ~400 Hz from 3 s").

Photograph the Wellspring's knobs for each distinct setting if that's easier than writing positions down.
