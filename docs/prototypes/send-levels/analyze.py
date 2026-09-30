#!/usr/bin/env python3
"""Send-level calibration study, step 2: measure the grid from render_grid.py.

Per stimulus x input level x ATTITUDE x DRIVE:
  splash_db   SPLASH 0.8 vs 0: 2-8 kHz energy of the wet (L+R), 20-400 ms after
              each event, summed over the events, in dB (as in the backlog's
              "DRIVE dampens SPLASH"). For 02_hits also per hit (each hit's own
              peak level), and for the two loudest hits (snare + rim at the
              stated peak) as `splash_loud_db`.
  drive_null  test_drive's DRIVE audibility measure, SPLASH 0: energy of
              (out at DRIVE d - out at DRIVE 0) over energy of out at DRIVE 0,
              both channels, whole render, dB; `_matched` = the DRIVE d render
              scaled to best match DRIVE 0 first (character, not loudness).
              -20 dB = "clearly coloured" (test_drive's DRIVEN/KICKED noon bar).
  out_db      output rms (both channels, whole render, SPLASH 0), dBFS, and
              out_minus_in_db = output rms - input rms.
Then: the DRIVE that restores each effect at -18 / -24 dBFS to its -6 dBFS
value (linear interpolation on the DRIVE grid; null if never reached).

Usage: python3 docs/prototypes/send-levels/analyze.py [--dir renders/send_levels]
       [--json docs/prototypes/send-levels/results.json]
"""
import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt

STIM = ["02_hits", "04_skank", "06_noise_bursts"]
LEVELS = [-6, -12, -18, -24]
ATTS = ["CLEAN", "DRIVEN", "KICKED"]
DRIVES = [0.0, 0.25, 0.5, 0.75, 1.0]
SR = 48000


def db(x):
    return 10 * np.log10(max(x, 1e-30))


def onsets(x, rel=0.03, gap=0.3):
    thr = rel * np.max(np.abs(x))
    idx = np.flatnonzero(np.abs(x) > thr)
    out, last = [], -10**9
    for i in idx:
        if i - last > gap * SR:
            out.append(int(i))
        last = i
    peaks = [20 * np.log10(np.max(np.abs(x[o:o + int(0.05 * SR)]))) for o in out]
    return out, peaks


