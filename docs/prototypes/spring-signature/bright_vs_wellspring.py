"""F 'bright tail' (diffuse-tank prototype) vs the Wellspring: stereo width, per-repeat pitch envelope, 'sweetness'.

Run with a venv that has numpy, scipy, soundfile. Reads renders/proto_diffuse_tank/ (gitignored).
"""
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt, hilbert, stft

ROOT = str(__import__("pathlib").Path(__file__).resolve().parents[3])  # the repo root; run from the main checkout
P = f"{ROOT}/renders/proto_diffuse_tank"
STIM = f"{ROOT}/test_audio/stimulus"
VERS = {"A today": "A_today", "F bright": "F_bright_tail", "W wellspring": "W_wellspring"}
OCT = [125, 250, 500, 1000, 2000, 4000, 8000]


def load(p):
    x, sr = sf.read(p, always_2d=True)
    return x.astype(np.float64), sr


def bp(x, lo, hi, sr, order=4):
    return sosfiltfilt(butter(order, [lo, min(hi, 0.45 * sr)], btype="band", fs=sr, output="sos"), x, axis=0)


def onsets(x, sr, db=-40, gap=1.0):
    a = np.abs(x).max(axis=1); th = a.max() * 10 ** (db / 20); out, last = [], -10 ** 9
    for i in np.flatnonzero(a > th):
        if i - last > gap * sr: out.append(i)
        last = i
    return out


cx, SR = load(f"{STIM}/01_clicks.wav")
CL = onsets(cx, SR)


def f(att, stim, v):
    return f"{P}/{att}/{stim}_{VERS[v]}.wav"


# ---------------- 1. stereo width ----------------
print("1. STEREO, click tails 50 ms-1.5 s: per octave band corr (L,R) and side/mid energy (dB); 0 dB S/M = as much difference as common")
print(f"{'':16}" + "".join(f"{c:>12}" for c in OCT))
for v in VERS:
    y, _ = load(f("clean", "01_clicks", v))
    cs, sm = [], []
    for c in OCT:
        b = bp(y, c / np.sqrt(2), c * np.sqrt(2), SR)
        L = np.concatenate([b[o + int(.05 * SR): o + int(1.5 * SR), 0] for o in CL])
        R = np.concatenate([b[o + int(.05 * SR): o + int(1.5 * SR), 1] for o in CL])
        cs.append(np.corrcoef(L, R)[0, 1]); sm.append(10 * np.log10(np.sum((L - R) ** 2) / np.sum((L + R) ** 2)))
    print(f"{v:16}" + "".join(f"{a:6.2f}/{b:5.1f}" for a, b in zip(cs, sm)))
print("   same, over time (all bands 200 Hz-5 kHz): S/M dB in windows after each click")
wins = [(0, .03), (.03, .08), (.08, .2), (.2, .5), (.5, 1.5)]
print(f"{'':16}" + "".join(f"{f'{a*1000:.0f}-{b*1000:.0f}ms':>12}" for a, b in wins))
for v in VERS:
    y, _ = load(f("clean", "01_clicks", v)); b = bp(y, 200, 5000, SR)
    row = []
    for a, bb in wins:
        L = np.concatenate([b[o + int(a * SR): o + int(bb * SR), 0] for o in CL]); R = np.concatenate([b[o + int(a * SR): o + int(bb * SR), 1] for o in CL])
        row.append(10 * np.log10(np.sum((L - R) ** 2) / np.sum((L + R) ** 2)))
    print(f"{v:16}" + "".join(f"{r:12.1f}" for r in row))
print("   program material, whole file: corr / S/M dB (200 Hz-5 kHz)")
for stim in ["02_hits", "04_skank"]:
    for v in VERS:
        y, _ = load(f("clean", stim, v)); b = bp(y, 200, 5000, SR)
        print(f"   {stim:9} {v:14} corr {np.corrcoef(b[:, 0], b[:, 1])[0, 1]:5.2f}  S/M {10*np.log10(np.sum((b[:,0]-b[:,1])**2)/np.sum((b[:,0]+b[:,1])**2)):5.1f} dB")

