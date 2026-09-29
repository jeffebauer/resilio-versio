# M1 work split and interface contracts

M1 = one Spring, CLEAN, plus Renderer tooling (SPEC §7 M1). Three parallel work streams. Each owns disjoint files. **Don't edit files owned by another stream** and don't edit `CMakeLists.txt`: it globs `core/dsp/*.cpp`, `host/common/*.cpp`, `host/render/*.cpp` and treats every `host/tests/test_*.cpp` as its own test executable with its own `main()`.

Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build --output-on-failure`.

Rules for all: C++17. `core/` never includes libDaisy, JUCE or any host header. Vocabulary from `CONTEXT.md`. Match existing code style (see `core/params/ParamSpec.h`, `host/render/main.cpp`). No new third-party dependencies or downloads.

## Stream A — Spring DSP (owns `core/**`, `host/tests/test_spring*.cpp`)

The Tank public API stays **source-compatible** (the Renderer, Plugin and Firmware already call it):

```cpp
namespace rv {
class Tank {
public:
    void  prepare(float sampleRate, int maxBlockSize); // allocation happens here only
    void  setParam(ParamId id, float normalised);      // clamps 0–1, any time, any thread-safe-ish (called before process)
    float param(ParamId id) const;
    void  kick(int sampleOffset);                      // may stay a no-op in M1
    void  process(const float* inL, const float* inR, float* outL, float* outR, int numSamples);
    void  reset();                                     // NEW: clear all state (tails) without re-prepare
};
}
```

- `process()` must accept any `numSamples` ≥ 1 and give **identical output regardless of block size** (Renderer determinism). It must be real-time safe: no allocation, no locks, no I/O.
- The output is the full MIX'd signal (dry/wet equal-power per MIX). Tests and Renderer use `mix=1` for wet-only.
- Delay-line memory: the Firmware later places it in SRAM/SDRAM. Keep buffers in plain members or a caller-supplied pool, and document the max bytes used at 48 kHz.
- Internal-unit mappings (DECAY → T60, TENSION → L, fC, a & M, etc.; BOING until ADR 0026) live in `core/params/` next to ParamSpec, so every Host shares them.

## Stream B — Renderer, metrics, sidecars (owns `host/render/**`, `host/common/**` except `Wav.h`, `host/tests/test_render*.cpp`, `host/tests/test_metrics*.cpp`)

`Wav.h` may be read, not changed (tell the lead if it needs a fix).

### CLI (extend the existing one; keep current usage working)

```
rv_render <in.wav> <out.wav> [--set key=value ...] [--preset p.json] [--auto a.json] [--block N] [--sidecar]
rv_render --sweep sweep.json --out-dir DIR
rv_render --analyze <in.wav> [--sidecar-out x.json]      # metrics + spectrogram for an existing WAV (e.g. Wellspring references)
```

- **Preset JSON:** `{ "decay": 0.8, "tension": 0.3, "springs": "2", "attitude": "CLEAN" }`. Knobs are numbers 0–1. Switches accept a label (`ParamSpec::choices`) or a number 0 / 0.5 / 1.
- **Automation JSON:** `{ "breakpoints": [ {"t": 0.0, "key": "decay", "value": 0.2}, {"t": 4.0, "key": "decay", "value": 1.0} ], "kicks": [1.5, 3.0] }`. Linear interpolation between breakpoints per key. Apply at sample accuracy by splitting blocks at breakpoints, or per block of ≤ 16 samples. Kicks go to `Tank::kick(offset)`.
- **Sweep JSON:** `{ "name": "m1_grid", "input": "test_audio/stimulus/01_clicks.wav", "base": {preset}, "grid": { "decay": [0, 0.5, 1], "tension": [0, 0.5, 1] }, "tail_seconds": 12 }`. Cartesian product. Output files are named `<name>__decay0.50_tension1.00.wav`. Append `tail_seconds` of silence to the input so tails ring out. Writes `manifest.json` in the out dir: `{ "name", "created", "input", "renders": [ {"wav", "sidecar", "params": {...}} ] }`.
- A hand-written minimal JSON reader/writer lives in `host/common/` (no dependencies).

### Sidecar JSON (one per WAV, `<wav-stem>.json`, the contract with Stream C)

```json
{
  "wav": "m1_grid__decay1.00_tension0.00.wav",
  "sample_rate": 48000,
  "duration_s": 61.0,
  "params": { "decay": 1.0, "tension": 0.0, "springs": "1", "attitude": "CLEAN" },
  "metrics": {
    "peak_dbfs": -3.1, "rms_dbfs": -24.0,
    "t60_s": 8.7,
    "resonance_peak_db": 7.5,
    "steady_tone": false,
    "nan_inf_count": 0, "clip_count": 0, "click_count": 0
  },
  "spectrogram": {
    "width": 600, "height": 160,
    "t0_s": 0.0, "t1_s": 61.0,
    "f_min_hz": 40, "f_max_hz": 16000, "freq_scale": "log",
    "db_min": -100, "db_max": 0,
    "data_b64": "<width*height uint8, row-major, row 0 = f_max (top), 0 = db_min, 255 = db_max>"
  }
}
```

Metric definitions (SPEC §4.10, §6.2, §7):
- **peak/rms** over the whole file, mono sum of channels ÷ channel count. Report −inf as −200.
- **t60_s:** Schroeder backward integration of the (mono) output. Fit −5 → −35 dB, extrapolate to 60 dB. For multi-event files (click trains), use the segment from the **first** event to just before the second (event = sample exceeding −40 dBFS after ≥ 0.5 s under it). "Just before" = the quietest 10 ms block between the tail's peak and the second event, so the second event's quiet build-up under −40 dBFS stays out of the fit (fixed 30 Sep 2026; before, the segment ran to the crossing and read long). `null` if not measurable.
- **resonance_peak_db:** from 1 s after the first event onward (the same segment as T60). Average power spectrum (Hann, 8192-point), 1/3-octave-smoothed median per bin. Max over 100 Hz–10 kHz of (bin dB − smoothed dB).
- **steady_tone:** true if some narrowband peak stays > 12 dB above the smoothed spectrum, at an unchanged bin (±1), across consecutive 0.5 s frames covering > 2 s while its level is > −30 dBFS.
- **clip_count:** samples with |x| ≥ 0.999. **nan_inf_count:** non-finite samples (a Renderer error should be printed too).
- **click_count:** sample-to-sample discontinuities: |x[n] − 2x[n−1] + x[n−2]| exceeding 20 dB above a local (±10 ms) RMS of that same second-difference signal, and above −60 dBFS absolute. Tune so the six stimulus files (which contain intentional clicks in `01_clicks.wav` only) behave sensibly. Document the result.

## Stream C — Review page (owns `tools/review/**`)

`python3 tools/review/make_review.py <out-dir> [--reference DIR ...]` reads `<out-dir>/manifest.json` + sidecars (and any `--reference` dir of WAVs that have sidecars, e.g. `test_audio/reference/`) and writes `<out-dir>/index.html`:
- One card per render: settings, metrics (colour-flag failures: nan/inf > 0, clip > 0, click > 0, steady_tone, resonance_peak_db > 12), an `<audio>` player (relative `src`, works over `file://`), and a spectrogram drawn on `<canvas>` from `data_b64` with a time axis and log-frequency axis labels.
- Grid view when the sweep has 2 axes (rows × columns). Filters by param value.
- An A/B strip pinning one Reference file next to any selected render.
- Plain HTML/CSS/JS inlined in one file, no CDN, no build step, stdlib-only Python. Light and dark mode. Works opened directly from Finder.
- Include `tools/review/sample/` with a tiny fake manifest + sidecars + short WAVs made by a stdlib script, so the page can be developed and tested before Streams A/B finish.

**Update (30 Sep 2026):** the default page is now the listening page the owner approved on `renders/splash_voicings` (`tools/review/listen.py` + `listen_template.html`): columns per group (usually ATTITUDE) with colour-coded headers, settings as chips, versions A/B/C switched in sync across panels, "My pick" per panel, notes per column, "Copy results for Claude", level-matched playback. It also reads folders without a top-level manifest (merged subfolder sweeps, plain A/B prototype WAVs). Layout overrides: `--columns`, `--variants`, `--rows`; `--out` to write elsewhere; `--classic` for the card/metrics page above. Usage at the top of `make_review.py`; tests: `python3 tools/review/test_make_review.py`.
