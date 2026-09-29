#!/usr/bin/env python3
"""Build a self-contained listening page for a folder of Resilio Versio renders.

Usage:
    python3 tools/review/make_review.py renders/<name> [--reference DIR ...] [--title T]
        [--columns KEY] [--variants KEY] [--rows KEY[,KEY]] [--out FILE]
        [--no-level-match] [--no-spectrograms] [--classic]

The default page is laid out like the SPLASH voicings page the owner approved
(renders/splash_voicings, Sep 2026):
  - panels in columns per group (usually ATTITUDE), colour-coded headers;
    rows are the material (hits, skank...) or whatever else changes;
  - inside each panel the versions A, B, C... play in sync: choosing one switches
    it at the same playback position, and "Switch every panel to" does it
    for every panel at once;
  - settings as readable chips (DECAY 0.6, TENSION noon), not raw JSON;
  - "My pick" per panel, notes per column plus overall notes, and
    "Copy results for Claude" (plain text) / "Download results";
  - level-matched playback (on by default: every version plays at the
    loudness of the quietest one in its panel), blind mode, loop, keys,
    a small spectrogram of the selected version, flags (clips, ringing...);
  - with --reference, a "Compare with" menu adds a reference recording to
    every panel as version R.
Picks and notes persist in the browser (localStorage) where it allows.

What it reads from <dir> (first that applies):
  1. <dir>/manifest.json (rv_render --sweep, docs/m1-contracts.md) + sidecars.
  2. manifest.json files in subfolders, merged on one page: before/ + after/,
     A/ B/ C/, or one sweep per ATTITUDE (renders/tension/decay_clean ...).
  3. Plain WAVs (A/B prototypes): 02_hits_A_today.wav, clean/02_hits_B_x.wav,
     02_hits_tension0_mix035_before.wav... A README.txt in the folder becomes
     the page's intro, and lines like "A_today   what it is" describe versions.

Layout is chosen automatically: ATTITUDE (else SPRINGS) as columns, the knob
with the most positions (or the lettered versions) as A/B/C, the material as
rows. Override with the names printed after "Layout:" (param keys like decay,
tension, attitude, or stimulus / material / folder / sweep / value / variant;
"none" for no columns):
    make_review.py renders/m1_click_grid --columns decay --variants tension

Output: <dir>/index.html by default (overwritten); --out NAME writes <dir>/NAME,
--out path/to/file.html writes there (audio paths stay relative to it).
Everything is inlined (CSS/JS/data), WAVs are referenced by relative path, so
the page works opened from Finder over file://.

--classic writes the older metrics page (per-render cards, full spectrograms,
filters, A/B strip) from template.html; it needs <dir>/manifest.json.

stdlib-only: no third-party dependencies. Tests: python3 tools/review/test_make_review.py
"""

import argparse
import json
import os
import sys
from pathlib import Path

TEMPLATE_NAME = "template.html"

# Params known from core/params/ParamSpec.h, used to order switch values
# sensibly (e.g. ATTITUDE: CLEAN/DRIVEN/KICKED) instead of alphabetically.
KNOWN_SWITCH_ORDER = {
    "springs": ["1", "2", "3"],
    "attitude": ["CLEAN", "DRIVEN", "KICKED"],
}


def die(msg):
    sys.stderr.write("make_review.py: error: {}\n".format(msg))
    sys.exit(1)


def load_json(path, what):
    if not path.exists():
        die("{} not found: {}".format(what, path))
    try:
        with open(path, "r") as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        die("{} is not valid JSON ({}): {}".format(what, e, path))
    except OSError as e:
        die("could not read {} ({}): {}".format(what, e, path))


def require_keys(obj, keys, what, path):
    missing = [k for k in keys if k not in obj]
    if missing:
        die("{} at {} is missing required key(s): {}".format(what, path, ", ".join(missing)))


