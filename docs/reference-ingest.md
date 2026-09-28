# Reference ingest: from recordings to measurements and A/B pages

One command turns the Wellspring/Magneto reference recordings (`docs/recording-recipe.md`,
`docs/recording-recipe-magneto.md`) into aligned measurements, a plain-language
report, and A/B review pages against our own renders.

## Run it

After recording, export the takes into `test_audio/reference/` following the
naming in the recipes (`wellspring_<take>_<desc>.wav`, `magneto_<take>_<desc>.wav`,
plus `NOTES.md`), then:

```bash
python3 tools/ingest_references.py test_audio/reference/
```

That's the whole command. It writes:

- `renders/references/<unit>/<unit>_<take>.wav` + `.json` — each take, trimmed
  to the stimulus timeline, with a `rv_render --analyze` sidecar (T60,
  spectrogram, stereo metrics, `ringing_db`).
- `renders/references/<unit>/ab/` — our own renders with DECAY searched to
  match the reference's T60, plus `index.html`, a review page
  (`tools/review/make_review.py --reference`) pinning the references next to
  them.
- `renders/references/summary.json` — everything below, as data.
- `docs/reference-report.md` — the same thing as prose and tables.

Re-running is safe: every output file name is deterministic and gets
overwritten, not appended to. Missing takes are listed, not fatal — whatever
exists is still processed, and the missing core takes are called out.

## What it checks, per take

