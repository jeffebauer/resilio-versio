"""Sonic signature comparison: real springs (Wellspring, Magneto, IR library) vs ours.

Per source, 1/3-octave band response (wet band energy / stimulus band energy),
normalised to the 500 Hz-2 kHz mean. Plus band T60s from click/IR decays.
"""
import glob, json, os, sys
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt

ROOT = str(__import__("pathlib").Path(__file__).resolve().parents[3])  # the repo root; run from the main checkout
REF = f"{ROOT}/test_audio/reference"
STIM = f"{ROOT}/test_audio/stimulus"
AB = f"{ROOT}/renders/references/wellspring/ab"
IRS = sorted(glob.glob(f"{ROOT}/renders/ir_library/*.wav"))
EXTRA = sys.argv[1] if len(sys.argv) > 1 else None  # dir of extra renders of ours

CENTRES = [31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
           1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000]


def load(path):
    x, sr = sf.read(path, always_2d=True)
    return x.astype(np.float64), sr


def band_energy(x, sr):
    """Energy per 1/3-octave band of a (frames, ch) signal, summed over channels (FFT)."""
    X = np.fft.rfft(x, axis=0)
    p = (np.abs(X) ** 2).sum(axis=1)
    f = np.fft.rfftfreq(x.shape[0], 1 / sr)
    out = []
    for c in CENTRES:
        lo, hi = c / 2 ** (1 / 6), c * 2 ** (1 / 6)
        m = (f >= lo) & (f < hi)
        out.append(p[m].sum() + 1e-30)
    return np.array(out)


def norm(db):
    mids = [i for i, c in enumerate(CENTRES) if 500 <= c <= 2000]
    return db - db[mids].mean()


def response(wet_path, stim_path, mono_stim=True):
    y, sr = load(wet_path)
    x, sr2 = load(stim_path)
    assert sr == sr2
    if mono_stim:
        x = x[:, :1]
    Ey = band_energy(y, sr) / y.shape[1]
    Ex = band_energy(x, sr)
    return norm(10 * np.log10(Ey / Ex))


def ir_response(path, seconds=4.0):
    y, sr = load(path)
    y = y[: int(seconds * sr)]
    return norm(10 * np.log10(band_energy(y, sr) / y.shape[1]))


def onsets(x, sr, thresh_db=-40, gap_s=1.0):
    """Sample indices where the stimulus crosses thresh after a quiet gap."""
    a = np.abs(x).max(axis=1)
    th = a.max() * 10 ** (thresh_db / 20)
    idx = np.flatnonzero(a > th)
    out, last = [], -10 ** 9
    for i in idx:
        if i - last > gap_s * sr:
            out.append(i)
        last = i
    return out


T60_BANDS = [(63, 125), (125, 250), (250, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000)]