def load_sidecar(path):
    sidecar = load_json(path, "sidecar")
    require_keys(sidecar, ["wav", "sample_rate", "duration_s", "metrics", "spectrogram"], "sidecar", path)
    require_keys(sidecar["spectrogram"], [
        "width", "height", "t0_s", "t1_s", "f_min_hz", "f_max_hz",
        "freq_scale", "db_min", "db_max", "data_b64",
    ], "spectrogram in sidecar", path)
    return sidecar


IGNORE_FLAGS = frozenset()  # set from manifest ignore_flags in main()


def is_stereo_flagged(metrics, ignore=frozenset()):
    """True if any of the M4/Stream E stereo metrics fail their threshold
    (docs/m4-contracts.md Stream E). `null`/absent (mono files, or old
    sidecars written before these metrics existed) never flags."""
    if not metrics:
        return False
    metrics = {k: v for k, v in metrics.items() if k not in ignore}
    sc = metrics.get("stereo_correlation")
    if isinstance(sc, (int, float)) and sc > 0.5:
        return True
    ml = metrics.get("mono_loss_db")
    if isinstance(ml, (int, float)) and ml < -1.5:
        return True
    mn = metrics.get("mono_notch_db")
    if isinstance(mn, (int, float)) and mn < -6:
        return True
    ms = metrics.get("max_step_db_100ms")
    if isinstance(ms, (int, float)) and ms > 3:
        return True
    return False


def is_flagged(metrics, ignore=frozenset()):
    if not metrics:
        return False
    metrics = {k: v for k, v in metrics.items() if k not in ignore}
    if (metrics.get("nan_inf_count") or 0) > 0:
        return True
    if (metrics.get("clip_count") or 0) > 0:
        return True
    if (metrics.get("click_count") or 0) > 0:
        return True
    if metrics.get("steady_tone") is True:
        return True
    # Ringing: the calibrated M6 metric (docs/m6-metric-calibration.md).
    # resonance_peak_db is informational only: it "fails" real tanks.
    rd = metrics.get("ringing_db")
    if isinstance(rd, (int, float)) and rd >= 15:
        return True
    if is_stereo_flagged(metrics, ignore):
        return True
    return False


def sort_values(key, values):
    """Sort raw (non-stringified) param values. Keeping the original JSON
    type matters: the page compares via JS String(value) on both the axis
    list and each render's params, which only agree if both sides came
    from the same JSON-typed value (e.g. JSON number 0.0 -> JS "0", not
    Python's str(0.0) == "0.0")."""
    order = KNOWN_SWITCH_ORDER.get(key)
    if order:
        return sorted(values, key=lambda v: order.index(str(v)) if str(v) in order else len(order))
    try:
        return sorted(values, key=lambda v: float(v))
    except (TypeError, ValueError):
        return sorted(values, key=lambda v: str(v))


def raw_values(key, renders):
    """Unique raw param values for `key` across renders, deduped by their
    canonical JSON encoding (so 0.5 and "0.5" aren't merged, but repeated
    0.5s are)."""
    seen = {}
    for r in renders:
        if key in r["params"]:
            v = r["params"][key]
            seen[json.dumps(v)] = v
    return list(seen.values())


def build_renders(out_dir, manifest, page_dir=None):
    renders = []
    for i, entry in enumerate(manifest.get("renders", [])):
        require_keys(entry, ["wav", "sidecar"], "manifest render entry #{}".format(i), out_dir / "manifest.json")
        wav_path = out_dir / entry["wav"]
        sidecar_path = out_dir / entry["sidecar"]
        if not wav_path.exists():
            die("render WAV referenced by manifest.json not found: {}".format(wav_path))
        sidecar = load_sidecar(sidecar_path)
        params = sidecar.get("params") or entry.get("params") or {}
        metrics = sidecar.get("metrics") or {}
        render = {
            "id": "render-{}".format(i),
            "label": entry["wav"],
            "wavRel": os.path.relpath(wav_path, page_dir or out_dir),
            "params": params,
            "metrics": metrics,
            "spectrogram": sidecar.get("spectrogram"),
            "flagged": is_flagged(metrics, IGNORE_FLAGS),
            "stereoFlagged": is_stereo_flagged(metrics, IGNORE_FLAGS),
        }
        renders.append(render)
    return renders


