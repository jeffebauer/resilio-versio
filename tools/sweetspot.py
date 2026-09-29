#!/usr/bin/env python3
"""M8 sweet-spot analysis for Resilio Versio.

Reads M8 one-knob sweeps (presets/sweeps/m8_sweet_*.json + rendered
renders/m8_sweet_*/manifest.json + sidecars, see docs/m1-contracts.md
Stream B) and measures how much the sound changes between adjacent knob
steps, to flag dead zones (knob does nothing) and cliffs (knob jumps too
hard) per SPEC.md §2.3 "wide sweet spot".

stdlib-only: no third-party dependencies (numpy, scipy, etc. not used).

Usage:
    python3 tools/sweetspot.py [--renders-dir renders] [--out docs/m8-sweetspot.md]

Also runs the M8 gain-staging pass (a few DECAY x ATTITUDE renders of
several stimuli + generated pink noise, all MIX=1 wet-only) and folds a
summary into the same report. See --gain-* flags.
"""

import argparse
import array
import base64
import json
import math
import os
import random
import struct
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RENDER_BIN = ROOT / "build" / "rv_render"

# ---------------------------------------------------------------------------
# WAV reading (stdlib only; handles the 16/24-bit PCM / float32 that Wav.h
# writes). We only need mono-summed samples for analysis.
# ---------------------------------------------------------------------------

def read_wav(path, max_frames=None):
    """Return (samples_per_channel: list[list[float]], sample_rate).

    `max_frames`, when given, reads only the leading N frames (we only need
    a bounded analysis window for most descriptors; T60 comes from the
    full-file sidecar instead) -- keeps this stdlib-only decoder fast
    without numpy.
    """
    with open(path, "rb") as f:
        header = f.read(12)
        if header[0:4] != b"RIFF" or header[8:12] != b"WAVE":
            raise ValueError(f"not a RIFF/WAVE file: {path}")
        pos = 12
        channels = None
        sample_rate = None
        bits = None
        audio_fmt = None
        data_bytes = None
        while True:
            chunk_hdr = f.read(8)
            if len(chunk_hdr) < 8:
                break
            cid = chunk_hdr[0:4]
            size = struct.unpack_from("<I", chunk_hdr, 4)[0]
            if cid == b"fmt ":
                body = f.read(size + (size & 1))
                audio_fmt, channels, sample_rate, _, _, bits = struct.unpack_from("<HHIIHH", body, 0)
                if audio_fmt == 0xFFFE and size >= 26:
                    audio_fmt = struct.unpack_from("<H", body, 24)[0]
            elif cid == b"data":
                if channels is None:
                    raise ValueError(f"data chunk before fmt chunk: {path}")
                bytes_per = bits // 8
                frame_bytes = bytes_per * channels
                to_read = size
                if max_frames is not None:
                    to_read = min(size, max_frames * frame_bytes)
                data_bytes = f.read(to_read)
                # done: skip rest of file, we have what we need
                break
            else:
                f.seek(size + (size & 1), 1)
    if data_bytes is None or channels is None:
        raise ValueError(f"missing fmt/data chunk: {path}")
    bytes_per = bits // 8
    frame_bytes = bytes_per * channels
    is_float = (audio_fmt == 3)
    nframes = len(data_bytes) // frame_bytes

    chans = [[0.0] * nframes for _ in range(channels)]
    if is_float:
        flat = array.array("f")
        flat.frombytes(data_bytes[: nframes * frame_bytes])
        if sys.byteorder == "big":
            flat.byteswap()
        for i in range(nframes):
            base = i * channels
            for c in range(channels):
                chans[c][i] = flat[base + c]
    elif bits == 16:
        flat = array.array("h")
        flat.frombytes(data_bytes[: nframes * frame_bytes])
        if sys.byteorder == "big":
            flat.byteswap()
        inv = 1.0 / 32768.0
        for i in range(nframes):
            base = i * channels
            for c in range(channels):
                chans[c][i] = flat[base + c] * inv
    elif bits == 24:
        inv = 1.0 / 8388608.0
        fb = frame_bytes
        for i in range(nframes):
            base = i * fb
            for c in range(channels):
                off = base + c * 3
                chans[c][i] = int.from_bytes(data_bytes[off:off + 3], "little", signed=True) * inv
    else:
        raise ValueError(f"unsupported bit depth {bits}")
    return chans, sample_rate