1. **Latency.** Take 0 (loopback: interface out straight to interface in)
   is cross-correlated against the click stimulus to find the interface's
   round-trip latency in samples. That one number is then used to align
   *every other take in the session* — not re-derived per take. A spring
   smears the stimulus's sharp click into a dispersive chirp, so there is no
   sharp feature left in a spring take to cross-correlate reliably against;
   the digital/analog signal path's latency is the same for every take
   recorded in one session regardless. (Per-take cross-correlation is only
   used as a fallback when no take 0 exists anywhere, with a lower-confidence
   note in the output. It works fine on the Magneto's *tape* takes, MW/MD,
   since tape doesn't disperse the way a spring does.)
2. **Levels.** Clip count (`|x| >= 0.999`), DC offset, and noise floor (RMS
   dBFS) from the pre-roll before the stimulus starts.
3. **"100% wet" (take A / MA only).** The recipe says SPRINGS should be
   fully wet, delay DRY/WET fully dry, and the analysis "looks for a direct
   click at each onset" — the recipe doesn't say precisely how. This tool's
   reading: reuse the M1 click-detector shape (a sample-to-sample second
   difference more than 20 dB above the local background, `docs/m1-contracts.md`)
   in a ~2.5 ms window right at each click's known onset time. A spring's own
   response, even though it starts at the same instant as the click, builds
   up continuously rather than jumping like a raw 2-sample click; a positive
   hit here means some dry signal is still leaking through and SPRINGS/DELAY
   DRY-WET should be rechecked.
4. **T60, spectrogram, stereo metrics, ringing_db** via
   `build/rv_render --analyze` on the aligned WAV (the M1 sidecar contract).
   T60 can legitimately come back `null` — `schroederT60()` only fits when
   the decay reaches -35 dB before the next click, and this can be a near
   thing when there's *any* low-level structure near the 8-second boundary
   between clicks (a distant reflection, residual dispersion, noise). It
   showed up in this tool's own `--selftest` fake data at one DECAY setting.
   Treat it the way the contract does: "not measurable" for that take, not
   an error, and the DECAY search below is prepared for it.
5. **Dispersion** (click takes: A, A-L, A-R, MA) via `tools/ir_dispersion.py`:
   chirp repeat time, lows-later dispersion, fC.
6. **Wow & flutter → WOBBLE targets** (Magneto MW0-MW4 only) via
   `tools/pitch_track.py` on the first 8 s of `08_held_tones.wav` (the 1 kHz
   tone; the chord that follows isn't single-pitched, so it isn't pitch-tracked).
   Reports median pitch, p95/peak cents deviation, and the dominant wobble
   rate + depth. MW1/MW2 (9 o'clock-noon) become the proposed **Drift**
   target range; MW3/MW4 (3 o'clock-CW) become the proposed **Warble** range
   (ADR 0008's zones).
7. **Drive colour** (Wellspring C, Magneto MD1-3): brightness (3-6 kHz vs
   0.7-1.4 kHz, as `tools/ir_analysis.py`), crest factor, and an HF-energy
   ratio used as a distortion **proxy**. This is *not* true THD — THD needs
   a stationary sine tone, and these takes are recordings of percussive hits
   (`02_hits.wav`) at different drive levels, not a sine sweep. The proxy is
   useful for relative comparison across the C / MD1 / MD2 / MD3 series
   (do things get brighter and harder as INPUT/REC LVL rises?), not as an
   absolute % figure.
8. **Matched A/B render.** DECAY is searched (bisection, since T60 increases
   monotonically with DECAY per SPEC M1) so our own render's T60 matches the
   Wellspring take A's (or Magneto MA's) measured T60, at MIX 1. The recipe
   and SPEC say "DECAY set to match T60" but don't say which ATTITUDE — this
   tool renders **both CLEAN and DRIVEN** rather than guessing one.

## Testing it (no real recordings needed)

```bash
python3 tools/ingest_references.py --selftest
python3 tools/pitch_track.py --selftest
```

`ingest_references.py --selftest` builds a stand-in reference set under
`renders/reference-fake/` (gitignored, like everything under `renders/`):

- take 0 (loopback): the click stimulus itself, delayed 137 samples
  (~2.85 ms) and attenuated 3 dB in plain Python — a loopback cable adds no
  DSP, just the interface's own latency and gain.
- take A: our own Tank (`build/rv_render`) rendering the click stimulus,
  standing in for "the Wellspring's own recording", with the same delay/gain
  applied afterward (as if it had been played out and re-recorded).
- take C: a Tank render of the hits stimulus at a different, more driven
  setting, to exercise the drive-colour proxy path.
- Magneto MW0-MW4: synthetic held tones (not through the Tank — it doesn't
  model tape wow & flutter, that's a hardware property being calibrated
  against) with known wobble depth/rate at each knob position.

It then runs the real pipeline on that set and checks the results against
the known ground truth: latency within 0.15 ms, T60 of the matched render
within 3% of the reference's, and wobble depth/rate within 15%.
`tools/pitch_track.py --selftest` checks the pitch tracker alone against
synthetic tones (see below).

## `tools/pitch_track.py`

Pitch vs. time for a held tone, via a bandpass + FFT-based Hilbert-transform
instantaneous-frequency track (not a per-frame autocorrelation lag: at
1 kHz, one sample of lag is already about 35 cents, far too coarse to
resolve a few cents of wobble — the analytic-signal phase derivative gets
sub-cent resolution instead).

```bash
python3 tools/pitch_track.py --selftest
python3 tools/pitch_track.py <wav> [--start-s S] [--end-s S] [--json out.json]
```

`--selftest` uses four synthetic 1 kHz tones: fixed, ±10 cents at 0.5 Hz,
±50 cents at 1.5 Hz, and a bounded random walk ("random drift"). The two
sinusoidal wobble cases must recover both depth and rate within 10% — they
do (typically within 1-3%). The **random-drift case has no single true
depth/rate** (it isn't a sinusoid), so unlike the other three, its check is
plausibility (median pitch, p95/peak cents within the drift's own clip
bound), not exact recovery. This is a place the M8 prep brief's phrasing
("must recover depth within ~10% and rate within ~10%") doesn't quite fit
literally, since "depth" and "rate" aren't well-defined for a random walk.

## What's missing and what's ambiguous

- **Missing takes:** listed under "Missing" per unit in the report, with
  missing *core* takes called out separately. Whatever takes exist are still
  fully processed. Matched A/B renders and the review page are skipped (with
  a warning, not an error) if the click take (A / MA) is itself missing or
  its T60 isn't measurable.
- **No take 0 anywhere:** falls back to per-take cross-correlation, flagged
  as lower-confidence, and warned about.
- **"Direct click at each onset" (ADR 0009, recipe step 7):** interpreted as
  above (#3); there's no stricter spec to check it against. Treat a flag as
  "go recheck SPRINGS/DELAY DRY-WET", not as a hard failure.
- **"THD ... drive colour references" (M8 prep brief):** the recipe only
  records hits at these takes, not a sine tone, so true THD isn't
  computable; a brightness/crest/HF-ratio proxy is reported instead (#7).
- **"DECAY chosen ... MIX 1, DRIVEN/CLEAN":** read as "produce both", not
  "pick one" (#8).
- **T60 `null` on a take that looks fine by ear:** a real possibility (#4,
  reproduced in `--selftest`), not a bug — `docs/m1-contracts.md`'s
  Schroeder fit is allowed to say "not measurable". The DECAY search retries
  a candidate at small DECAY offsets before giving up on the whole search,
  since this can be a one-point edge case rather than a real dead end.
