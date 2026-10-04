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

Why L only: the Wellspring is **true stereo** (L input → left tank, R input → right tank). With R unplugged, its jack normals the L signal into R, so both tanks get an identical copy. Sending the same mono signal to both inputs from Ableton would do the same, but it needs two outputs at exactly matched levels. Our stimulus is mono, and Resilio Versio sums its input to mono before its Springs, so this compares like with like.

Plus once per session, a **loopback take** (take 0) to measure your interface's latency and level: a cable from Interface OUT 1 straight to Interface IN 1. This measures the **analog TRS path** the Wellspring uses. Anything recorded through the eurorack (the Magneto, later the Versio) goes through the BoredBrain OPTX2 and ADAT instead, which has its own latency and level, so it gets its own loopback take (see `recording-recipe-magneto.md`).

## 3. Ableton setup

- Project sample rate **48 kHz**, record **24-bit WAV**.
- One mono audio track plays the stimulus file to OUT 1. One stereo audio track records IN 1 + IN 2 (= Wellspring wet L + R). Monitoring off on the recording track.
- Record from bar 1 so the file starts with the stimulus's 1 s of silence.

## 4. Wellspring settings: "spring only"

Set once, then check before every take:

| Control | Setting | Why |
|---|---|---|
| Input selector | **LINE** | Rear inputs |
| INPUT | As high as possible with the **CLIP light never on**, set while playing **`04_skank`**: its held chords clip before `02_hits`' snares do (same −6 dBFS peak, far more sustained energy). Keep this one position for takes A, B, D, E, E2. Note it. | Best spring signal-to-noise (manual), with every clean take clean and comparable |
| OUTPUT | **Top centre** (unity) | |
| Delay **DRY/WET** | **Fully CCW (dry)** | Springs hear only the clean input |
| FEEDBACK | Minimum | Belt and braces |
| **MAGIC** | **Zero** | No spring → delay feedback |
| DELAY mod, FILTER mod | Zero | |
| Filter | HIGH PASS, FREQUENCY fully CCW | The manual's "bypass" setting |
| **SPRINGS** DRY/WET | **Fully CW (wet)** | Spring only |

Don't change the interface input gain during the session. If the wet signal clips the interface, turn the interface gain down and redo take 0.

## 5. Takes

| Take | Export as | Stimulus | Change from "spring only" | Purpose |
|---|---|---|---|---|
| 0 | `wellspring_0_loopback.wav` | `01_clicks` | Loopback cable instead of the Wellspring | Interface latency + level |
| A | `wellspring_A_clicks.wav` | `01_clicks` | none | Chirp spacing, dispersion, T60 |
| B | `wellspring_B_hits.wav` | `02_hits` | none | Priority sound (snare/rim) |
| C | `wellspring_C_hits_hot.wav` | `02_hits` | The **only take with INPUT raised**: turn it up from the base position until the CLIP light flashes on the loudest snare (on purpose: this take overdrives the input). Note the position, check the recording doesn't clip in Ableton, and **turn INPUT back to the base position** afterwards (easiest: record C last). | Driven-spring reference for M5 DRIVE |
| D | `wellspring_D_sweep.wav` | `03_sweep` | none. Don't touch anything during the take | Precise impulse + frequency response |
| E | `wellspring_E_skank.wav` | `04_skank` | none | Musical A/B material |
| E2 | `wellspring_E2_skank_mix.wav` | `04_skank` | **SPRINGS DRY/WET** where it sounds best to you for dub. Note the position | Hints at a good MIX taper |
| F | `wellspring_F_knocks.wav` | `05_silence_for_kicks` | none. Knock the top of the case ~6 times, ~6 s apart: 2 soft, 2 medium, 2 firm, not violent. Let each ring out. | Kick reference. The tanks are shock-mounted to block outside vibration, so this may come out quiet or dull. **If it does, skip it**: we'll tune Kick against dub records (ADR 0016). |
| G | `wellspring_G_ringing.wav` | `06_noise_bursts` | Optional, and the **only take with delay + MAGIC on**: delay DRY/WET up, FEEDBACK high, MAGIC up, delay mod **zero**, until the ringing tone appears. Keep any take where it shows up. | "Known bad" Ringing case to prove the AntiRes metric catches it (ADR 0010). Not a spring reference. |
| A-L | `wellspring_A-L_clicks_left.wav` | `01_clicks` | **Dummy plug** (an unconnected cable) in the R input, so the normal breaks and **only the left tank** gets signal | One tank's own response |
| A-R | `wellspring_A-R_clicks_right.wav` | `01_clicks` | Stimulus into **R only**, L input empty (the right tank only) | The other tank's own response. With A-L, shows how different the two physical tanks are: real-world data for Spring detuning and stereo width, and for a possible stereo-in mode |

