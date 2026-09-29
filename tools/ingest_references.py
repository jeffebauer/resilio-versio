#!/usr/bin/env python3
"""Turn Wellspring / Magneto reference recordings into measurements and A/B
review pages, in one command. See docs/reference-ingest.md.

Usage:
  python3 tools/ingest_references.py test_audio/reference/
  python3 tools/ingest_references.py --selftest

Reads `<unit>_<take>_<desc>.wav` files (docs/recording-recipe.md,
docs/recording-recipe-magneto.md) from a directory. For each take found:
  - detects interface latency from take 0 (loopback) by cross-correlating
    against the matching stimulus file, and aligns/trims every other take
    to the stimulus timeline the same way;
  - checks levels (clipping, DC offset, noise floor from the pre-roll);
  - checks "100% wet" on take A / MA (looks for a direct click at each
    click onset -- see docs/reference-ingest.md for what that means here);
  - runs `build/rv_render --analyze` (T60, spectrogram, stereo metrics,
    ringing_db) and, on click takes, `tools/ir_dispersion.py` (chirp
    repeat time / dispersion / fC);
  - runs `tools/pitch_track.py` on the Magneto MW wow & flutter takes,
    turning cents-of-wobble into proposed WOBBLE depth targets per zone
    (ADR 0008);
  - measures a drive-colour proxy (brightness + a distortion PROXY, not
    true THD -- see docs/reference-ingest.md) on the MD takes and
    Wellspring take C;
  - searches DECAY so our own render's T60 matches the Wellspring take A's
    measured T60 (MIX 1, ATTITUDE CLEAN and DRIVEN), then builds an A/B
    review page pinning the references next to those matched renders.

Missing takes are listed, not fatal: whatever exists is still processed.
Idempotent: every output file name is deterministic and gets overwritten,
not appended to.

Stdlib only (subprocess calls out to build/rv_render and
tools/ir_dispersion.py; tools/pitch_track.py is imported directly).
"""

import argparse
import json
import math
import re
import shutil
import struct
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STIMULUS_DIR = ROOT / "test_audio" / "stimulus"
RV_RENDER = ROOT / "build" / "rv_render"
IR_DISPERSION = ROOT / "tools" / "ir_dispersion.py"
MAKE_REVIEW = ROOT / "tools" / "review" / "make_review.py"
MAKE_STIMULUS = ROOT / "tools" / "make_stimulus.py"

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pitch_track as pt  # noqa: E402  (sibling module, tools/pitch_track.py)

SR = 48000

TAKE_STIMULUS = {
    "wellspring": {
        "0": "01_clicks.wav", "A": "01_clicks.wav", "B": "02_hits.wav", "C": "02_hits.wav",
        "D": "03_sweep.wav", "E": "04_skank.wav", "E2": "04_skank.wav",
        "F": "05_silence_for_kicks.wav", "G": "06_noise_bursts.wav",
        "A-L": "01_clicks.wav", "A-R": "01_clicks.wav",
    },
    "magneto": {
        "0": "01_clicks.wav",
        "MA": "01_clicks.wav", "MB": "02_hits.wav", "ME": "04_skank.wav",
        "MW0": "08_held_tones.wav", "MW1": "08_held_tones.wav", "MW2": "08_held_tones.wav",
        "MW3": "08_held_tones.wav", "MW4": "08_held_tones.wav",
        "MD1": "02_hits.wav", "MD2": "02_hits.wav", "MD3": "02_hits.wav",
    },
}
CORE_TAKES = {"wellspring": ["0", "A", "B", "C", "D", "E"], "magneto": ["MA", "MB", "ME"]}
OPTIONAL_TAKES = {
    "wellspring": ["E2", "F", "G", "A-L", "A-R"],
    "magneto": ["MW0", "MW1", "MW2", "MW3", "MW4", "MD1", "MD2", "MD3"],
}
ALL_TAKES = {u: CORE_TAKES[u] + OPTIONAL_TAKES[u] for u in CORE_TAKES}
CLICK_TAKES = {"wellspring": ["A", "A-L", "A-R"], "magneto": ["MA"]}
WET_CHECK_TAKES = {"wellspring": ["A"], "magneto": ["MA"]}
WOW_TAKES = ["MW0", "MW1", "MW2", "MW3", "MW4"]
WOW_POSITIONS = {"MW0": "fully CCW", "MW1": "9 o'clock", "MW2": "12 o'clock", "MW3": "3 o'clock", "MW4": "fully CW"}
DRIVE_TAKES = {"wellspring": ["C"], "magneto": ["MD1", "MD2", "MD3"]}
FNAME_RE = re.compile(r"^(wellspring|magneto)_([A-Za-z0-9]+(?:-[A-Za-z0-9]+)?)_(.+)\.wav$")


# ---------------------------------------------------------------- WAV I/O

def read_wav(path):
    """-> (channels: list[list[float]], sample_rate)."""
    with wave.open(str(path), "rb") as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 3:
        vals = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    elif width == 2:
        vals = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    elif width == 4:
        vals = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(raw) // 4), raw)]
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")
    return [vals[c::ch] for c in range(ch)], sr


def write_wav(path, channels, sr):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    n = len(channels[0])
    frames = bytearray()
    for i in range(n):
        for c in channels:
            v = max(-1.0, min(1.0, c[i]))
            frames += struct.pack("<i", int(round(v * 8388607)))[:3]
    with wave.open(str(path), "wb") as w:
        w.setnchannels(len(channels))
        w.setsampwidth(3)
        w.setframerate(sr)
        w.writeframes(bytes(frames))


def mono_of(channels):
    if len(channels) == 1:
        return channels[0]
    return [sum(v) / len(channels) for v in zip(*channels)]


# ---------------------------------------------------------------- small DSP (self-contained, mirrors pitch_track.py's helpers)

def one_pole_lp(x, sr, fc):
    a = math.exp(-2 * math.pi * fc / sr)
    y, out = 0.0, []
    for s in x:
        y = (1 - a) * s + a * y
        out.append(y)
    return out


