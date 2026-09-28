# TENSION prototype: "which tank is fitted" instead of BOING

29 Sep 2026. A sound sketch on branch `worktree-agent-a8f1e2d0d038291be`, not a decision. Background: docs/TASKS.md "Later" (TENSION), ADRs 0006, 0007, 0012, 0024, docs/ir-dispersion-study.md.

## The idea in one paragraph

A real spring tank has no BOING knob and usually no decay knob. What it sounds like depends on which tank is fitted: a short, tight tank repeats quickly with a small, bright chirp; a long, loose tank repeats slowly with a big, darker chirp. In the prototype, the second knob (today's BOING) becomes **TENSION**, which picks the tank. **DECAY** only sets how long the tank rings. It no longer changes the tank's size.

## What TENSION does

TENSION moves four things together, so every position sounds like one plausible tank:

| | Tight (TENSION 0) | Noon (0.5) | Loose (1) |
|---|---|---|---|
| **Echo spacing** (how often the boing comes back) | 33 ms: quick, pingy repeats | 69 ms | 110 ms: slow, spaced "boing … boing" |
| **Chirp size** (how far the highs lag behind the lows on each trip) | 5 ms: a small "tick" of a chirp, still clearly a spring | 15 ms | 37 ms: a big, obvious "bwoing" |
| **Brightness** (where the chirp stops, fC) | 4.8 kHz: brighter | 3.4 kHz | 2.8 kHz: darker |
| Under the hood: Loop delay L · allpass a · stages M | 33 ms · 0.40 · 24 | 69 ms · 0.47 · 40 | 110 ms · 0.55 · 64 |

The chirp runs highs-later (ADR 0024) in the prototype, as in every real tank.

### Measured, and where the real tanks sit

Click renders, Spring A alone (1 Spring), CLEAN, measured with `tools/ir_dispersion.py` the same way as the IR study. "Chirp" is highs later per trip, with the band capped at the model's fC (as in the study's "Tuned HighsLater" section). All five positions measure the same at DECAY 0, 0.5 and 1: repeat within 0.3 ms, chirp within 0.3 ms. That is the point of the change.

| TENSION | repeat ms | chirp ms | fC (model / measured) | real tanks nearby (repeat · chirp · fC) |
|---|---|---|---|---|
| 0 (tight) | 33.1 | 5.1 | 4.78 / 4.80 kHz | Space Echo Spring 42 · 3 · 3.6 k (the only short classic); nothing in the library is shorter. 33 ms is the dub slap (ADR 0006) |
| 0.25 | 47.8 | 8.8 | 4.05 / 4.04 kHz | Amp Spring Dull 54 · 7 · 2.0 k; Amp Spring Bright/High 53 · 14–15 · 2.6–2.8 k |
| 0.5 (noon) | 69.2 | 14.6 | 3.43 / 3.5–3.8 kHz | **library median: 69 · 15 · 3.1 k**; Amazing Stereo 66 · 15.5 · 3.6 k |
| 0.75 | 87.5 | 23.8 | 3.10 / 3.1–3.5 kHz | Farfi Dirtier Wider 73 · 25–30 · 4.3 k; Classic Amp Spring 87 (chirp not trackable) |
| 1 (loose) | 110.5 | 36.8 | 2.81 / 2.85–2.98 kHz | Swissecho 116 (length); SNRA500 58 · 31, Farfi Wide R 64 · 35 (chirp size) |

For comparison, today's DECAY × BOING grid (HEAD, highs-later on, same measurement): repeat 30–31 ms at DECAY 0, 55–56 ms at 0.5 and 99–101 ms at 1, whatever BOING does. Chirp 5–23 ms at DECAY 0, 6–30 ms at 0.5 and 8–36 ms at 1. T60 (target 0.40 / 1.90 / 9.0 s): prototype 0.42–0.49 / 1.92–1.99 / 8.98–9.17 s. The looser the tank, the slightly longer a short DECAY rings, because only ~4 trips fit in 0.4 s. Raw numbers: `renders/tension_proto/measure/results.json`.

The six reference tanks on the TENSION page, tight to loose: Space Echo Spring (42 ms), Amp Spring Bright (53), SNRA500 Plucky (58, big chirp), Amazing Stereo (66, the median), Farfi Dirtier Wider R (73, big chirp), Swissecho 1.5 s Wide (116).

## What changed about DECAY

