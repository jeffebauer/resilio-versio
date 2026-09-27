# Wellspring reference recording recipe

Why: gives Resilio Versio a measurable target (chirp spacing, T60, spectrum, driven colour, Kick character). See ADR 0009.

## What the Wellspring is (from its manual)

A desktop stereo **BBD delay + stereo spring reverb** (Teaching Machines). Signal flow:

```
input ─► delay section (time, feedback, filter, modulation) ─► delay DRY/WET ─► springs ─► SPRINGS DRY/WET ─► OUTPUT
                ▲                                                                   │
                └──────────────────────── MAGIC (spring → delay feedback) ──────────┘
```

- Two spring tanks, each with a pair of 15" springs. Left and right are separate springs, so **the wet signal is true stereo**.
- The springs are fed from *after* the delay DRY/WET. With delay DRY/WET fully dry and MAGIC at zero, the springs hear only the clean input. The delay, filter, feedback and modulation drop out of the path. **That's how we isolate the spring.**
- The spring has **no decay control**. Its decay is fixed by the tanks. So the Wellspring gives one reference point, not a range. We match DECAY to it rather than it to DECAY.
- INPUT drives the unit and it's "designed to distort in a pleasing way". The CLIP light only shows clipping of the clean dry path. Higher input also lowers spring noise.
- The Ringing the owner has heard comes from delay + MAGIC feedback. The manual's own fix is delay-time modulation, the same idea as our Micro-mod floor (ADR 0010).

## 1. Make the stimulus

```bash
python3 tools/make_stimulus.py
```

Writes eight files to `test_audio/stimulus/` (48 kHz, 24-bit, mono, deterministic):

| File | What it is | What we learn from it |
|---|---|---|
| `01_clicks.wav` | 6 clicks, 8 s apart | Chirp spacing, dispersion, T60 |
| `02_hits.wav` | Snare ×3 levels, rim ×3 levels, 6 s apart | Priority sound; how level changes Splash |
| `03_sweep.wav` | 10 s sine sweep 20 Hz–20 kHz | Precise impulse response + frequency response |
| `04_skank.wav` | 4 bars of offbeat chord stabs, then tail | Musical check, A/B material |
| `05_silence_for_kicks.wav` | 40 s of silence | Bed to record physical knocks over |
| `06_noise_bursts.wav` | Short and long noise bursts, 10 s apart | Optional: delay-feedback Ringing example for AntiRes detector tests |
| `07_click_single.wav`, `08_held_tones.wav` | One click; held sine + chord | Renderer grids; Magneto wow measurement (`docs/recording-recipe-magneto.md`). Not needed for the Wellspring |

## 2. Patch

```
Interface OUT 1 ─► Wellspring rear LINE L/MONO in      (mono in is sent to both channels)
Wellspring rear OUT L ─► Interface IN 1
Wellspring rear OUT R ─► Interface IN 2
```

Leave the Wellspring's LINE R input unplugged. Both outputs are always recorded (the manual: "Always use both outputs").

Plus once per session, a **loopback take** (take 0) to measure your interface's latency and level: a cable from Interface OUT 1 straight to Interface IN 1.

## 3. Ableton setup

- Project sample rate **48 kHz**, record **24-bit WAV**.
- One mono audio track plays the stimulus file to OUT 1. One stereo audio track records IN 1 + IN 2 (= Wellspring wet L + R). Monitoring off on the recording track.
- Record from bar 1 so the file starts with the stimulus's 1 s of silence.

## 4. Wellspring settings: "spring only"

Set once, then check before every take:

| Control | Setting | Why |
|---|---|---|
| Input selector | **LINE** | Rear inputs |
| INPUT | As high as possible with the **CLIP light never on** (on the snare at −6 dBFS in `02_hits`). Note the position. | Best spring signal-to-noise (manual) |
| OUTPUT | **Top centre** (unity) | |
| Delay **DRY/WET** | **Fully CCW (dry)** | Springs hear only the clean input |
| FEEDBACK | Minimum | Belt and braces |
| **MAGIC** | **Zero** | No spring → delay feedback |
| DELAY mod, FILTER mod | Zero | |
| Filter | HIGH PASS, FREQUENCY fully CCW | The manual's "bypass" setting |
| **SPRINGS** DRY/WET | **Fully CW (wet)** | Spring only |

Don't change the interface input gain during the session. If the wet signal clips the interface, turn the interface gain down and redo take 0.

## 5. Takes

| Take | Stimulus | Change from "spring only" | Purpose |
|---|---|---|---|
| 0 | `01_clicks` | Loopback cable instead of the Wellspring | Interface latency + level |
| A | `01_clicks` | none | Chirp spacing, dispersion, T60 |
| B | `02_hits` | none | Priority sound (snare/rim) |
| C | `02_hits` | **INPUT turned up** until the CLIP light flashes on the loudest snare. Note the position. | Driven-spring reference for M5 DRIVE |
| D | `03_sweep` | none. Don't touch anything during the take | Precise impulse + frequency response |
| E | `04_skank` | none | Musical A/B material |
| E2 | `04_skank` | **SPRINGS DRY/WET** where it sounds best to you for dub. Note the position | Hints at a good MIX taper |
| F | `05_silence_for_kicks` | none. Knock the top of the case ~6 times, ~6 s apart: 2 soft, 2 medium, 2 firm, not violent. Let each ring out. | Kick reference. The tanks are shock-mounted to block outside vibration, so this may come out quiet or dull. **If it does, skip it**: we'll tune Kick against dub records (ADR 0016). |
| G | `06_noise_bursts` | Optional, and the **only take with delay + MAGIC on**: delay DRY/WET up, FEEDBACK high, MAGIC up, delay mod **zero**, until the ringing tone appears. Keep any take where it shows up. | "Known bad" Ringing case to prove the AntiRes metric catches it (ADR 0010). Not a spring reference. |

A–E is the core spring set. C is the drive reference. F and G are optional.

## 6. Naming and notes

Export each take as a stereo WAV named `test_audio/reference/wellspring_<take>_<short-desc>.wav`, e.g. `wellspring_A_clicks.wav`, `wellspring_C_hits_hot.wav`, `wellspring_0_loopback.wav`.

Add a line per take to `test_audio/reference/NOTES.md` with the INPUT position, the SPRINGS DRY/WET position for E2 (clock face is fine, e.g. "2 o'clock"), and anything you heard.

## 7. What happens next

`rv_render --analyze` measures each reference: T60, spectrum, chirp spacing and a spectrogram. For the M1 listening check, DECAY is set to match the Wellspring's measured T60, and the review page puts both side by side. Take A also confirms that SPRINGS fully CW is really 100% wet: the analysis looks for a direct click at each onset.