A–E is the core spring set. C is the drive reference. F, G, A-L and A-R are optional.

## 5b. Session 2 (for the transducer and stereo modelling, ≈20 min)

Same patch, same Ableton set, **same "spring only" settings and the same base INPUT position as session 1** (check NOTES.md; set it back if it moved). Don't change the interface gain. No new loopback take needed if nothing in the patch changed; if anything did, record take 0 again first. Make the new stimulus files once: `python3 tools/make_stimulus.py` (13–16) and `python3 tools/make_sustain_stimulus.py` (10). Record in this order:

| Take | Export as | Stimulus | Change from "spring only" | Purpose |
|---|---|---|---|---|
| H | `wellspring_H_sweep_quiet.wav` | `13_sweep_quiet` | none | Level series: the response at a low level |
| I | `wellspring_I_sweep_hot.wav` | `14_sweep_hot` | none (it's 9 dB hotter than D on purpose; check the recording doesn't clip in Ableton) | Level series: how the input stage darkens, squashes or distorts when pushed |
| D-L | `wellspring_D-L_sweep_left.wav` | `03_sweep` | **Dummy plug in the R input**, as for A-L (left tank only) | Stereo matrix: how the left tank reaches each output |
| D-R | `wellspring_D-R_sweep_right.wav` | `03_sweep` | Stimulus into **R only**, L input empty (right tank only) | Stereo matrix: the right tank |
| J | `wellspring_J_tone_bursts.wav` | `15_tone_bursts` | none (back to the default: L in, R unplugged, so both tanks) | Per-octave decay and darkening, the metallic modes |
| K | `wellspring_K_pink_noise.wav` | `09_pink_noise` | none | Steady-state colour and the cleanest per-band tail lengths |
| L | `wellspring_L_held_tones.wav` | `08_held_tones` | none | How a real tank builds up on held sounds |
| M | `wellspring_M_pad.wav` | `10_pad_cminor` | none | The same, on a pad (compare with our sustain trim) |
| N | `wellspring_N_silence.wav` | `16_silence_30s` | none: everything patched, nothing playing, don't touch | The hiss's level and colour (the green spectrogram background) |

Tell Claude when they're in `test_audio/reference/`; `python3 tools/ingest_references.py test_audio/reference/` knows all of them.

## 6. Naming and notes

Export each take as a stereo 48 kHz / 24-bit WAV into `test_audio/reference/`, named as in the takes table's **Export as** column (the pattern is `wellspring_<take>_<short-desc>.wav`; the analysis reads the take from the part between the first two underscores). Tip: name each recorded clip in Ableton the same way (without `.wav`) so the export name is already there.

Add a line per take to `test_audio/reference/NOTES.md` with the INPUT position, the SPRINGS DRY/WET position for E2 (clock face is fine, e.g. "2 o'clock"), and anything you heard.

## 7. What happens next

`rv_render --analyze` measures each reference: T60, spectrum, chirp spacing and a spectrogram. For the M1 listening check, DECAY is set to match the Wellspring's measured T60, and the review page puts both side by side. Take A also confirms that SPRINGS fully CW is really 100% wet: the analysis looks for a direct click at each onset.
