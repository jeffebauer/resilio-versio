# 0039 — The gate throws: it opens the Springs' send

**Status:** Proposed, 4 Oct 2026 (branch `feat/throw-hold`; owner's call in the dub-lens critique, `docs/research/dub-lens-critique.md` §8: "gate = throw, button = Kick"). Listening page: `renders/feat_throw_hold/index.html` (`docs/prototypes/throw-hold/make_page.sh`). Supersedes SPEC §3 "Gate in = KICK" (ADR 0005's gate half); the button stays KICK.

**Context:**
- The throw is dub's main move with a spring: open the reverb send for one snare, one stab or one bar, close it, and let the tail ring on while everything else stays dry. On the Versio the only way to do that today is MIX, which also cuts the tail that is already ringing.
- The Versio's gate input is digital (on/off, no velocity) and has no jack detection: unpatched it reads low.
- Sending a gate to the Kick duplicated the button. A sequenced Kick is still possible from the button's MIDI twin in the Plugin, and on hardware the throw is the more useful job for the jack.
- SPRINGS position 3 is to become the tape echo, whose clock the gate would be (another build). The gate's role must be able to depend on SPRINGS.

**Decision:**
- **Gate high = the send open, gate low = closed.** The send is the mono input to everything in the Tank that listens to it: the Splash (a thrown snare splashes, one outside the throw doesn't), DriveIn, TONE's tilt and low cut, and the Springs. **MIX, the dry path and the wet are never touched**, so whatever is already in the tank rings on after the send closes.
- **Unpatched nothing changes.** The throw switches on at the gate's **first rising edge** after power-up and stays on until power-off (`Tank::reset()`). Until then the send is open and the Tank is bit for bit what it was before (checked on every ATTITUDE).
- **Ramps:** opens over **2 ms**, closes over **15 ms**, each a smoothstep (no corner at either end). The springs smear the send's edges so much that the click check reads 0 down to 0.5 ms / 3 ms (only a hard 1-sample gate reads one click, in KICKED), so the times are musical: 2 ms lands on a snare's crack without rounding it; 15 ms lets a chord cut mid-cycle fade rather than snap, without letting the next off-beat in after a one-hit throw.
- **The Kick is never gated** (button, MIDI): it is a knock on the tank, not something sent into it.
- **The gate's role per SPRINGS:** `throwhold::gateRole(position)`. Every position throws for now; position 3 becomes the echo's clock with the echo build.
- **Hosts:** the firmware passes every gate change to `Tank::gate(high, 0)` at the block's start (≤ 1 ms). The Plugin has an automatable **THROW** switch (ParamSpec `throw_gate`, a new `Toggle` kind, default off; a latching button right of KICK on the panel); like the gate it only takes effect from the first time it goes on. MIDI notes stay Kicks. The Renderer takes sample-accurate `"gates": [[rise, fall], ...]` in automation files, or `throw_gate` breakpoints.

**Testable:** `test_throw_hold`: gate-low events alone leave the Tank bit for bit (CLEAN / DRIVEN / KICKED); the latch (send open before the first rise; opens in 1.0 ms, closed in 15 ms; reset = power-off); the tail rings on after the close and input after it doesn't reach the wet (-300 dB); the Kick rings with the gate low; a 0 dBFS low chord thrown on and off 8 times reads 0 clicks at every ATTITUDE, DECAY 0.5 and 0.95; the THROW param equals `gate()` at the block's start. PluginHostTest checks the switch is a 2-step automatable parameter.

**Consequences:**
- A patched gate that is low means a dry module (wet only what was already ringing). That is the point, but it means a forgotten patch cable silences the springs until it goes high once more. The first edge is the opt-in.
- Costs: release firmware +1.9 KB for throw and Hold together (112,628 → 114,484 B, 16.2 KB free); desktop CPU within 1–2 % of main in every case measured (whole Tank, 3 Springs: Hold freeze / layer, the throw toggling, KICKED). Not yet measured on the module (the next CPU run).
- ParamSpec grows a ninth entry (`throw_gate`, appended, so no saved Ableton parameter moves; the key must never be renamed). The panel has no knob for it.
