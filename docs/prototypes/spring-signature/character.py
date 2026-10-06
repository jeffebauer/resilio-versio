"""Owner's three observations, measured: diffusion + top end, stereo flicker, mid decay between skank hits."""
import glob
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt
from scipy.special import erfc

ROOT = str(__import__("pathlib").Path(__file__).resolve().parents[3])  # the repo root; run from the main checkout
REF = f"{ROOT}/test_audio/reference"
STIM = f"{ROOT}/test_audio/stimulus"
AB = f"{ROOT}/renders/references/wellspring/ab"
EQ = f"{ROOT}/renders/eq_preview"
IRS = sorted(glob.glob(f"{ROOT}/renders/ir_library/*.wav"))


def load(p):
    x, sr = sf.read(p, always_2d=True)
    return x.astype(np.float64), sr


def onsets(x, sr, thresh_db=-30, gap_s=0.25):
    a = np.abs(x).max(axis=1)
    th = a.max() * 10 ** (thresh_db / 20)
    out, last = [], -10 ** 9
    for i in np.flatnonzero(a > th):
        if i - last > gap_s * sr:
            out.append(i)
        last = i
    return out


def bp(x, lo, hi, sr):
    return sosfiltfilt(butter(4, [lo, hi], btype="band", fs=sr, output="sos"), x, axis=0)


SRC = {
    "Wellspring": (f"{REF}/wellspring_A_clicks.wav", f"{REF}/wellspring_E_skank.wav"),
    "Magneto": (f"{REF}/magneto_MA_clicks.wav", f"{REF}/magneto_ME_skank.wav"),
    "Ours CLEAN": (f"{AB}/resilio_match_CLEAN_clicks.wav", f"{AB}/resilio_match_CLEAN_skank.wav"),
    "Ours TAPE": (f"{AB}/resilio_match_TAPE_clicks.wav", f"{AB}/resilio_match_TAPE_skank.wav"),  # DRIVEN until v1.0.43
}
cx, sr0 = load(f"{STIM}/01_clicks.wav")
CLICKS = onsets(cx, sr0, -40, 1.0)
sx, _ = load(f"{STIM}/04_skank.wav")

print("channels:", {k: load(v[0])[0].shape[1] for k, v in SRC.items()})

