#!/usr/bin/env python3
"""Generate a tiny fake render batch to develop/test make_review.py against,
before Streams A/B (DSP + Renderer) land.

Writes:
  tools/review/sample/out/manifest.json + sidecars + short WAVs
    (a 3 x 2 grid over decay x boing, ~6 renders, one deliberately flagged)
  tools/review/sample/reference/wellspring_A_test.wav + .json
    (a fake stereo reference: wet L + R, per docs/recording-recipe.md)

stdlib-only (no numpy): implements a small iterative radix-2 FFT in pure
Python to compute spectrograms that match the Stream B sidecar contract
(docs/m1-contracts.md) exactly, so the review page's rendering can be
verified before the real Renderer exists.
"""

import base64
import cmath
import json
import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 48000
HERE = Path(__file__).resolve().parent
OUT_DIR = HERE / "out"
REF_DIR = HERE / "reference"

FFT_SIZE = 2048
SPEC_WIDTH = 160
SPEC_HEIGHT = 100
F_MIN = 40.0
F_MAX = 16000.0
DB_MIN = -100.0
DB_MAX = 0.0


# ---------------------------------------------------------------- FFT ----

def fft(samples):
    """In-place iterative radix-2 Cooley-Tukey FFT. len(samples) must be a
    power of two. Returns a new list of complex values."""
    n = len(samples)
    a = list(samples)
    # bit-reversal permutation
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    length = 2
    while length <= n:
        ang = -2 * math.pi / length
        wlen = complex(math.cos(ang), math.sin(ang))
        half = length // 2
        for start in range(0, n, length):
            w = complex(1.0, 0.0)
            for k in range(half):
                u = a[start + k]
                v = a[start + k + half] * w
                a[start + k] = u + v
                a[start + k + half] = u - v
                w *= wlen
        length <<= 1
    return a


def hann_window(n):
    return [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]


_HANN = hann_window(FFT_SIZE)


# ---------------------------------------------------------- synth audio ----

def make_signal(decay, boing, duration_s, flagged=False):
    """3 s of decaying, dispersive chirp-ish noise. decay in [0,1] controls
    T60-ish tail length; boing in [0,1] controls chirp sweep rate."""
    n = int(duration_s * SAMPLE_RATE)
    out = [0.0] * n
    rng = random.Random(int(decay * 1000) * 31 + int(boing * 1000) * 7 + (999 if flagged else 0))

    tau = 0.15 + decay * 2.2  # seconds, exponential decay time constant
    f0 = 1800.0 + boing * 4200.0  # chirp starts higher for more boing
    f1 = 120.0 + boing * 60.0
    chirp_dur = 0.9 + (1.0 - boing) * 0.6

    phase = 0.0
    for i in range(n):
        t = i / SAMPLE_RATE
        env = math.exp(-t / tau)
        chirp_t = min(t, chirp_dur)
        frac = chirp_t / chirp_dur
        inst_f = f0 + (f1 - f0) * frac  # descending chirp: highs before lows
        phase += 2 * math.pi * inst_f / SAMPLE_RATE
        tone = math.sin(phase)
        noise = rng.uniform(-1.0, 1.0)
        s = env * (0.6 * tone + 0.4 * noise * math.exp(-t / (tau * 0.5)))
        out[i] = s

    if flagged:
        # Inject a steady narrowband tone (Ringing) held > 2 s above -30 dBFS,
        # plus a couple of hard clips, to exercise the flagged-metrics path.
        ring_f = 440.0
        ring_phase = 0.0
        for i in range(n):
            t = i / SAMPLE_RATE
            if t < 2.5:
                ring_phase += 2 * math.pi * ring_f / SAMPLE_RATE
                out[i] += 0.5 * math.sin(ring_phase)
        # hard clips near the start
        for i in range(200, 260):
            out[i] = 1.3 if (i % 2 == 0) else -1.3

    peak = max(1e-9, max(abs(x) for x in out))
    if not flagged:
        # normalise comfortably under 0 dBFS for the "clean" renders
        target = 0.85
        out = [x * (target / peak) for x in out]
    else:
        # keep the injected clips intact: only tame everything else a bit
        out = [max(-1.5, min(1.5, x)) for x in out]
    return out


