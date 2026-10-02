"""Wellspring-fit metrics: the numbers the fit compares with the Wellspring recording.

All measured on the click take (01_clicks: six 2-sample pulses, 8 s apart, so each response is the unit's
impulse response), mono (L+R)/2 unless noted, median over the six clicks.

  arrival   first-echo group delay, 1/6 octave 500 Hz-6.3 kHz: energy-weighted arrival time of each band
            (zero-phase band-pass, squared) inside the first-echo window, ms after the 400-600 Hz band's.
            Window: from 3 ms before the 400-600 Hz band first comes within 20 dB of its peak, 25 ms long
            (the Wellspring's two first arcs, 31 and 41 ms, fit; its highs-only arcs at ~63 ms do not).
  arrival_pd  the same curve from the windowed phase derivative (tau = Re(FFT(t x) FFT(x)*) / |FFT(x)|^2,
            energy-weighted over each band): a check that the two methods agree.
  rise      echo attack rise time, 1-4 kHz, 10->90 % of each echo's envelope peak, 30-300 ms (ms)
  tone      late-tail tone 0.5-1.5 s, 1/3 octave 250 Hz-6.3 kHz, dB re the 500 Hz-1 kHz bands
  peaky     late-tail fine-spectrum peakiness (std dB of 1/48 re 1/3 octave smoothing, 200 Hz-4 kHz, 0.3-1.5 s)
  t60       per-octave T60 (s), 250 Hz-4 kHz, Schroeder backward integral, -5..-25 dB fit
  wobble    tail wavers: rms dB of the 20 ms envelope around a straight decay, 0.15-1.5 s (4 bands)
  width     late tail 0.2-1.5 s, 250-500 Hz: side re mid (dB) and L/R correlation (stereo)
  contrast  echo envelope p90/p10 (dB), 200 Hz-5 kHz, 30-400 ms
  resp      1/3-octave response re 500 Hz-2 kHz, 50 Hz-3.15 kHz (for the rms error vs W, any stimulus)
  hi_period the 2-5 kHz envelope's strongest autocorrelation lag 20-150 ms (the highs' own echo spacing)
"""
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt, find_peaks
from scipy.signal.windows import tukey

ROOT = "/Users/jesse/Documents/Sites/resilio-versio"
STIM = f"{ROOT}/test_audio/stimulus"
REF = f"{ROOT}/test_audio/reference"
SR = 48000
ARR_F = [500 * 2 ** (k / 6) for k in range(0, 23)]          # 500 Hz .. 6.35 kHz
THIRD = [250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300]
OCT = [250, 500, 1000, 2000, 4000]
RESP_C = [31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
          1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000]
RESP_FIT = np.array([50 <= c <= 3150 for c in RESP_C])
RESP_MID = np.array([500 <= c <= 2000 for c in RESP_C])


def load(p):
    x, sr = sf.read(p, always_2d=True)
    assert sr == SR, p
    return x.astype(np.float64)


def onsets(x, db=-40, gap=1.0):
    a = np.abs(x).max(axis=1); th = a.max() * 10 ** (db / 20); out, last = [], -10 ** 9
    for i in np.flatnonzero(a > th):
        if i - last > gap * SR: out.append(i)
        last = i
    return out


CLICKS = onsets(load(f"{STIM}/01_clicks.wav"))


def bp(y, lo, hi, order=4):
    return sosfiltfilt(butter(order, [lo, min(hi, .45 * SR)], btype="band", fs=SR, output="sos"), y, axis=0)


def _first_window(m, o, wlen=0.025):
    e = bp(m[o:o + int(.15 * SR)], 400, 600, 3) ** 2
    n = int(.001 * SR); e = np.convolve(e, np.ones(n) / n, "same")
    t0 = int(np.argmax(e > e.max() * 0.01))            # within 20 dB of the band's peak
    a = max(0, t0 - int(.003 * SR))
    return o + a, o + a + int(wlen * SR)


def arrival(m, clicks=CLICKS, wlen=0.025):
    """Energy-weighted arrival per 1/6-octave band re 400-600 Hz (ms), and the phase-derivative version."""
    rows, rows_pd, levels = [], [], []
    for o in clicks:
        a, b = _first_window(m, o, wlen)
        pad = int(.02 * SR)
        seg = m[a - pad:b + pad]
        t = (np.arange(b - a)) / SR

        def centroid(lo, hi):
            e = bp(seg, lo, hi, 3)[pad:-pad] ** 2
            return np.sum(t * e) / np.sum(e), np.sum(e)
        ref, _ = centroid(400, 600)
        r, lv = [], []
        for f in ARR_F:
            c, en = centroid(f * 2 ** (-1 / 12), f * 2 ** (1 / 12)); r.append((c - ref) * 1000); lv.append(en)
        rows.append(r); levels.append(10 * np.log10(np.array(lv) / max(lv)))
        # phase derivative on the same (tapered) window
        x = seg[pad:-pad] * tukey(b - a, 0.1)
        nfft = 1 << 16
        X = np.fft.rfft(x, nfft); Xt = np.fft.rfft(x * t, nfft); fq = np.fft.rfftfreq(nfft, 1 / SR)
        P = np.abs(X) ** 2; num = np.real(Xt * np.conj(X))

        def gd(lo, hi):
            s = (fq >= lo) & (fq < hi)
            return np.sum(num[s]) / np.sum(P[s])
        ref = gd(400, 600)
        rows_pd.append([(gd(f * 2 ** (-1 / 12), f * 2 ** (1 / 12)) - ref) * 1000 for f in ARR_F])
    return np.median(rows, 0), np.median(rows_pd, 0), np.median(levels, 0)


