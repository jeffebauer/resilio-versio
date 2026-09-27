# Magneto benchmark recording recipe

Why: the Strymon Magneto is a respected **digital** spring + tape echo. It's a **benchmark, not a target** (ADR 0020). The Wellspring answers "is this a spring?"; the Magneto answers "does ours hold up next to a good emulation?", plus it calibrates WOBBLE and DRIVEN. Page numbers refer to the Magneto manual (Rev B).

Same stimulus as the Wellspring (`python3 tools/make_stimulus.py`), plus `08_held_tones.wav` (a held 1 kHz sine, then a held chord) for measuring pitch wobble.

## What the Magneto is (from its manual)

- 4-head tape delay + integrated spring reverb, 96 kHz internal, Eurorack levels (max 20 Vpp in/out).
- The spring has **no decay or tone control**. Only **SPRING** (reverb output level) and the DRY/WET knobs feeding it (p.11). Like the Wellspring, it's one reference point.
- **Dual Split Mode** (rear DIP switch **S2 = ON**, p.19) splits the module into two independent mono paths: **LEFT IN/OUT = tape delay**, **RIGHT IN/OUT = spring reverb only**, with no tape in its path. That's our "spring only" setting, and it lets one session record both.
- Tape controls (p.11): **WOW & FLUTTER** (CCW "perfectly tuned tape machine" → CW "in need of service"), **CRINKLE**, **TAPE AGE**, **LOW CUT**, **REC LVL** (LED: green clean → amber onset of saturation → red heavy saturation).

## 1. One-time setup: Dual Split Mode

1. **Power off the rack**, take the Magneto out, set rear DIP switch **S2 to ON**, put it back, power on.
2. Afterwards, set S2 back to OFF for normal use.

## 2. Patch

```
Spring takes:  interface OUT 1 ─► [to rack level] ─► Magneto RIGHT IN;   Magneto RIGHT OUT ─► interface IN 1
Tape takes:    interface OUT 1 ─► [to rack level] ─► Magneto LEFT IN;    Magneto LEFT OUT  ─► interface IN 1
```

Record mono (IN 1), 48 kHz / 24-bit, same Ableton setup as the Wellspring recipe. Use the same interface gains for the whole session. Record take 0 (loopback) once if you haven't already this session.

## 3. Base settings

| Control | Setting |
|---|---|
| SPRING | **Fully CW** (spring takes). Note if any dry signal is still audible; the analysis also checks for a direct click. |
| DRY | **Zero** |
| WET | Spring takes: **zero**. Tape takes: see below |
| REPEATS | Zero, and all four **FEEDBACK** buttons **OFF** (no regeneration) |
| WOW & FLUTTER, CRINKLE, TAPE AGE, LOW CUT | **Fully CCW** (clean) unless the take says otherwise |
| REC LVL | **Green** (clean) on the loudest part of the stimulus, unless the take says otherwise |
| PLAYBACK LEVEL | Head 1 up, heads 2–4 down (tape takes) |
| HEADS / MODE | EVEN / Echo |
| No CV patched | Especially SPRING CV, WET CV, INFINITE |

## 4. Takes

| Take | Path | Stimulus | Change from base | Purpose |
|---|---|---|---|---|
| MA | Spring (R) | `01_clicks` | none | Benchmark chirp + T60 |
| MB | Spring (R) | `02_hits` | none | Priority sound |
| ME | Spring (R) | `04_skank` | none | Musical A/B |
| MW0–MW4 | Tape (L) | `08_held_tones` | WET up so one clean echo is at a similar level to the input. **WOW & FLUTTER** at 5 positions: **fully CCW, 9, 12, 3 o'clock, fully CW** | Measures pitch wobble in cents at each position → calibrates Drift / Warble (ADR 0008) |
| MD1–MD3 | Tape (L) | `02_hits` | WET as above, WOW & FLUTTER CCW. **REC LVL** at **green, amber, red** (on the loudest snare) | Tape saturation reference for DRIVEN (M5) |

MA, MB, ME are the core. MW is the most valuable extra: it turns "subtle drift" and "worn-tape warble" into numbers.

## 5. Naming and notes

`test_audio/reference/magneto_<take>_<desc>.wav`, e.g. `magneto_MA_clicks.wav`, `magneto_MW2_wow12oclock.wav`, `magneto_MD3_reclvl_red.wav`. Add a line per take to `test_audio/reference/NOTES.md`: knob positions for anything not at base, and what you heard.