# ---------------- 2. per-repeat pitch envelope ----------------
print("\n2. REPEATS (clean clicks, mono): echo period, and within each echo the energy-weighted frequency (centroid, 150 Hz-6 kHz)")
print("   at 0-4 / 4-8 / 8-16 / 16-32 ms after the echo's start; 'sweep' = max-min in octaves; 'arr' = when each octave band peaks (ms after echo start)")
for v in VERS:
    y, _ = load(f("clean", "01_clicks", v)); m = y.mean(axis=1)
    # period from envelope autocorrelation, averaged over clicks
    env = np.sqrt(np.convolve(m ** 2, np.ones(48) / 48, "same"))
    ac = 0
    for o in CL:
        e = env[o + int(.02 * SR): o + int(1.0 * SR)]; e = e - e.mean()
        ac = ac + np.correlate(e, e, "full")[len(e) - 1:]
    lo, hi = int(.02 * SR), int(.2 * SR)
    per = (lo + int(np.argmax(ac[lo:hi]))) / SR
    # first echo: first envelope maximum after 10 ms, averaged
    fr, tt, Z = stft(m, SR, nperseg=256, noverlap=256 - 48)  # 1 ms hop, 5.3 ms window
    Pz = np.abs(Z) ** 2
    band = (fr >= 150) & (fr <= 6000)
    rows = []
    for o in CL:
        e0 = env[o + int(.008 * SR): o + int(.008 * SR) + int(per * SR)]
        t1 = o + int(.008 * SR) + int(np.argmax(e0))
        for k in range(4):
            s = t1 + int(k * per * SR) - int(0.004 * SR)
            fi = lambda t: int(round(t / 48))
            cents, arr = [], []
            for a, b in [(0, .004), (.004, .008), (.008, .016), (.016, .032)]:
                i0, i1 = fi(s + a * SR), fi(s + b * SR)
                p = Pz[band][:, i0:i1].sum(axis=1); cents.append(np.sum(fr[band] * p) / np.sum(p))
            for c in [250, 500, 1000, 2000, 4000]:
                bb = (fr >= c / np.sqrt(2)) & (fr < c * np.sqrt(2))
                i0, i1 = fi(s), fi(s + .045 * SR)
                arr.append(np.argmax(Pz[bb][:, i0:i1].sum(axis=0)))
            rows.append((k, cents, arr))
    print(f"   {v}: echo period {per*1000:.1f} ms")
    for k in range(4):
        cs = np.median(np.array([r[1] for r in rows if r[0] == k]), axis=0)
        ar = np.median(np.array([r[2] for r in rows if r[0] == k]), axis=0)
        sweep = np.log2(cs.max() / cs.min())
        print(f"     echo {k+1}: centroid Hz {np.round(cs).astype(int)}  sweep {sweep:4.2f} oct   arr ms 250/500/1k/2k/4k {ar.astype(int)}")

# ---------------- 3. sweetness ----------------
print("\n3. SWEETNESS")
def peakiness(seg):
    X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    fq = np.fft.rfftfreq(len(seg), 1 / SR)
    def smooth(frac):
        out = np.empty_like(X)
        cs = np.concatenate([[0], np.cumsum(X)])
        for i, fz in enumerate(fq):
            if fz <= 0: out[i] = X[i]; continue
            a, b = np.searchsorted(fq, fz * 2 ** (-frac / 2)), np.searchsorted(fq, fz * 2 ** (frac / 2))
            out[i] = (cs[max(b, a + 1)] - cs[a]) / max(1, b - a)
        return out
    fine, coarse = 10 * np.log10(smooth(1 / 48) + 1e-30), 10 * np.log10(smooth(1 / 3) + 1e-30)
    m = (fq > 200) & (fq < 4000)
    d = fine[m] - coarse[m]
    return d.std(), np.percentile(d, 99)
def rough(seg):
    out = []
    for lo, hi in [(500, 1000), (1000, 2000), (2000, 4000)]:
        e = np.abs(hilbert(bp(seg, lo, hi, SR, 3)))
        E = np.abs(np.fft.rfft(e - e.mean())) ** 2; fq = np.fft.rfftfreq(len(e), 1 / SR)
        out.append(10 * np.log10(E[(fq >= 20) & (fq < 150)].sum() / E[(fq > 0.5) & (fq < 20)].sum()))
    return out
for v in VERS:
    y, _ = load(f("clean", "01_clicks", v)); m = y.mean(axis=1)
    pk = np.array([peakiness(m[o + int(.3 * SR): o + int(1.5 * SR)]) for o in CL])
    ro = np.array([rough(m[o + int(.15 * SR): o + int(1.0 * SR)]) for o in CL])
    # echo contrast: in 30-400 ms, how far the envelope (2 ms) swings: p90/p10 dB
    env = np.sqrt(np.convolve(bp(m, 200, 5000, SR) ** 2, np.ones(96) / 96, "same"))
    con = [20 * np.log10(np.percentile(env[o + int(.03 * SR): o + int(.4 * SR)], 90) / np.percentile(env[o + int(.03 * SR): o + int(.4 * SR)], 10)) for o in CL]
    con2 = [20 * np.log10(np.percentile(env[o + int(.4 * SR): o + int(1.2 * SR)], 90) / np.percentile(env[o + int(.4 * SR): o + int(1.2 * SR)], 10)) for o in CL]
    print(f"   {v:14} tail peakiness std {np.median(pk[:,0]):4.1f} dB, p99 {np.median(pk[:,1]):4.1f} dB | roughness (20-150 Hz flutter re slow) 0.5-1k/1-2k/2-4k {np.round(np.median(ro,axis=0),1)} dB | echo contrast 30-400 ms {np.median(con):4.1f} dB, 0.4-1.2 s {np.median(con2):4.1f} dB")
for stim in ["02_hits", "04_skank"]:
    for v in VERS:
        y, _ = load(f("clean", stim, v)); m = y.mean(axis=1)
        n = len(m) // 2
        pk = peakiness(m[n: n + int(1.2 * SR)])
        print(f"   {stim:9} {v:14} peakiness std {pk[0]:4.1f} dB, p99 {pk[1]:4.1f} dB (1.2 s from mid-file)")
