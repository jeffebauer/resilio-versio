---
name: resilio-start
description: Start a Resilio Versio working session from a cold context: read the handoff and rules, check git, worktrees, build and installed-plugin state, and propose the next step. Use at the beginning of a session, after clearing context, or when the owner says "start", "pick up where we left off" or "what's next".
---

# /resilio-start: pick up cold

The previous session left a handoff. Read it before touching anything; don't re-derive decisions it records.

## Steps

1. **Read, in order:** `CLAUDE.md` (rules, commands), `docs/handoff/HANDOFF.md` (state, in flight, next steps, what will bite), `docs/TASKS.md` (the owner's list, installed plugin, waiting-on-Claude), then the "Where to look" files the handoff names. Skim `docs/adr/` titles for decisions newer than you'd expect.

2. **Check reality against the handoff.**
   ```bash
   git status --short
   git log --oneline -10
   git worktree list
   git branch --no-merged main
   git fetch -q && git status -sb | head -1     # ahead/behind origin (fetch may fail on this network's GitHub routing drops)
   cat dist/installed_plugin.txt
   test -x build/rv_render || echo "rebuild needed"
   ```
   ⚠ Uncommitted changes that the handoff doesn't explain are probably an agent's unreviewed work from the last session. Read the diff and the handoff's "In flight" notes before committing or discarding anything. Never discard without asking.

3. **Rebuild if needed** (don't delete `build/`):
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
   ```
   Run `ctest --test-dir build` if the handoff says gates were red or the tree has changed since. It takes ~5 minutes; run it in the background while you read.

4. **Check new owner input.** Look for new reference recordings (`test_audio/reference/*.wav` → run `python3 tools/ingest_references.py test_audio/reference/`), ticked boxes or notes in `docs/TASKS.md`, and M0/M2 results the owner may mention.

5. **Report to the owner** in a short, plain-language message: where things stand (one line per active milestone), anything that surprised you in step 2, what's waiting on them, and your proposed next step with a one-line reason. Then wait for their go-ahead, unless the handoff says the next step was already agreed.

## Rules that matter at the start
Everything in `CLAUDE.md`'s Hard rules applies from the first command, especially: explicit-path commits, never delete `build/`, no system-wide Audio Unit commands (the owner may have Ableton open), and install plugin builds only via `tools/install_plugin.sh` with a `docs/TASKS.md` note.
