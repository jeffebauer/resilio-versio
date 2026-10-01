# Brief: SPLASH stronger at the top and less tied to DRIVE (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md`, `CONTEXT.md`, ADR 0032 (SPLASH comes from the hit) and 0033 (DRIVE is the INPUT, with its amendments), the backlog section "SPLASH/DRIVE build" in `docs/m8-tuning-backlog.md`, `core/params/SplashVoicing.h` and `core/dsp/Splash.*`. The owner is a designer, new to DSP. Speak in plain language.

## What the owner found (1 Oct 2026, plugin and flashed module, a range of material)
"The splash knob feels very subtle — although that could be a result of the material I'm feeding in." Asked what matters most, they picked: **stronger at the top** (turned up, SPLASH should be unmistakable on any hit) and **less tied to DRIVE** (SPLASH should work fully with DRIVE low). Not picked: reacting to soft, dark attacks; evenness across the knob (keep it even anyway).

Why it's subtle today (Claude's reading): SPLASH's Clang (a hit's highs fed harder into the Springs) and Bite (DRIVEN/KICKED) follow a detector after the INPUT gain, and above noon both grow with DRIVE (×1 at DRIVE 0.8, ×0.64 below noon, 1.28 at 1). So at low DRIVE, or with line-level sends, there's little to trigger. The owner often plays with DRIVE fully down.

## Build: voicings on one page
A hidden, Renderer-only key `splash_voicing` (default **0 = today** until the owner picks; firmware and plugin use the default):
- **0 = today** (reference).
- **1 = stronger top:** the top quarter of SPLASH clearly bigger (a big crash of the springs on any snare or rim), the rest of the knob unchanged.
- **2 = stronger top + independent of DRIVE:** as 1, and the splash's strength and sensitivity no longer depend on DRIVE: DRIVE 0 and DRIVE 0.8 give about the same amount of splash for the same hit (DRIVE still adds its grit and level). Keep the guard that quiet ghost notes in a groove don't splash (≤ −51 dB of a backbeat's envelope, `test_m7_tank`).
- **3 = 2, voiced more dramatic:** the same, with the Clang's character pushed further (more of the springs' metallic ring, a longer splash): a bolder option to bracket the pick.

## Keep
- The Kick's crash unchanged. SPLASH 0 bit-for-bit unchanged. No Ringing (M6 grid at SPLASH 1), Howl checks pass, the output limiter not hammered by a big splash at MIX 1 (report peaks), the sustain trim still never engages on hits (exactly 0 dB).
- **Budgets:** flash is tight (release 126.5 KB of 128 KB): keep added code small. CPU: report desktop ns/sample vs `main` (SPLASH cost ~2.3 points on the chip today).

## Setup and rules
- Branch `proto/splash-stronger` from `main`. Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`; first `python3 tools/make_stimulus.py && python3 tools/make_sustain_stimulus.py` (gitignored WAVs).
- Owned files: `core/params/SplashVoicing.h`, `core/dsp/Splash.*`, the DRIVE-coupling of the splash in `core/params/DriveVoicing.h` only, `host/common/ParamsJson.*` (the hidden key), SPLASH tests, ADR 0032 amendment (Proposed), the backlog (new section "SPLASH stronger"), sweep JSONs. A parallel session works on SPRINGS 3 (`core/dsp/Spring.*`, the Tank's Spring wiring): don't edit those. Commit with explicit paths (never `git commit -a`), push, don't merge to `main`. Scratch ≤ ~10 GB. No recordings in the repo.
- `ctest`'s summary line must read `100% tests passed`. Don't loosen a limit without saying so plainly. `test_wobble` may fail on Linux containers; it passes on the Mac.

## Deliver
- Sweep JSONs for one page: voicings 0–3 (`--set splash_voicing=N`) at SPLASH 0.25 / 0.5 / 0.75 / 1, at DRIVE 0 and 0.8, on `02_hits` (snare/rim), `04_skank` and `01_clicks`; CLEAN and KICKED; 2 Springs, DECAY noon, MIX 1. The exact local render + page commands, versions side by side.
- Per voicing: splash level per SPLASH step at DRIVE 0 and 0.8 (dB, as the backlog measures it), peaks, CPU and flash.