RIDGE_F = [1000 * 2 ** (k / 12) for k in range(0, 33)]   # 1 kHz .. 6.7 kHz, 1/12 octave


def ridge(m, clicks=CLICKS, wlen=0.035, rel=0.3, back=0.0008, ahead=0.003):
    """The first arc's shape: ms after its 1 kHz, per 1/12-octave band (RIDGE_F).

    The Wellspring's first echo is two arcs 10 ms apart (two springs), whose balance changes with frequency, so
    the energy-weighted arrival mixes them. This follows the first arc: band envelopes (zero-phase, 1/6 octave
    wide, 0.5 ms smoothing); the 1 kHz band's first peak (>= rel of its largest in the window) starts it (below
    ~1.3 kHz the Wellspring's arcs are vertical); each next band takes its local maximum nearest to the band
    below's time, from `back` before to `ahead` after it (arcs only rise). NaN where no such peak is within
    30 dB of the loudest band (the arc has ended); the track then carries on from the last time found."""
    rows = []
    n = int(.0005 * SR)
    for o in clicks:
        a, b = _first_window(m, o, wlen)
        pad = int(.02 * SR); seg = m[a - pad:b + pad]
        envs = [np.convolve(bp(seg, f * 2 ** (-1 / 12), f * 2 ** (1 / 12), 2) ** 2, np.ones(n) / n, "same")[pad:-pad] for f in RIDGE_F]
        top = max(e.max() for e in envs)
        e0 = envs[0]; pk, _ = find_peaks(e0, height=e0.max() * rel)
        if not len(pk): continue
        k = pk[0]; t = [k]
        for e in envs[1:]:
            pk, _ = find_peaks(e)
            pk = pk[(pk >= k - int(back * SR)) & (pk <= k + int(ahead * SR)) & (e[pk] >= top * 1e-3)]
            if not len(pk):
                t.append(np.nan); continue
            k = int(pk[np.argmin(np.abs(pk - k))]); t.append(k)
        rows.append((np.array(t, float) - t[0]) / SR * 1000)
    return np.nanmedian(rows, 0) if rows else np.full(len(RIDGE_F), np.nan)


def rise(m, clicks=CLICKS):
    b = np.abs(bp(m, 1000, 4000)); n = int(.001 * SR)
    env = np.convolve(b, np.ones(n) / n, "same"); out = []
    for o in clicks:
        e = env[o + int(.03 * SR):o + int(.3 * SR)]
        pk, _ = find_peaks(e, height=e.max() * .2, distance=int(.008 * SR))
        for p in pk:
            s = e[max(0, p - int(.01 * SR)):p + 1]; top = e[p]
            out.append((np.argmax(s > .9 * top) - np.argmax(s > .1 * top)) / SR * 1000)
    return float(np.median(out))


def tone(m, clicks=CLICKS, t0=.5, t1=1.5):
    v = []
    for c in THIRD:
        v.append(10 * np.log10(np.mean([np.mean(bp(m[o + int(t0 * SR) - 4800:o + int(t1 * SR) + 4800], c / 2 ** (1 / 6), c * 2 ** (1 / 6), 3)[4800:-4800] ** 2)
                                        for o in clicks]) + 1e-30))
    v = np.array(v)
    return v - v[[THIRD.index(c) for c in (500, 630, 800, 1000)]].mean()


def peaky(m, clicks=CLICKS):
    def one(seg):
        X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2; fq = np.fft.rfftfreq(len(seg), 1 / SR)
        cs = np.concatenate([[0], np.cumsum(X)])

        def sm(fr):
            a = np.searchsorted(fq, fq * 2 ** (-fr / 2)); b = np.maximum(np.searchsorted(fq, fq * 2 ** (fr / 2)), a + 1)
            return (cs[np.minimum(b, len(X))] - cs[a]) / (b - a)
        k = (fq > 200) & (fq < 4000)
        return (10 * np.log10(sm(1 / 48) + 1e-30) - 10 * np.log10(sm(1 / 3) + 1e-30))[k].std()
    return float(np.median([one(m[o + int(.3 * SR):o + int(1.5 * SR)]) for o in clicks]))


