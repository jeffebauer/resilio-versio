#!/usr/bin/env python3
"""Build a self-contained review page for a Resilio Versio render batch.

Usage:
    python3 tools/review/make_review.py <out-dir> [--reference DIR ...] [--title T]

Reads <out-dir>/manifest.json (written by rv_render --sweep, see
docs/m1-contracts.md, Stream B) plus each render's sidecar JSON, and any
--reference directories of WAVs with sidecar JSONs next to them (e.g.
test_audio/reference/, produced by `rv_render --analyze`). Writes
<out-dir>/index.html: a single HTML file with everything inlined (CSS/JS),
referencing the WAVs by relative path so it works opened directly from
Finder over file://.

stdlib-only: no third-party dependencies.
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
    rp = metrics.get("resonance_peak_db")
    if isinstance(rp, (int, float)) and rp > 12:
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


def build_renders(out_dir, manifest):
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
            "wavRel": os.path.relpath(wav_path, out_dir),
            "params": params,
            "metrics": metrics,
            "spectrogram": sidecar.get("spectrogram"),
            "flagged": is_flagged(metrics, IGNORE_FLAGS),
        }
        renders.append(render)
    return renders


def build_references(out_dir, reference_dirs):
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
                "wavRel": os.path.relpath(wav_path, out_dir),
                "params": sidecar.get("params") or {},
                "metrics": metrics,
                "spectrogram": sidecar.get("spectrogram"),
                "flagged": is_flagged(metrics, IGNORE_FLAGS),
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out_dir", type=Path, help="Directory containing manifest.json + sidecars; index.html is written here")
    parser.add_argument("--reference", dest="reference_dirs", action="append", default=[],
                         help="Directory of reference WAVs with sidecar JSONs next to them (repeatable)")
    parser.add_argument("--title", default=None, help="Override the page title (default: sweep name)")
    args = parser.parse_args()

    out_dir = args.out_dir
    if not out_dir.is_dir():
        die("out-dir not found or not a directory: {}".format(out_dir))

    manifest_path = out_dir / "manifest.json"
    manifest = load_json(manifest_path, "manifest.json")
    global IGNORE_FLAGS
    IGNORE_FLAGS = frozenset(manifest.get("ignore_flags") or [])
    require_keys(manifest, ["name", "created", "input", "renders"], "manifest.json", manifest_path)

    renders = build_renders(out_dir, manifest)
    references = build_references(out_dir, args.reference_dirs)

    grid = detect_grid(renders)
    filter_params = build_filter_params(renders)
    renders_by_id = {r["id"]: r for r in renders}

    title = args.title or manifest["name"]

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

    out_path = out_dir / "index.html"
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


if __name__ == "__main__":
    main()
