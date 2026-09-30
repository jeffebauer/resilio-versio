#!/usr/bin/env python3
"""Tests for make_review.py (the listening page and the --classic page).

    python3 tools/review/test_make_review.py

Builds tiny render folders in a temp dir (short WAVs + sidecars), runs the
CLI on them and checks the layout it chose and that every audio path on the
page resolves from where the page was written. stdlib-only.
"""

import base64
import json
import math
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "make_review.py"
sys.path.insert(0, str(HERE))
import listen  # noqa: E402


def write_wav(path, level=0.1, seconds=0.05, sr=8000):
    path.parent.mkdir(parents=True, exist_ok=True)
    n = int(seconds * sr)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(b"".join(struct.pack("<h", int(level * 32767 * math.sin(i / 3.0))) for i in range(n)))


def sidecar(wav_name, params, rms=-20.0, metrics=None):
    m = {"rms_dbfs": rms, "clip_count": 0}
    m.update(metrics or {})
    return {"wav": wav_name, "sample_rate": 8000, "duration_s": 0.05, "params": params, "metrics": m,
            "spectrogram": {"width": 8, "height": 4, "t0_s": 0, "t1_s": 0.05, "f_min_hz": 40, "f_max_hz": 4000,
                            "freq_scale": "log", "db_min": -100, "db_max": 0,
                            "data_b64": base64.b64encode(bytes(range(32))).decode()}}


def make_sweep(folder, name, grid, base=None, stim="test_audio/stimulus/02_hits.wav", ignore=None):
    """A folder like `rv_render --sweep` writes. grid: {key: [values]} (1 or 2 keys)."""
    folder.mkdir(parents=True, exist_ok=True)
    keys = list(grid)
    combos = [{}]
    for k in keys:
        combos = [dict(c, **{k: v}) for c in combos for v in grid[k]]
    renders = []
    for c in combos:
        stem = name + "__" + "_".join("{}{:.2f}".format(k, v if isinstance(v, float) else 0.0) for k, v in c.items())
        params = dict({"decay": 0.5, "tone": 0.5, "tension": 0.5, "springs": "2", "attitude": "CLEAN"}, **(base or {}))
        params.update(c)
        write_wav(folder / (stem + ".wav"), level=0.05 + 0.05 * len(renders) % 0.3)
        (folder / (stem + ".json")).write_text(json.dumps(sidecar(stem + ".wav", params, rms=-30 + len(renders))))
        renders.append({"wav": stem + ".wav", "sidecar": stem + ".json", "params": params})
    man = {"name": name, "created": "2026-09-30T00:00:00Z", "input": stim, "renders": renders}
    if ignore:
        man["ignore_flags"] = ignore
    (folder / "manifest.json").write_text(json.dumps(man))


def run(*args, ok=True):
    r = subprocess.run([sys.executable, str(SCRIPT)] + [str(a) for a in args], capture_output=True, text=True)
    if ok and r.returncode != 0:
        raise AssertionError("make_review.py failed:\n" + r.stdout + r.stderr)
    return r


def page_data(html_path):
    html = Path(html_path).read_text(encoding="utf-8")
    m = re.search(r'<script id="rv-data" type="application/json">(.*?)</script>', html, re.S)
    assert m, "no embedded data"
    return json.loads(m.group(1))


class ListenPageTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def assertAudioResolves(self, page):
        data = page_data(page)
        for it in list(data["items"].values()) + data["references"]:
            self.assertTrue((Path(page).parent / it["wav"]).exists(), it["wav"])
        return data

    def variant_names(self, data, panel="r0|c0"):
        return [v["n"] for v in data["panels"][panel]["variants"]]

    def test_sweep_attitude_by_splash(self):
        d = self.root / "m7_splash"
        make_sweep(d, "m7_splash", {"attitude": [0.0, 0.5, 1.0], "splash": [0.0, 0.5, 1.0]})
        run(d)
        data = self.assertAudioResolves(d / "index.html")
        self.assertEqual([c["name"] for c in data["cols"]], ["CLEAN", "DRIVEN", "KICKED"])
        self.assertEqual([c["color"] for c in data["cols"]], ["--clean", "--driven", "--kicked"])
        self.assertEqual(self.variant_names(data), ["SPLASH 0", "SPLASH 0.5", "SPLASH 1"])
        self.assertEqual(data["legend"][1], {"k": "B", "n": "SPLASH 0.5", "d": "noon"})
        self.assertEqual(data["title"], "M7 splash")
        self.assertEqual(data["rows"][0]["marks"][0], ["Loud snare", 1])  # 02_hits jump buttons
        fixed = {c["k"]: c for c in data["fixed"]}
        self.assertEqual(fixed["DECAY"], {"k": "DECAY", "v": "0.5", "c": "noon"})
        self.assertNotIn("SPLASH", fixed)
        self.assertFalse(data["levelMatch"])  # knob sweep: loudness is part of the knob

    def test_level_match_defaults_and_flags(self):
        sweep = self.root / "decay_sweep"
        make_sweep(sweep, "decay_sweep", {"decay": [0.0, 0.5, 1.0]})
        proto = self.root / "proto"
        for v in ("A_today", "B_new"):
            write_wav(proto / "02_hits_{}.wav".format(v))
        run(sweep)
        run(proto)
        self.assertFalse(page_data(sweep / "index.html")["levelMatch"])
        self.assertTrue(page_data(proto / "index.html")["levelMatch"])
        run(sweep, "--level-match")
        run(proto, "--no-level-match")
        self.assertTrue(page_data(sweep / "index.html")["levelMatch"])
        self.assertFalse(page_data(proto / "index.html")["levelMatch"])
        self.assertNotEqual(run(sweep, "--level-match", "--no-level-match", ok=False).returncode, 0)

    def test_level_match_median_and_cap(self):
        d = self.root / "loud_quiet"
        for v, rms in (("A_loud", -5.0), ("B_mid", -20.0), ("C_quiet", -45.0)):
            name = "02_hits_{}".format(v)
            write_wav(d / (name + ".wav"))
            (d / (name + ".json")).write_text(json.dumps(sidecar(name + ".wav", {}, rms=rms)))
        run(d)
        data = page_data(d / "index.html")
        panel = data["panels"]["r0|c0"]
        self.assertEqual(panel["medianRms"], -20.0)
        # -15 and +25 dB to reach the median, capped at 12 either way.
        self.assertEqual([v["lm"] for v in panel["variants"]], [-12.0, 0.0, 12.0])
        self.assertEqual(data["levelMatchCapDb"], 12.0)

    def test_overrides_and_boing_alias(self):
        d = self.root / "grid"
        make_sweep(d, "grid", {"decay": [0.0, 0.5, 1.0], "boing": [0.0, 1.0]})
        r = run(d, "--columns", "decay", "--variants", "boing", "--out", "v2.html", "--no-level-match")
        self.assertIn("--columns decay --variants tension", r.stdout)
        data = self.assertAudioResolves(d / "v2.html")
        self.assertFalse((d / "index.html").exists())
        self.assertEqual(len(data["cols"]), 3)
        self.assertEqual(self.variant_names(data), ["TENSION 0", "TENSION 1"])
        self.assertFalse(data["levelMatch"])
        bad = run(d, "--columns", "wobble", ok=False)
        self.assertNotEqual(bad.returncode, 0)
        self.assertIn("choose from", bad.stderr)

    def test_prototype_wavs_with_readme(self):
        d = self.root / "splash_voicings"
        for att in ("clean", "driven", "kicked"):
            for stim in ("02_hits", "04_skank"):
                for v in ("A_today", "B_drive_led", "C_bright_led"):
                    write_wav(d / att / "{}_{}.wav".format(stim, v))
        (d / "README.txt").write_text("SPLASH voicings. Same hits in each ATTITUDE.\n\n"
                                      "A_today       a burst of noise (reference)\n"
                                      "B_drive_led   hits push the transducer harder\n")
        run(d)
        data = self.assertAudioResolves(d / "index.html")
        self.assertEqual([r["name"] for r in data["rows"]], ["Hits", "Skank"])
        self.assertEqual([c["name"] for c in data["cols"]], ["CLEAN", "DRIVEN", "KICKED"])
        self.assertEqual([(v["k"], v["n"]) for v in data["legend"]], [("A", "today"), ("B", "drive led"), ("C", "bright led")])
        self.assertEqual(data["legend"][0]["d"], "a burst of noise (reference)")
        self.assertEqual(data["lede"], "SPLASH voicings. Same hits in each ATTITUDE.")
        self.assertIsNotNone(next(iter(data["items"].values()))["rms"])  # measured from the WAV itself

    def test_two_character_version_labels(self):
        # SPLASH round 4 (renders/splash_round4): T1 / T2 / TC next to A and E.
        d = self.root / "splash_round4"
        for att in ("driven", "kicked"):
            for stim in ("02_hits", "04_skank"):
                for v in ("A_burst", "E_drive_alone", "T1_hits_bite_gentle", "TC_both"):
                    write_wav(d / att / "{}_{}.wav".format(stim, v))
        (d / "README.txt").write_text("Round 4.\n\nT1_hits_bite_gentle   T1 hits bite harder, gentle\n")
        run(d)
        data = self.assertAudioResolves(d / "index.html")
        self.assertEqual([r["name"] for r in data["rows"]], ["Hits", "Skank"])
        self.assertEqual([(v["k"], v["n"]) for v in data["legend"]],
                         [("A", "burst"), ("E", "drive alone"), ("T1", "hits bite gentle"), ("TC", "both")])
        self.assertEqual(data["legend"][2]["d"], "T1 hits bite harder, gentle")

    def test_before_after_suffix(self):
        d = self.root / "predelay_ab"
        for stim in ("02_hits", "04_skank"):
            for v in ("after", "before"):
                write_wav(d / "{}_tension0_mix035_{}.wav".format(stim, v))
        run(d)
        data = self.assertAudioResolves(d / "index.html")
        self.assertEqual(self.variant_names(data), ["before", "after"])
        self.assertEqual(data["rows"][0]["name"], "Hits · tension0 mix035")

    def test_merged_before_after_folders(self):
        for v in ("after", "before"):
            make_sweep(self.root / "tone_ab" / v, "tone", {"tone": [0.5, 1.0]})
        run(self.root / "tone_ab")
        data = self.assertAudioResolves(self.root / "tone_ab" / "index.html")
        self.assertEqual(data["axes"]["variants"], "folder1")
        self.assertEqual(self.variant_names(data), ["before", "after"])
        self.assertEqual([c["name"] for c in data["cols"]], ["TONE 0.5", "TONE 1"])

    def test_merged_sweeps_per_attitude(self):
        for knob in ("decay", "tension"):
            for i, att in enumerate(("clean", "kicked")):
                make_sweep(self.root / "sweet" / "{}_{}".format(knob, att), "m8_sweet_{}_{}".format(knob, att),
                           {knob: [0.0, 0.5, 1.0]}, base={"attitude": ["CLEAN", "KICKED"][i]})
        run(self.root / "sweet")
        data = self.assertAudioResolves(self.root / "sweet" / "index.html")
        self.assertEqual([r["name"] for r in data["rows"]], ["DECAY sweep", "TENSION sweep"])
        self.assertEqual([c["name"] for c in data["cols"]], ["CLEAN", "KICKED"])
        self.assertEqual(self.variant_names(data, "r1|c0"), ["TENSION 0", "TENSION 0.5", "TENSION 1"])
        self.assertEqual(data["panels"]["r0|c0"]["chips"], [{"k": "TENSION", "v": "0.5", "c": "noon"}])

    def test_out_elsewhere_and_references(self):
        d = self.root / "ab"
        make_sweep(d, "ab", {"attitude": [0.0, 0.5]})
        ref = self.root / "refs"
        write_wav(ref / "wellspring_B.wav")
        out = self.root / "pages" / "ab.html"
        out.parent.mkdir()
        run(d, "--reference", ref, "--out", out)
        data = self.assertAudioResolves(out)
        self.assertEqual([r["name"] for r in data["references"]], ["wellspring_B"])
        self.assertEqual(data["references"][0]["flags"], [])

    def test_flags_respect_ignore(self):
        d = self.root / "flags"
        make_sweep(d, "flags", {"decay": [0.0, 1.0]}, ignore=["max_step_db_100ms"])
        sc = json.loads((d / "flags__decay1.00.json").read_text())
        sc["metrics"].update({"clip_count": 3, "max_step_db_100ms": 9})
        (d / "flags__decay1.00.json").write_text(json.dumps(sc))
        run(d)
        flags = [it["flags"] for it in page_data(d / "index.html")["items"].values()]
        self.assertIn(["clips ×3"], flags)

    def test_classic_page_still_builds(self):
        d = self.root / "classic"
        make_sweep(d, "classic", {"decay": [0.0, 1.0], "tension": [0.0, 1.0]})
        r = run(d, "--classic")
        self.assertIn("Grid view", r.stdout)
        self.assertIn("__RV", (HERE / "template.html").read_text())
        self.assertNotIn("__RV_DATA_JSON__", (d / "index.html").read_text())


class HelpersTest(unittest.TestCase):
    def test_clock(self):
        self.assertEqual(listen.clock(0.5), "noon")
        self.assertEqual(listen.clock(0.0), "fully left")
        self.assertEqual(listen.clock(1.0), "fully right")
        self.assertEqual(listen.clock(0.6000000238), "1 o'clock")
        self.assertEqual(listen.clock(0.25), "9:30")

    def test_level_offsets(self):
        self.assertEqual(listen.level_offsets([-10.0, -20.0, -50.0]), [-10.0, 0.0, 12.0])
        self.assertEqual(listen.level_offsets([-10.0, -14.0]), [-2.0, 2.0])  # even count: mean of the middle two
        self.assertEqual(listen.level_offsets([None, -10.0]), [None, 0.0])
        self.assertEqual(listen.level_offsets([None]), [None])

    def test_canon_params(self):
        self.assertEqual(listen.canon_params({"boing": 0.5, "attitude": 1.0, "springs": 0.5}),
                         {"tension": 0.5, "attitude": "KICKED", "springs": "2"})
        self.assertEqual(listen.fmt_num(0.703125), "0.7")


if __name__ == "__main__":
    unittest.main()
