#!/usr/bin/env python3
"""Big Knob circuit study: a 3rd-order passive LC high-pass (series C,
shunt L, series C: a "T" section), designed for 600 ohm in and out, and what
happens to its response when it isn't loaded as designed.

Why: a passive LC filter only has its textbook (flat, Butterworth) shape
when the source and load are both its design impedance. Driven from a
low-impedance output and/or feeding a high-impedance (bridging) input, as on
a console insert, it peaks near the cutoff. This script measures that peak
(size in dB, where it sits, and the equivalent 1st-order + 2nd-order
section: fc, Q) for a few plausible loadings, and the effect of the coil's
winding resistance. docs/research/big-knob.md uses the numbers.

Usage: python3 tools/research/big_knob_circuit.py   (needs numpy)
"""
import math
import numpy as np

R0 = 600.0  # design impedance (ohm)
# Butterworth n = 3 low-pass prototype (g1, g2, g3) = (1, 2, 1). The LP->HP
# transform turns series L into series C = 1/(wc R0 g) and shunt C into shunt
# L = R0/(wc g): series C1, shunt L2, series C3.
G = (1.0, 2.0, 1.0)


def response(f, fc, rs, rl, rdc):
    """Vout / (Vs * rl/(rs+rl)) at frequencies f: 1 = the level a straight
    wire would give into the same load."""
    wc = 2 * math.pi * fc
    c1 = 1 / (wc * R0 * G[0])
    l2 = R0 / (wc * G[1])
    c3 = 1 / (wc * R0 * G[2])
    s = 2j * math.pi * np.asarray(f, dtype=float)
    z1 = 1 / (s * c1)
    y2 = 1 / (s * l2 + rdc)
    z3 = 1 / (s * c3)
    # ABCD of series z1, shunt y2, series z3
    a = 1 + z1 * y2
    b = z1 + z3 * (1 + z1 * y2)
    c = y2
    d = 1 + z3 * y2
    if math.isinf(rl):
        h = 1 / (a + rs * c)  # open-circuit load: Vout/Vs
        ref = 1.0
    else:
        h = rl / (a * rl + b + rs * (c * rl + d))
        ref = rl / (rs + rl)
    return h / ref


def poles(fc, rs, rl, rdc):
    """Fit the cubic denominator from samples and return (f1, f2, q):
    the real pole as a 1st-order corner, the complex pair as fc and Q."""
    wc = 2 * math.pi * fc
    c1 = 1 / (wc * R0 * G[0]); l2 = R0 / (wc * G[1]); c3 = 1 / (wc * R0 * G[2])
    rl_eff = 1e12 if math.isinf(rl) else rl
    # Node analysis (node A after C1, node B the output): the poles are the
    # roots of the admittance matrix's determinant, cleared of denominators.
    P = np.polynomial.Polynomial
    s = P([0, 1])
    # Branch admittances as rational with common factor handled by numerators:
    # yC1+rs: series rs + 1/(s c1)  -> admittance s c1 / (1 + s rs c1)
    # yL: 1/(s l2 + rdc); yC3: s c3; yRL: 1/rl
    nA1, dA1 = s * c1, 1 + s * rs * c1
    nL, dL = P([1]), rdc + s * l2
    nC3 = s * c3
    gl = 1 / rl_eff
    # Admittance matrix [[yA1 + yL + yC3, -yC3], [-yC3, yC3 + gl]]; det * dA1 * dL:
    det = ((nA1 * dL + nL * dA1) + nC3 * dA1 * dL) * (nC3 + gl) - nC3 * nC3 * dA1 * dL
    # Drop the numerical root at "infinity" (the open-load limit, rl_eff).
    r = [p for p in det.roots() if abs(p) < 1e3 * wc]
    real = [p for p in r if abs(p.imag) < 1e-6 * abs(p)]
    cplx = [p for p in r if p.imag > 1e-6 * abs(p)]
    out = {"f1": [abs(p.real) / (2 * math.pi) for p in real]}
    if cplx:
        p = cplx[0]
        w0 = abs(p)
        out["f2"] = w0 / (2 * math.pi)
        out["q"] = w0 / (-2 * p.real)
    return out


def describe(fc, rs, rl, rdc):
    f = np.geomspace(fc / 8, fc * 20, 4000)
    m = 20 * np.log10(np.abs(response(f, fc, rs, rl, rdc)))
    hf = 20 * np.log10(abs(response([fc * 200], fc, rs, rl, rdc)[0]))
    k = int(np.argmax(m))
    at = lambda x: 20 * np.log10(abs(response([x], fc, rs, rl, rdc)[0])) - hf
    # -3 dB point relative to the passband
    idx = np.where(m - hf >= -3.0)[0]
    f3 = f[idx[0]] if len(idx) else float("nan")
    # slope one octave well below cutoff
    slope = at(fc / 4) - at(fc / 8)
    return {
        "peak_db": m[k] - hf, "peak_at": f[k] / fc, "f3": f3 / fc, "pass_db": hf,
        # peak_at = fc * 20 (the end of the scan) means no peak: monotonic
        "slope": slope, "at_fc": at(fc), "at_half": at(fc / 2), **poles(fc, rs, rl, rdc),
    }


CASES = [
    ("as designed: 600 ohm in, 600 ohm out", 600.0, 600.0),
    ("600 ohm source, bridging 10k load", 600.0, 10e3),
    ("low-Z source (50 ohm), 600 ohm load", 50.0, 600.0),
    ("low-Z source (50 ohm), bridging 10k load", 50.0, 10e3),
    ("low-Z source (50 ohm), open (no load)", 50.0, math.inf),
]

if __name__ == "__main__":
    fc = 1000.0  # results scale with fc (shapes are the same at every step)
    for rdc_note, rdc_frac in (("ideal coil", 0.0), ("coil resistance 5% of 600 ohm (30 ohm at 1 kHz design)", 0.05)):
        print(f"\n== {rdc_note} ==")
        print(f"{'case':44s} {'peak dB':>8s} {'peak at':>8s} {'-3dB at':>8s} {'@fc':>6s} {'@fc/2':>6s} {'slope':>6s} {'pass dB':>8s}  1st-order fc   2nd-order fc, Q")
        for name, rs, rl in CASES:
            # coil resistance scales with the coil (lower steps = bigger
            # coils); here a fixed fraction of the design impedance.
            d = describe(fc, rs, rl, rdc_frac * R0)
            f1s = ", ".join(f"{x / fc:.2f} fc" for x in d["f1"]) or "-"
            q = d.get("q"); f2 = d.get("f2")
            qs = f"{f2 / fc:.2f} fc, Q {q:.2f}" if q else "-"
            print(f"{name:44s} {d['peak_db']:8.2f} {d['peak_at']:7.2f}x {d['f3']:7.2f}x {d['at_fc']:6.1f} {d['at_half']:6.1f} {d['slope']:6.1f} {d['pass_db']:8.2f}  {f1s:12s} {qs}")
