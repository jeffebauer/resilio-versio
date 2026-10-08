#!/usr/bin/env python3
"""Demos round 2: fetch the owner's picked sample sets (8 Oct 2026) into test_audio/demo_sources/cc0/.
Needs the GitHub CLI (gh) for directory listings. Samples are never committed.
Usage (repo root): python3 docs/minisite/tools/demos/v2/fetch_samples.py"""
import json, re, subprocess, urllib.parse, urllib.request
from pathlib import Path

OUT = Path("test_audio/demo_sources/cc0")
# (local folder, repo, branch, path in repo, filename regex)
SETS = [
    ("virtuosity/tom_hi", "sfzinstruments/virtuosity_drums", "master", "Samples/mid/htom", r"vl[2-4]"),
    ("virtuosity/tom_lo", "sfzinstruments/virtuosity_drums", "master", "Samples/mid/ltom", r"vl[2-4]"),
    ("virtuosity/hh_open", "sfzinstruments/virtuosity_drums", "master", "Samples/mid/hh", r"_(half|open)_vl[2-4]"),
    ("bass_babyblue", "sfzinstruments/karoryfer.black-and-blue-basses", "main", "Samples/babyblue/reg", r"_f_rr[12]\."),
    ("guitar_emily", "sfzinstruments/karoryfer.emilyguitar", "master", "notes", r"_mf_rr[12]\."),
    ("organ_drawbar", None, None, None, None),  # already unpacked by the audition (FreePats tarball)
    ("trombone_short", "sgossner/VSCO-2-CE", "master", "Brass/OldTrombone/Short", r"_v1_1\."),
    ("clav_fm", "sgossner/VCSL", "master", "Electrophones/TX81Z/Clavisynth", r"_vl2\."),
    ("vibes_soft", "sgossner/VCSL", "master", "Idiophones/Struck Idiophones/Vibraphone/Soft Mallets", r"_v1_rr1_Main"),
    ("dantranh", "sgossner/VCSL", "master", "Chordophones/Zithers/Dan Tranh/Normal", r"_f_1\."),
    ("bongos", "sgossner/VCSL", "master", "Membranophones/Struck Membranophones/Bongos", r"_rr[12]_Mid"),
    ("cowbell_808", "tidalcycles/sounds-tr808-fischer", "main", "cb8", r"\.WAV$"),
]

def ls(repo, branch, path):
    q = urllib.parse.quote(path)
    out = subprocess.run(["gh", "api", f"repos/{repo}/contents/{q}?ref={branch}"], capture_output=True, text=True, check=True).stdout
    return json.loads(out)

for folder, repo, branch, path, pat in SETS:
    if repo is None:
        continue
    dest = OUT / folder
    dest.mkdir(parents=True, exist_ok=True)
    items = [i for i in ls(repo, branch, path) if i["type"] == "file" and re.search(pat, i["name"])]
    for i in items:
        f = dest / i["name"]
        if not f.exists() or f.stat().st_size < 1000:
            with urllib.request.urlopen(i["download_url"], timeout=120) as r:
                f.write_bytes(r.read())
    print(f"{folder}: {len(items)} files")
