"""Listening page for a render folder: columns per group, synced A/B/C switching,
picks, notes and "Copy results for Claude". Used by make_review.py (the default
page); see its docstring for the CLI.

The page is laid out like the SPLASH voicings page the owner approved
(renders/splash_voicings, Sep 2026):

    rows      the material (stimulus), or whatever else is left over
    columns   colour-coded groups, usually ATTITUDE
    variants  the versions inside one panel (A, B, C...), switched in sync:
              choosing B switches every panel to B at the same playback position

Where the renders come from (first that applies):
    1. <dir>/manifest.json               one `rv_render --sweep` batch
    2. manifest.json files in subfolders  several batches merged (e.g. before/ + after/,
                                          or one sweep per ATTITUDE)
    3. plain WAVs                          names like 02_hits_A_today.wav or
                                          clean/02_hits_B_drive_led.wav (A/B prototypes)

stdlib-only.
"""

import base64
import json
import math
import os
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEMPLATE_NAME = "listen_template.html"

# core/params/ParamSpec.h order and panel labels. "boing" is the pre-ADR 0026
# name of TENSION (older renders still carry it).
PARAM_ORDER = ["decay", "tone", "tension", "splash", "drive", "wobble", "mix", "springs", "attitude"]
PARAM_NAMES = {k: k.upper() for k in PARAM_ORDER}
# Panel names since v1.0.43 (ADR 0044); the keys stay "mix" and "springs".
PARAM_NAMES.update({"mix": "BLEND", "springs": "TANK"})
PARAM_ALIASES = {"boing": "tension"}
KNOBS = frozenset(["decay", "tone", "tension", "splash", "drive", "wobble", "mix"])
SWITCH_LABELS = {"springs": ["1", "2", "ECHO"], "attitude": ["CLEAN", "TAPE", "VALVE"]}
# Labels from before v1.0.43 (older renders carry them), shown as today's.
OLD_SWITCH_LABELS = {"springs": {"3": "ECHO"}, "attitude": {"DRIVEN": "TAPE", "KICKED": "VALVE", "AMP": "VALVE"}}
ATTITUDE_WORDS = ("clean", "tape", "valve", "driven", "kicked")
ATTITUDE_WHAT = {"CLEAN": "hi-fi tank", "TAPE": "tape saturation, the dub colour", "VALVE": "cranked valve, rattle and Howl"}
BEFORE_AFTER = ["before", "old", "a", "after", "new", "b"]
BEFORE_AFTER_SET = frozenset(["before", "after", "old", "new"])

# Stimulus files (tools/make_stimulus.py). Marks are jump buttons (seconds);
# "loud" are the moments the L key jumps between.
STIMULI = {
    "01_clicks": {"name": "Clicks", "note": "6 clicks, 8 s apart",
                  "marks": [["Click 1", 1], ["Click 2", 9], ["Click 3", 17]], "loud": [1, 9, 17, 25, 33, 41]},
    "02_hits": {"name": "Hits", "note": "snare, then rim · each at −6 / −12 / −18 dBFS · loud hits should splash most",
                "marks": [["Loud snare", 1], ["Quiet snare", 13], ["Loud rim", 19], ["Quiet rim", 31]], "loud": [1, 19]},
    "03_sweep": {"name": "Sweep", "note": "sine sweep 20 Hz to 20 kHz, then 12 s of tail",
                 "marks": [["Start", 1], ["Tail", 11]], "loud": [1]},
    "04_skank": {"name": "Skank", "note": "offbeat chords", "marks": [["Start", 1], ["Middle", 12]], "loud": [1]},
    "05_silence_for_kicks": {"name": "Kicks", "note": "single Kicks at 1, 6, 11 s · a pair at 16 s · a gate train at 20 s",
                             "marks": [["Single", 1], ["Pair", 16], ["Gate train", 20], ["Last", 27]],
                             "loud": [1, 6, 11, 16, 20, 27]},
    "06_noise_bursts": {"name": "Noise bursts", "note": "short and long noise bursts, 10 s apart",
                        "marks": [["Short", 1], ["Long", 11]], "loud": [1, 11, 21, 31]},
    "07_click_single": {"name": "Single click", "note": "one click, then the tail", "marks": [["Click", 1]], "loud": [1]},
    "08_held_tones": {"name": "Held tones", "note": "1 kHz sine, then a held A minor chord",
                      "marks": [["Sine", 1], ["Chord", 10]], "loud": [1, 10]},
    "09_pink_noise": {"name": "Pink noise", "note": "steady pink noise", "marks": [], "loud": []},
}
STIM_BY_SHORT = {k.split("_", 1)[1]: k for k in STIMULI}  # "hits" -> "02_hits"

LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
SPEC_MAX_W = 360
SPEC_MAX_H = 72


def warn(msg):
    sys.stderr.write("make_review.py: warning: {}\n".format(msg))


class LayoutError(Exception):
    pass


# ---------------------------------------------------------------- values

def canon_key(k):
    return PARAM_ALIASES.get(k, k)


def canon_value(key, v):
    labels = SWITCH_LABELS.get(key)
    if labels:
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            # Normalised 0 / 0.5 / 1 (ParamSpec normalisedToSwitch).
            return labels[0 if v < 0.25 else (1 if v < 0.75 else 2)]
        s = str(v).upper()
        return OLD_SWITCH_LABELS.get(key, {}).get(s, s)
    return v


def canon_params(raw):
    out = {}
    for k, v in (raw or {}).items():
        ck = canon_key(k)
        out[ck] = canon_value(ck, v)
    return out


def fmt_num(v):
    if isinstance(v, bool):
        return str(v).lower()
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        s = "{:.2f}".format(v).rstrip("0").rstrip(".")
        return "0" if s in ("-0", "") else s
    return str(v)


def clock(v):
    """Knob position on the panel: 0 = fully left (7 o'clock), 0.5 = noon, 1 = fully right (5 o'clock)."""
    if isinstance(v, bool) or not isinstance(v, (int, float)) or v < 0 or v > 1:
        return ""
    if abs(v - 0.5) < 0.005:
        return "noon"
    if v <= 0.005:
        return "fully left"
    if v >= 0.995:
        return "fully right"
    h = 7 + 10 * v
    hh = int(h)
    mm = int(round((h - hh) * 60 / 5.0)) * 5
    if mm == 60:
        hh, mm = hh + 1, 0
    hh12 = (hh - 1) % 12 + 1
    return "{} o'clock".format(hh12) if mm == 0 else "{}:{:02d}".format(hh12, mm)


def param_name(key):
    return PARAM_NAMES.get(key, key.replace("_", " ").upper())


def chip(key, v):
    c = {"k": param_name(key), "v": fmt_num(v)}
    if key in KNOBS:
        c["c"] = clock(v)
    return c


def param_sort_key(key):
    return (PARAM_ORDER.index(key) if key in PARAM_ORDER else len(PARAM_ORDER), key)


def stim_lookup(text):
    """Known stimulus for a stimulus id / material name, and the leftover words."""
    if not text:
        return None, ""
    t = str(text)
    for sid in sorted(STIMULI, key=len, reverse=True):
        for cand in (sid, sid.split("_", 1)[1]):
            if t == cand or t.startswith(cand + "_"):
                return sid, t[len(cand) + 1:]
    return None, t


def pretty_words(s):
    return s.replace("_", " ").strip()


# ---------------------------------------------------------------- files

