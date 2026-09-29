"""Fit a tank-like EQ (high-pass + presence peak) that moves our response onto real springs."""
import itertools
import numpy as np
from scipy.signal import butter, sosfreqz, tf2sos
import importlib.util, sys

spec = importlib.util.spec_from_file_location("sig", sys.argv[1])
FS = 48000
C = np.array([31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
              1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000])
# measured (from sig1.txt), dB re 500-2k mean
W = np.array([-32.9,-32.4,-30.2,-23.1,-19.4,-15.2,-10.1,-8.1,-5.9,-4.0,-3.2,-2.1,-1.5,-1.1,-0.3,0.6,1.2,1.7,-0.7,-5.5,-11.0,-17.3,-26.5,-34.1,-35.5,-35.9,-35.9,-34.9])
M = np.array([-38.6,-40.4,-35.3,-28.1,-28.0,-23.3,-18.9,-14.6,-11.1,-8.0,-4.3,-2.4,-0.9,-0.8,-0.1,0.1,1.2,1.0,-0.5,-2.3,-3.8,-7.2,-11.4,-17.5,-35.8,-51.4,-50.3,-47.6])
IR = np.array([-24.9,-21.6,-18.6,-15.7,-13.1,-12.5,-11.0,-7.7,-6.2,-5.2,-4.0,-3.2,-1.8,-1.0,-0.5,0.5,0.8,1.4,1.6,1.2,0.7,-1.2,-1.8,-3.5,-10.4,-11.6,-15.2,-18.7])
OURS = np.array([-16.9,-11.8,-6.7,-3.8,-1.3,-1.1,0.2,1.9,0.8,1.9,2.0,1.7,2.0,1.7,1.2,1.0,0.0,-2.2,-3.6,-6.0,-9.3,-12.3,-15.3,-18.2,-21.0,-25.3,-33.8,-52.8])
TARGET = (W + M + IR) / 3
FIT = (C >= 50) & (C <= 3150)  # where the question is (lows + body); tops handled separately
MIDS = (C >= 500) & (C <= 2000)


def peaking(f0, gain_db, q):
    A = 10 ** (gain_db / 40); w = 2 * np.pi * f0 / FS; al = np.sin(w) / (2 * q)
    b = [1 + al * A, -2 * np.cos(w), 1 - al * A]; a = [1 + al / A, -2 * np.cos(w), 1 - al / A]
    return tf2sos(b, a)


def eq_sos(hp, order, pk_f, pk_g, pk_q):
    s = [butter(order, hp, "highpass", fs=FS, output="sos")]
    if pk_g:
        s.append(peaking(pk_f, pk_g, pk_q))
    return np.vstack(s)


def eq_db(sos):
    _, h = sosfreqz(sos, worN=C, fs=FS)
    return 20 * np.log10(np.abs(h) + 1e-12)


def renorm(x):
    return x - x[MIDS].mean()


best = []
for hp, order, pf, pg, pq in itertools.product([100, 125, 140, 160, 180, 200, 225, 250], [2, 3, 4],
                                               [1000, 1250, 1400, 1600, 2000], [0, 2, 3, 4, 5, 6, 8], [0.5, 0.7, 1.0]):
    sos = eq_sos(hp, order, pf, pg, pq)
    r = renorm(OURS + eq_db(sos))
    err = np.sqrt(np.mean((r[FIT] - TARGET[FIT]) ** 2))
    best.append((err, hp, order, pf, pg, pq))
best.sort()
print("rms err before:", round(float(np.sqrt(np.mean((OURS[FIT] - TARGET[FIT]) ** 2))), 1))
for b in best[:8]:
    print([round(float(v), 2) if isinstance(v, float) else v for v in b])
# also the simplest: high-pass only
hp_only = sorted([x for x in best if x[4] == 0])[:3]
print("HP only:", hp_only)
e, hp, order, pf, pg, pq = best[0]
r = renorm(OURS + eq_db(eq_sos(hp, order, pf, pg, pq)))
print("\nband     " + "".join(f"{int(c) if c >= 100 else c:>7}" for c in C))
for name, v in [("W", W), ("M", M), ("IR", IR), ("target", TARGET), ("ours", OURS), ("ours+EQ", r)]:
    print(f"{name:<9}" + "".join(f"{x:7.1f}" for x in v))