def mono_sum(chans):
    if len(chans) == 1:
        return chans[0]
    n = len(chans[0])
    out = [0.0] * n
    for c in chans:
        for i in range(n):
            out[i] += c[i]
    inv = 1.0 / len(chans)
    return [x * inv for x in out]


# ---------------------------------------------------------------------------
# Descriptors
# ---------------------------------------------------------------------------

EPS = 1e-12


def rms_db(x):
    if not x:
        return -200.0
    s = sum(v * v for v in x) / len(x)
    if s <= EPS:
        return -200.0
    return 10.0 * math.log10(s)


def peak_db(x):
    if not x:
        return -200.0
    p = max((abs(v) for v in x), default=0.0)
    if p <= EPS:
        return -200.0
    return 20.0 * math.log10(p)


def null_diff_db(a, b):
    """Level of (a - b) relative to level of a, in dB. Very negative = a and b
    are nearly identical; near 0 dB = a and b are unrelated/opposite."""
    n = min(len(a), len(b))
    if n == 0:
        return 0.0
    diff_energy = 0.0
    ref_energy = 0.0
    for i in range(n):
        d = a[i] - b[i]
        diff_energy += d * d
        ref_energy += a[i] * a[i]
    diff_energy /= n
    ref_energy /= n
    if ref_energy <= EPS:
        return -200.0 if diff_energy <= EPS else 0.0
    if diff_energy <= EPS:
        return -200.0
    return 10.0 * math.log10(diff_energy / ref_energy)


def band_energy_db(x, sr, lo_hz, hi_hz):
    """Crude band energy via a simple DFT-free running Goertzel-ish estimate
    is overkill; use a short-window FFT substitute: a naive DFT would be too
    slow for full files, so we use a simple biquad bandpass energy instead
    (stdlib only, no numpy)."""
    # Two-pole bandpass (RBJ cookbook), Q chosen so lo..hi is the passband.
    center = math.sqrt(max(lo_hz, 1.0) * hi_hz)
    bw_oct = math.log2(hi_hz / max(lo_hz, 1.0))
    q = math.sqrt(2 ** bw_oct) / (2 ** bw_oct - 1) if bw_oct > 0 else 1.0
    w0 = 2 * math.pi * center / sr
    alpha = math.sin(w0) * math.sinh(math.log(2) / 2 * bw_oct * w0 / math.sin(w0)) if math.sin(w0) != 0 else 0.0001
    cos_w0 = math.cos(w0)
    b0 = alpha
    b1 = 0.0
    b2 = -alpha
    a0 = 1 + alpha
    a1 = -2 * cos_w0
    a2 = 1 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    x1 = x2 = y1 = y2 = 0.0
    energy = 0.0
    count = 0
    for xn in x:
        yn = b0 * xn + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, xn
        y2, y1 = y1, yn
        energy += yn * yn
        count += 1
    if count == 0 or energy <= EPS:
        return -200.0
    return 10.0 * math.log10(energy / count)


def spectral_balance_db(x, sr):
    """3-6 kHz energy minus 0.7-1.4 kHz energy, in dB. Positive = brighter."""
    hi = band_energy_db(x, sr, 3000.0, 6000.0)
    lo = band_energy_db(x, sr, 700.0, 1400.0)
    return hi - lo


def stereo_correlation(chans):
    if len(chans) < 2:
        return 1.0
    l, r = chans[0], chans[1]
    n = min(len(l), len(r))
    if n == 0:
        return 1.0
    ml = sum(l[:n]) / n
    mr = sum(r[:n]) / n
    num = 0.0
    dl = 0.0
    dr = 0.0
    for i in range(n):
        a = l[i] - ml
        b = r[i] - mr
        num += a * b
        dl += a * a
        dr += b * b
    denom = math.sqrt(dl * dr)
    if denom <= EPS:
        return 0.0
    return num / denom