def build_references(out_dir, reference_dirs, page_dir=None):
    references = []
    for ref_dir in reference_dirs:
        ref_dir = Path(ref_dir)
        if not ref_dir.is_dir():
            die("--reference directory not found: {}".format(ref_dir))
        wavs = sorted(ref_dir.glob("*.wav"))
        if not wavs:
            sys.stderr.write("make_review.py: warning: no .wav files found in reference dir {}\n".format(ref_dir))
        for wav_path in wavs:
            sidecar_path = wav_path.with_suffix(".json")
            if not sidecar_path.exists():
                sys.stderr.write(
                    "make_review.py: warning: skipping {} (no matching sidecar {})\n".format(
                        wav_path.name, sidecar_path.name
                    )
                )
                continue
            sidecar = load_sidecar(sidecar_path)
            metrics = sidecar.get("metrics") or {}
            references.append({
                "id": "reference-{}".format(len(references)),
                "label": wav_path.stem,
                "wavRel": os.path.relpath(wav_path, page_dir or out_dir),
                "params": sidecar.get("params") or {},
                "metrics": metrics,
                "spectrogram": sidecar.get("spectrogram"),
                "flagged": is_flagged(metrics, IGNORE_FLAGS),
                "stereoFlagged": is_stereo_flagged(metrics, IGNORE_FLAGS),
            })
    return references


def detect_grid(renders):
    """Return grid info if exactly 2 param keys vary across renders, else None."""
    if not renders:
        return None
    keys = set()
    for r in renders:
        keys.update(r["params"].keys())
    varying = []
    for k in sorted(keys):
        values = set(str(r["params"].get(k)) for r in renders if k in r["params"])
        if len(values) > 1:
            varying.append(k)
    if len(varying) != 2:
        return None
    row_key, col_key = varying[0], varying[1]
    row_values = sort_values(row_key, raw_values(row_key, renders))
    col_values = sort_values(col_key, raw_values(col_key, renders))
    return {
        "rowAxis": {"key": row_key, "values": row_values},
        "colAxis": {"key": col_key, "values": col_values},
    }


def build_filter_params(renders):
    keys = set()
    for r in renders:
        keys.update(r["params"].keys())
    filters = []
    for k in sorted(keys):
        values = raw_values(k, renders)
        if len(values) < 2:
            continue
        filters.append({"key": k, "values": sort_values(k, values)})
    return filters


def resolve_out_path(out_dir, out):
    if not out:
        return out_dir / "index.html"
    p = Path(out)
    if len(p.parts) == 1:
        return out_dir / p
    return p


def write_classic(out_dir, out_path, reference_dirs, title):
    """The older metrics page (template.html): one card per render, filters, A/B strip."""
    manifest_path = out_dir / "manifest.json"
    manifest = load_json(manifest_path, "manifest.json")
    global IGNORE_FLAGS
    IGNORE_FLAGS = frozenset(manifest.get("ignore_flags") or [])
    require_keys(manifest, ["name", "created", "input", "renders"], "manifest.json", manifest_path)

    page_dir = out_path.parent
    renders = build_renders(out_dir, manifest, page_dir)
    references = build_references(out_dir, reference_dirs, page_dir)

    grid = detect_grid(renders)
    filter_params = build_filter_params(renders)
    renders_by_id = {r["id"]: r for r in renders}

    title = title or manifest["name"]

    data = {
        "title": title,
        "sweepName": manifest["name"],
        "input": manifest["input"],
        "created": manifest["created"],
        "renders": renders,
        "rendersById": renders_by_id,
        "references": references,
        "grid": grid,
        "filterParams": filter_params,
        "ignoreFlags": sorted(IGNORE_FLAGS),
    }

    template_path = Path(__file__).resolve().parent / TEMPLATE_NAME
    if not template_path.exists():
        die("template file missing (expected next to make_review.py): {}".format(template_path))
    template = template_path.read_text(encoding="utf-8")

    data_json = json.dumps(data)
    # Guard against a literal "</script>" inside embedded JSON strings breaking
    # the surrounding <script> tag when the page is parsed as HTML.
    data_json = data_json.replace("</", "<\\/")

    html = template.replace("__RV_TITLE__", title.replace("&", "&amp;").replace("<", "&lt;"))
    html = html.replace("__RV_DATA_JSON__", data_json)

    out_path.write_text(html, encoding="utf-8")

    flagged_count = sum(1 for r in renders if r["flagged"])
    print("Wrote {} ({} renders, {} references, {} flagged)".format(
        out_path, len(renders), len(references), flagged_count
    ))
    if grid:
        print("Grid view: rows={} ({}), cols={} ({})".format(
            grid["rowAxis"]["key"], len(grid["rowAxis"]["values"]),
            grid["colAxis"]["key"], len(grid["colAxis"]["values"]),
        ))
    else:
        print("List view (0, 1, or >2 varying params detected)")