def decimate(x, sr, target_hz):
    factor = max(1, int(sr // target_hz))
    if factor <= 1:
        return list(x), float(sr)
    new_rate = sr / factor
    y = x
    for _ in range(2):
        y = one_pole_lp(y, sr, 0.45 * new_rate)
    return y[::factor], new_rate


def db(x):
    return 20 * math.log10(x) if x and x > 0 else None


# ---------------------------------------------------------------- alignment (cross-correlation, coarse-to-fine)

def best_lag_direct(rec, templ, lag_range):
    t_energy = math.sqrt(sum(v * v for v in templ)) or 1.0
    best_lag, best_score = None, -1e18
    for lag in lag_range:
        if lag < 0 or lag + len(templ) > len(rec):
            continue
        seg = rec[lag:lag + len(templ)]
        dot = sum(a * b for a, b in zip(seg, templ))
        e = math.sqrt(sum(v * v for v in seg)) or 1.0
        score = dot / (e * t_energy)
        if score > best_score:
            best_score, best_lag = score, lag
    return best_lag, best_score


def find_latency(rec_mono, stim_mono, sr, max_lag_s=1.0, coarse_hz=1000.0):
    """rec[lag] aligns with stim[0]. Returns (lag_samples, confidence 0-1) or
    (None, 0). Coarse stage: a wide (2 s) decimated template, cheap and
    tolerant of where the stimulus's own leading silence ends. Fine stage:
    a short template *anchored on the stimulus's first event* (an all-silence
    window, e.g. the first 0.125 s of a file that leads with 1 s of silence,
    would correlate with everything and nothing)."""
    rec_d, dsr = decimate(rec_mono, sr, coarse_hz)
    stim_d, _ = decimate(stim_mono, sr, coarse_hz)
    templ_len = min(len(stim_d), int(2.0 * dsr))
    if templ_len < 8 or len(rec_d) < templ_len:
        return None, 0.0
    templ = stim_d[:templ_len]
    max_off = min(int(max_lag_s * dsr), len(rec_d) - templ_len)
    if max_off < 0:
        return None, 0.0
    lag_d, score_d = best_lag_direct(rec_d, templ, range(0, max_off + 1))
    if lag_d is None:
        return None, 0.0
    factor = max(1, int(round(sr / dsr)))
    centre = lag_d * factor

    events = detect_events(stim_mono, sr)
    anchor = events[0] if events else 0
    fine_len = min(len(stim_mono) - max(0, anchor - 2000), int(0.125 * sr))
    if fine_len < 100:
        return centre, score_d
    window_start = max(0, anchor - 2000)
    fine_templ = stim_mono[window_start:window_start + fine_len]

    lo = max(0, centre + window_start - 2 * factor)
    hi = min(len(rec_mono) - fine_len, centre + window_start + 2 * factor)
    if hi < lo:
        return centre, score_d
    lag_f, score_f = best_lag_direct(rec_mono, fine_templ, range(lo, hi + 1))
    if lag_f is None:
        return centre, score_d
    return lag_f - window_start, score_f


# ---------------------------------------------------------------- event detection

def detect_events(x, sr, thresh_db=-40.0, min_gap_s=0.5):
    """Sample indices where |x| crosses thresh_db after >= min_gap_s below it
    (matches the M1 metrics contract's definition of an 'event')."""
    thr = 10 ** (thresh_db / 20)
    min_gap = int(min_gap_s * sr)
    events = []
    last = -min_gap - 1
    for i, v in enumerate(x):
        if abs(v) >= thr and i - last >= min_gap:
            events.append(i)
            last = i
    return events


# ---------------------------------------------------------------- level checks

def level_report(channels, sr, pre_roll_end):
    out = {}
    labels = ["L", "R"][:len(channels)] if len(channels) > 1 else ["mono"]
    for lab, x in zip(labels, channels):
        clip_count = sum(1 for v in x if abs(v) >= 0.999)
        end = pre_roll_end if pre_roll_end and pre_roll_end > sr * 0.05 else min(len(x), int(0.5 * sr))
        pre = x[:max(1, end - int(0.02 * sr))]
        dc = sum(pre) / len(pre) if pre else None
        noise_rms = math.sqrt(sum(v * v for v in pre) / len(pre)) if pre else None
        out[lab] = {
            "clip_count": clip_count,
            "dc_offset": dc,
            "noise_floor_dbfs": db(noise_rms) if noise_rms else None,
        }
    return out


# ---------------------------------------------------------------- "100% wet" check (docs/reference-ingest.md: heuristic)

def click_at_onset(x, sr, onset_i, window_s=0.0025, bg_s=0.02):
    """Reuses the M1 click-detector shape (second-difference discontinuity
    vs local RMS) right at a click onset, to catch a raw dry click leaking
    through a supposedly 100%-wet path -- see docs/reference-ingest.md."""
    def second_diff(seg):
        return [seg[i] - 2 * seg[i - 1] + seg[i - 2] for i in range(2, len(seg))]
    w = max(3, int(window_s * sr))
    bgw = max(w, int(bg_s * sr))
    at = x[max(0, onset_i - 2):onset_i + w]
    bg = x[max(0, onset_i - bgw - 2):max(0, onset_i - 2)]
    if len(at) < 4 or len(bg) < 4:
        return None, False
    at_d, bg_d = second_diff(at), second_diff(bg)
    if not at_d or not bg_d:
        return None, False
    at_peak = max(abs(v) for v in at_d)
    bg_rms = math.sqrt(sum(v * v for v in bg_d) / len(bg_d))
    if at_peak <= 0:
        return None, False
    db_above = 200.0 if bg_rms <= 1e-9 else 20 * math.log10(at_peak / bg_rms)
    detected = db_above >= 20.0 and at_peak >= 10 ** (-60 / 20)
    return db_above, detected


def wet_check(rec_mono, sr, lag, stim_events):
    results = []
    for ev in stim_events:
        onset_i = lag + ev
        if onset_i < 0 or onset_i >= len(rec_mono):
            continue
        db_above, detected = click_at_onset(rec_mono, sr, onset_i)
        results.append({"onset_s": ev / sr, "db_above_bg": db_above, "direct_click_detected": detected})
    flagged = sum(1 for r in results if r["direct_click_detected"])
    return {"onsets_checked": len(results), "flagged": flagged, "per_onset": results}


# ---------------------------------------------------------------- drive-colour proxy (docs/reference-ingest.md: not true THD)

def drive_colour(mono, sr, onset_i, dur_s=2.0):
    seg = mono[onset_i:onset_i + int(dur_s * sr)] if onset_i is not None else mono
    if len(seg) < sr * 0.1:
        return None
    peak = max(abs(v) for v in seg) or 1e-12
    rms = math.sqrt(sum(v * v for v in seg) / len(seg)) or 1e-12
    e_hi = sum(v * v for v in pt.bandpass(seg, sr, 4240.0, 3030.0))
    e_mid = sum(v * v for v in pt.bandpass(seg, sr, 1000.0, 714.0))
    e_dist = sum(v * v for v in pt.bandpass(seg, sr, 8500.0, 14166.0))
    e_fund = sum(v * v for v in pt.bandpass(seg, sr, 900.0, 1800.0))
    return {
        "peak_dbfs": db(peak),
        "rms_dbfs": db(rms),
        "crest_db": db(peak) - db(rms) if db(peak) is not None and db(rms) is not None else None,
        "brightness_db": 10 * math.log10(e_hi / e_mid) if e_hi > 0 and e_mid > 0 else None,
        "hf_distortion_proxy_db": 10 * math.log10(e_dist / e_fund) if e_dist > 0 and e_fund > 0 else None,
    }


# ---------------------------------------------------------------- subprocess helpers

def run_rv_render(args, timeout=600):
    if not RV_RENDER.exists():
        return None, "build/rv_render not found -- build Core/Renderer first (not attempted here per instructions)"
    try:
        r = subprocess.run([str(RV_RENDER)] + [str(a) for a in args], capture_output=True, text=True, timeout=timeout)
    except Exception as e:  # noqa: BLE001
        return None, f"rv_render failed to run: {e}"
    if r.returncode != 0:
        return r, f"rv_render exited {r.returncode}: {(r.stderr or r.stdout).strip()[:500]}"
    return r, None


def rv_analyze(wav_path, sidecar_out, channel="mix"):
    _, err = run_rv_render(["--analyze", wav_path, "--sidecar-out", sidecar_out, "--channel", channel])
    return err


def rv_render_set(in_wav, out_wav, sets):
    args = [in_wav, out_wav]
    for k, v in sets.items():
        args += ["--set", f"{k}={v}"]
    args.append("--sidecar")
    _, err = run_rv_render(args)
    return err


def run_ir_dispersion(wav_path):
    if not IR_DISPERSION.exists():
        return None, "tools/ir_dispersion.py missing"
    tmp = wav_path.with_suffix(".dispersion.json")
    try:
        r = subprocess.run([sys.executable, str(IR_DISPERSION), str(wav_path), "--json", str(tmp)],
                            capture_output=True, text=True, timeout=300)
    except Exception as e:  # noqa: BLE001
        return None, f"ir_dispersion.py failed to run: {e}"
    if r.returncode != 0 or not tmp.exists():
        return None, f"ir_dispersion.py exited {r.returncode}: {(r.stderr or r.stdout).strip()[:400]}"
    doc = json.loads(tmp.read_text())
    tmp.unlink(missing_ok=True)
    irs = doc.get("irs") or []
    return (irs[0] if irs else None), None


# ---------------------------------------------------------------- DECAY search (matched Resilio A/B render)

ATTITUDE_NORM = {"CLEAN": 0.0, "DRIVEN": 0.5, "KICKED": 1.0}


def make_search_stimulus(tmp_dir):
    """First-to-second-click segment of 01_clicks.wav only, so DECAY search
    renders are ~9.5 s instead of the full ~49 s file (same T60 estimate:
    the metrics contract measures T60 from the first event to just before
    the second one anyway)."""
    channels, sr = read_wav(STIMULUS_DIR / "01_clicks.wav")
    mono = channels[0]
    events = detect_events(mono, sr)
    end = events[1] if len(events) > 1 else len(mono)
    end = min(len(mono), end + int(0.2 * sr))
    trimmed = [ch[:end] for ch in channels]
    out = tmp_dir / "_decay_search_stimulus.wav"
    write_wav(out, trimmed, sr)
    return out


def measure_t60(search_stim, tmp_dir, sets, tag):
    out_wav = tmp_dir / f"{tag}.wav"
    err = rv_render_set(search_stim, out_wav, sets)
    if err:
        return None, err
    sidecar_path = out_wav.with_suffix(".json")
    if not sidecar_path.exists():
        return None, "rv_render did not write a sidecar"
    t60 = json.loads(sidecar_path.read_text()).get("metrics", {}).get("t60_s")
    return t60, None


def search_decay(target_t60, search_stim, tmp_dir, base_sets, iterations=14):
    """Bisection on DECAY (T60 increases monotonically with it, SPEC M1). A
    candidate's T60 can legitimately come back null -- schroederT60() only
    fits when the decay reaches -35 dB before the next click, and a real
    (or rendered) tail's own late, very quiet structure can sit right on
    that edge (see docs/reference-ingest.md). Nudge the candidate slightly
    before giving up on the whole search over one unlucky sample point."""
    lo, hi = 0.0, 1.0
    history = []
    mid = 0.5
    for it in range(iterations):
        mid = (lo + hi) / 2
        t60 = err = None
        for nudge in (0.0, 0.003, -0.003, 0.007, -0.007):
            d = min(1.0, max(0.0, mid + nudge))
            sets = dict(base_sets)
            sets["decay"] = round(d, 6)
            t60, err = measure_t60(search_stim, tmp_dir, sets, tag=f"_decay_search_{it}")
            history.append({"decay": d, "t60_s": t60, "error": err})
            if t60 is not None:
                break
        if t60 is None:
            return mid, history, err or "T60 not measurable (even after nudging DECAY)"
        if abs(t60 - target_t60) <= max(0.05, 0.02 * target_t60):
            break
        if t60 < target_t60:
            lo = mid
        else:
            hi = mid
    return mid, history, None


# ---------------------------------------------------------------- take discovery

def parse_takes(ref_dir):
    """-> {unit: {take: Path}}, warnings list for unrecognised files."""
    found = {"wellspring": {}, "magneto": {}}
    warnings = []
    for p in sorted(Path(ref_dir).glob("*.wav")):
        m = FNAME_RE.match(p.name)
        if not m:
            warnings.append(f"unrecognised reference filename (skipped): {p.name}")
            continue
        unit, take, _desc = m.groups()
        if take not in ALL_TAKES.get(unit, []):
            warnings.append(f"{p.name}: take '{take}' not in the known {unit} take list, ingesting anyway")
        found[unit][take] = p
    return found, warnings


# ---------------------------------------------------------------- per-take processing

def process_take(unit, take, path, out_dir, warnings, unit_lag=None):
    """Align to the stimulus, write the aligned WAV, run rv_render --analyze,
    level/wet checks. Returns a dict of results (never raises; failures are
    recorded as warnings and partial results).

    `unit_lag`: the session's interface latency in samples, from take 0. All
    takes in a session share one analog signal path, so once take 0 has
    measured it, every other take is aligned with that same value rather
    than re-deriving it by cross-correlation -- which is unreliable for a
    spring take, since the spring smears the stimulus's sharp click/attack
    into a dispersive chirp with no sharp feature left to correlate on. Only
    falls back to per-take cross-correlation when take 0 is unavailable."""
    stim_name = TAKE_STIMULUS[unit][take]
    stim_path = STIMULUS_DIR / stim_name
    if not stim_path.exists():
        warnings.append(f"{unit} {take}: stimulus {stim_name} missing, run tools/make_stimulus.py")
        return {"take": take, "file": path.name, "ok": False, "reason": "stimulus missing"}

    channels, sr = read_wav(path)
    rec_mono = mono_of(channels)
    stim_channels, stim_sr = read_wav(stim_path)
    stim_mono = stim_channels[0]
    if stim_sr != sr:
        warnings.append(f"{unit} {take}: sample rate {sr} != stimulus {stim_sr}")

    if unit_lag is not None:
        lag, confidence, source = unit_lag, 1.0, "take_0 (session interface latency)"
    else:
        lag, confidence = find_latency(rec_mono, stim_mono, sr)
        source = "per-take cross-correlation (no take 0 available -- less reliable on spring-smeared takes)"
    result = {"take": take, "file": path.name, "stimulus": stim_name, "ok": lag is not None,
              "latency_samples": lag, "latency_ms": (lag / sr * 1000.0) if lag is not None else None,
              "alignment_confidence": confidence, "latency_source": source}
    if lag is None:
        warnings.append(f"{unit} {take}: could not align to {stim_name}")
        return result

    # Trim to the stimulus timeline (drop the interface latency, pad/trim to stimulus+tail length).
    tail = max(0, len(rec_mono) - lag - len(stim_mono))
    end = lag + len(stim_mono) + min(tail, int(2.0 * sr))
    aligned = [ch[lag:end] if lag < len(ch) else [] for ch in channels]
    aligned_path = out_dir / f"{unit}_{take}.wav"
    write_wav(aligned_path, aligned, sr)
    result["aligned_wav"] = str(aligned_path.relative_to(ROOT))

    events = detect_events(stim_mono, sr)
    pre_roll_end = min(events) if events else int(0.5 * sr)
    result["levels"] = level_report(channels, sr, lag + pre_roll_end)

    sidecar_path = aligned_path.with_suffix(".json")
    err = rv_analyze(str(aligned_path), str(sidecar_path))
    if err:
        warnings.append(f"{unit} {take}: {err}")
        result["analyze_error"] = err
    else:
        sidecar = json.loads(sidecar_path.read_text())
        result["metrics"] = sidecar.get("metrics")

    if take in WET_CHECK_TAKES.get(unit, []):
        aligned_mono = mono_of(aligned)
        result["wet_check"] = wet_check(aligned_mono, sr, 0, events)

    if take in CLICK_TAKES.get(unit, []):
        ir, err = run_ir_dispersion(aligned_path)
        if err:
            warnings.append(f"{unit} {take}: ir_dispersion: {err}")
        result["dispersion"] = ir

    if take in DRIVE_TAKES.get(unit, []):
        aligned_mono = mono_of(aligned)
        onset = events[0] if events else 0
        result["drive_colour"] = drive_colour(aligned_mono, sr, onset)

    return result


def process_wow_take(unit, take, path, out_dir, warnings, unit_lag=None):
    """MW takes: track pitch on the 1 kHz portion of 08_held_tones.wav (the
    chord that follows isn't single-pitched, so it is not tracked here --
    see docs/reference-ingest.md). These go through the Magneto's tape path,
    not the spring, so its attack isn't dispersively smeared and per-take
    cross-correlation is reliable; take 0's latency is still preferred when
    available, for consistency with the rest of the session."""
    stim_path = STIMULUS_DIR / "08_held_tones.wav"
    channels, sr = read_wav(path)
    rec_mono = mono_of(channels)
    stim_channels, _ = read_wav(stim_path)
    stim_mono = stim_channels[0]
    if unit_lag is not None:
        lag, confidence = unit_lag, 1.0
    else:
        lag, confidence = find_latency(rec_mono, stim_mono, sr)
    if lag is None:
        warnings.append(f"{unit} {take}: could not align to 08_held_tones.wav")
        return {"take": take, "file": path.name, "ok": False}
    # held_tones(): 1 s silence, 8 s @ 1 kHz, 1 s gap, 8 s chord, 2 s tail.
    tone_start = lag + int(1.05 * sr)
    tone_end = lag + int((1 + 8 - 0.1) * sr)
    r = pt.analyze(rec_mono, sr, start_s=tone_start / sr, end_s=tone_end / sr)
    r["take"] = take
    r["file"] = path.name
    r["knob_position"] = WOW_POSITIONS.get(take)
    r["latency_ms"] = lag / sr * 1000.0
    r["alignment_confidence"] = confidence
    if not r["ok"]:
        warnings.append(f"{unit} {take}: pitch_track: {r.get('reason')}")
    return r


# ---------------------------------------------------------------- orchestration

def ingest(ref_dir, out_root, notes_text=None, tmp_dir=None):
    ref_dir = Path(ref_dir).resolve()
    out_root = Path(out_root).resolve()
    warnings = []

    if not any(STIMULUS_DIR.glob("*.wav")):
        subprocess.run([sys.executable, str(MAKE_STIMULUS)], check=False, cwd=ROOT)

    found, w = parse_takes(ref_dir)
    warnings += w

    summary = {"units": {}, "warnings": warnings}
    for unit in ("wellspring", "magneto"):
        unit_out = out_root / unit
        unit_out.mkdir(parents=True, exist_ok=True)
        tmp = tmp_dir or (unit_out / "_tmp")
        tmp.mkdir(parents=True, exist_ok=True)

        takes_found = found.get(unit, {})
        missing = [t for t in ALL_TAKES[unit] if t not in takes_found]
        missing_core = [t for t in CORE_TAKES[unit] if t not in takes_found]

        unit_summary = {
            "takes_found": sorted(takes_found.keys()),
            "takes_missing": missing,
            "takes_missing_core": missing_core,
            "take_details": {},
            "wow_flutter": {},
        }

        # Take 0 latency: prefer this unit's own loopback, else fall back to
        # the other unit's (one loopback per session covers both, ADR 0009/0020).
        take0_path = takes_found.get("0")
        if take0_path is None:
            other = "magneto" if unit == "wellspring" else "wellspring"
            take0_path = found.get(other, {}).get("0")
            if take0_path is not None:
                warnings.append(f"{unit}: no take 0 of its own, using {other}'s loopback")
        unit_lag = None
        if take0_path is not None:
            r = process_take(unit, "0", take0_path, unit_out, warnings)
            unit_summary["take_details"]["0"] = r
            unit_summary["interface_latency_ms"] = r.get("latency_ms")
            unit_lag = r.get("latency_samples")
        else:
            warnings.append(f"{unit}: no take 0 (loopback) found anywhere; per-take latency is measured "
                             f"independently against each take's own stimulus (less reliable on spring takes)")

        for take, path in sorted(takes_found.items()):
            if take == "0":
                continue
            if take in WOW_TAKES:
                r = process_wow_take(unit, take, path, unit_out, warnings, unit_lag=unit_lag)
            else:
                r = process_take(unit, take, path, unit_out, warnings, unit_lag=unit_lag)
            unit_summary["take_details"][take] = r

        # Wow & flutter -> WOBBLE depth targets (ADR 0008 zones).
        wow = {t: unit_summary["take_details"][t] for t in WOW_TAKES if t in unit_summary["take_details"]}
        if wow:
            def depth(t):
                d = wow.get(t, {})
                return d.get("wobble_depth_cents") if d.get("ok") else None
            drift_vals = [depth(t) for t in ("MW1", "MW2") if depth(t) is not None]
            warble_vals = [depth(t) for t in ("MW3", "MW4") if depth(t) is not None]
            unit_summary["wow_flutter"] = {
                "per_take_cents": {t: depth(t) for t in WOW_TAKES},
                "drift_target_cents_range": [min(drift_vals), max(drift_vals)] if drift_vals else None,
                "warble_target_cents_range": [min(warble_vals), max(warble_vals)] if warble_vals else None,
            }

        # Matched Resilio A/B renders against the click take's measured T60.
        click_take = "A" if unit == "wellspring" else "MA"
        click_detail = unit_summary["take_details"].get(click_take, {})
        target_t60 = (click_detail.get("metrics") or {}).get("t60_s")
        if target_t60:
            ab_dir = unit_out / "ab"
            ab_dir.mkdir(parents=True, exist_ok=True)
            search_stim = make_search_stimulus(tmp)
            matched = {}
            renders_entries = []
            for attitude in ("CLEAN", "DRIVEN"):
                base_sets = {"mix": 1.0, "attitude": ATTITUDE_NORM[attitude]}
                decay, history, err = search_decay(target_t60, search_stim, tmp, base_sets)
                matched[attitude] = {"decay": decay, "iterations": len(history), "error": err,
                                      "final_t60_s": history[-1].get("t60_s") if history else None}
                if err:
                    warnings.append(f"{unit} matched render ({attitude}): {err}")
                    continue
                final_wav = ab_dir / f"resilio_match_{attitude}.wav"
                sets = dict(base_sets)
                sets["decay"] = round(decay, 6)
                rerr = rv_render_set(str(STIMULUS_DIR / "01_clicks.wav"), str(final_wav), sets)
                if rerr:
                    warnings.append(f"{unit} matched render ({attitude}): {rerr}")
                    continue
                renders_entries.append({"wav": final_wav.name, "sidecar": final_wav.with_suffix(".json").name,
                                         "params": sets})
            unit_summary["matched_decay_search"] = matched
            if renders_entries:
                manifest = {"name": f"{unit}_ab", "created": "generated-by-ingest_references",
                            "input": "test_audio/stimulus/01_clicks.wav", "renders": renders_entries}
                (ab_dir / "manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")
                if MAKE_REVIEW.exists():
                    r = subprocess.run([sys.executable, str(MAKE_REVIEW), str(ab_dir), "--reference", str(unit_out)],
                                        capture_output=True, text=True, cwd=ROOT)
                    if r.returncode != 0:
                        warnings.append(f"{unit}: make_review.py failed: {(r.stderr or r.stdout).strip()[:400]}")
                    else:
                        unit_summary["review_page"] = str((ab_dir / "index.html").relative_to(ROOT))
        else:
            warnings.append(f"{unit}: no measured T60 for {click_take}, skipping matched A/B renders")

        if tmp_dir is None:
            shutil.rmtree(tmp, ignore_errors=True)  # scratch DECAY-search renders only

        summary["units"][unit] = unit_summary

    summary["warnings"] = warnings
    out_root.mkdir(parents=True, exist_ok=True)
    (out_root / "summary.json").write_text(json.dumps(summary, indent=1, default=str) + "\n")
    write_report(summary, ROOT / "docs" / "reference-report.md", notes_text)
    return summary


# ---------------------------------------------------------------- markdown report

def fmt(v, n=2, suffix=""):
    if v is None:
        return "-"
    if isinstance(v, bool):
        return str(v)
    try:
        return f"{v:.{n}f}{suffix}"
    except (TypeError, ValueError):
        return str(v)


def write_report(summary, path, notes_text):
    lines = ["# Reference recording report", "", "Generated by `tools/ingest_references.py`. Plain language first; "
             "numbers and per-take detail below.", ""]

    for unit in ("wellspring", "magneto"):
        u = summary["units"].get(unit, {})
        lines.append(f"## {unit.capitalize()}")
        lines.append("")
        found = u.get("takes_found", [])
        missing = u.get("takes_missing", [])
        missing_core = u.get("takes_missing_core", [])
        lines.append(f"Found {len(found)} take(s): {', '.join(found) if found else '(none)'}.")
        if missing:
            core_flag = " (**core takes missing**: " + ", ".join(missing_core) + ")" if missing_core else ""
            lines.append(f"Missing: {', '.join(missing)}.{core_flag}")
        lat = u.get("interface_latency_ms")
        if lat is not None:
            lines.append(f"Interface latency (from take 0): **{fmt(lat, 2, ' ms')}**.")
        lines.append("")

        click_take = "A" if unit == "wellspring" else "MA"
        click = u.get("take_details", {}).get(click_take)
        if click and click.get("metrics"):
            m = click["metrics"]
            lines.append(f"**{click_take} (clicks):** T60 {fmt(m.get('t60_s'), 2, ' s')}, "
                         f"ringing_db {fmt(m.get('ringing_db'), 1, ' dB')}, "
                         f"peak {fmt(m.get('peak_dbfs'), 1, ' dBFS')}.")
            disp = click.get("dispersion")
            if disp and disp.get("ok"):
                lines.append(f"Chirp repeat {fmt(disp.get('repeat_ms'), 1, ' ms')}, "
                             f"dispersion (lows-later) {fmt(disp.get('lows_later_ms'), 2, ' ms')}, "
                             f"fC {fmt(disp.get('fc_hz'), 0, ' Hz')}.")
            wet = click.get("wet_check")
            if wet:
                verdict = "no direct click found at any onset (fully wet)" if wet["flagged"] == 0 else \
                    f"**direct click flagged at {wet['flagged']}/{wet['onsets_checked']} onsets -- check SPRINGS DRY/WET**"
                lines.append(f"100% wet check: {verdict}.")
            lines.append("")

        matched = u.get("matched_decay_search")
        if matched:
            lines.append("**Matched Resilio A/B render** (DECAY searched so T60 matches the reference, MIX 1):")
            for att, m in matched.items():
                if m.get("error"):
                    lines.append(f"- {att}: search failed ({m['error']})")
                else:
                    lines.append(f"- {att}: DECAY {fmt(m['decay'], 4)} -> T60 {fmt(m['final_t60_s'], 2, ' s')}")
            if u.get("review_page"):
                lines.append(f"- Review page: `{u['review_page']}`")
            lines.append("")

        wow = u.get("wow_flutter")
        if wow and wow.get("per_take_cents"):
            lines.append("**Wow & flutter -> WOBBLE targets (ADR 0008):**")
            for t in WOW_TAKES:
                c = wow["per_take_cents"].get(t)
                lines.append(f"- {t} ({WOW_POSITIONS.get(t)}): {fmt(c, 1, ' cents depth') if c is not None else 'missing/unmeasurable'}")
            if wow.get("drift_target_cents_range"):
                lines.append(f"- Proposed Drift target range (9 o'clock-noon): "
                             f"{fmt(wow['drift_target_cents_range'][0],1)}-{fmt(wow['drift_target_cents_range'][1],1)} cents")
            if wow.get("warble_target_cents_range"):
                lines.append(f"- Proposed Warble target range (3 o'clock-CW): "
                             f"{fmt(wow['warble_target_cents_range'][0],1)}-{fmt(wow['warble_target_cents_range'][1],1)} cents")
            lines.append("")

        drive_takes = [t for t in DRIVE_TAKES[unit] if t in u.get("take_details", {})]
        if drive_takes:
            lines.append("**Drive colour (proxy, not true THD -- see docs/reference-ingest.md):**")
            for t in drive_takes:
                dc = u["take_details"][t].get("drive_colour")
                if dc:
                    lines.append(f"- {t}: brightness {fmt(dc.get('brightness_db'), 1, ' dB')}, "
                                 f"crest {fmt(dc.get('crest_db'), 1, ' dB')}, "
                                 f"HF-distortion proxy {fmt(dc.get('hf_distortion_proxy_db'), 1, ' dB')}")
            lines.append("")

        lines.append("### Per-take detail")
        lines.append("")
        lines.append("| Take | Latency (ms) | Peak dBFS | T60 s | Ringing dB | Clip count | Notes |")
        lines.append("|---|---|---|---|---|---|---|")
        for t in ALL_TAKES[unit]:
            d = u.get("take_details", {}).get(t)
            if not d:
                lines.append(f"| {t} | - | - | - | - | - | missing |")
                continue
            m = d.get("metrics") or {}
            levels = d.get("levels") or {}
            clip = max((v.get("clip_count", 0) for v in levels.values()), default=0)
            note = d.get("analyze_error") or d.get("reason") or ("wow/flutter, see above" if t in WOW_TAKES else "")
            lines.append(f"| {t} | {fmt(d.get('latency_ms'),2)} | {fmt(m.get('peak_dbfs'),1)} | "
                         f"{fmt(m.get('t60_s'),2)} | {fmt(m.get('ringing_db'),1)} | {clip} | {note} |")
        lines.append("")

    if notes_text:
        lines.append("## Session notes (`NOTES.md`, verbatim)")
        lines.append("")
        lines.append("```")
        lines.append(notes_text.rstrip())
        lines.append("```")
        lines.append("")

    if summary.get("warnings"):
        lines.append("## Warnings")
        lines.append("")
        for w in summary["warnings"]:
            lines.append(f"- {w}")
        lines.append("")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


# ---------------------------------------------------------------- fake reference generator + end-to-end selftest

def build_fake_references(fake_dir):
    """Stand-in reference set (renders/reference-fake/, gitignored) for
    testing the whole pipeline without real recordings:
    - take 0: the click stimulus itself, straight-delayed + attenuated in
      Python (a loopback cable adds no DSP, just interface latency + gain).
    - take A / MA: our own Tank (build/rv_render) rendering the click
      stimulus, standing in for "the Wellspring/Magneto's own recording",
      then the same delay+gain applied (as if that render had been played
      out and re-recorded through an interface).
    - take C: Tank render of the hits stimulus at a different, more driven
      setting, for the drive-colour proxy path.
    - MW0-MW4: synthetic held tones (tools/pitch_track.py's own tone
      synthesiser) with known wobble depth/rate at each position, since the
      Tank doesn't model tape wow & flutter -- that's a hardware property
      being calibrated against, not something we render.
    Returns the known delay/gain/wobble ground truth for verification.
    """
    if not RV_RENDER.exists():
        raise RuntimeError("build/rv_render not found; cannot build the fake reference set")
    if not any(STIMULUS_DIR.glob("*.wav")):
        subprocess.run([sys.executable, str(MAKE_STIMULUS)], check=False, cwd=ROOT)

    fake_dir = Path(fake_dir)
    fake_dir.mkdir(parents=True, exist_ok=True)
    KNOWN_DELAY_SAMPLES = 137     # ~2.85 ms, a plausible interface round trip
    KNOWN_GAIN_DB = -3.0

    def delay_gain(mono, sr, delay, gain_db):
        g = 10 ** (gain_db / 20)
        out = [0.0] * delay + [v * g for v in mono]
        return out

    # take 0: loopback of 01_clicks.wav
    stim0, sr = read_wav(STIMULUS_DIR / "01_clicks.wav")
    l0 = delay_gain(stim0[0], sr, KNOWN_DELAY_SAMPLES, KNOWN_GAIN_DB)
    write_wav(fake_dir / "wellspring_0_loopback.wav", [l0, [0.0] * len(l0)], sr)

    # take A: our Tank rendering the click stimulus, "recorded" with the same delay/gain
    tank_a = fake_dir / "_tank_A.wav"
    # decay=0.4 (not, say, 0.55): its T60 (~1.5s) leaves enough margin
    # before the next click (8s later) that the Schroeder T60 fit isn't
    # sensitive to the tiny absolute-threshold event-boundary shift the
    # -3 dB fake "interface" gain below introduces -- a longer decay can
    # land the boundary right on the edge of the fit's -35 dB requirement
    # and make T60 legitimately unmeasurable (schroederT60 returns null by
    # design when it can't reach -35 dB; see docs/reference-ingest.md).
    err = rv_render_set(str(STIMULUS_DIR / "01_clicks.wav"), str(tank_a),
                         {"decay": 0.4, "tension": 0.5, "mix": 1.0, "attitude": ATTITUDE_NORM["CLEAN"]})
    if err:
        raise RuntimeError(f"fake take A render failed: {err}")
    a_ch, a_sr = read_wav(tank_a)
    aL = delay_gain(a_ch[0], a_sr, KNOWN_DELAY_SAMPLES, KNOWN_GAIN_DB)
    aR = delay_gain(a_ch[1] if len(a_ch) > 1 else a_ch[0], a_sr, KNOWN_DELAY_SAMPLES, KNOWN_GAIN_DB)
    write_wav(fake_dir / "wellspring_A_clicks.wav", [aL, aR], a_sr)

    # take C: driven hits, for the drive-colour proxy path
    tank_c = fake_dir / "_tank_C.wav"
    err = rv_render_set(str(STIMULUS_DIR / "02_hits.wav"), str(tank_c),
                         {"decay": 0.55, "tension": 0.5, "mix": 1.0, "drive": 0.8,
                          "attitude": ATTITUDE_NORM["DRIVEN"]})
    if err:
        raise RuntimeError(f"fake take C render failed: {err}")
    c_ch, c_sr = read_wav(tank_c)
    cL = delay_gain(c_ch[0], c_sr, KNOWN_DELAY_SAMPLES, KNOWN_GAIN_DB)
    cR = delay_gain(c_ch[1] if len(c_ch) > 1 else c_ch[0], c_sr, KNOWN_DELAY_SAMPLES, KNOWN_GAIN_DB)
    write_wav(fake_dir / "wellspring_C_hits_hot.wav", [cL, cR], c_sr)
    for f in (tank_a, tank_c, tank_a.with_suffix(".json"), tank_c.with_suffix(".json")):
        Path(f).unlink(missing_ok=True)

    # MW0-MW4: synthetic held tones with known wobble depth/rate, mimicking
    # 08_held_tones.wav's timeline (1 s silence, 8 s @ 1 kHz, 1 s gap, 8 s chord, 2 s tail).
    known_wobble = {
        "MW0": (0.0, 0.0), "MW1": (4.0, 0.6), "MW2": (9.0, 1.0),
        "MW3": (25.0, 1.4), "MW4": (55.0, 1.8),
    }
    chord = (220.0, 261.63, 329.63)
    for take, (depth, rate) in known_wobble.items():
        n_tone = int(8 * sr)
        fade = int(0.01 * sr)
        tone = []
        phase = 0.0
        for i in range(n_tone):
            c = depth * math.sin(2 * math.pi * rate * i / sr)
            f_inst = 1000.0 * (2 ** (c / 1200.0))
            phase += 2 * math.pi * f_inst / sr
            env = min(1.0, i / fade, (n_tone - 1 - i) / fade)
            tone.append(0.3 * math.sin(phase) * env)
        n_chord = int(8 * sr)
        chord_sig = [0.3 * min(1.0, i / fade, (n_chord - 1 - i) / fade) *
                     sum(math.sin(2 * math.pi * f * i / sr) for f in chord) / 3 for i in range(n_chord)]
        mono = [0.0] * sr + tone + [0.0] * sr + chord_sig + [0.0] * int(2 * sr)
        rec = delay_gain(mono, sr, KNOWN_DELAY_SAMPLES, KNOWN_GAIN_DB)
        write_wav(fake_dir / f"magneto_{take}_wow.wav", [rec], sr)

    return {"delay_samples": KNOWN_DELAY_SAMPLES, "delay_ms": KNOWN_DELAY_SAMPLES / sr * 1000.0,
            "gain_db": KNOWN_GAIN_DB, "wobble": known_wobble}


def selftest():
    print("Building a fake reference set (renders/reference-fake/) ...")
    fake_dir = ROOT / "renders" / "reference-fake"
    out_dir = ROOT / "renders" / "reference-fake-out"
    try:
        truth = build_fake_references(fake_dir)
    except RuntimeError as e:
        print(f"FAIL  could not build fake references: {e}")
        return 1

    print("Running ingest_references.py on it ...")
    summary = ingest(fake_dir, out_dir)

    fails = 0
    w = summary["units"]["wellspring"]

    got_lat = w.get("interface_latency_ms")
    want_lat = truth["delay_ms"]
    ok = got_lat is not None and abs(got_lat - want_lat) <= 0.15  # within ~7 samples @ 48k
    print(f"{'PASS' if ok else 'FAIL'}  latency: measured {fmt(got_lat,3)} ms, truth {want_lat:.3f} ms")
    fails += not ok

    a = w["take_details"].get("A", {})
    a_lat = a.get("latency_ms")
    ok = a_lat is not None and abs(a_lat - want_lat) <= 0.15
    print(f"{'PASS' if ok else 'FAIL'}  take A latency: measured {fmt(a_lat,3)} ms, truth {want_lat:.3f} ms")
    fails += not ok

    matched = w.get("matched_decay_search", {}).get("CLEAN", {})
    a_t60 = (a.get("metrics") or {}).get("t60_s")
    m_t60 = matched.get("final_t60_s")
    ok = a_t60 and m_t60 and abs(m_t60 - a_t60) <= max(0.1, 0.03 * a_t60)
    print(f"{'PASS' if ok else 'FAIL'}  T60 match: reference A {fmt(a_t60,3)} s, matched render {fmt(m_t60,3)} s")
    fails += not ok

    wet = a.get("wet_check", {})
    ok = wet.get("onsets_checked", 0) >= 5
    print(f"{'PASS' if ok else 'FAIL'}  wet check ran: {wet.get('onsets_checked')} onsets checked "
          f"({wet.get('flagged')} flagged)")
    fails += not ok

    dc = a.get("drive_colour")
    print(f"      (take A has no drive_colour -- only take C does; ignore)" if dc is None else "")
    c = w["take_details"].get("C", {})
    ok = c.get("drive_colour") is not None
    print(f"{'PASS' if ok else 'FAIL'}  drive colour computed for take C: {c.get('drive_colour')}")
    fails += not ok

    print()
    m = summary["units"]["magneto"]
    for take, (depth, rate) in truth["wobble"].items():
        d = m["take_details"].get(take, {})
        if depth == 0.0:
            ok = d.get("ok") and d.get("peak_cents", 99) < 2.0
            print(f"{'PASS' if ok else 'FAIL'}  {take} (no wobble): peak {fmt(d.get('peak_cents'),2)} cents")
        else:
            got_d, got_r = d.get("wobble_depth_cents"), d.get("wobble_rate_hz")
            ok = (d.get("ok") and got_d is not None and got_r is not None
                  and abs(got_d - depth) <= max(1.0, 0.15 * depth) and abs(got_r - rate) <= max(0.1, 0.15 * rate))
            print(f"{'PASS' if ok else 'FAIL'}  {take}: depth measured {fmt(got_d,2)}c (truth {depth}c), "
                 f"rate measured {fmt(got_r,3)}Hz (truth {rate}Hz)")
        fails += not ok

    print()
    print(("PASS" if fails == 0 else "FAIL") + f"  ingest_references selftest ({fails} failures)")
    print(f"Summary: {out_dir / 'summary.json'}")
    print(f"Report: docs/reference-report.md")
    return fails


# ---------------------------------------------------------------- CLI

def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ref_dir", nargs="?", help="directory of <unit>_<take>_<desc>.wav reference recordings")
    ap.add_argument("--out", default=str(ROOT / "renders" / "references"), help="output directory")
    ap.add_argument("--selftest", action="store_true", help="build a fake reference set and verify the pipeline")
    args = ap.parse_args(argv)

    if args.selftest:
        return 1 if selftest() else 0

    if not args.ref_dir:
        ap.error("ref_dir is required unless --selftest is given")

    ref_dir = Path(args.ref_dir)
    if not ref_dir.is_dir():
        print(f"error: {ref_dir} is not a directory", file=sys.stderr)
        return 2

    notes_path = ref_dir / "NOTES.md"
    notes_text = notes_path.read_text() if notes_path.exists() else None

    summary = ingest(ref_dir, args.out, notes_text=notes_text)
    n_warn = len(summary["warnings"])
    print(f"Done. {sum(len(u['takes_found']) for u in summary['units'].values())} take(s) processed, "
          f"{n_warn} warning(s).")
    print(f"Summary: {Path(args.out) / 'summary.json'}")
    print(f"Report: docs/reference-report.md")
    for unit, u in summary["units"].items():
        if u.get("review_page"):
            print(f"{unit} A/B review page: {u['review_page']}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