def write_wav_mono(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for x in samples:
            v = max(-1.0, min(1.0, x))
            frames += struct.pack("<h", int(v * 32767))
        w.writeframes(bytes(frames))


def write_wav_stereo(path, left, right):
    assert len(left) == len(right)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for l, r in zip(left, right):
            l = max(-1.0, min(1.0, l))
            r = max(-1.0, min(1.0, r))
            frames += struct.pack("<hh", int(l * 32767), int(r * 32767))
        w.writeframes(bytes(frames))


# -------------------------------------------------------------- metrics ----

def dbfs(x):
    if x <= 1e-10:
        return -200.0
    return 20 * math.log10(x)


def compute_basic_metrics(samples, flagged):
    peak = max(abs(x) for x in samples) if samples else 0.0
    rms = math.sqrt(sum(x * x for x in samples) / len(samples)) if samples else 0.0
    clip_count = sum(1 for x in samples if abs(x) >= 0.999)
    nan_inf_count = sum(1 for x in samples if math.isnan(x) or math.isinf(x))
    # This is fake data for UI development, not a real click detector (that's
    # Stream B's job against the real spec §4.10 definition) -- just give the
    # deliberately-flagged render a nonzero count so the "clicks" flag chip
    # has something to display, and leave clean renders at zero.
    click_count = 37 if flagged else 0
    return {
        "peak_dbfs": round(dbfs(peak), 2),
        "rms_dbfs": round(dbfs(rms), 2),
        "t60_s": None,
        "resonance_peak_db": 15.5 if flagged else round(3.0 + random.random() * 4, 1),
        "steady_tone": bool(flagged),
        "nan_inf_count": nan_inf_count,
        "clip_count": clip_count,
        "click_count": click_count,
    }


# --------------------------------------------------------- spectrogram ----

def compute_spectrogram(samples, duration_s):
    n = len(samples)
    hop = max(1, (n - FFT_SIZE) // (SPEC_WIDTH - 1)) if n > FFT_SIZE else 1

    # Precompute log-spaced target frequencies per output row (row 0 = f_max).
    log_min, log_max = math.log(F_MIN), math.log(F_MAX)
    row_freqs = []
    for row in range(SPEC_HEIGHT):
        frac = row / (SPEC_HEIGHT - 1)
        f = math.exp(log_max - frac * (log_max - log_min))
        row_freqs.append(f)
    bin_hz = SAMPLE_RATE / FFT_SIZE
    row_bins = [min(FFT_SIZE // 2 - 1, max(0, round(f / bin_hz))) for f in row_freqs]

    data = bytearray(SPEC_WIDTH * SPEC_HEIGHT)
    for col in range(SPEC_WIDTH):
        start = min(n - FFT_SIZE, col * hop) if n > FFT_SIZE else 0
        start = max(0, start)
        window = samples[start:start + FFT_SIZE]
        if len(window) < FFT_SIZE:
            window = window + [0.0] * (FFT_SIZE - len(window))
        windowed = [window[i] * _HANN[i] for i in range(FFT_SIZE)]
        spectrum = fft([complex(v, 0.0) for v in windowed])
        mags_db = None  # computed lazily per row below via cache
        mag_cache = {}
        for row in range(SPEC_HEIGHT):
            b = row_bins[row]
            if b not in mag_cache:
                mag = abs(spectrum[b]) / FFT_SIZE
                mag_cache[b] = 20 * math.log10(mag) if mag > 1e-12 else -300.0
            db = mag_cache[b]
            norm = (db - DB_MIN) / (DB_MAX - DB_MIN)
            norm = max(0.0, min(1.0, norm))
            data[row * SPEC_WIDTH + col] = round(norm * 255)

    return {
        "width": SPEC_WIDTH,
        "height": SPEC_HEIGHT,
        "t0_s": 0.0,
        "t1_s": duration_s,
        "f_min_hz": F_MIN,
        "f_max_hz": F_MAX,
        "freq_scale": "log",
        "db_min": DB_MIN,
        "db_max": DB_MAX,
        "data_b64": base64.b64encode(bytes(data)).decode("ascii"),
    }


# ------------------------------------------------------------------ main ----

def build_render(decay, boing, index, flagged=False):
    duration_s = 3.0
    samples = make_signal(decay, boing, duration_s, flagged=flagged)
    stem = "m1_sample__decay{:.2f}_boing{:.2f}".format(decay, boing)
    wav_name = stem + ".wav"
    sidecar_name = stem + ".json"

    write_wav_mono(OUT_DIR / wav_name, samples)

    metrics = compute_basic_metrics(samples, flagged)
    spectrogram = compute_spectrogram(samples, duration_s)

    sidecar = {
        "wav": wav_name,
        "sample_rate": SAMPLE_RATE,
        "duration_s": duration_s,
        "params": {
            "decay": decay,
            "boing": boing,
            "springs": "1",
            "attitude": "CLEAN",
        },
        "metrics": metrics,
        "spectrogram": spectrogram,
    }
    with open(OUT_DIR / sidecar_name, "w") as f:
        json.dump(sidecar, f, indent=2)

    return {"wav": wav_name, "sidecar": sidecar_name, "params": sidecar["params"]}


def build_reference():
    REF_DIR.mkdir(parents=True, exist_ok=True)
    duration_s = 2.0
    dry = make_signal(0.5, 0.0, duration_s, flagged=False)
    # "wet" channel: same dry seed, run through a slightly different decay so
    # it looks distinct in the spectrogram (this is fake data, not real DSP).
    wet = make_signal(0.9, 0.6, duration_s, flagged=False)
    name = "wellspring_A_test"
    write_wav_stereo(REF_DIR / (name + ".wav"), dry, wet)

    mono = [(l + r) / 2.0 for l, r in zip(dry, wet)]
    metrics = compute_basic_metrics(mono, flagged=False)
    spectrogram = compute_spectrogram(wet, duration_s)  # spectrogram of the wet (spring) side

    sidecar = {
        "wav": name + ".wav",
        "sample_rate": SAMPLE_RATE,
        "duration_s": duration_s,
        "params": {},
        "metrics": metrics,
        "spectrogram": spectrogram,
    }
    with open(REF_DIR / (name + ".json"), "w") as f:
        json.dump(sidecar, f, indent=2)


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    decays = [0.0, 0.5, 1.0]
    boings = [0.0, 1.0]
    renders = []
    flagged_done = False
    for decay in decays:
        for boing in boings:
            flagged = (not flagged_done) and decay == 1.0 and boing == 1.0
            entry = build_render(decay, boing, len(renders), flagged=flagged)
            if flagged:
                flagged_done = True
            renders.append(entry)

    manifest = {
        "name": "m1_sample",
        "created": "2026-09-28T00:00:00Z",
        "input": "test_audio/stimulus/01_clicks.wav",
        "renders": renders,
    }
    with open(OUT_DIR / "manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)

    build_reference()

    print("Wrote {} renders + manifest to {}".format(len(renders), OUT_DIR))
    print("Wrote fake reference to {}".format(REF_DIR))


if __name__ == "__main__":
    main()