def band_t60(seg, sr):
    """T60 per octave band from Schroeder decay, fit -5..-25 dB (x3), noise-safe."""
    out = []
    for lo, hi in T60_BANDS:
        sos = butter(4, [lo, hi], btype="band", fs=sr, output="sos")
        b = sosfiltfilt(sos, seg, axis=0)
        e = (b ** 2).sum(axis=1)
        # noise floor: mean of last 10 % ; subtract (Lundeby-lite)
        floor = e[-len(e) // 10:].mean()
        e = np.maximum(e - floor, 0)
        edc = np.cumsum(e[::-1])[::-1]
        if edc[0] <= 0:
            out.append(np.nan); continue
        d = 10 * np.log10(edc / edc[0] + 1e-30)
        t = np.arange(len(d)) / sr
        m = (d <= -5) & (d >= -25)
        if m.sum() < sr * 0.05:
            out.append(np.nan); continue
        slope = np.polyfit(t[m], d[m], 1)[0]
        out.append(-60 / slope if slope < 0 else np.nan)
    return np.array(out)


def click_t60(wet_path, stim_path, seg_s=3.0, lat=0):
    y, sr = load(wet_path)
    x, _ = load(stim_path)
    ons = onsets(x, sr)
    res = []
    for o in ons:
        s = o + lat
        seg = y[s: s + int(seg_s * sr)]
        if len(seg) < seg_s * sr * 0.9:
            continue
        res.append(band_t60(seg, sr))
    return np.nanmedian(np.array(res), axis=0) if res else None


def early_late(wet_path, stim_path, lat=0):
    """Per click: band energy in 0-80 ms vs 300-1500 ms; returns late-minus-early dB (normalised to mids)."""
    y, sr = load(wet_path)
    x, _ = load(stim_path)
    E, L = 0, 0
    for o in onsets(x, sr):
        s = o + lat
        e = y[s: s + int(0.08 * sr)]
        l = y[s + int(0.3 * sr): s + int(1.5 * sr)]
        if len(l) < int(1.2 * sr) - 10:
            continue
        E = E + band_energy(np.pad(e, ((0, len(l) - len(e)), (0, 0))), sr)
        L = L + band_energy(l, sr)
    return norm(10 * np.log10(E)), norm(10 * np.log10(L))


def summarise(name, r):
    c = np.array(CENTRES)
    def at(f):
        return r[CENTRES.index(f)]
    # low corner: going down from 500 Hz, first band <= -6 dB
    lo6 = next((f for f in reversed(CENTRES[:CENTRES.index(500)]) if at(f) <= -6), None)
    hi6 = next((f for f in CENTRES[CENTRES.index(2000):] if at(f) <= -6), None)
    pk = CENTRES[int(np.argmax(r[2:-3])) + 2]
    lowshare = r[CENTRES.index(63):CENTRES.index(160) + 1].mean()
    return dict(name=name, lo6=lo6, hi6=hi6, peak=pk, low_63_160=round(float(lowshare), 1),
                b63=round(float(at(63)), 1), b125=round(float(at(125)), 1), b250=round(float(at(250)), 1),
                b4k=round(float(at(4000)), 1), b8k=round(float(at(8000)), 1))


results = {}
S = STIM
pairs = {
    "W clicks": (f"{REF}/wellspring_A_clicks.wav", f"{S}/01_clicks.wav"),
    "W sweep": (f"{REF}/wellspring_D_sweep.wav", f"{S}/03_sweep.wav"),
    "W hits": (f"{REF}/wellspring_B_hits.wav", f"{S}/02_hits.wav"),
    "W skank": (f"{REF}/wellspring_E_skank.wav", f"{S}/04_skank.wav"),
    "M clicks": (f"{REF}/magneto_MA_clicks.wav", f"{S}/01_clicks.wav"),
    "M sweep": (f"{REF}/magneto_MS_sweep.wav", f"{S}/03_sweep.wav"),
    "M hits": (f"{REF}/magneto_MB_hits.wav", f"{S}/02_hits.wav"),
    "M skank": (f"{REF}/magneto_ME_skank.wav", f"{S}/04_skank.wav"),
}
for att in ["CLEAN", "DRIVEN"]:
    pairs[f"Ours {att} clicks"] = (f"{AB}/resilio_match_{att}_clicks.wav", f"{S}/01_clicks.wav")
    pairs[f"Ours {att} hits"] = (f"{AB}/resilio_match_{att}_hits.wav", f"{S}/02_hits.wav")
    pairs[f"Ours {att} skank"] = (f"{AB}/resilio_match_{att}_skank.wav", f"{S}/04_skank.wav")
if EXTRA:
    for p in sorted(glob.glob(f"{EXTRA}/*.wav")):
        stim = os.path.basename(p).split("__")[1].replace(".wav", "")
        pairs["X " + os.path.basename(p).split("__")[0] + " " + stim] = (p, f"{S}/{stim}.wav")

for k, (w, s) in pairs.items():
    if os.path.exists(w):
        results[k] = response(w, s)

ir = np.array([ir_response(p) for p in IRS])
results["IR median"] = np.median(ir, axis=0)
results["IR p25"] = np.percentile(ir, 25, axis=0)
results["IR p75"] = np.percentile(ir, 75, axis=0)

print("1/3-octave response, dB re 500 Hz-2 kHz mean")
hdr = "".join(f"{int(c) if c >= 100 else c:>7}" for c in CENTRES)
print(f"{'source':<24}{hdr}")
for k, r in results.items():
    print(f"{k:<24}" + "".join(f"{v:7.1f}" for v in r))

print("\nSummary")
for k, r in results.items():
    print(summarise(k, r))

# IR library spread of the low corner
lows = [summarise("", r)["lo6"] for r in ir]
print("\nIR library -6 dB low corner distribution:", sorted([l for l in lows if l]), "none:", lows.count(None))

print("\nOctave-band T60 (s), median over clicks; bands", T60_BANDS)
for k in ["W clicks", "M clicks", "Ours CLEAN clicks", "Ours DRIVEN clicks"] + [k for k in pairs if k.startswith("X") and "clicks" in k]:
    w, s = pairs[k]
    t = click_t60(w, s)
    print(f"{k:<24}", None if t is None else " ".join(f"{v:5.2f}" for v in t))
def _irt(p):
    y, sr = load(p)
    return band_t60(y[: int(4 * sr)], sr)
irt = np.array([_irt(p) for p in IRS])
print(f"{'IR median':<24}", " ".join(f"{v:5.2f}" for v in np.nanmedian(irt, axis=0)))
irn = irt / irt[:, [3]]
print(f"{'IR median (re 500-1k)':<24}", " ".join(f"{v:5.2f}" for v in np.nanmedian(irn, axis=0)))

print("\nEarly (0-80 ms) and late (0.3-1.5 s) click spectra, dB re mids")
for k in ["W clicks", "M clicks", "Ours CLEAN clicks", "Ours DRIVEN clicks"]:
    w, s = pairs[k]
    e, l = early_late(w, s)
    print(f"{k+' early':<24}" + "".join(f"{v:7.1f}" for v in e))
    print(f"{k+' late':<24}" + "".join(f"{v:7.1f}" for v in l))
