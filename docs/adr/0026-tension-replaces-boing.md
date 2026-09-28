# 0026 — TENSION replaces BOING; DECAY becomes tail length only

**Status:** Accepted, 29 Sep 2026. Supersedes ADR 0012 (DECAY bends pitch) and changes ADRs 0006 and 0007 as noted below. Implementation staged (see "Stages").

**Decision:** Knob K2 becomes **TENSION**, "which tank". Echo spacing (Loop delay L), chirp size (allpass `a`, stage count M) and brightness (transition frequency fC) move together from **tight** (short tank, small chirp, quick repeats, brighter) to **loose** (long tank, big chirp, slow repeats, darker), anchored to the IR library (ADR 0021). **DECAY sets tail length (T60) only.** It no longer changes the tank's size or bends pitch; turning TENSION mid-tail does, like stretching the tank.

**Why:** Owner, after the prototype (`renders/tension_proto/`, branch `proto/tension`): "I definitely prefer this tension knob over boing." Real tanks have no BOING and usually no decay control; their character comes from which tank is fitted. It gives each knob one job, and removes BOING's side effect on decay length (M8 backlog item 1).

**Prototype mapping** (highs-later chirp, ADR 0024), measured:

| TENSION | L | fC | a | M | repeat | chirp | nearest real tanks |
|---|---|---|---|---|---|---|---|
| 0 tight | 33 ms | 4.6 kHz | 0.40 | 24 | 33 ms | 5 ms | Space Echo spring |
| 0.5 | 69 ms | 3.3 kHz | 0.47 | 40 | 69 ms | 15 ms | IR-library median |
| 1 loose | 110 ms | 2.7 kHz | 0.55 | 64 | 110 ms | 37 ms | Swissecho (length), SNRA500 (chirp) |

These are identical at every DECAY. T60 stays 0.4–9 s (ADR 0001).

**Changes to earlier ADRs:**
- **0006** (shortest DECAY is a tight slap): the T60 range holds. The *tight slap* now also needs TENSION low (short tank). DECAY alone no longer shrinks the tank.
- **0007** (BOING always a spring): now applies to TENSION's tight end (24-stage floor, chirp still present).
- **0012** (turning DECAY bends pitch): **superseded.** The bend moves to TENSION.

**Trade-off (accepted):** no "short tank with a huge chirp" or "long tank with a tiny chirp". A few real tanks sit there (SNRA500, Farfi, Swissecho).

## Stages
Run after the highs-later switch-on (ADR 0024) and CLEAN's splash (ADR 0025) have landed, since they touch the same files.
1. **Port** the `proto/tension` mapping onto `main` (rebase onto the highs-later retune). Rename the parameter: ParamSpec `boing` → `tension`, name "TENSION"; update presets/sweeps using `boing`; the Plugin follows ParamSpec automatically. Firmware knob K2 keeps its slot.
2. **Fix what the prototype broke:** test_spring (mapping and chirp checks assume a DECAY-sized tank), test_drive (TONE-0 chirp timing), test_clicks (the chord scan no longer reaches the limiter), and two **real tuning jobs**: test_tank's mono notch −7 to −10 dB at DECAY 0 × TENSION 1 (limit −4.5), and test_kick's low end at KICKED DECAY 1 (18.7 dB against 20 in 300 ms).
3. **Re-verify:** M6 ringing grid, sweet-spot sweeps for TENSION and DECAY (no dead zones, no cliffs), CPU and flash (expected ~unchanged; ~10 % more delay memory), round-1 results (no flam, SPLASH, DRIVE, wet level).
4. **Docs:** SPEC §3 (K2 TENSION), §4.4 (DECAY no longer couples size), §7 criteria mentioning BOING, CONTEXT.md (Tension; Boing becomes the name of the chirp only), panel template labels, SPEC changelog.
5. **Owner:** install the build (`tools/install_plugin.sh`), add a listening task to `docs/TASKS.md`.