def write_listen(out_dir, out_path, args):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import listen
    try:
        items, source, name = listen.collect(out_dir, with_spec=not args.no_spectrograms)
        refs = listen.load_references(out_path, args.reference_dirs, not args.no_spectrograms)
        rows = [r.strip() for r in args.rows.split(",") if r.strip()] if args.rows is not None else None
        data = listen.build_page_data(out_dir, out_path, items, refs, rows=rows, columns=args.columns,
                                      variants=args.variants, title=args.title or (name and name.replace("_", " ")),
                                      level_match=not args.no_level_match)
    except listen.LayoutError as e:
        die(str(e))
    out_path.write_text(listen.render_html(data), encoding="utf-8")
    print("Wrote {} (from {}: {})".format(out_path, {"manifest": "manifest.json", "manifests": "manifests in subfolders",
                                                    "wavs": "WAV file names"}[source], listen.describe(data)))
    ax = data["axes"]
    print("Layout: --rows {} --columns {} --variants {}".format(ax["rows"] or "none", ax["columns"] or "none",
                                                               ax["variants"] or "none"))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", type=Path, help="Render folder (manifest.json + sidecars, subfolders of them, or plain WAVs)")
    parser.add_argument("--reference", dest="reference_dirs", action="append", default=[],
                        help="Directory of reference WAVs (sidecar JSONs optional) to compare against (repeatable)")
    parser.add_argument("--title", default=None, help="Page title (default: folder name; classic: sweep name)")
    parser.add_argument("--columns", default=None, help="What the colour-coded columns are (default: attitude, else springs, else automatic; 'none' for one column)")
    parser.add_argument("--variants", default=None, help="What A/B/C inside a panel are (default: lettered versions, else the knob with the most positions)")
    parser.add_argument("--rows", default=None, help="Comma-separated: what the rows are (default: material + whatever is left over; '' for none)")
    parser.add_argument("--out", default=None, help="Output file name in <dir> or a path (default: <dir>/index.html)")
    parser.add_argument("--no-level-match", action="store_true", help="Start with level-matching off (the page still has the toggle)")
    parser.add_argument("--no-spectrograms", action="store_true", help="Leave out the small spectrograms (smaller page)")
    parser.add_argument("--classic", action="store_true", help="Write the older metrics page (template.html) instead")
    args = parser.parse_args()

    out_dir = args.out_dir
    if not out_dir.is_dir():
        die("out-dir not found or not a directory: {}".format(out_dir))
    out_path = resolve_out_path(out_dir, args.out)
    if not out_path.parent.is_dir():
        die("output folder not found: {}".format(out_path.parent))

    if args.classic:
        write_classic(out_dir, out_path, args.reference_dirs, args.title)
    else:
        write_listen(out_dir, out_path, args)


if __name__ == "__main__":
    main()