- **DECAY is now feedback only:** tail length (T60 0.4 → 9 s, same curve as today, ADRs 0001/0006) and nothing else. The echo spacing, chirp and brightness stay where TENSION put them.
- **Turning DECAY mid-tail no longer bends the pitch.** Measured on a held 1 kHz tone, DECAY 0.3 → 0.9 over 3 s: pitch moves by **0.05 cents** (nothing). Today's DECAY does the same move with a bend of up to **77 cents** (p95 47).
- **Turning TENSION mid-tail bends the pitch instead**, like stretching the tank: TENSION 0.3 → 0.7 over 3 s bends by up to **89 cents** (p95 59), about as much as today's DECAY move. The bend stays smooth because TENSION keeps BOING's gliding smoothing (ADR 0015).
- The KICKED Howl zone still sits at the top of DECAY (it is about feedback, so it belongs there).

## What's lost, what's gained

**Gained**
- Each knob has one job: TENSION = which tank, DECAY = how long. Easier to learn, and closer to how real springs behave.
- Every TENSION position is a believable tank: small chirps go with short tanks and big chirps with long ones.
- DECAY is "set and forget". Riding it for a longer tail doesn't retune the echoes or bend a chord out of tune.
- It absorbs the M8 backlog item "BOING shortens decay". T60 no longer depends on the chirp knob.
- Pitch bends are still there, now on TENSION, which is where a "stretch the tank" move makes sense.
- New combinations: a **long tank with a short tail** (TENSION 1, DECAY 0: a few big, spaced boings, then gone) and a **short tank with a long tail** (TENSION 0, DECAY 1: a dense, pingy wash). Today, short tail meant short tank.

**Lost**
- **No "short tank with a huge chirp"** (today's DECAY 0 + BOING 1: 31 ms repeats with a 23 ms chirp), and no "long tank with a tiny chirp" (today's DECAY 1 + BOING 0). Some real tanks sit off TENSION's line too: SNRA500 and the Farfis have big chirps at medium length (58–73 ms, 25–35 ms), and the Swissechos are long with small chirps (116 ms, 2–5 ms). TENSION goes through the middle of the library, not its corners.
- Less direct control of the cartoon boing. A big boing now always comes with slow repeats.
- The tight slap from ADR 0006 now needs **both** knobs down (TENSION 0 + DECAY 0). DECAY 0 on a loose tank gives a few discrete echoes, not a slap.
- DECAY-only automation no longer bends pitch. Patches that relied on this (ADR 0012) will sound different.

## Listening guide

Open `/Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto/index.html` from Finder. It links all four pages and repeats the table above. Settings everywhere: MIX 1, SPRINGS 2, DRIVEN, DRIVE 0.3, SPLASH 0, WOBBLE 0, TONE noon.

