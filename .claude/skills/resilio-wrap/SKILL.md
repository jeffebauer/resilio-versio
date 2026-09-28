---
name: resilio-wrap
description: Close a Resilio Versio working session so the next one starts cold without losing anything: account for uncommitted and in-flight work (agents, worktrees, branches), run the gates, sync the owner's task list and docs, and write the handoff. Use when the owner says they're wrapping up, clearing context, running low on context, or asks for a handoff.
---

# /resilio-wrap: end a session on purpose

The next session knows nothing this one knew. Write down **what is true now, what is half-done, and what will bite**, for someone who wasn't here. Not a summary of the conversation.

**Wrap before you have to**, at the end of a coherent piece of work, not when the window is nearly full.

## Steps

1. **Wait for, or account for, running agents.** Background agents report only to the session that started them. If any are still running, tell the owner and wait for them. Otherwise their work will sit unreviewed in the working tree. Anything you can't wait for goes in the handoff under "In flight", with what the agent was doing and what to check.

2. **Account for every change.**
   ```bash
   git status --short
   git worktree list
   git branch --no-merged main
   ```
   Each file is: work to commit now (stage **explicit paths**, never `-a`), a deliberate local-only artefact (say which), or a mistake to revert. Each worktree and unmerged branch is: merge, keep on purpose (name it, e.g. `proto/tension`), or remove. ⚠ Copy any renders out of a worktree before removing it.

3. **Run the gates and record the actual numbers.**
   ```bash
   cmake --build build && ctest --test-dir build
   export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"; make -C firmware all-variants
   cat dist/installed_plugin.txt
   ```
   Record in the handoff: suites passed/failed (name any red one: that's the most important line), each firmware variant's bytes and %, and which commit is installed in Ableton vs `main`'s HEAD.

4. **Sync the owner-facing docs.**
   - `docs/TASKS.md`: the "Last updated" line, the "Plugin installed" line, ticked items moved to Done with the date, new owner tasks added, "Waiting on Claude" current.
   - Decisions made this session each have an ADR in `docs/adr/`, a SPEC changelog line, and CONTEXT.md terms if new vocabulary appeared.
   - `docs/m8-tuning-backlog.md` (or the current milestone's backlog): status of each item.
   - Memory (`~/.claude/projects/-Users-jesse-Documents-Sites-resilio-versio/memory/`): add a feedback memory for any new "learned the hard way" rule; consider whether it also belongs in `CLAUDE.md`'s Hard rules.

5. **Write `docs/handoff/HANDOFF.md`** (overwrite; the previous one is in git history). Sections:
   - **State:** milestone status table (M0–M9: built / owner check pending / not started), `main` HEAD, installed plugin commit, gate numbers from step 3.
   - **In flight:** running or unfinished agent work, worktrees and branches kept on purpose, with what to verify.
   - **Next steps:** ordered, each concrete enough to start without re-deriving it (files, criteria, relevant ADRs).
   - **Waiting on the owner:** decisions and hardware/recording tasks, pointing at `docs/TASKS.md` items.
   - **Will bite:** known risks and traps (thin test margins, CPU headroom, flaky environment issues such as the GitHub routing drops, anything surprising this session).
   - **Where to look:** the 3–6 files the next session should read first for the next step.

6. **Commit and push** the handoff and doc updates (explicit paths). If the push fails (GitHub has had routing drops on this network), say so in the reply; the commit is safe locally.

7. **Tell the owner** in a few lines: what's committed, what's in flight, and that it's safe to clear context (or why not yet).