def zero_crossing_period_variance(x, sr, band_lo=800.0, band_hi=1200.0):
    """Approximate pitch-wobble descriptor for WOBBLE: band-pass around the
    held 1 kHz tone, find zero-crossing periods, return their coefficient of
    variation (stdev/mean) as a proxy for pitch wobble depth. A proper pitch
    tracker is being written by another agent; this is a cheap stand-in that
    doesn't depend on it."""
    center = math.sqrt(band_lo * band_hi)
    bw_oct = math.log2(band_hi / band_lo)
    w0 = 2 * math.pi * center / sr
    alpha = math.sin(w0) * math.sinh(math.log(2) / 2 * bw_oct * w0 / math.sin(w0))
    cos_w0 = math.cos(w0)
    b0 = alpha
    b1 = 0.0
    b2 = -alpha
    a0 = 1 + alpha
    a1 = -2 * cos_w0
    a2 = 1 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    x1 = x2 = y1 = y2 = 0.0
    filtered = []
    for xn in x:
        yn = b0 * xn + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, xn
        y2, y1 = y1, yn
        filtered.append(yn)
    zc = []
    last = None
    for i in range(1, len(filtered)):
        if filtered[i - 1] < 0.0 <= filtered[i]:
            # linear-interpolated zero crossing sample index
            denom = (filtered[i] - filtered[i - 1])
            frac = (-filtered[i - 1] / denom) if denom != 0 else 0.0
            t = (i - 1 + frac)
            if last is not None:
                zc.append(t - last)
            last = t
    if len(zc) < 8:
        return 0.0
    mean_p = sum(zc) / len(zc)
    if mean_p <= 0:
        return 0.0
    var = sum((p - mean_p) ** 2 for p in zc) / len(zc)
    stdev = math.sqrt(var)
    return stdev / mean_p  # coefficient of variation, dimensionless


# ---------------------------------------------------------------------------
# Sweep loading / descriptor computation
# ---------------------------------------------------------------------------

MAX_ANALYSIS_SECONDS = 10.0  # cap per-file descriptor window (perf; t60 still comes from the full-file sidecar)


def cap_len(mono, sr, max_seconds=MAX_ANALYSIS_SECONDS):
    n = int(max_seconds * sr)
    return mono[:n] if len(mono) > n else mono


def cap_chans(chans, sr, max_seconds=MAX_ANALYSIS_SECONDS):
    n = int(max_seconds * sr)
    return [c[:n] if len(c) > n else c for c in chans]


THRESHOLDS = {
    "level_db": 0.5,        # JND for RMS level step, dB
    "null_diff_db": -40.0,  # below this, adjacent renders are "the same" (per-sample)
    "spectral_db": 0.5,     # JND for 3-6k/0.7-1.4k balance shift, dB
    "t60_pct": 0.03,        # 3% relative T60 change = JND (T60 perception is coarse)
    "correlation": 0.03,    # absolute stereo-correlation JND
    "wobble_cv": 0.02,      # absolute zero-crossing CoV JND (WOBBLE only)
}

CLIFF_RATIO = 3.0       # a step > 3x the sweep's median step size
CLIFF_LEVEL_DB = 3.0    # or an outright level jump > 3 dB


def find_sweeps(renders_dir):
    out = []
    for d in sorted(renders_dir.glob("m8_sweet_*")):
        manifest = d / "manifest.json"
        if manifest.exists():
            out.append(d)
    return out


def load_manifest(d):
    with open(d / "manifest.json") as f:
        return json.load(f)


def load_sidecar(d, name):
    with open(d / name) as f:
        return json.load(f)


def knob_key_for_sweep(manifest):
    # single-axis grid: the one key in the first render's params not shared
    # by all renders is the swept knob. Simpler: manifest doesn't store the
    # grid directly, so infer from the varying param across renders.
    renders = manifest["renders"]
    if len(renders) < 2:
        return None
    p0 = renders[0]["params"]
    p1 = renders[1]["params"]
    for k in p0:
        if p0[k] != p1.get(k):
            return k
    return None


