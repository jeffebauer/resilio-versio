"""Offline EQ preview: our matched renders with a tank-like low cut (+ spring midrange) vs Wellspring/Magneto."""
import os, shutil
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfilt, tf2sos

ROOT = "/Users/jesse/Documents/Sites/resilio-versio"
AB = f"{ROOT}/renders/references/wellspring/ab"
REF = f"{ROOT}/test_audio/reference"
OUT = f"{ROOT}/renders/eq_preview"
FS = 48000


def peaking(f0, gain_db, q):
    A = 10 ** (gain_db / 40); w = 2 * np.pi * f0 / FS; al = np.sin(w) / (2 * q)
    return tf2sos([1 + al * A, -2 * np.cos(w), 1 - al * A], [1 + al / A, -2 * np.cos(w), 1 - al / A])


HP = butter(2, 160, "highpass", fs=FS, output="sos")
MID = np.vstack([HP, peaking(2000, 8, 0.5)])

os.makedirs(OUT, exist_ok=True)
stims = {"02_hits": ("hits", "wellspring_B_hits.wav", "magneto_MB_hits.wav"),
         "04_skank": ("skank", "wellspring_E_skank.wav", "magneto_ME_skank.wav")}
for att in ["CLEAN", "DRIVEN"]:
    d = f"{OUT}/{att.lower()}"
    os.makedirs(d, exist_ok=True)
    for stim, (tag, w, m) in stims.items():
        x, sr = sf.read(f"{AB}/resilio_match_{att}_{tag}.wav", always_2d=True)
        assert sr == FS
        variants = {"A_today": x, "B_low_cut": sosfilt(HP, x, axis=0), "C_low_cut_spring_mids": sosfilt(MID, x, axis=0)}
        for k, y in variants.items():
            y = y / max(1.0, np.abs(y).max() / 0.98)
            sf.write(f"{d}/{stim}_{k}.wav", y.astype(np.float32), FS, subtype="FLOAT")
        shutil.copyfile(f"{REF}/{"wellspring_C_hits_hot.wav" if (att == "DRIVEN" and stim == "02_hits") else w}", f"{d}/{stim}_W_wellspring.wav")
        shutil.copyfile(f"{REF}/{m}", f"{d}/{stim}_M_magneto.wav")

open(f"{OUT}/README.txt", "w").write(
    "Low-end check: our reverb at the settings matched to your Wellspring (DECAY about 2 o'clock, TONE noon, "
    "2 Springs, MIX full wet), with an EQ applied afterwards to test the direction before changing the DSP.\n"
    "The measurements say real tanks (your Wellspring, the Magneto, 45 Ableton spring IRs) have 10-20 dB less "
    "below 160 Hz than ours, and their energy centres around 1-2 kHz where ours centres near 300 Hz.\n\n"
    "A today: our reverb as it is now.\n"
    "B low cut: a tank-like low cut (160 Hz, 12 dB per octave), like a real spring's driver coil.\n"
    "C low cut + spring mids: B plus a broad lift centred near 2 kHz, the honky spring midrange (closest fit to the real tanks).\n"
    "W wellspring: your Wellspring recording (hits = take B, skank = take E).\n"
    "M magneto: your Magneto's spring (hits = MB, skank = ME).\n\n"
    "Offline EQ only: this previews the balance, not the tail. Real changes would go in front of the springs.\n")
print("done", OUT)