def try_json(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except (OSError, ValueError) as e:
        warn("could not read {} ({})".format(path, e))
        return None


def wav_rms_dbfs(path, max_frames=300000):
    """RMS of a PCM/float WAV (all channels), sampled with a stride so big files stay quick."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = list(struct.unpack("<HHIIHH", body[:16]))
            if fmt[0] == 0xFFFE and len(body) >= 26:  # WAVE_FORMAT_EXTENSIBLE: real tag opens the sub-format GUID
                fmt[0] = struct.unpack("<H", body[24:26])[0]
        elif cid == b"data":
            raw = body
        pos += 8 + size + (size & 1)
    if not fmt or raw is None:
        return None
    tag, ch, _sr, _br, align, bits = fmt
    width = bits // 8
    if align <= 0 or width not in (2, 3, 4):
        return None
    frames = len(raw) // align
    if frames == 0:
        return None
    stride = max(1, frames // max_frames)
    acc, n = 0.0, 0
    for fi in range(0, frames, stride):
        base = fi * align
        for c in range(ch):
            o = base + c * width
            if tag == 3 and width == 4:
                x = struct.unpack_from("<f", raw, o)[0]
            elif width == 2:
                x = struct.unpack_from("<h", raw, o)[0] / 32768.0
            elif width == 3:
                x = int.from_bytes(raw[o:o + 3], "little", signed=True) / 8388608.0
            else:
                x = struct.unpack_from("<i", raw, o)[0] / 2147483648.0
            acc += x * x
            n += 1
    if n == 0 or acc <= 0:
        return -200.0
    return 10 * math.log10(acc / n)


def small_spectrogram(spec):
    """Shrink a sidecar spectrogram (docs/m1-contracts.md) by max-pooling, so a page
    with dozens of renders stays a few MB. Row 0 is the top (f_max)."""
    try:
        w, h = int(spec["width"]), int(spec["height"])
        data = base64.b64decode(spec["data_b64"])
    except (KeyError, TypeError, ValueError):
        return None
    if len(data) < w * h or w <= 0 or h <= 0:
        return None
    nw, nh = min(w, SPEC_MAX_W), min(h, SPEC_MAX_H)
    out = bytearray(nw * nh)
    for y in range(nh):
        y0, y1 = y * h // nh, max(y * h // nh + 1, (y + 1) * h // nh)
        for x in range(nw):
            x0, x1 = x * w // nw, max(x * w // nw + 1, (x + 1) * w // nw)
            m = 0
            for yy in range(y0, y1):
                row = yy * w
                seg = data[row + x0:row + x1]
                if seg:
                    mm = max(seg)
                    if mm > m:
                        m = mm
            out[y * nw + x] = m
    return {"w": nw, "h": nh, "t0": spec.get("t0_s", 0), "t1": spec.get("t1_s", 0),
            "b64": base64.b64encode(bytes(out)).decode("ascii")}


def flag_reasons(metrics, ignore=frozenset()):
    """Short reasons a render is flagged (same thresholds as the classic page)."""
    if not metrics:
        return []
    m = {k: v for k, v in metrics.items() if k not in ignore}
    num = lambda k: isinstance(m.get(k), (int, float)) and not isinstance(m.get(k), bool)
    r = []
    if (m.get("nan_inf_count") or 0) > 0:
        r.append("NaN/Inf")
    if (m.get("clip_count") or 0) > 0:
        r.append("clips ×{}".format(m["clip_count"]))
    if (m.get("click_count") or 0) > 0:
        r.append("clicks ×{}".format(m["click_count"]))
    if m.get("steady_tone") is True:
        r.append("steady tone")
    if num("ringing_db") and m["ringing_db"] >= 15:
        r.append("ringing {:.0f} dB".format(m["ringing_db"]))
    if num("stereo_correlation") and m["stereo_correlation"] > 0.5:
        r.append("narrow stereo")
    if num("mono_loss_db") and m["mono_loss_db"] < -1.5:
        r.append("mono loss")
    if num("mono_notch_db") and m["mono_notch_db"] < -6:
        r.append("mono notch")
    if num("max_step_db_100ms") and m["max_step_db_100ms"] > 3:
        r.append("level jump")
    return r


# ---------------------------------------------------------------- items

class Item(object):
    """One WAV on the page."""

    def __init__(self, wav, label):
        self.wav = Path(wav)
        self.label = label
        self.params = {}
        self.dims = {}
        self.metrics = {}
        self.spec = None
        self.ignore = frozenset()

    def read_sidecar(self, sidecar_path, with_spec):
        sc = try_json(sidecar_path) if sidecar_path and Path(sidecar_path).exists() else None
        if not isinstance(sc, dict):
            return {}
        self.metrics = sc.get("metrics") or {}
        if with_spec and isinstance(sc.get("spectrogram"), dict):
            self.spec = small_spectrogram(sc["spectrogram"])
        return canon_params(sc.get("params"))


def letterish(v):
    s = str(v)
    return bool(re.match(r"^[A-Z][A-Z0-9]?(_.+)?$", s)) or s.lower() in BEFORE_AFTER_SET


def collect_from_manifests(root, manifests, with_spec):
    groups = []  # (manifest dir, manifest, [items])
    for mp in manifests:
        man = try_json(mp)
        if not isinstance(man, dict) or not isinstance(man.get("renders"), list):
            warn("skipping {} (no renders list)".format(mp))
            continue
        mdir = mp.parent
        ignore = frozenset(man.get("ignore_flags") or [])
        inp = str(man.get("input") or "")
        stim = Path(inp).stem if inp and "," not in inp else None
        items = []
        for i, entry in enumerate(man["renders"]):
            if not isinstance(entry, dict) or "wav" not in entry:
                warn("{}: render entry #{} has no 'wav'".format(mp, i))
                continue
            wav = mdir / entry["wav"]
            if not wav.exists():
                warn("render WAV referenced by {} not found: {}".format(mp, wav))
                continue
            it = Item(wav, Path(entry["wav"]).stem)
            it.ignore = ignore
            params = it.read_sidecar(mdir / entry["sidecar"] if entry.get("sidecar") else None, with_spec)
            for k, v in canon_params(entry.get("params")).items():
                params.setdefault(k, v)
            s = params.pop("stimulus", None) or stim
            it.params = params
            it.dims = dict((k, v) for k, v in params.items())
            if s:
                it.dims["stimulus"] = str(s)
            items.append(it)
        groups.append((mdir, man, items))
    if len(groups) > 1:
        merge_manifest_groups(root, groups)
    return [it for _, _, items in groups for it in items]


def varying_in(items, key):
    return len(set(json.dumps(it.dims.get(key)) for it in items)) > 1


def merge_manifest_groups(root, groups):
    """Several sweep folders on one page. Adds folder dims where the folder name
    carries information the params don't (before/ vs after/, A/ B/ C/), and a
    Sweep/Value pair when the folders sweep different knobs."""
    swept = []
    for _, _, items in groups:
        keys = sorted(k for k in set(k for it in items for k in it.params) if varying_in(items, k))
        swept.append(tuple(keys))
    hetero = len(set(swept)) > 1
    if hetero:
        all_swept = set(k for ks in swept for k in ks)
        for (_, _, items), keys in zip(groups, swept):
            for it in items:
                for k in all_swept:
                    it.dims.pop(k, None)
                if keys:
                    it.dims["_sweep"] = "+".join(keys)
                    it.dims["_value"] = it.params.get(keys[0]) if len(keys) == 1 else \
                        " · ".join(fmt_num(it.params.get(k)) for k in keys)

    # What each folder's renders look like apart from their folder name.
    def signature(items, keys):
        consts = {}
        for k in set(k for it in items for k in it.dims):
            if k.startswith("folder") or k in ("_value",):
                continue
            vals = set(json.dumps(it.dims.get(k)) for it in items)
            consts[k] = sorted(vals)
        return json.dumps([list(keys) if hetero else [], sorted(consts.items())])

    sigs = [signature(items, keys) for (_, _, items), keys in zip(groups, swept)]
    parts = [mdir.relative_to(root).parts if mdir != root else () for mdir, _, _ in groups]
    depth = max(len(p) for p in parts) if parts else 0
    for lvl in range(depth):
        vals = [p[lvl] if lvl < len(p) else "" for p in parts]
        if len(set(vals)) < 2:
            continue
        keep = all(letterish(v) for v in vals)
        if not keep:
            by_sig = {}
            for s, v in zip(sigs, vals):
                by_sig.setdefault(s, set()).add(v)
            keep = any(len(v) > 1 for v in by_sig.values())  # folder not explained by the params
        if keep:
            for (_, _, items), v in zip(groups, vals):
                for it in items:
                    it.dims["folder{}".format(lvl + 1)] = v


def collect_from_wavs(root, with_spec):
    """A/B prototype folders: no manifest, meaning lives in the file and folder names."""
    wavs = []
    for dirpath, dirnames, filenames in os.walk(str(root)):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        rel_depth = len(Path(dirpath).relative_to(root).parts)
        if rel_depth > 3:
            dirnames[:] = []
            continue
        for fn in sorted(filenames):
            if fn.lower().endswith(".wav") and not fn.startswith("."):
                wavs.append(Path(dirpath) / fn)
    items = []
    for w in wavs:
        it = Item(w, w.stem)
        sc = w.with_suffix(".json")
        it.params = it.read_sidecar(sc, with_spec) if sc.exists() else {}
        items.append(it)
    if not items:
        return items

    rels = [it.wav.relative_to(root) for it in items]
    dir_parts = [r.parts[:-1] for r in rels]
    depth = max(len(p) for p in dir_parts)
    variant_from_dir = False
    for lvl in range(depth):
        vals = [p[lvl] if lvl < len(p) else "" for p in dir_parts]
        if len(set(vals)) < 2:
            continue
        if all(v.lower() in ATTITUDE_WORDS for v in vals):
            for it, v in zip(items, vals):
                it.dims["attitude"] = canon_value("attitude", v)
        elif not variant_from_dir and all(letterish(v) for v in vals):
            variant_from_dir = True
            for it, v in zip(items, vals):
                it.dims["variant"] = v
        else:
            for it, v in zip(items, vals):
                it.dims["folder{}".format(lvl + 1)] = v

    toks = [it.wav.stem.split("_") for it in items]
    if "attitude" not in items[0].dims:
        att_idx = []
        for t in toks:
            hits = [i for i, x in enumerate(t) if x.lower() in ATTITUDE_WORDS]
            att_idx.append(hits[0] if len(hits) == 1 else None)
        if all(i is not None for i in att_idx):
            for it, t, i in zip(items, toks, att_idx):
                it.dims["attitude"] = canon_value("attitude", t[i])
                del t[i]
    if not variant_from_dir:
        letter_idx = []
        for t in toks:
            hits = [i for i, x in enumerate(t) if i > 0 and re.match(r"^[A-Z][A-Z0-9]?$", x)]
            letter_idx.append(hits[0] if hits else None)
        if all(i is not None for i in letter_idx):
            for it, t, i in zip(items, toks, letter_idx):
                it.dims["variant"] = "_".join(t[i:])
                del t[i:]
        elif all(len(t) > 1 and t[-1].lower() in BEFORE_AFTER_SET for t in toks):
            for it, t in zip(items, toks):
                it.dims["variant"] = t.pop()
        elif all(len(t) > 1 for t in toks):
            for it, t in zip(items, toks):
                it.dims["variant"] = t.pop()
    for it, t in zip(items, toks):
        it.dims["material"] = "_".join(t) or it.wav.stem
    return items


def load_references(out_path, reference_dirs, with_spec):
    refs = []
    for d in reference_dirs:
        d = Path(d)
        if not d.is_dir():
            raise LayoutError("--reference directory not found: {}".format(d))
        for w in sorted(d.glob("*.wav")):
            it = Item(w, w.stem)
            sc = w.with_suffix(".json")
            it.read_sidecar(sc if sc.exists() else None, with_spec)
            refs.append(it)
        if not any(True for _ in d.glob("*.wav")):
            warn("no .wav files found in reference dir {}".format(d))
    return refs


# ---------------------------------------------------------------- layout

def dim_order_key(k):
    fixed = ["stimulus", "material", "_sweep"]
    if k in fixed:
        return (0, fixed.index(k), k)
    if k.startswith("folder"):
        return (1, 0, k)
    if k in PARAM_ORDER:
        return (2, PARAM_ORDER.index(k), k)
    if k in ("variant", "_value"):
        return (4, 0, k)
    return (3, 0, k)


def value_sort_key(key, v):
    if v is None:
        return (-1, 0, "")  # a key only some renders have: the plain ones first
    s = str(v)
    labels = SWITCH_LABELS.get(key)
    if labels and s in labels:
        return (0, labels.index(s), "")
    if s.lower() in BEFORE_AFTER:
        return (1, BEFORE_AFTER.index(s.lower()), "")
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        return (2, float(v), "")
    return (3, 0, s)


def distinct(items, key):
    seen = {}
    for it in items:
        v = it.dims.get(key)
        seen.setdefault(json.dumps(v), v)
    return sorted(seen.values(), key=lambda v: value_sort_key(key, v))


ROLE_ALIASES = {"boing": "tension", "sweep": "_sweep", "value": "_value", "version": "variant"}


def resolve_key(name, keys):
    if name is None:
        return None
    n = ROLE_ALIASES.get(name.lower(), name.lower())
    if n == "none":
        return "none"
    if n in keys:
        return n
    if n in ("stimulus", "material"):
        other = "material" if n == "stimulus" else "stimulus"
        if other in keys:
            return other
    if n == "folder":
        f = sorted(k for k in keys if k.startswith("folder"))
        if f:
            return f[0]
    raise LayoutError("'{}' is not something that changes between these renders; choose from: {}".format(
        name, ", ".join(display_key(k) for k in sorted(keys, key=dim_order_key)) or "(nothing varies)"))


def display_key(k):
    return {"_sweep": "sweep", "_value": "value"}.get(k, k)


def choose_roles(items, rows=None, columns=None, variants=None):
    keys = sorted(set(k for it in items for k in it.dims), key=dim_order_key)
    varying = [k for k in keys if len(distinct(items, k)) > 1]
    rows = [resolve_key(r, keys) for r in rows] if rows is not None else None
    if rows is not None:
        rows = [r for r in rows if r != "none"]
    cols = resolve_key(columns, keys)
    var = resolve_key(variants, keys)
    used = set(rows or []) | set(x for x in (cols, var) if x not in (None, "none"))
    free = [k for k in varying if k not in used]
    row_pref = ("stimulus", "material", "_sweep")

    if cols is None:
        for k in ("attitude", "springs"):
            if k in free:
                cols = k
                free.remove(k)
                break
    if var is None:
        cand = [k for k in free if k in ("variant", "_value")]
        if not cand:
            cand = [k for k in free if k.startswith("folder") and all(letterish(v) for v in distinct(items, k))]
        if not cand:
            knobs = [k for k in free if k in KNOBS]
            cand = sorted(knobs, key=lambda k: -len(distinct(items, k)))[:1]
        if not cand:
            cand = [k for k in free if k not in row_pref]
        var = cand[0] if cand else None
        if var:
            free.remove(var)
    if cols is None:
        cand = [k for k in free if k not in row_pref]
        if cand:
            cols = min(cand, key=lambda k: len(distinct(items, k)))
            free.remove(cols)
    if rows is None:
        rows = free
    return rows, (None if cols == "none" else cols), (None if var == "none" else var)


def dim_display(key, v, item=None):
    """(name, detail) for one dim value."""
    if v is None:
        return ("(none)", "")
    if key in ("stimulus", "material"):
        sid, rest = stim_lookup(v)
        if sid:
            return (STIMULI[sid]["name"] + (" · " + pretty_words(rest) if rest else ""), "")
        return (pretty_words(str(v)), "")
    if key == "_sweep":
        return (" + ".join(param_name(k) for k in str(v).split("+")) + " sweep", "")
    if key == "_value":
        sk = item.dims.get("_sweep", "") if item else ""
        if sk and "+" not in sk:
            return (param_name(sk) + " " + fmt_num(v), clock(v) if sk in KNOBS else "")
        return (fmt_num(v), "")
    if key == "attitude":
        return (str(v), ATTITUDE_WHAT.get(str(v), ""))
    if key == "springs":
        return ("TANK {}".format(v), "")
    if key in KNOBS:
        return (param_name(key) + " " + fmt_num(v), clock(v))
    if key.startswith("folder") or key == "variant":
        return (pretty_words(str(v)), "")
    return (param_name(key) + " " + fmt_num(v), "")


def read_readme(folder):
    for name in ("README.txt", "README.md", "readme.txt"):
        p = Path(folder) / name
        if p.exists():
            try:
                return p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                return None
    return None


def readme_descriptions(text):
    """Lines like 'A_today   a burst of noise...' -> {'A_today': 'a burst...'}."""
    out = {}
    for line in (text or "").splitlines():
        m = re.match(r"^\s*([A-Za-z0-9][\w.\-]*)\s{2,}(\S.*)$", line)
        if m:
            out[m.group(1)] = m.group(2).strip()
    return out


def variant_display(key, v, item, pos, readme_desc):
    """(letter, name, detail) for a variant."""
    letter = LETTERS[pos] if pos < len(LETTERS) else str(pos + 1)
    if key is None:
        return (letter, pretty_words(item.label), "")
    s = str(v)
    desc = readme_desc.get(s, "")
    if key in ("variant",) or key.startswith("folder"):
        m = re.match(r"^([A-Z][A-Z0-9]?)(?:_(.+))?$", s)
        if m:
            return (m.group(1), pretty_words(m.group(2) or ""), desc or readme_desc.get(m.group(1), ""))
        return (letter, pretty_words(s), desc)
    name, detail = dim_display(key, v, item)
    return (letter, name, desc or detail)


PALETTE = ["--g1", "--g2", "--g3", "--g4", "--g5", "--g6"]
ATT_COLOR = {"CLEAN": "--clean", "TAPE": "--driven", "VALVE": "--kicked"}  # CSS names predate v1.0.43


def build_page_data(root, out_path, items, refs, rows=None, columns=None, variants=None,
                    title=None, level_match=None):
    """level_match: True / False, or None for the default (see default_level_match)."""
    if not items:
        raise LayoutError("no renders found in {} (no manifest.json and no .wav files)".format(root))
    row_keys, col_key, var_key = choose_roles(items, rows, columns, variants)
    out_dir = out_path.parent

    def rel(p):
        return os.path.relpath(str(p), str(out_dir)).replace(os.sep, "/")

    # Rows and columns.
    def row_val(it):
        return tuple(it.dims.get(k) for k in row_keys)

    row_vals = []
    for it in items:
        rv = row_val(it)
        if rv not in row_vals:
            row_vals.append(rv)
    row_vals.sort(key=lambda rv: tuple(value_sort_key(k, v) for k, v in zip(row_keys, rv)))
    col_vals = distinct(items, col_key) if col_key else [None]

    readme = read_readme(root)
    rdesc = readme_descriptions(readme)

    rows_out = []
    for ri, rv in enumerate(row_vals):
        sample = next(it for it in items if row_val(it) == rv)
        names = [dim_display(k, v, sample)[0] for k, v in zip(row_keys, rv) if v is not None]
        stim = None
        for k, v in zip(row_keys, rv):
            if k in ("stimulus", "material"):
                stim = stim_lookup(v)[0]
        if stim is None:
            s = sample.dims.get("stimulus") or sample.dims.get("material")
            stim = stim_lookup(s)[0] if s else None
            if stim and len(set(it.dims.get("stimulus") or it.dims.get("material") for it in items)) == 1 and not names:
                names = [STIMULI[stim]["name"]]
        info = STIMULI.get(stim, {})
        rows_out.append({"id": "r{}".format(ri), "name": " · ".join(names) if names else (info.get("name") or ""),
                         "note": info.get("note", ""), "marks": info.get("marks", []), "loud": info.get("loud", [])})

    cols_out = []
    for ci, cv in enumerate(col_vals):
        if col_key is None:
            cols_out.append({"id": "c0", "name": "", "what": "", "color": "--g1"})
            continue
        sample = next(it for it in items if it.dims.get(col_key) == cv)
        name, what = dim_display(col_key, cv, sample)
        color = ATT_COLOR.get(str(cv)) if col_key == "attitude" else None
        cols_out.append({"id": "c{}".format(ci), "name": name, "what": what,
                         "color": color or PALETTE[ci % len(PALETTE)]})

    # Constant settings: across the page, then extra ones per panel.
    all_param_keys = sorted(set(k for it in items for k in it.params), key=param_sort_key)

    def constants(group):
        out = {}
        for k in all_param_keys:
            vals = set(json.dumps(it.params.get(k)) for it in group)
            if len(vals) == 1 and all(k in it.params for it in group):
                out[k] = group[0].params[k]
        return out

    page_const = constants(items)
    shown_keys = set(row_keys) | set(x for x in (col_key, var_key) if x)

    items_out = {}
    panels = {}
    for ri, rv in enumerate(row_vals):
        for ci, cv in enumerate(col_vals):
            group = [it for it in items if row_val(it) == rv and (col_key is None or it.dims.get(col_key) == cv)]
            if not group:
                continue
            if var_key:
                group.sort(key=lambda it: (value_sort_key(var_key, it.dims.get(var_key)), it.label))
            else:
                group.sort(key=lambda it: it.label)
            pconst = constants(group)
            extra = [chip(k, v) for k, v in pconst.items()
                     if k not in page_const and k not in shown_keys and not (var_key == "_value" and k == group[0].dims.get("_sweep"))]
            varying_here = [k for k in all_param_keys if k not in pconst and k != var_key
                            and not (var_key == "_value" and k == group[0].dims.get("_sweep"))]
            vs = []
            for pos, it in enumerate(group):
                iid = "i{}".format(len(items_out))
                k, n, d = variant_display(var_key, it.dims.get(var_key) if var_key else None, it, pos, rdesc)
                vchips = [chip(p, it.params[p]) for p in varying_here if p in it.params and p not in shown_keys]
                vs.append({"k": k, "n": n, "d": d, "item": iid, "chips": vchips})
                items_out[iid] = item_json(it, rel)
            rms = [items_out[v["item"]]["rms"] for v in vs]
            for v, off in zip(vs, level_offsets(rms)):
                v["lm"] = off
            panels["r{}|c{}".format(ri, ci)] = {"chips": extra, "variants": vs, "medianRms": median(rms)}

    # One legend for the page when every panel offers the same versions.
    lists = [[(v["k"], v["n"]) for v in p["variants"]] for p in panels.values()]
    uniform = bool(lists) and all(l == lists[0] for l in lists)
    legend = None
    if uniform and var_key:
        first = next(iter(panels.values()))["variants"]
        legend = [{"k": v["k"], "n": v["n"], "d": v["d"]} for v in first]

    refs_out = []
    for i, r in enumerate(refs):
        # No flags on references: they are recordings of real units, not renders to fix.
        refs_out.append(dict(item_json(r, rel), id="ref{}".format(i), name=r.label, flags=[]))

    lede_lines = []
    if readme:
        for line in readme.strip().splitlines():
            if not line.strip():
                break
            lede_lines.append(line.strip())

    fixed = [chip(k, v) for k, v in page_const.items() if k not in shown_keys]
    page_title = title or pretty_words(root.name)
    page_title = page_title[:1].upper() + page_title[1:]
    return {
        "title": page_title,
        "source": source_label(root),
        "page": out_path.name,
        "storeKey": "rv-review-v2:" + source_label(root) + "/" + out_path.name,
        "lede": " ".join(lede_lines),
        "readme": readme or "",
        "fixed": fixed,
        "axes": {"rows": ", ".join(display_key(k) for k in row_keys), "columns": display_key(col_key) if col_key else "",
                 "variants": display_key(var_key) if var_key else ""},
        "colsTitle": axis_title(col_key),
        "variantsTitle": variant_title(var_key, items),
        "rows": rows_out,
        "cols": cols_out,
        "panels": panels,
        "legend": legend,
        "items": items_out,
        "references": refs_out,
        "levelMatch": default_level_match(var_key) if level_match is None else bool(level_match),
        "levelMatchCapDb": LEVEL_MATCH_CAP_DB,
    }


# Level-match: each version is moved towards the median loudness of its panel,
# by at most this much either way.
LEVEL_MATCH_CAP_DB = 12.0


def default_level_match(var_key):
    """On for lettered versions / prototype voicings (A/B/C, before/after, one render
    per panel against a reference): there loudness would only bias the choice.
    Off for a knob or switch sweep: the loudness change is part of what the knob does."""
    return var_key is None or var_key == "variant" or var_key.startswith("folder")


def median(values):
    v = sorted(x for x in values if x is not None)
    if not v:
        return None
    m = len(v) // 2
    return v[m] if len(v) % 2 else (v[m - 1] + v[m]) / 2.0


def level_offsets(rms_values, cap=LEVEL_MATCH_CAP_DB):
    """dB to add to each version so it sits at the panel's median loudness, capped at
    +/- cap. None where the loudness is unknown."""
    med = median(rms_values)
    out = []
    for r in rms_values:
        if r is None or med is None:
            out.append(None)
        else:
            out.append(round(max(-cap, min(cap, med - r)), 1))
    return out


def source_label(root):
    """Repo-relative where possible (renders/<name>), so pasted results say which page they came from."""
    s = Path(root).resolve().as_posix()
    if "/renders/" in s:
        return "renders/" + s.split("/renders/", 1)[1]
    return s


def axis_title(key):
    if not key:
        return ""
    if key in PARAM_NAMES:
        return param_name(key)
    return {"stimulus": "material", "material": "material", "_sweep": "sweep"}.get(
        key, "folder" if key.startswith("folder") else display_key(key).replace("_", " "))


def variant_title(var_key, items):
    if not var_key:
        return "versions"
    if var_key == "_value":
        sw = set(it.dims.get("_sweep") for it in items)
        return "knob position" if len(sw) > 1 else param_name(next(iter(sw)) or "value")
    if var_key in ("variant",) or var_key.startswith("folder"):
        return "versions"
    return param_name(var_key)


def item_json(it, rel):
    rms = it.metrics.get("rms_dbfs") if isinstance(it.metrics, dict) else None
    if not isinstance(rms, (int, float)) or isinstance(rms, bool):
        rms = wav_rms_dbfs(it.wav)
    return {
        "wav": rel(it.wav),
        "label": it.label,
        "rms": None if rms is None or rms <= -150 else round(float(rms), 2),
        "flags": flag_reasons(it.metrics, it.ignore),
        "spec": it.spec,
    }


def collect(root, with_spec=True):
    root = Path(root)
    top = root / "manifest.json"
    if top.exists():
        man = try_json(top)
        name = man.get("name") if isinstance(man, dict) else None
        return collect_from_manifests(root, [top], with_spec), "manifest", name
    manifests = []
    for dirpath, dirnames, filenames in os.walk(str(root)):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        if len(Path(dirpath).relative_to(root).parts) > 3:
            dirnames[:] = []
            continue
        if "manifest.json" in filenames:
            manifests.append(Path(dirpath) / "manifest.json")
    if manifests:
        return collect_from_manifests(root, sorted(manifests), with_spec), "manifests", None
    return collect_from_wavs(root, with_spec), "wavs", None


def render_html(data):
    template_path = HERE / TEMPLATE_NAME
    template = template_path.read_text(encoding="utf-8")
    data_json = json.dumps(data, ensure_ascii=False).replace("</", "<\\/")
    title = data["title"].replace("&", "&amp;").replace("<", "&lt;")
    return template.replace("__RV_TITLE__", title).replace("__RV_DATA_JSON__", data_json)


def describe(data):
    ax = data["axes"]
    n = len(data["items"])
    parts = ["{} renders".format(n)]
    parts.append("rows: {} ({})".format(ax["rows"] or "-", len(data["rows"])))
    parts.append("columns: {} ({})".format(ax["columns"] or "-", len(data["cols"])))
    nv = max(len(p["variants"]) for p in data["panels"].values()) if data["panels"] else 0
    parts.append("versions: {} ({})".format(ax["variants"] or "-", nv))
    if data["references"]:
        parts.append("{} references".format(len(data["references"])))
    return ", ".join(parts)
