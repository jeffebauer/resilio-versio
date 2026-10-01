# Brief: a gentler Sustain trim that keeps held sounds alive (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md`, `CONTEXT.md`, ADR 0035 and the backlog sections "Sustain trim" and "Sustain trim round 2" on branch `proto/sustain-trim-2`, plus `docs/briefs/sustain-trim.md` and `docs/briefs/sustain-trim-retune.md`. The owner is a designer, new to DSP. Speak in plain language.

## What the owner heard (1 Oct 2026, page `renders/proto_sustain_trim2/`: A = `main`, B = round 2's trim)
- **B on hits, skank, the synthetic pad and drone** (every cell, one no-pick).
- **A on their real pad in every cell**, and **A on the organ** at TONE 0 and at TENSION 1 (B only at TONE 0.3 / TENSION 0.8).
- Their words: "For the most part sounding better, but in the last 'in' examples [the real pad], the limiter is causing an audible dip and swell in volume that dips down and back up. And for organ tone examples, it's feeling less alive due to the limiting. Is there a middle ground we can strike to keep things characterful?"
- Measured on the Mac: in B the real pad's output never passed −2.0 dBFS (the limiter's knee is −1.7), so the dip and swell they heard is **the trim moving**, not the limiter. Note: A (`main`) has **no limiter hold**; the 30 ms hold exists only on the trim branches.

## The middle ground to build
A **safety net**, not a level rider: let held sounds breathe and only stop the limiter from working hard.
- The limiter's 30 ms hold stays (owner asked for it; it removes the "driven" sound of light limiting).
- The trim engages only when the wet would otherwise push the limiter past ~1.5–2 dB of gain reduction. It aims just under the knee (around −2.5 dBFS peaks, not −5 / −7), cuts at most ~4 dB, comes down smoothly (no faster than ~0.2–0.3 s) and lets go slowly. It doesn't chase swells inside a wide band (±2–3 dB): small natural swells pass.
- Hits and stabs stay exactly untrimmed (keep `test_sustain_trim`'s checks).
- Tune the numbers by measurement; explain the trade-off in musical words.

## Build it as voicings on one page
Like WOBBLE's `wobble_voicing`, add a hidden, Renderer-only key `sustain_voicing` (default = the new gentle one; the firmware and plugin use the default): **0 = off** (limiter hold only), **1 = round 2** (today's branch), **2 = gentle** (new).

## Setup and rules
- Branch `proto/sustain-trim-3` from `origin/proto/sustain-trim-2` (it already has `main` merged). Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`; first `python3 tools/make_stimulus.py && python3 tools/make_sustain_stimulus.py`.
- Owned files: `core/params/DriveVoicing.h` "Sustain trim", `core/dsp/Tank.*` (trim and limiter only), `host/common/ParamsJson.*` + `host/render/main.cpp` (the hidden key), `host/tests/test_sustain_trim.cpp`, ADR 0035 (add "Round 3"), the backlog, sweep JSONs. Commit with explicit paths (never `git commit -a`), push, don't merge to `main`. Scratch ≤ ~10 GB. No recordings in the repo.
- Tests: re-target `test_sustain_trim` to the gentle voicing's promise. At the owner's settings (input −6 dBFS peak; CLEAN, DRIVE 0, SPLASH 0, DECAY noon; every SPRINGS × TONE 0 / 0.5 / 0.9 × TENSION 0.5 / 0.8 / 1; WOBBLE default and right of noon), the limiter pulls ≤ ~2 dB (the hold keeps that clean; the output LED's red is at 0.5 dB: report how often it would light), and the trim's movement over a held drone or pad is ≤ ~1 dB. Say plainly which limits changed and why. Report the left-of-noon cells too. `ctest`'s summary line must read `100% tests passed`.
- Note: `test_wobble` fails on Linux containers on `main` too ("each 0.1 step ≥ 1.15x"); it passes on the Mac. Don't chase it.

## Deliver
- Sweep JSONs for the page: the voicings via `sustain_voicing` 0 / 1 / 2, plus A = `main` rendered from a `main` build; stimulus organ, pad, drone, `02_hits`, `04_skank`; the owner's settings with SPRINGS 2 / 3 × TENSION 0.8 / 1 × TONE 0 / 0.3 (as round 2's page). The exact local render + page commands (Claude adds the owner's real pad on the Mac).
- Per voicing: worst limiter pull, how often ≥ 0.5 dB (LED red), trim movement over the hold, and the held level vs `main` on pad, organ and drone.
- Final message: what changed, in the owner's words.