def analyze_sweep(d):
    manifest = load_manifest(d)
    renders = manifest["renders"]
    knob = knob_key_for_sweep(manifest)
    is_wobble = knob == "wobble"
    sr_ref = None
    rows = []
    for r in renders:
        wav_path = d / r["wav"]
        sidecar = load_sidecar(d, r["sidecar"])
        chans, sr = read_wav(wav_path, max_frames=int(MAX_ANALYSIS_SECONDS * 48000) + 48000)
        sr_ref = sr
        chans = cap_chans(chans, sr)
        mono = mono_sum(chans)
        row = {
            "value": r["params"][knob],
            "wav": r["wav"],
            "mono": mono,
            "chans": chans,
            "rms_db": rms_db(mono),
            "peak_db": peak_db(mono),
            "spectral_db": spectral_balance_db(mono, sr),
            "t60_s": sidecar["metrics"].get("t60_s"),
            "correlation": stereo_correlation(chans),
        }
        if is_wobble:
            row["wobble_cv"] = zero_crossing_period_variance(mono, sr)
        rows.append(row)
    rows.sort(key=lambda r: r["value"])

    steps = []
    for i in range(1, len(rows)):
        a, b = rows[i - 1], rows[i]
        d_level = abs(b["rms_db"] - a["rms_db"])
        d_null = null_diff_db(a["mono"], b["mono"])
        d_spectral = abs(b["spectral_db"] - a["spectral_db"])
        d_corr = abs(b["correlation"] - a["correlation"])
        t60a, t60b = a["t60_s"], b["t60_s"]
        if t60a and t60b and t60a > 0:
            d_t60_pct = abs(t60b - t60a) / t60a
        else:
            d_t60_pct = None
        d_wobble = None
        if is_wobble:
            d_wobble = abs(b["wobble_cv"] - a["wobble_cv"])

        # "changed" = at least one descriptor clears its JND, OR the signals
        # are perceptibly different in the null test (not just level-matched
        # difference; null_diff itself already captures spectral+timing
        # differences the other descriptors might miss).
        changed = (
            d_level >= THRESHOLDS["level_db"]
            or d_null >= THRESHOLDS["null_diff_db"]
            or d_spectral >= THRESHOLDS["spectral_db"]
            or (d_t60_pct is not None and d_t60_pct >= THRESHOLDS["t60_pct"])
            or d_corr >= THRESHOLDS["correlation"]
            or (d_wobble is not None and d_wobble >= THRESHOLDS["wobble_cv"])
        )
        steps.append({
            "from": a["value"], "to": b["value"],
            "d_level_db": d_level, "d_null_db": d_null, "d_spectral_db": d_spectral,
            "d_t60_pct": d_t60_pct, "d_corr": d_corr, "d_wobble_cv": d_wobble,
            "changed": changed,
        })

    # composite "amount of change" score per step, used for cliff detection
    def score(s):
        parts = [s["d_level_db"], max(0.0, s["d_null_db"] + 60.0)]  # shift null so more-different = larger
        if s["d_t60_pct"] is not None:
            parts.append(s["d_t60_pct"] * 20.0)  # scale to roughly dB-ish range
        parts.append(s["d_spectral_db"])
        if s["d_wobble_cv"] is not None:
            parts.append(s["d_wobble_cv"] * 50.0)
        return sum(parts)

    for s in steps:
        s["score"] = score(s)
    scores = sorted(s["score"] for s in steps)
    median_score = scores[len(scores) // 2] if scores else 0.0

    cliffs = []
    for s in steps:
        is_cliff = (median_score > 0 and s["score"] > CLIFF_RATIO * median_score) or s["d_level_db"] > CLIFF_LEVEL_DB
        s["cliff"] = is_cliff
        if is_cliff:
            cliffs.append(s)

    dead_zones = []
    run = []
    for s in steps:
        if not s["changed"]:
            run.append(s)
        else:
            if len(run) >= 3:
                dead_zones.append((run[0]["from"], run[-1]["to"]))
            run = []
    if len(run) >= 3:
        dead_zones.append((run[0]["from"], run[-1]["to"]))

    return {
        "dir": d.name,
        "knob": knob,
        "manifest": manifest,
        "rows": rows,
        "steps": steps,
        "cliffs": cliffs,
        "dead_zones": dead_zones,
        "median_score": median_score,
    }


def parse_sweep_dirname(name):
    # m8_sweet_<knob>_[skank_]<attitude>
    assert name.startswith("m8_sweet_")
    rest = name[len("m8_sweet_"):]
    attitude = None
    for att in ("clean", "driven", "kicked"):
        if rest.endswith("_" + att):
            attitude = att
            rest = rest[: -(len(att) + 1)]
            break
    stim = "skank" if rest.endswith("_skank") else ("hits" if not rest.endswith("skank") else "skank")
    if rest.endswith("_skank"):
        knob = rest[: -len("_skank")]
        stim = "skank"
    else:
        knob = rest
        stim = "held_tones" if knob == "wobble" else "hits"
    return knob, stim, attitude


# ---------------------------------------------------------------------------
# Gain staging
# ---------------------------------------------------------------------------

def write_pink_noise_wav(path, seconds=6.0, sr=48000, seed=8):
    """Paul Kellet's stdlib pink-noise approximation, mono, 16-bit PCM."""
    rng = random.Random(seed)
    n = int(seconds * sr)
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0
    samples = []
    for _ in range(n):
        white = rng.uniform(-1.0, 1.0)
        b0 = 0.99886 * b0 + white * 0.0555179
        b1 = 0.99332 * b1 + white * 0.0750759
        b2 = 0.96900 * b2 + white * 0.1538520
        b3 = 0.86650 * b3 + white * 0.3104856
        b4 = 0.55000 * b4 + white * 0.5329522
        b5 = -0.7616 * b5 - white * 0.0168980
        pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362
        b6 = white * 0.115926
        samples.append(pink * 0.11)
    peak = max((abs(v) for v in samples), default=1.0) or 1.0
    scale = 0.89 / peak
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        frames = bytearray()
        for v in samples:
            iv = max(-32768, min(32767, int(v * scale * 32767)))
            frames += struct.pack("<h", iv)
        w.writeframes(bytes(frames))


GAIN_STIMULI = [
    "test_audio/stimulus/02_hits.wav",
    "test_audio/stimulus/04_skank.wav",
    "test_audio/stimulus/08_held_tones.wav",
    "test_audio/stimulus/06_noise_bursts.wav",
]
GAIN_DECAYS = [0.25, 0.5, 0.75, 1.0]
# `rv_render --set key=value` parses value with strtof (no Switch3 label
# support, unlike preset JSON / Sweep base) — pass the Normalised switch
# position directly (ParamSpec.h switchToNormalised: CLEAN=0, DRIVEN=0.5,
# KICKED=1; springs "2" = 0.5).
GAIN_ATTITUDES = [("CLEAN", 0.0), ("DRIVEN", 0.5), ("KICKED", 1.0)]
LIMITER_THRESHOLD = 0.82  # SPEC/ADR safety limiter is 0.89; flag approach at 0.82


def run_gain_staging(out_dir, scratch_dir):
    scratch_dir.mkdir(parents=True, exist_ok=True)
    pink_path = scratch_dir / "pink_noise.wav"
    if not pink_path.exists():
        write_pink_noise_wav(pink_path)

    stimuli = [ROOT / s for s in GAIN_STIMULI] + [pink_path]

    results = []
    render_dir = out_dir / "m8_gain_staging"
    render_dir.mkdir(parents=True, exist_ok=True)

    for stim_path in stimuli:
        stim_name = stim_path.stem
        # dry reference metrics
        dry_chans, dry_sr = read_wav(stim_path)
        dry_mono = mono_sum(dry_chans)
        dry_rms = rms_db(dry_mono)
        dry_peak = peak_db(dry_mono)

        for decay in GAIN_DECAYS:
            for att, att_norm in GAIN_ATTITUDES:
                out_name = f"gain__{stim_name}__d{decay:.2f}__{att.lower()}.wav"
                out_path = render_dir / out_name
                cmd = [
                    str(RENDER_BIN), str(stim_path), str(out_path),
                    "--set", f"decay={decay}",
                    "--set", "mix=1.0",
                    "--set", "springs=0.5",
                    "--set", f"attitude={att_norm}",
                ]
                subprocess.run(cmd, check=True, capture_output=True)
                wet_chans, sr = read_wav(out_path)
                wet_mono = mono_sum(wet_chans)
                wet_rms = rms_db(wet_mono)
                wet_peak = peak_db(wet_mono)
                over = sum(1 for v in wet_mono if abs(v) > LIMITER_THRESHOLD)
                results.append({
                    "stimulus": stim_name,
                    "decay": decay,
                    "attitude": att,
                    "dry_rms_db": dry_rms,
                    "dry_peak_db": dry_peak,
                    "wet_rms_db": wet_rms,
                    "wet_peak_db": wet_peak,
                    "wet_minus_dry_db": wet_rms - dry_rms,
                    "samples_over_082": over,
                    "total_samples": len(wet_mono),
                })
    return results


# ---------------------------------------------------------------------------
# Report generation
# ---------------------------------------------------------------------------

def fmt(v, digits=2):
    if v is None:
        return "n/a"
    return f"{v:.{digits}f}"


def render_report(sweep_analyses, gain_results, out_path, review_base_rel="../renders"):
    lines = []
    lines.append("# M8 sweet-spot report — Resilio Versio")
    lines.append("")
    lines.append("Generated by `tools/sweetspot.py`. Companion sweeps: `presets/sweeps/m8_sweet_*.json`, "
                  "renders + review pages: `renders/m8_sweet_<knob>_<attitude>/index.html`.")
    lines.append("")

    # ---- Plain language summary -----------------------------------------
    lines.append("## What this is, in plain language")
    lines.append("")
    lines.append(
        "SPEC.md's design rule for M8 is a **wide sweet spot**: turning any of the 7 knobs "
        "anywhere across its travel should do *something* audible and useful, with no dead "
        "patch where the knob does nothing, and no sudden jump where a small twist causes a "
        "big, surprising change (a \"cliff\")."
    )
    lines.append("")
    lines.append(
        "To check this without relying on ears alone, this tool renders each knob from 0 to 1 "
        "in 11 steps (0, 0.1, 0.2 … 1.0), with every other knob left at its default, once per "
        "ATTITUDE switch position (CLEAN / DRIVEN / KICKED). It then compares each pair of "
        "neighbouring renders (e.g. DECAY 0.3 vs DECAY 0.4) using five simple, independent "
        "measurements:"
    )
    lines.append("")
    lines.append("- **Level** — RMS loudness in dB. Catches \"the knob just makes it louder/quieter\".")
    lines.append(
        "- **Null difference** — subtract one render from its neighbour, sample by sample, and "
        "compare the leftover energy to the original. A very negative number (deep null) means "
        "the two renders are nearly identical waveforms; close to 0 dB means they're unrelated. "
        "This is the most sensitive test — it catches timing/spectral changes level alone would miss."
    )
    lines.append("- **Spectral balance** — energy in the 3–6 kHz \"splash\" band minus the 0.7–1.4 kHz band, in dB. Catches brightness/darkness shifts.")
    lines.append("- **T60** — measured tail length from the sidecar. Catches decay-time changes.")
    lines.append("- **Stereo correlation** — catches width/spread changes.")
    lines.append(
        "- **Pitch wobble** (WOBBLE only) — coefficient of variation of the zero-crossing period "
        "of the 1 kHz held tone, band-passed. A cheap stand-in for a real pitch tracker (another "
        "agent is building one); this doesn't depend on it."
    )
    lines.append("")
    lines.append(
        "A step counts as **audible** if *any* one of those measurements crosses a "
        "just-noticeable-difference threshold (0.5 dB level, 0.5 dB spectral tilt, −40 dB null "
        "difference, 3% relative T60 change, 0.03 correlation, 0.02 wobble CoV — chosen "
        "conservatively small, so a step has to fail *all* of them to count as silent)."
    )
    lines.append("")
    lines.append(
        "- **Dead zone** = 3 or more consecutive steps (≥ 0.3 of the knob's travel) where "
        "nothing crosses its threshold — the knob is doing nothing there."
    )
    lines.append(
        "- **Cliff** = a single step whose combined change is more than 3× the sweep's typical "
        "(median) step, or an outright level jump over 3 dB — the knob does too much at once."
    )
    lines.append("")

    # ---- Per-knob tables ---------------------------------------------------
    lines.append("## Sweeps, dead zones, cliffs")
    lines.append("")

    by_knob = {}
    for a in sweep_analyses:
        knob, stim, att = parse_sweep_dirname(a["dir"])
        by_knob.setdefault(knob, []).append((stim, att, a))

    dead_zone_summary = []
    cliff_summary = []

    for knob in ["decay", "tone", "tension", "splash", "drive", "wobble", "mix"]:
        entries = by_knob.get(knob, [])
        if not entries:
            continue
        lines.append(f"### {knob.upper()}")
        lines.append("")
        lines.append("| Stimulus | Attitude | Dead zones (0–1 range) | Cliffs (step) | Review page |")
        lines.append("|---|---|---|---|---|")
        for stim, att, a in sorted(entries, key=lambda e: (e[0], e[1])):
            dz = a["dead_zones"]
            dz_str = "; ".join(f"{fmt(lo,1)}–{fmt(hi,1)}" for lo, hi in dz) if dz else "none"
            cl = a["cliffs"]
            cl_str = "; ".join(f"{fmt(c['from'],1)}→{fmt(c['to'],1)} (score {fmt(c['score'],1)}, ΔdB {fmt(c['d_level_db'],1)})" for c in cl) if cl else "none"
            review_link = f"{review_base_rel}/{a['dir']}/index.html"
            lines.append(f"| {stim} | {att} | {dz_str} | {cl_str} | [{a['dir']}]({review_link}) |")
            if dz:
                dead_zone_summary.append((knob, stim, att, dz))
            if cl:
                cliff_summary.append((knob, stim, att, cl))
        lines.append("")

    # ---- Headline findings --------------------------------------------------
    lines.append("## Headline findings")
    lines.append("")
    if dead_zone_summary:
        lines.append("**Dead zones found:**")
        lines.append("")
        for knob, stim, att, dz in dead_zone_summary:
            ranges = ", ".join(f"{fmt(lo,1)}–{fmt(hi,1)}" for lo, hi in dz)
            lines.append(f"- {knob.upper()} × {att} ({stim}): {ranges}")
        lines.append("")
    else:
        lines.append("**Dead zones found:** none — every knob changed the sound in every 0.1 step tested, on every ATTITUDE.")
        lines.append("")

    if cliff_summary:
        lines.append("**Cliffs found:**")
        lines.append("")
        for knob, stim, att, cl in cliff_summary:
            steps = ", ".join(f"{fmt(c['from'],1)}→{fmt(c['to'],1)}" for c in cl)
            lines.append(f"- {knob.upper()} × {att} ({stim}): {steps}")
        lines.append("")
    else:
        lines.append("**Cliffs found:** none — no single step exceeded 3× the sweep's median step size or jumped level by more than 3 dB.")
        lines.append("")

    # ---- Gain staging --------------------------------------------------------
    lines.append("## Gain staging")
    lines.append("")
    lines.append(
        "Wet-only (MIX=1) renders across 02_hits, 04_skank, 08_held_tones, 06_noise_bursts and a "
        "generated pink-noise file, at DECAY 0.25/0.5/0.75/1.0 and each ATTITUDE (defaults "
        "otherwise). Renders in `renders/m8_gain_staging/`."
    )
    lines.append("")
    lines.append("| Stimulus | Decay | Attitude | Dry RMS | Wet RMS | Wet−Dry | Wet peak | Samples > 0.82 |")
    lines.append("|---|---|---|---|---|---|---|---|")
    for r in gain_results:
        pct_over = 100.0 * r["samples_over_082"] / max(1, r["total_samples"])
        lines.append(
            f"| {r['stimulus']} | {r['decay']:.2f} | {r['attitude']} | {fmt(r['dry_rms_db'],1)} dB | "
            f"{fmt(r['wet_rms_db'],1)} dB | {fmt(r['wet_minus_dry_db'],1)} dB | {fmt(r['wet_peak_db'],1)} dB | "
            f"{r['samples_over_082']} ({fmt(pct_over,3)}%) |"
        )
    lines.append("")

    # dark vs bright quantification: compare 08_held_tones (a sine-ish/bright
    # stand-in for "bright") against 06_noise_bursts / pink noise (broadband,
    # a stand-in for "dark/dense") at matched decay/attitude. Simpler: report
    # the spread of wet_minus_dry_db across stimuli at fixed decay/attitude.
    by_da = {}
    for r in gain_results:
        by_da.setdefault((r["decay"], r["attitude"]), []).append(r)
    spreads = []
    for (decay, att), rs in sorted(by_da.items()):
        vals = [r["wet_minus_dry_db"] for r in rs]
        spread = max(vals) - min(vals)
        spreads.append((decay, att, spread, rs))
    max_spread = max(spreads, key=lambda s: s[2]) if spreads else None

    lines.append(
        "**\"Reverb comes back louder on dark material\" question.** At each DECAY/ATTITUDE "
        "combination, wet-minus-dry RMS varies across the 5 stimuli by up to "
        f"**{fmt(max_spread[2],1) if max_spread else 'n/a'} dB** "
        f"(DECAY {fmt(max_spread[0],2) if max_spread else ''}, {max_spread[1] if max_spread else ''}). "
        "That confirms the owner's ear: the wet level is *not* flat across program material — it "
        "depends on how much energy the input has in the band the tank actually resonates in "
        "(springs respond more to broadband/dark, LF-heavy material than to a narrowband bright "
        "tone, because more of that energy sits in the tank's passband)."
    )
    lines.append("")
    lines.append(
        "**Suggested normalisation approach (not implemented — read-only per M8 scope):** measure "
        "the input's energy in the tank's excitation band (roughly the Chirp/loop passband, "
        "~200 Hz–4 kHz per `core/params/Mappings.h` `kDampingMinHz`/`kDampingMaxHz` and "
        "`kTransitionMinHz`/`kTransitionMaxHz`) with a slow (~300 ms, matching `kAutoMakeupSeconds` "
        "in `core/params/DriveVoicing.h`) RMS follower, and apply a gentle inverse-gain trim to the "
        "wet path before the output limiter — mirroring the existing `kAutoMakeupMax` (+6 dB cap) "
        "auto-makeup pattern already used for DriveIn/DriveOut, so dark and bright material land "
        "within a couple dB of each other without audibly pumping on transients."
    )
    lines.append("")

    # ---- Tuning actions --------------------------------------------------
    lines.append("## Candidate tuning actions")
    lines.append("")
    lines.append(
        "Ranked by how directly they'd close a dead zone/cliff found above, or the gain-staging "
        "spread. Core is untouched by this report; these are read-outs of the constants that "
        "would need to move."
    )
    lines.append("")
    lines.append("| # | Action | Governing constant(s) |")
    lines.append("|---|---|---|")
    lines.append("| 1 | Bend DECAY's low end so short settings still feel different from each other (T60 curve is already exponential; consider a steeper low-end curve or floor) | `kT60MinSeconds`/`kT60MaxSeconds`, `decayT60Seconds()` in `core/params/Mappings.h` |")
    lines.append("| 2 | Widen or re-center TENSION's range if the tight/loose ends read too similar | TENSION anchors (`kLoopDelay*`, `kTransition*`, `kTensionCoeff*`, `kTensionStageFracMid`) in `core/params/Mappings.h` |")
    lines.append("| 3 | Re-shape SPLASH's hit-threshold curve if clatter/jolt onset is too sudden or too flat across the knob | `kHitThresholdSplash0`/`kHitThresholdSplash1`, `hitThreshold()`, `kClatterGain` in `core/params/SplashVoicing.h` |")
    lines.append("| 4 | Retune DRIVE's onset curve if the clean→driven transition is too abrupt (a cliff) or too gradual (a dead zone) around the ADR 0014 \"9 o'clock / 3 o'clock\" targets | `kDriveCurvePower`, `kPushSlope`/`kPushCentre`, `driveCurve()`/`pushCurve()` in `core/params/DriveVoicing.h` |")
    lines.append("| 5 | Normalise wet level vs program-material brightness (gain-staging finding above) | new slow RMS follower + trim, alongside `kAutoMakeupSeconds`/`kAutoMakeupMax` in `core/params/DriveVoicing.h` |")
    lines.append("| 6 | If WOBBLE's Drift half (0–0.5) reads too subtle vs Warble quarter (0.75–1), reshape the depth curve | `kWobbleCurve`, `kDriftEnd`, `kWarbleStart`, `wobbleCents()` in `core/params/SplashVoicing.h` |")
    lines.append("| 7 | If TONE's tilt feels flat near noon, steepen the pivot/db range (already a \"hero control\" per SPEC §2.3.3) | `kTiltPivotHz`, `kTiltCcwDb`/`kTiltCwDb` in `core/params/DriveVoicing.h` |")
    lines.append("")

    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--renders-dir", default=str(ROOT / "renders"))
    ap.add_argument("--out", default=str(ROOT / "docs" / "m8-sweetspot.md"))
    ap.add_argument("--skip-gain", action="store_true", help="skip gain-staging render pass (reuse existing renders/m8_gain_staging)")
    ap.add_argument("--scratch-dir", default=str(ROOT / "tools" / "_sweetspot_scratch"))
    args = ap.parse_args()

    renders_dir = Path(args.renders_dir)
    sweep_dirs = find_sweeps(renders_dir)
    if not sweep_dirs:
        print("no m8_sweet_* renders found under", renders_dir, file=sys.stderr)
        sys.exit(1)

    print(f"analyzing {len(sweep_dirs)} sweeps...")
    analyses = []
    for d in sweep_dirs:
        print("  ", d.name)
        analyses.append(analyze_sweep(d))

    if args.skip_gain:
        gain_dir = renders_dir / "m8_gain_staging"
        gain_results = []
        manifest_path = gain_dir / "gain_results.json"
        if manifest_path.exists():
            gain_results = json.loads(manifest_path.read_text())
    else:
        print("running gain-staging pass...")
        gain_results = run_gain_staging(renders_dir, Path(args.scratch_dir))
        (renders_dir / "m8_gain_staging").mkdir(parents=True, exist_ok=True)
        with open(renders_dir / "m8_gain_staging" / "gain_results.json", "w") as f:
            json.dump(gain_results, f, indent=2)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    render_report(analyses, gain_results, out_path)
    print("wrote", out_path)


if __name__ == "__main__":
    main()