1. **TENSION sweep:** `/Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto/tension_sweep/index.html`. TENSION 0 → 1 at DECAY 0.5 on a click, 02_hits, a rimshot and 08_held_tones, with the six real tanks as references. Listen for:
   - Does each column sound like *one tank* (not "a reverb with a chirp effect")?
   - Tight: quick pingy repeats and a small bright chirp. Is it still clearly a spring? (ADR 0007's floor.)
   - Loose: slow spaced boings with a big chirp. Too cartoonish, or a proper dub tank?
   - Compare the click column with the reference tanks. Which TENSION is nearest to the tanks you like?
2. **Today's BOING:** `/Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto/today_boing/index.html`. Same stimuli, today's BOING 0 → 1 at DECAY 0.5 (highs-later on). Today's knob only changes the chirp and keeps ~55 ms repeats. Do you miss "chirp without changing the spacing"?
3. **DECAY sweep:** `/Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto/decay_sweep/index.html`. DECAY 0 → 1 at TENSION 0.5. The echo spacing should stay identical and only the length should change. Is DECAY 0 on a noon tank still a usable "short" setting?
4. **Turning knobs mid-tail:** `/Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto/turning/index.html`. Rows: TENSION 0.3 → 0.7 (new), DECAY 0.3 → 0.9 (new), DECAY 0.3 → 0.9 (today). Columns: chord (knob moves 3.8–6.8 s, after the chord stops), one rimshot (knob 1.6–4.6 s), a held 1 kHz tone (knob 3.0–6.0 s, the tone keeps playing). The new TENSION and today's DECAY should both bend; the new DECAY should only bloom longer, with no bend.

The page flags the held tones as "steady tone" (they are steady by design) and a few DECAY 0 renders for their stereo (see "Tests" below).

## If adopted

- **ADRs:** a new ADR supersedes **0006** (the tight slap moves to TENSION 0 + DECAY 0), **0007** (BOING's floor becomes TENSION 0's floor: 24 stages, a = 0.40, a 5 ms chirp) and **0012** (DECAY no longer bends; TENSION does). 0015's smoothing tiers stay (TENSION keeps BOING's 60 ms gliding tier, DECAY keeps 80 ms). SPEC §3 (K2) and §4.4 (mappings), CONTEXT.md ("Chirp / Boing", DECAY) change too.
- **ParamSpec / panel:** K2 label `BOING` → `TENSION`. Rename the key `"boing"` → `"tension"` (breaks saved plugin state and presets; a one-line alias in ParamsJson would keep old presets loading), and `ParamId::Boing` → `ParamId::Tension`. Panel SVG legend (tools/make_panel_svg.py) and the firmware knob table (firmware/main.cpp) follow.
- **Code:** mapping only. `Mappings.h` gets the TENSION anchors (this branch), `Tank::controlTick` feeds L, fC, a and M from TENSION and only T60 from DECAY, and `SpringModes.h` gets `tensionStages`. The old `decayLoopDelaySeconds`, `decayTransitionHz`, `boingCoefficient` and `boingStages` can go once the tests move over. In the prototype they still exist (the tests use them) but they now span TENSION's new L range (33–110 ms) and fC range (4.6–2.7 kHz). `tools/ir_dispersion.py` `MAP` needs the TENSION anchors.
- **Tests that would change** (prototype run: 9 of 14 pass; baseline HEAD + highs-later fails test_drive too, see aliasing):
  - `test_spring`: the "Mappings" check (L 30–100 ms, K grows with DECAY) is now about TENSION. "Chirp BOING 0 (Tank)" times its bands with DECAY's old geometry; at the tight end the chirp is small and the lows' echo lands first in that window. Both need rewriting around TENSION.
  - `test_drive`: "TONE 0 chirp" (×3 ATTITUDEs) uses the same old windows (at BOING 0 = tight tank now). Aliasing, Tank wet DRIVEN DRIVE 1, 3 Springs: -51.4 dB against a -60 dB limit. This is **not caused by TENSION**: HEAD with highs-later switched on fails the same check at -54.4 dB (and KICKED at -52.8 dB, plus the KICKED DRIVE level spread, 2.21 against 2 dB, both of which pass in the prototype). It is one of the round-1 checks the highs-later switch-on is already retuning.
  - `test_tank` stereo grid: the corner **DECAY 0 × TENSION 1** (loose tank, 0.4 s tail, a combination that didn't exist before) has a mono notch of -7 to -10 dB against a -4.5 dB limit, with 2 and 3 Springs. With only ~4 trips, the near-aligned first echoes of A and B dominate. The DECAY sweep shows it milder at TENSION 0.5 (-5.7 dB on hits, -8.8 dB on held tones at DECAY 0). A real tuning job: the pickup offsets or decorrelator for long tanks with short tails.
  - `test_clicks`: "held chord scan drives the limiter" is a coverage check. The scan (DECAY 0.40–0.70) no longer pushes the wet into the limiter at noon TENSION (0 of 31 settings). The scan needs new settings; nothing clicks.
  - `test_kick`: KICKED DECAY 1, Kick < 100 Hz energy drops 18.7 dB in 300 ms against ≥ 20 (a noon tank at 9 s T60 is now 69 ms instead of 100 ms, so more trips). Marginal; retune or relax.
  - `test_antires` passes, but its Spring-level grid still walks the old DECAY × BOING functions, so it needs re-parametrising over TENSION × DECAY to cover the new corners (loose + short, tight + long).
- **CPU / flash:** about none. The stage range is the same (24 … mode cap), so the worst case (3 Springs, 52 stages) doesn't change. The allpass rings don't change (fC min 2.7 kHz, same as today). Delay memory grows ~10 % (L max 100 → 110 ms): a few KB per Spring in SDRAM. Flash: a couple of extra inline functions per control tick.

## Reproduce

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
python3 tools/make_stimulus.py
python3 tools/tension_proto_render.py build/rv_render <HEAD+HighsLater rv_render> \
    /Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto \
    /Users/jesse/Documents/Sites/resilio-versio/renders/m8_round1/dynamics_colour/stim/rimshot_m9.wav \
    /Users/jesse/Documents/Sites/resilio-versio/renders/ir_library
python3 tools/tension_proto_measure.py /Users/jesse/Documents/Sites/resilio-versio/renders/tension_proto --json .../measure/results.json
python3 tools/review/make_review.py <page dir> [--reference .../tanks]
```

The "today" renderer was HEAD 9b10fc2 built from a detached scratch worktree with `kChirpDirection = HighsLater`.
