# 0045 — Going public: licences, name, credit, what's published

**Status:** Accepted, 6 Oct 2026 (the owner's decisions of 6 Oct, session 9). Not yet carried out: the repo stays private until the owner says "flip it". The readiness sweep (session 9) found no audio, Ableton IRs, binaries or secrets anywhere in git history, so no history rewrite is needed.

**Context:**
- The owner wants a public minisite (built in their website repo, from the brief in `docs/minisite/`) with a "download the latest version" link. The repo `jeffebauer/resilio-versio` and its releases are private.
- The plugin is built on JUCE 9.0.2, which is AGPLv3 or a paid JUCE licence. The firmware uses no JUCE: libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0). No Noise Engineering code, headers or bootloader are in the repo or the binaries.
- Until now the repo had no licence, and the friends' plugin zips carried no notices.

**Decision (owner, 6 Oct 2026):**
1. **Make this repo public** (not a fresh snapshot), history as it is.
2. **Our own code, tools and docs: MIT.** A `NOTICE` file lists libDaisy (MIT), the STM32 HAL (BSD-3), CMSIS (Apache-2.0) and JUCE (AGPLv3).
3. **The plugin binaries are AGPLv3** (no paid JUCE licence). Every plugin download carries the AGPL text, `LICENSE`, `NOTICE` and a link to the exact source (the release's tag).
4. **The commit email** (hello@jessebauer.xyz) goes public as it is.
5. **The two Wellspring spectrogram images stay** (our analysis pictures, not audio). The rule stands for the recordings themselves: reference recordings of commercial units and Ableton's IRs are never committed or published.
6. **Credit:** Jesse Bauer, linking to jessebauer.xyz (site, README, plugin About, release notes).
7. **Feedback:** GitHub Issues, with simple templates (a bug, a sound idea).
8. **Old material:** stale `proto/*` branches are deleted on GitHub (their decisions live in the ADRs; local refs can stay); the three friends' releases stay as history, marked as pre-public; the first public release gets public-facing notes.
9. **The name stays "Resilio Versio"**, with a clear disclaimer everywhere it's presented: "firmware for the Noise Engineering Versio; not affiliated with or endorsed by Noise Engineering", plus trademark lines for Versio (Noise Engineering), VST (Steinberg) and Audio Units (Apple).

**Before the flip (reversible, on a branch):** `LICENSE` and `NOTICE`; `.idea/` in `.gitignore`; remove the unused `libs/DaisySP` submodule (it holds an LGPL folder); `tools/make_release.sh` uploads stable-named assets (`resilio-versio-plugin-macos.zip`, `resilio-versio-firmware.bin`, `SHA256SUMS.txt`) with licence files in the zip, and marks full releases `--latest`, so `releases/latest/download/<name>` always works; README credit, disclaimer and the "up to two springs" fix; issue templates; repo-relative paths in prototype scripts; the plugin's layout fix (GATE over DRIVE's label, "THR…") before any public screenshot.

**Irreversible steps (each needs the owner's explicit go):** flipping the visibility (copies can't be recalled), publishing the first public release, deleting remote branches.
