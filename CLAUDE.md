# Resilio Versio

Dub spring reverb firmware for the Noise Engineering Versio (Daisy Seed, STM32H750). One DSP Core, three Hosts: Renderer (offline CLI), JUCE Plugin (AU/VST3 test bench), Versio Firmware.

**Start a session with `/resilio-start`, end one with `/resilio-wrap`.** The handoff lives in `docs/handoff/HANDOFF.md`.

## Read first
- `SPEC.md`: the spec (frozen v1.0; changes via ADR + changelog line). `CONTEXT.md`: the glossary (use its terms in code, comments and chat). `docs/adr/`: every decision. `docs/TASKS.md`: the owner's to-do list.
- Current tuning work: `docs/m8-tuning-backlog.md`.

## The owner
A senior designer, new to DSP and embedded C++. Brings the ears and musical goals. Explain DSP in plain language. Ask technical decisions as musical questions ("should the tail fade or hold?"). Keep `docs/TASKS.md` current whenever something changes for the owner (new check, rescan, recording, decision) in the same commit.

## Hard rules (each learned the hard way)
- `core/` never includes libDaisy, JUCE or any host header. Every Host uses the one ParamSpec table (`core/params/ParamSpec.h`); mappings live in `core/params/`.
- Sound work happens on desktop first (Renderer, then Plugin); hardware is for profiling and final feel.
- **Git:** stage explicit paths; never `git commit -a` / `-am` (it once swept an agent's work into doc commits). `git apply --3way` also stages, so check `git status --short` for pre-staged files before any commit. Keep the main checkout on `main` (the owner reads docs from it); do branch work in a worktree. Commit after each meaningful step. End messages with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Gate on ctest's summary line, not its exit code:** a pipe (`| tail`) reports 0 even when suites fail (it once hid a broken sound decision). Log to a file and check `100% tests passed`.
- **Never delete or recreate `build/`**: agents and tools share `build/rv_render`. Experiments use a scratch build dir or a worktree.
- **Audio system:** never run `auval -a`, kill `AudioComponentRegistrar` or `coreaudiod`, or reset AU caches. That once froze CoreAudio and Ableton, and the owner needed `sudo killall coreaudiod`. Validate only our AU (`auval -v aumf RsVs Rslo`), and only with Ableton closed.
- **Plugin in Ableton changes only on purpose:** dev builds don't install (`RV_INSTALL_PLUGIN` off). Install a milestone with `tools/install_plugin.sh <commit>` (it signs, verifies, and records the version in `dist/installed_plugin.txt`), then update the "Plugin installed" line in `docs/TASKS.md` with a rescan note.
- **Agent briefs** state: owned files, "don't commit" (or commit in its worktree branch with explicit paths), never delete `build/`, never run system-wide AU commands, and musical targets in the owner's words. Review every agent's diff and re-run the gates yourself before committing. Parallel agents on the same Core files go in separate worktrees; merge and re-test the combination.
- The M0 hardware check uses `dist/resilio_versio_m0_test.bin` (not `firmware/build/`). Never have USB and rack power connected at once.
- Ableton's IRs (`renders/ir_library/`) and reference recordings of commercial units must never be committed or published.

## Commands
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # once
cmake --build build && ctest --test-dir build              # 15 suites incl. plugin_host_test (~5 min)
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
make -C firmware all-variants                              # release / m0test / profile, each <= 128 KB
build/rv_render --sweep presets/sweeps/<name>.json --out-dir renders/<name>
python3 tools/review/make_review.py renders/<name> [--reference DIR]
python3 tools/ingest_references.py test_audio/reference/  # after the owner records
python3 tools/sweetspot.py                                # dead zones / cliffs per knob
tools/install_plugin.sh <commit>                          # put a build in Ableton
```

## Where things are
`core/` DSP + params · `host/render`, `host/common`, `host/tests` · `plugin/` (+ `plugin/tests`) · `firmware/` (`README.md` for the variants) · `tools/` (stimulus, review pages, IR/pitch/reference analysis, panel SVG) · `presets/sweeps/` · `renders/` (gitignored output) · `docs/` (ADRs, contracts, recipes, checks, backlog, handoff).
