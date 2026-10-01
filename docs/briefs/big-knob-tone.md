# Brief: Big Knob TONE experiment (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md`, `CONTEXT.md`, `docs/dub-spring-reference.md` §6B ("Inductor-voiced TONE, the Big Knob character") and §8 (Claude's notes: "Big Knob on TONE's right side"), ADR 0017 (TONE), SPEC §2.3.4 and §3's TONE row, and the TONE code: `core/params/DriveVoicing.h` (`toneLowCutHz`, the tilt) and where the Tank applies it before the Springs. The owner is a designer, new to DSP. Speak in plain language.

## The idea (owner, 30 Sep / 1 Oct 2026)
King Tubby's "Big Knob": the high-pass filter on his MCI desk (an inductor filter) that he swept to thin a sound out, telephone-like, before it hit the spring. On Resilio, **TONE's right side becomes that**: a steeper low cut with a nasal, "ringy" bump just above the cutoff, rising as you turn. Owner decisions already made: **no clicks or steps** (a smooth sweep, not the original's stepped switch); **explore the inductor's tonal character**. Left of noon stays today's warm, dark tilt; noon stays neutral. Open design question in the owner's list: "TONE fully right: thin and splashy enough, too thin, or should the low cut start earlier?" This experiment answers it by ear.

Today's right side: a gentle tilt plus a 2nd-order (12 dB/oct) high-pass before the Springs, 20 Hz at noon → ~105 Hz at 3 o'clock → 300 Hz fully right.

## Build: voicings on one page
Add a hidden, Renderer-only key `tone_voicing` (like `wobble_voicing` / `sustain_voicing`; the default stays **0 = today** until the owner picks; firmware and plugin use the default):
- **0 = today** (reference).
- **1 = steep:** ~18 dB/oct low cut on TONE's right half, reaching higher (towards ~1–1.5 kHz fully right for "telephone"; the Springs' useful band is ~200 Hz–4 kHz, so stay well below ~2 kHz). Smooth all the way.
- **2 = steep + bump:** as 1, plus a resonant peak just above the cutoff that grows as TONE turns right (the nasal, ringy part). Choose its size by measurement and say why; it must not make the tank ring.
- **3 = steep + bump + "ringier when driven":** as 2, plus level-dependent softening/saturation of the loud lows, coupled to DRIVE, folded into the existing DriveIn stage (no new nonlinear stage; see §8). Only if it fits the budgets below; otherwise say so and skip it.

Placement: before the Springs, where the tilt and today's low cut already sit (dub practice: thin what hits the tank). Replace today's 12 dB/oct right-side low cut in voicings 1–3 rather than stacking another filter on it.

## Guarantees to keep (ADR 0017, SPEC §2.3.4)
- The Chirp (the spring's "boing") stays audible at every TONE setting.
- Loudness within ±3 dB across the whole sweep (measure on hits, skank and held chords).
- TONE is never needed to fight Ringing, and the M6 grid still passes (a bump is exactly where a narrow peak could feed tank modes: run the grid at TONE 0.5 / 0.7 / 0.85 / 1).
- Hits and skank at TONE ≤ 0.5 are bit-for-bit unchanged in every voicing.
- The sustain trim (ADR 0035) still keeps held pads off the limiter at the owner's settings with the bump on.
- **Budgets:** firmware flash is tight (release 126.5 KB of 128 KB): keep the added code under ~1.5 KB (report the estimate from the desktop object size). CPU: a couple of filter stages on the mono pre-tank signal; report desktop ns/sample vs `main`.

## Setup and rules
- Branch `proto/big-knob-tone` from `main`. Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`; first `python3 tools/make_stimulus.py && python3 tools/make_sustain_stimulus.py` (gitignored WAVs).
- Owned files: `core/params/DriveVoicing.h` (TONE section), the Tank's pre-tank filter code, DriveIn only if voicing 3 needs it, `host/common/ParamsJson.*` (the hidden key), TONE tests, a new ADR (next free number, Proposed), the backlog (`docs/m8-tuning-backlog.md`, new section "Big Knob TONE"), sweep JSONs. Commit with explicit paths (never `git commit -a`), push, don't merge to `main`. Scratch ≤ ~10 GB. No recordings in the repo.
- `ctest`'s summary line must read `100% tests passed` (never trust a piped exit code). Don't loosen a limit without saying so plainly. `test_wobble` may fail on Linux containers on `main` too; it passes on the Mac.

## Deliver
- Sweep JSONs for a listening page: voicings 0 / 1 / 2 / 3 via `--set tone_voicing=N`, at TONE 0.5 / 0.7 / 0.85 / 1, on `02_hits` (snare/rim), `04_skank`, held chords and `01_clicks`, in CLEAN and KICKED (2 Springs, DECAY noon, TENSION noon, SPLASH 0.3, DRIVE 0.25, MIX 1); for voicing 3 also DRIVE 0.8. The exact local render + page commands (`build/rv_render --sweep … --out-dir renders/proto_big_knob … --set tone_voicing=N`, then `python3 tools/review/make_review.py …` with versions side by side).
- Per voicing: cutoff and bump size at each TONE step, loudness across the sweep, M6 result, sustain-trim check, CPU and flash estimates.
- Final message: what each voicing sounds like, in the owner's words, and what to listen for.