# ---- 1. Diffusion: normalised echo density (Abel & Huang), 20 ms windows, 1-4 kHz and broadband
def ned(seg, sr, win=0.02):
    n = int(win * sr); out = []
    for i in range(0, len(seg) - n, n // 2):
        w = seg[i:i + n]
        s = w.std()
        out.append(0 if s == 0 else np.mean(np.abs(w) > s) / erfc(1 / np.sqrt(2)))
    return np.array(out)

print("\n1a. Echo density (1 = fully diffuse noise-like; low = separate echoes), median over clicks, mono sum")
times = [(0, 0.05), (0.05, 0.15), (0.15, 0.4), (0.4, 1.0)]
print(f"{'':<14}" + "".join(f"{f'{a*1000:.0f}-{b*1000:.0f}ms':>12}" for a, b in times))
def ned_row(path, clicks):
    y, sr = load(path); m = y.mean(axis=1)
    rows = []
    for o in clicks:
        seg = m[o: o + sr]
        if len(seg) < sr: continue
        d = ned(seg, sr); t = np.arange(len(d)) * 0.01
        rows.append([d[(t >= a) & (t < b)].mean() for a, b in times])
    return np.median(np.array(rows), axis=0)
for k, (c, _) in SRC.items():
    print(f"{k:<14}" + "".join(f"{v:12.2f}" for v in ned_row(c, CLICKS)))
ir = []
for p in IRS:
    y, sr = load(p); m = y.mean(axis=1)
    o = int(np.argmax(np.abs(m) > np.abs(m).max() * 0.1))
    d = ned(m[o:o + sr], sr); t = np.arange(len(d)) * 0.01
    ir.append([d[(t >= a) & (t < b)].mean() for a, b in times])
print(f"{'IR median':<14}" + "".join(f"{v:12.2f}" for v in np.median(np.array(ir), axis=0)))

# ---- 1b. Top end over time + noise floor (is the Wellspring's 'air' tail or hiss?)
print("\n1b. Band level (dB re 500-2k band) in the tail windows after each click; and the file's quietest 0.5 s")
bands = [(500, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]
for k, (c, _) in SRC.items():
    y, sr = load(c); m = y.mean(axis=1)
    lv = {b: [] for b in bands}
    for (a, bb) in [(0.0, 0.1), (0.3, 1.0), (1.0, 2.5)]:
        e = {}
        for b in bands:
            f = bp(m, b[0], min(b[1], 0.45 * sr), sr)
            e[b] = np.mean([np.mean(f[o + int(a * sr): o + int(bb * sr)] ** 2) for o in CLICKS])
        lv[(a, bb)] = [10 * np.log10(e[b] / e[bands[0]]) for b in bands]
    # quietest 0.5 s window, absolute dBFS per band
    n = int(0.5 * sr); rms = [np.mean(m[i:i + n] ** 2) for i in range(0, len(m) - n, n)]
    q = int(np.argmin(rms)) * n
    fl = [10 * np.log10(np.mean(bp(m[q:q + n], b[0], min(b[1], 0.45 * sr), sr) ** 2) + 1e-20) for b in bands]
    tailref = 10 * np.log10(np.mean([np.mean(bp(m, 500, 2000, sr)[o + int(0.3 * sr): o + sr] ** 2) for o in CLICKS]))
    print(f"{k:<14} 0-100ms {np.round(lv[(0.0,0.1)],1)}  0.3-1s {np.round(lv[(0.3,1.0)],1)}  1-2.5s {np.round(lv[(1.0,2.5)],1)}"
          f"  floor dB re tail mids {np.round(np.array(fl) - tailref, 1)}")

# ---- 2. Stereo: short-window L/R balance during tails
print("\n2. Stereo in click tails (50 ms-1.5 s): correlation, balance swing, flips per second")
def stereo(path):
    y, sr = load(path)
    if y.shape[1] < 2: return None
    n = int(0.015 * sr); res = []
    for o in CLICKS:
        seg = y[o + int(0.05 * sr): o + int(1.5 * sr)]
        seg = bp(seg, 200, 5000, sr)
        L, R = seg[:, 0], seg[:, 1]
        corr = np.corrcoef(L, R)[0, 1]
        bal = []
        for i in range(0, len(L) - n, n):
            el, er = np.sum(L[i:i+n]**2) + 1e-20, np.sum(R[i:i+n]**2) + 1e-20
            bal.append(10 * np.log10(el / er))
        bal = np.array(bal)
        side = np.sign(bal) * (np.abs(bal) > 3)
        nz = side[side != 0]
        flips = np.sum(nz[1:] != nz[:-1]) / 1.45
        res.append((corr, np.median(np.abs(bal)), np.percentile(np.abs(bal), 90), flips))
    return np.median(np.array(res), axis=0)
for k, (c, _) in SRC.items():
    r = stereo(c)
    print(f"{k:<14}", "mono" if r is None else f"corr {r[0]:5.2f}  |balance| median {r[1]:4.1f} dB, 90% {r[2]:4.1f} dB, side-flips/s {r[3]:5.1f}")
for extra in ["wellspring_A-L_clicks_left.wav", "wellspring_A-R_clicks_right.wav"]:
    r = stereo(f"{REF}/{extra}")
    print(f"{extra[:24]:<14}", None if r is None else f"corr {r[0]:5.2f}  |balance| median {r[1]:4.1f}, 90% {r[2]:4.1f}, flips/s {r[3]:5.1f}")
irs = [stereo_ir for stereo_ir in []]

# ---- 3. Mid 'splosh' decay after each skank chord
print("\n3. Skank: 500 Hz-2 kHz level after each chord, dB re its peak (median over chords); and 125-500 Hz")
son = onsets(sx, sr0, -25, 0.2)
def skank_decay(path, lo, hi):
    y, sr = load(path); m = bp(y.mean(axis=1), lo, hi, sr) ** 2
    k = int(0.01 * sr); env = np.convolve(m, np.ones(k) / k, mode="same")
    rows = []
    for i, o in enumerate(son[:-1]):
        nxt = son[i + 1]
        w = env[o: nxt]
        if len(w) < int(0.3 * sr): continue
        p = w[: int(0.1 * sr)].max() + 1e-20
        rows.append([10 * np.log10(w[min(int(t * sr), len(w) - 1)] / p + 1e-20) for t in (0.1, 0.2, 0.3)])
    return np.median(np.array(rows), axis=0), len(rows)
for k, (_, s) in SRC.items():
    (mid, n), (low, _) = skank_decay(s, 500, 2000), skank_decay(s, 125, 500)
    print(f"{k:<14} mids @100/200/300 ms {np.round(mid,1)}   low-mids {np.round(low,1)}  ({n} chords)")
for v in ["A_today", "C_low_cut_spring_mids"]:
    p = f"{EQ}/clean/04_skank_{v}.wav"
    (mid, n), _ = skank_decay(p, 500, 2000), None
    print(f"{'EQ '+v:<24} mids {np.round(mid,1)}")
