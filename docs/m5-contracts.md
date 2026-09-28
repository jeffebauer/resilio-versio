# M5 brief: drive chain + TONE tilt

M5 = the Drive chain (SPEC §4.9) and TONE as a hero tilt control (§3 K1, ADR 0017), per ATTITUDE (§3 SW1, §4.5 loop-sat column). Criteria: SPEC §7 M5. Relevant ADRs: 0002 (Howl allowed only in KICKED, top ~10% of DECAY), 0003 (ATTITUDE Morphs the live tail: smooth, never stepped), 0010, 0014 (DRIVE onset curve), 0015 (DRIVE/TONE snappy smoothing), 0017, 0018 (Howl exits naturally).

Owner references for later listening (not needed to build): Wellspring take C (hot INPUT), Magneto MD1–MD3 (REC LVL green/amber/red).

## Scope (single stream, owns `core/**` + `host/tests/test_drive*.cpp`)

- **Tank-level stages** (CONTEXT: Tank-level stage):
  - DriveIn = input transducer (band-limit HPF ~80–150 Hz, LPF ~5 kHz, soft asymmetric saturator) → tape (pre-emphasis → soft saturator → de-emphasis, drive-dependent HF smear).
  - TONE tilt pre-tank.
  - DriveOut = output pickup (light soft-clip + band-limit).
- **Per Spring:** LoopSat inside the feedback loop. Off in CLEAN, gentle symmetric in DRIVEN, hard asymmetric in KICKED. Keeps feedback bounded; enables Howl in KICKED at the top ~10% of DECAY (ADR 0002, 0019): rough, moving, may lean to a pitch, never a steady sine.
- **ATTITUDE** sets which stages engage and how hard (§4.9 table). Changes Morph over ~20 ms+ (ADR 0003), never stepped.
- **DRIVE** curve per ADR 0014: clean-ish to ~25%, colour builds to ~85%, properly driven above. **Automatic gain compensation**: loudness within ±2 dB across DRIVE. Reverb clearly audible at DRIVE 0.
- **TONE:** CCW warm dub dark with boing still audible; noon neutral; CW splashy, never harsh (ADR 0017). Tilt pre-tank + Loop damping; loudness within ±3 dB across TONE. Loop gain must stay below target at every frequency at every TONE (AntiRes layer 1, ADR 0010).
- **Oversampling ×2 minimum** on nonlinear stages. Cheap polyphase halfband (IIR allpass-based preferred for low latency). Factor is one constant in `core/params/` so M3 profiling can change it. **Count the oversampler's latency inside the Loop** in the round-trip/g design (the spec review flagged this). Plugin latency stays 0, or is reported via `setLatencySamples` if a Tank-level stage adds latency (tell the lead: it's a plugin/ change).
- **CPU:** report estimated Daisy cycles/sample per ATTITUDE × SPRINGS at worst case (same method as M1/M4). The 3-Spring KICKED max-DRIVE max-BOING case is the SPEC §5 worst case (≤ 65%).
- Determinism, block-size independence, Plugin == Renderer bit-identical, Kick timing, all existing tests: must keep passing.

## Tests (`host/tests/test_drive.cpp`)

1. ATTITUDE levels within ±2 dB of each other at the same settings.
2. DRIVE sweep 0→1: loudness within ±2 dB; THD rising monotonically (CLEAN: only mild).
3. Reverb audible at DRIVE 0 with a 10 Vpp-equivalent input (define: 0 dBFS ≈ 10 Vpp; wet within 6 dB of dry at MIX noon).
4. Aliasing: 5–15 kHz sine sweep at max DRIVE / KICKED → alias products ≤ −60 dB relative to the fundamental.
5. TONE: chirp still present at full CCW (same chirp test as M1); energy > 10 kHz at full CW ≤ +6 dB vs noon; loudness within ±3 dB across TONE.
6. Loop magnitude response (per Spring, every ATTITUDE × TONE corner, small-signal): below 1 at every frequency outside the KICKED Howl zone.
7. ATTITUDE Morph mid-tail in all 6 directions: click-free (same click measure as test_tank).
8. Stability grid incl. ATTITUDE × DRIVE × DECAY × SPRINGS: finite, bounded; decays in CLEAN/DRIVEN at max DECAY (ADR 0001); KICKED Howl zone sustains, but stays below the limiter ceiling and exits ≥ 30 dB within ~3 s after DECAY is pulled out of the zone (ADR 0018).
9. Firmware: `make -C firmware size` still ≤ 128 KB.