def cross_at(xs, ys, target):
    """First x where y reaches target (y rising or falling toward it), linear interp."""
    ys = np.asarray(ys, float)
    rising = ys[-1] >= ys[0]
    for i in range(len(xs)):
        if (ys[i] >= target) if rising else (ys[i] <= target):
            if i == 0:
                return xs[0]
            t = (target - ys[i - 1]) / (ys[i] - ys[i - 1])
            return round(float(xs[i - 1] + t * (xs[i] - xs[i - 1])), 3)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default="renders/send_levels")
    ap.add_argument("--json", default="docs/prototypes/send-levels/results.json")
    a = ap.parse_args()
    d = Path(a.dir)
    sos = butter(4, [2000, 8000], btype="band", fs=SR, output="sos")

    def load(s, lv, att, dr, sp):
        y, sr = sf.read(d / "wav" / f"{s}_{-lv:02d}_{att}_d{dr:.2f}_s{sp:.1f}.wav", dtype="float64")
        assert sr == SR
        return y if y.ndim == 2 else np.stack([y, y], 1)

    res = {"settings": {"springs": "2", "decay": 0.7, "tone": 0.5, "tension": 0.5, "wobble": 0.0, "mix": 1.0,
                        "splash": [0.0, 0.8], "drives": DRIVES, "levels_dbfs_peak": LEVELS},
           "grid": {}}
    for s in STIM:
        res["grid"][s] = {}
        for lv in LEVELS:
            x, _ = sf.read(d / "stim" / f"{s}_{-lv:02d}.wav", dtype="float64")
            on, pk = onsets(x)
            in_db = db(np.mean(x**2))
            loud = [i for i, p in enumerate(pk) if p > lv - 1.0]
            res["grid"][s][str(lv)] = {"events": len(on), "event_peaks_dbfs": [round(p, 1) for p in pk],
                                       "in_rms_db": round(in_db, 2)}
            for att in ATTS:
                rows = []
                y00 = load(s, lv, att, 0.0, 0.0)
                for dr in DRIVES:
                    y0 = load(s, lv, att, dr, 0.0)
                    y8 = load(s, lv, att, dr, 0.8)
                    b0 = sosfiltfilt(sos, y0.sum(1))
                    b8 = sosfiltfilt(sos, y8.sum(1))
                    e0 = [np.sum(b0[o + int(.02 * SR):o + int(.4 * SR)]**2) for o in on]
                    e8 = [np.sum(b8[o + int(.02 * SR):o + int(.4 * SR)]**2) for o in on]
                    pr = np.sum(y00**2)
                    dn = db(np.sum((y0 - y00)**2) / pr)
                    g = np.sum(y00 * y0) / max(np.sum(y0**2), 1e-30)
                    dm = db(np.sum((g * y0 - y00)**2) / pr)
                    out_db = db(np.mean(y0**2) / 2 * 2)  # mean over both channels
                    row = {"drive": dr, "splash_db": round(db(sum(e8) / sum(e0)), 2),
                           "drive_null_db": round(dn, 1), "drive_null_matched_db": round(dm, 1),
                           "out_rms_db": round(out_db, 2), "out_minus_in_db": round(out_db - in_db, 2)}
                    if s == "02_hits":
                        row["splash_loud_db"] = round(db(sum(e8[i] for i in loud) / sum(e0[i] for i in loud)), 2)
                        row["splash_per_hit_db"] = [round(db(p / q), 2) for p, q in zip(e8, e0)]
                    rows.append(row)
                res["grid"][s][str(lv)][att] = rows

    # DRIVE needed at -18 / -24 dBFS to match the -6 dBFS reference.
    need = {}
    for s in STIM:
        need[s] = {}
        for att in ATTS:
            g6 = res["grid"][s]["-6"][att]
            key = "splash_loud_db" if s == "02_hits" else "splash_db"
            ref_splash = g6[0][key]            # -6 dBFS, DRIVE 0
            ref_drive = g6[2]["drive_null_matched_db"]  # -6 dBFS, DRIVE noon
            need[s][att] = {"ref_splash_db_at_-6_drive0": ref_splash, "ref_drive_null_matched_at_-6_drive0.5": ref_drive}
            for lv in ["-18", "-24"]:
                g = res["grid"][s][lv][att]
                sp = [r[key] for r in g]
                nm = [r["drive_null_matched_db"] for r in g]
                need[s][att][lv] = {
                    "splash_db_by_drive": sp,
                    "drive_for_splash": cross_at(DRIVES, sp, ref_splash) if max(sp) >= ref_splash else None,
                    "drive_null_matched_by_drive": nm,
                    "drive_for_noon_drive_sound": cross_at(DRIVES, nm, ref_drive) if max(nm) >= ref_drive else None,
                }
    res["drive_needed"] = need

    # DRIVE's pre-gain into DriveIn's saturators (DriveVoicing.h driveDbMin/Max,
    # driveCurve p = 1.3). DriveIn's makeup divides it back out (x trim / preGain,
    # times the measured squash, clamped to [1, preGain]), so the level reaching
    # the Splash detector and the Tank does not change with DRIVE.
    vox = {"CLEAN": (-6.0, 12.0), "DRIVEN": (-6.0, 16.0), "KICKED": (-11.0, 20.0)}
    res["drive_pregain_db"] = {att: {str(dr): round(lo + (hi - lo) * dr**1.3, 2) for dr in DRIVES}
                               for att, (lo, hi) in vox.items()}
    # Hit detector model at SPLASH 0.8 for an isolated snare (SplashVoicing.h):
    # T = 0.30 * (0.015/0.30)^0.8; d ~ 0.37 x the snare's peak (0.15-0.22 at
    # -6 dBFS); Hit = r^3 / (1 + r^3), r = d / T (P ~ 0 after silence).
    T = 0.30 * (0.015 / 0.30)**0.8
    res["hit_model_splash0.8"] = {"T": round(T, 4), "hit_by_snare_peak_dbfs": {
        str(p): round((0.37 * 10**(p / 20) / T)**3 / (1 + (0.37 * 10**(p / 20) / T)**3), 2)
        for p in [-6, -12, -18, -21, -24, -27, -30, -36]}}
    Path(a.json).write_text(json.dumps(res, indent=1))

    # Compact console summary.
    for s in STIM:
        key = "splash_loud_db" if s == "02_hits" else "splash_db"
        print(f"\n== {s}: {key} | drive_null_matched | out_minus_in (DRIVE 0/.25/.5/.75/1)")
        for att in ATTS:
            for lv in LEVELS:
                g = res["grid"][s][str(lv)][att]
                print(f"{att:6s} {lv:4d}  " + " ".join(f"{r[key]:5.1f}" for r in g) + "  |  "
                      + " ".join(f"{r['drive_null_matched_db']:6.1f}" for r in g) + "  |  "
                      + " ".join(f"{r['out_minus_in_db']:5.1f}" for r in g))
    print("\nper-hit splash, 02_hits DRIVE 0 (hit peak dBFS: dB):")
    for att in ATTS:
        pts = []
        for lv in LEVELS:
            e = res["grid"]["02_hits"][str(lv)]
            for p, v in zip(e["event_peaks_dbfs"], e[att][0]["splash_per_hit_db"]):
                pts.append((round(p), v))
        pts.sort(reverse=True)
        print(att, pts)


if __name__ == "__main__":
    main()