def t60(m, clicks=CLICKS):
    out = []
    for c in OCT:
        vals = []
        for o in clicks:
            e = bp(m[o:o + int(4 * SR)], c / np.sqrt(2), c * np.sqrt(2)) ** 2
            sch = np.cumsum(e[::-1])[::-1]; d = 10 * np.log10(sch / sch[0] + 1e-30)
            i5, i25 = np.argmax(d < -5), np.argmax(d < -25)
            if i25 <= i5: continue
            tt = np.arange(i5, i25) / SR; sl = np.polyfit(tt, d[i5:i25], 1)[0]
            vals.append(-60 / sl)
        out.append(float(np.median(vals)))
    return np.array(out)


def wobble(m, clicks=CLICKS):
    row = []
    for lo, hi in [(250, 500), (500, 1000), (1000, 2000), (2000, 4000)]:
        b = bp(m, lo, hi); res = []
        for o in clicks:
            seg = b[o + int(.15 * SR):o + int(1.5 * SR)] ** 2; n = int(.02 * SR)
            e = seg[:len(seg) // n * n].reshape(-1, n).mean(1)
            d = 10 * np.log10(e + 1e-20); t = np.arange(len(d)) * .02; ok = d > d.max() - 45
            fit = np.polyval(np.polyfit(t[ok], d[ok], 1), t[ok]); res.append(np.sqrt(np.mean((d[ok] - fit) ** 2)))
        row.append(float(np.median(res)))
    return np.array(row)


def width(y, clicks=CLICKS):
    b = bp(y, 250, 500)
    L = np.concatenate([b[o + int(.2 * SR):o + int(1.5 * SR), 0] for o in clicks])
    R = np.concatenate([b[o + int(.2 * SR):o + int(1.5 * SR), 1] for o in clicks])
    return np.array([10 * np.log10(np.sum((L - R) ** 2) / np.sum((L + R) ** 2)), np.corrcoef(L, R)[0, 1]])


def contrast(m, clicks=CLICKS):
    env = np.sqrt(np.convolve(bp(m, 200, 5000) ** 2, np.ones(96) / 96, "same"))
    return float(np.median([20 * np.log10(np.percentile(env[o + int(.03 * SR):o + int(.4 * SR)], 90)
                                          / np.percentile(env[o + int(.03 * SR):o + int(.4 * SR)], 10)) for o in clicks]))


def hi_period(m, clicks=CLICKS):
    env = np.sqrt(np.convolve(bp(m, 2000, 5000) ** 2, np.ones(48) / 48, "same")); ac = 0
    for o in clicks:
        e = env[o + int(.02 * SR):o + int(1.0 * SR)]; e = e - e.mean(); ac = ac + np.correlate(e, e, "full")[len(e) - 1:]
    ac = ac / ac[0]; lo, hi = int(.02 * SR), int(.15 * SR); k = lo + int(np.argmax(ac[lo:hi]))
    return k / SR * 1000, float(ac[k])


def resp(y, stim, stim_path=None):
    x = load(stim_path or f"{STIM}/{stim}.wav")[:, :1]

    def be(z):
        X = np.fft.rfft(z, axis=0); p = (np.abs(X) ** 2).sum(axis=1); f = np.fft.rfftfreq(z.shape[0], 1 / SR)
        return np.array([p[(f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))].sum() + 1e-30 for c in RESP_C])
    r = 10 * np.log10((be(y) / y.shape[1]) / be(x))
    return r - r[RESP_MID].mean()


def deconvolved_ir(y, stim, length=1.6, eps_db=-40):
    """Average impulse response of a recording of any stimulus (regularised deconvolution, mono)."""
    x = load(f"{STIM}/{stim}.wav")[:, 0]; m = y.mean(1)[:len(x)]
    n = 1 << int(np.ceil(np.log2(len(x) + int(length * SR))))
    X = np.fft.rfft(x, n); Y = np.fft.rfft(m, n)
    P = np.abs(X) ** 2; eps = P.max() * 10 ** (eps_db / 10)
    h = np.fft.irfft(Y * np.conj(X) / (P + eps), n)
    return h[:int(length * SR)]


def clicks_all(y):
    """Every clicks metric of one stereo render."""
    m = y.mean(1)
    arr, arr_pd, lev = arrival(m)
    return {"arrival": arr, "arrival_pd": arr_pd, "arrival_level_db": lev, "ridge": ridge(m), "rise": rise(m), "tone": tone(m),
            "peaky": peaky(m), "t60": t60(m), "wobble": wobble(m), "width": width(y), "contrast": contrast(m),
            "hi_period": hi_period(m), "resp_clicks": resp(y, "01_clicks")}
