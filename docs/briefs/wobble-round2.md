# Brief: bipolar WOBBLE round 2 (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md` and `CONTEXT.md` first, then ADR 0034 and `core/params/WobbleVoicing.h` on branch `proto/bipolar-wobble`, and the backlog section "Bipolar WOBBLE" there. The owner is a designer, new to DSP. Speak in plain language.

## Setup
- Create branch `proto/bipolar-wobble-2` from `proto/bipolar-wobble`, then merge `main` into it. `main` now has the SPLASH/DRIVE build (ADR 0032/0033, SPEC v1.0.21). Conflicts: keep both sides. Renumber the WOBBLE changelog line to SPEC **v1.0.22**.
- Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`. The plugin and firmware can't build here, and that's expected: say so in your report.
- **Owned files:** `core/params/WobbleVoicing.h`, `core/dsp/Wobble.*`, WOBBLE tests, ADR 0034, SPEC's WOBBLE row + changelog, the backlog section "Bipolar WOBBLE", a new sweep JSON under `presets/sweeps/`. Anything else: explain why in the commit.
- Commit to `proto/bipolar-wobble-2` with explicit paths (never `git commit -a`), and push it. Don't merge to `main`. Keep scratch output under ~10 GB. No recordings exist in the repo; don't go looking for any.

## What the owner said (1 Oct 2026, after round 1's page)
"I'm liking the bipolar control, however I think we should slightly tone down the amount of modulation at the top end of each side. Given there's no speed control for the modulation, we need to tune it. On the smooth side, we could make it feel more like a vibrato or flutter, and the left remains for wow and flutter." No "tape snags" (random pitch warps): "they can feel like glitches".

## Build
1. **Right side (sine) → vibrato.** Rate from ~1.5 Hz just right of noon to ~5–6 Hz fully right (today 0.6 → 1.4 Hz), keeping the small rate wander. Top depth gentler: try ~6 cents per pass in the Loop (today 10), with the first echo scaled to match. Each step across the side must still be clearly audible.
2. **Left side (random wow + flutter): top turned down ~30 %.** Keep the middle of the side about where it is (reshape the curve if needed).
3. **Flutter tremolo (from the Wear & Tear manual):** a small volume wobble that follows the flutter line, left side only, growing with the amount. Subtle: at most ~1 dB peak at the end stop. It should make the flutter sound like a tape transport, not a pitch effect.
4. **Flutter speed follows the wow:** the wow line nudges the flutter's rate (e.g. ±20 %), so the flutter never settles on one speed.
5. **Two strengths of toning down** as constants, **B** (gentle, ~−25 %) and **C** (more, ~−45 %), selectable at compile time or as a hidden render-only variant, so one page can compare A (round 1) / B / C.
6. **The M6 metric flags** at WOBBLE off-noon (round 1: 7 cells at 0.25, worst 21.6 dB; main also flags at 0.2): the reading jumps 3 → 21 dB between neighbouring 0.05 steps, so the metric is unstable on tight-tank burst tails with any pitch movement. Propose and implement a metric fix (e.g. the audibility floor from the tight-ringing work) *or* show that tuning fixes it. Don't loosen a limit without saying so plainly.

## Deliver
- A sweep JSON for the page: held tones + skank, A / B / C at WOBBLE fully left, 9 o'clock, noon, 3 o'clock, fully right (knob 0 / 0.25 / 0.5 / 0.75 / 1), and the exact local commands to render it and build the page (`build/rv_render --sweep … --out-dir renders/proto_bipolar_wobble2` and `python3 tools/review/make_review.py …`). Renders are made on the owner's Mac, not here.
- Gates: `ctest` log's summary line must read `100% tests passed` (never trust a piped exit code). Report the per-knob-step cents numbers (held tone), the M6 result, and the CPU estimate (desktop ns/sample vs round 1).
- ADR 0034 updated (still Proposed), SPEC v1.0.22 line, backlog notes. Final message: what changed, in the owner's words, plus the local render commands.
