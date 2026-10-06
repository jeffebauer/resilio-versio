# 0044 — Panel names: BLEND, TANK 1 · 2 · ECHO, ATTITUDE CLEAN · TAPE · VALVE, THROW / TAP

**Status:** Accepted, 6 Oct 2026 (the owner's decisions of 6 Oct; branch `feat/panel-names`). SPEC v1.0.43. Labels only: the sound is bit for bit the same. Code: `core/params/ParamSpec.h` (`name`, `choices`), `host/common/ParamsJson.cpp` (`switchPosition`, the old labels), `plugin/PluginEditor.cpp` (the button's label). Tests: `host/tests/test_set_args.cpp`, `tools/review/test_make_review.py`.

**Context:**
- The panel names grew with the build and stopped describing what the controls do. **SPRINGS 3** has been echo mode since ADR 0041 (a tape echo into the 2-Spring tank), not three springs. **DRIVEN** and **KICKED** said how hard, not what kind of colour. **KICK** left with the Kick (ADR 0043); the button throws and taps tempo, but had no name of its own.
- Noise Engineering's own Versio modules call their dry/wet knob **BLEND**.
- Hosts store a plugin's parameters by their ParamSpec key (the VST3 ID is a hash of it). Renaming a key leaves a dead slot in every saved Ableton set (it happened once: BOING → TENSION, 29 Sep 2026). The display name and the switch labels can change freely.

**Decision (owner, 6 Oct 2026):**
1. **MIX → BLEND**, consistent with Noise Engineering's Versio modules.
2. **SPRINGS → TANK**, positions **1 · 2 · ECHO**. Position 3 is echo mode: "echo isn't a spring". TANK says what the switch picks: which tank is fitted (one spring, two, or the echo into two).
3. **ATTITUDE** keeps its name; its positions become **CLEAN · TAPE · VALVE** (were CLEAN · DRIVEN · KICKED), named for their saturation:
   - **TAPE** = tape saturation: the dub colour, a fine grain.
   - **VALVE** = hard, asymmetric, even-harmonic saturation inside the tank: the growl of an overdriven valve stage, with the rattle and lurch on hits, coarse grit and the Howl at DECAY's top.
   - **VALVE rather than AMP** (AMP was the name for a few hours on 6 Oct): "given the modular context, AMP could be construed as VCA".
4. **The button → THROW / TAP**: THROW in TANK 1–2 (a hand throw, ADR 0043), TAP in TANK ECHO (tap tempo).

**Rules:**
- **Keys never change.** `mix`, `springs`, `attitude` and `throw_gate` stay; so do the switch positions (0 / 0.5 / 1). Only ParamSpec's `name` and `choices` change, so every saved Ableton set keeps its values; existing devices may show the old names until Ableton rescans the plugin or a fresh instance loads.
- **Code identifiers stay**: `ParamId::Mix`, `ParamId::Springs`, "driven" / "kicked" in names such as `kToneVoicingDriven`, `kickedOpen`, `holdNotKicked`, the `springs3_voicing` key, file names. `CONTEXT.md` maps each panel name to its code name ("TAPE: ATTITUDE position 1, 'driven' in code, was DRIVEN until v1.0.43").
- **The Renderer reads both.** `--set`, presets and sweep JSON accept the new labels and the old ones (`springs=3` = ECHO, `attitude=DRIVEN` = TAPE, `attitude=KICKED` = VALVE, and `AMP` = VALVE), so older presets, sweeps, scripts and notes render the same. Sidecars and manifests write the new labels. The review pages (`tools/review/`) show old renders under the new names.
- **History stays as written**: ADRs 0001–0043, old changelog lines, the backlog and research keep the names of their day.

**Plugin:** the panel's labels and the host's parameter names come from ParamSpec, so they follow. The button reads **THROW**, and **TAP** while it taps (TANK ECHO): "THROW / TAP" doesn't fit a 7 mm button. The GATE switch beside it is unchanged (the `throw_gate` parameter, named THROW in the host's list).

**Consequences:**
- Sound unchanged: renders with old and new labels are identical bit for bit to the build before (the six starting points, every TANK × ATTITUDE).
- The firmware draws no text; the longer switch labels in the shared ParamSpec table add 8 bytes to each image (release 126,380 B, profile 127,520 B).
- Docs checked against the code while renaming (owner's ask): two starting points, Skank chord wash and Mix-bus spring, still sat on the old third position (voiced as three Springs, now echo mode); they move to TANK 2, their other settings unchanged (`docs/presets.md`).
- "TAPE" now names an ATTITUDE position and echo mode's delay line (Tape, CONTEXT.md). Write TAPE in capitals for the position; the echo's tape is the Tape.
