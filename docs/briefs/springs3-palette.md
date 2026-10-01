# Brief: a more distinct SPRINGS 3 position, several ideas on one page (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md`, `CONTEXT.md`, SPEC §3 (SPRINGS row) and §4, the ADRs on the Springs (ADR 0003 SPRINGS crossfade, 0027 per-Spring damping/decay spread, 0029 first echo; `grep -l -i spring docs/adr/*`), `core/dsp/Spring.*`, `core/dsp/Tank.*` and `core/params/SpringModes.h`. The owner is a designer, new to DSP. Speak in plain language.

## What the owner found (1 Oct 2026, playing the plugin and the flashed module)
"There isn't a very noticeable difference between 2 springs and 3 springs … I'm wondering how we could make the 3 spring option more distinct from the others, or even consider a different approach to that option to provide a broader sonic palette." Asked to hear every idea rather than judge from descriptions. Positions 1 and 2 stay as they are.

Why it's subtle today (Claude's reading): the three Springs are near copies; their rates differ only ~±13 % (`kSpringRate` 0.87 / 1 / 1.13 in WOBBLE; check the Spring lengths, damping and pickups too), so a third Spring mostly adds density.

## Build: SPRINGS 3 voicings on one page
A hidden, Renderer-only key `springs3_voicing` (like `wobble_voicing`; default **0 = today** until the owner picks; firmware and plugin use the default). It changes only what SPRINGS = 3 does:
- **0 = today** (reference).
- **1 = long, big tank:** Springs clearly longer than in positions 1–2 (a slower, deeper drip, a longer, lower boing, a darker and longer tail), like a big "long decay" tank.
- **2 = tanks in series:** the sound passes through one tank and then into another (a known dub trick): thicker, more washed, a doubled boing, a smoother tail. Build it from the Springs you already have (e.g. Spring A's output feeding B and C), not a fourth Spring.
- **3 = wide stereo spread:** three clearly different Springs (lengths and colours well apart) placed left / centre / right, so the drips bounce across the stereo field. Mono-safe: check the mono sum (no hollow, no cancellation; the existing `monoloss` / `notch` metrics).
- **4 = different tank type:** a contrasting character, e.g. a short, bright, metallic tank ("pan" / small reverb unit), or a dense, plate-like shimmer. Pick the one you can make most convincing, and say why.
Switching into and out of position 3 must stay click-free (ADR 0003's crossfade).

## Constraints
- **Memory:** the delay-line pool is nearly full (Tank ~29,000 of 30,000 floats at 48 kHz). A longer tank needs more delay memory: find room (e.g. reuse the third Spring's memory, smaller buffers where there's slack) or show what it would take to use the Daisy's SDRAM, with the cost. Report memory per voicing.
- **CPU:** 3 Springs is today's worst case (run 13: 66 % peak against a 70 % target, and WOBBLE and the sustain trim have added a little). Report desktop ns/sample per voicing vs today at 3 Springs; anything over today's 3-Spring cost needs a number and a plan.
- **Flash:** release firmware 126.5 KB of 128 KB: keep added code small (report the estimate).
- **Safety:** the M6 Ringing grid and the Howl checks pass at SPRINGS 3 in every voicing; the sustain trim (ADR 0035) still holds held pads at the owner's settings; hits and skank at SPRINGS 1 and 2 are bit-for-bit unchanged.

## Setup and rules
- Branch `proto/springs3-palette` from `main`. Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`; first `python3 tools/make_stimulus.py && python3 tools/make_sustain_stimulus.py` (gitignored WAVs).
- Owned files: `core/dsp/Spring.*`, `core/dsp/Tank.*` (Spring wiring for position 3 only), `core/params/SpringModes.h` (or a new `Springs3Voicing.h`), `host/common/ParamsJson.*` (the hidden key), Spring/Tank tests, a new ADR **0037** (Proposed; 0036 is reserved for a parallel Big Knob TONE session), the backlog (new section "SPRINGS 3 palette"), sweep JSONs. A parallel session works on SPLASH (`core/params/SplashVoicing.h`, `core/dsp/Splash.*`): don't edit those. Commit with explicit paths (never `git commit -a`), push, don't merge to `main`. Scratch ≤ ~10 GB. No recordings in the repo.
- `ctest`'s summary line must read `100% tests passed`. Don't loosen a limit without saying so plainly. `test_wobble` may fail on Linux containers; it passes on the Mac.

## Deliver
- Sweep JSONs for one page: SPRINGS 2 (today, for contrast) and SPRINGS 3 in voicings 0–4 (`--set springs3_voicing=N`), on `01_clicks`, `02_hits`, `04_skank`, a held chord and the Kick; CLEAN and KICKED; DECAY noon and 0.85; other knobs at defaults. The exact local render + page commands, versions side by side.
- Per voicing: what it sounds like in plain words, memory, CPU, flash, M6/Howl results, mono check.
