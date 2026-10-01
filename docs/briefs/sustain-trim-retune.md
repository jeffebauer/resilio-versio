# Brief: retune the Sustain trim on top of bipolar WOBBLE (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md`, `CONTEXT.md`, ADR 0035 and the backlog section "Sustain trim" on branch `proto/sustain-trim`, then `docs/briefs/sustain-trim.md` (the first round's brief). The owner is a designer, new to DSP. Speak in plain language.

## Where things stand (1 Oct 2026)
- The owner picked **B (the trim) in every panel** of the first round's page, including their real pad. They also asked for the **limiter hold** (30 ms), which is now on `proto/sustain-trim` (`Tank.h` kLimitHoldS / kLimitHoldRefresh). ADR renumbered to **0035**.
- `main` now has **bipolar WOBBLE, voicing D** (ADR 0034, accepted). Merged with it, `test_sustain_trim` fails 2 checks:
  - **Held organ** (`12_organ_chord`, CLEAN, DRIVE 0, DECAY noon, SPRINGS 2, TONE 0, TENSION 1): the limiter pulls **1.49 dB** (limit < 0.5). It happens at the **onset**: the first 0.3 s is let through like a hit (kSusOnsetSeconds). Output peak per 0.5 s window, merged vs trim-only: −1.3 vs −2.3 dBFS on the first window, then both settle around −4 to −7. The organ already had only 0.2 dB of headroom there; WOBBLE's new modulation near noon (default 0.45) moved the onset ~1 dB.
  - **No pumping** on a held drone: the settled trim moves **2.87 dB** (limit 2).
- So the trim is right in principle but too finely balanced. Make it robust, not tuned to one WOBBLE.

## Setup
- Branch `proto/sustain-trim-2` from `proto/sustain-trim`, then merge `main` into it. Expected conflicts: `docs/TASKS.md` (take **main's**), `CONTEXT.md` (main's WOBBLE rows + the Sustain trim row), `docs/m8-tuning-backlog.md` (keep both sections), `host/tests/test_m7_tank.cpp` (take main's WOBBLE step check, and set `s.sustainOn = false` inside its `measure` lambda: WOBBLE's depth, not the trim's level).
- Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`. The tests need the stimulus WAVs: run `python3 tools/make_stimulus.py` and `python3 tools/make_sustain_stimulus.py` first (gitignored).
- **Owned files:** `core/params/DriveVoicing.h` "Sustain trim", `core/dsp/Tank.*` (trim and limiter only), `host/tests/test_sustain_trim.cpp`, ADR 0035, the backlog section "Sustain trim", SPEC changelog. Commit to `proto/sustain-trim-2` with explicit paths (never `git commit -a`) and push. Don't merge to `main`. Scratch ≤ ~10 GB. No recordings in the repo.

## Do
1. **Onsets of held sounds:** a held sound's first moments shouldn't reach the limiter at the owner's settings (input peak −6 dBFS), while hits and stabs stay untouched (exactly 0 dB trim: the owner judged hits "the same", keep it). Ideas: recognise "held" sooner when the input's level stops falling (a stab falls within ~100 ms; an organ or pad doesn't), or start a gentle pre-trim at the onset that is released for anything that turns out to be a hit. Explain the trade-off in musical words.
2. **Pumping:** the settled trim should move ≤ 2 dB on a held drone (it reads 2.87 now). Slower up-moves or a wider steady band (kSusSteadyDb) are candidates; check the pad still doesn't swell.
3. **Robust across WOBBLE:** run the test grid at WOBBLE 0, 0.25, 0.45 (default), 0.75 and 1, not only the default. Report the worst limiter pull and trim movement per WOBBLE.
4. Keep every other check in `test_sustain_trim` and the suite passing; don't loosen a limit without saying so plainly.
5. ADR 0035: **Accepted** (owner, 1 Oct 2026), with what changed in this round. SPEC changelog **v1.0.23** (next after main's v1.0.22): the Sustain trim and the limiter's 30 ms hold.

## Deliver
- `ctest` log summary line reading `100% tests passed` (never trust a piped exit code). The two failing checks' new numbers, and the per-WOBBLE table.
- A sweep JSON for a short confirm page (A = main, B = the retuned trim; organ, pad, drone, `02_hits`, `04_skank`; owner's settings, TENSION 0.8 and 1, TONE 0 and 0.3, SPRINGS 2 and 3) and the exact local render + page commands. Renders are made on the owner's Mac, where Claude adds the owner's real pad.
- Final message: what changed, in the owner's words.
