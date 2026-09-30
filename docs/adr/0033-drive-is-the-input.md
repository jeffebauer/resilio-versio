# 0033 — DRIVE is the INPUT: one input gain, partly louder, no push on the Loop

**Status:** Accepted, 30 Sep 2026 (owner decisions after the send-level study and SPLASH round 4). Amends ADR 0014 and ADR 0022 (see below). Numbers: `core/params/DriveVoicing.h` "DRIVE is the INPUT"; measurements: `docs/m8-tuning-backlog.md` "SPLASH/DRIVE build".

**Context:**
- On hardware the drums came from a mixer's FX send, peaking below −18 dBFS, and SPLASH was barely there. DRIVE couldn't help: its pre-gain was fully compensated before the Splash listened, so it gave 0 dB of real input gain (send-level study).
- "As I increase drive, it seems to dampen the splash": the Splash listened after the saturators, which flattened the hits it was listening for.
- "The clean signal seems to decay longer than the driven and kicked variants, which feels counter to driving the tanks harder": ADR 0022's DRIVE push on the Loop's saturator squashed the tail on every round trip (KICKED, DECAY 0.6: the tail at 0.6 s re the hit fell ~3.5 dB more at DRIVE 1 than at DRIVE 0).

**Decision:**
1. **DRIVE sets one input gain G** at the front of the Tank, the same in every ATTITUDE: 0 dB at DRIVE 0 up to **+24 dB** at DRIVE 1 along the DRIVE curve (+9.7 dB at noon, +18 dB at ~0.8). +18 dB turns a −24 dBFS send into a −6 dBFS DAW-level hit.
2. **The Splash hears the input after G, before any saturation** (ADR 0032). DRIVE up can only add splash; a quiet send splashes like a DAW-level hit once DRIVE makes up the difference.
3. **The saturators' colour stays per ATTITUDE:** the input transducer and tape see G × the ATTITUDE's voicing offset, i.e. the pre-gain curve ADR 0022 tuned (unchanged); the output pickups keep their DRIVE push.
4. **Partly louder:** the automatic gain compensation gives the saturators' gain back, and the Tank then lets **a quarter of G (in dB) through: +6 dB at DRIVE 1** (+2.4 dB at noon, +4.5 dB at 0.8), the same in every ATTITUDE. Why a quarter: the owner asked for "a few dB" (a gentle throw by CV), target about +6; half of G (+12 dB) would make DRIVE a volume knob and put DAW-level material into the output limiter. The level is added on the Springs' output, not into them, so the Loop's saturator, the AntiRes quiet-tail fade and the Howl see the same level at every DRIVE (DRIVE never changes the tail's length); the pickups' hardness is divided by the same gain, so they bend the louder tail exactly as ADR 0022 voiced them; a Kick's Loop feed is divided by it too, so a Kick keeps its size (ADR 0005).
5. **DRIVE no longer pushes the LoopSat.** It keeps its ATTITUDE's fixed, gentle DRIVE-0 hardness. DRIVE drives the input (transducer, tape) and output (pickup) stages only. The Howl comes from DECAY and is unchanged (ADR 0002, 0018).
6. To keep KICKED's cranked top without the Loop push, KICKED's pickup push rises 26 → 28 dB (level-matched DRIVE 0 vs 1 null −5.3 dB, bar −6); the static wet makeup now gives back the pushed pickups' squash (DRIVEN 0, KICKED 0.8 dB at DRIVE 1).

**Amends ADR 0022:** its "level stays constant (±2 dB across DRIVE)" becomes: **the level follows G's heard share, +6 dB at DRIVE 1, within ±2 dB**, judged at SPLASH 0 (DRIVE's own level; SPLASH's Bite adds level on drum hits by design, ADR 0032). Its audibility bars now apply to level-matched nulls (the raw ones include the intended level rise). Its "saturation that survives the tank … more drive into LoopSat" hint is withdrawn.

**Amends ADR 0014:** "automatic gain compensation keeps loudness roughly level throughout" becomes "… except the few dB DRIVE adds on purpose". The onset (clean-ish to ~9:30, coloured by noon, driven by ~3:30) is unchanged: the saturators see the same curve.

**Testable:** test_drive (loudness vs DRIVE on the ADR 0033 curve ±2 dB at SPLASH 0, snare hits and steady noise; ATTITUDE levels ±2 dB; level-matched audibility; DRIVE doesn't shorten the tail after a hit; aliasing), test_m7_tank (DRIVE never reduces the splash; a −24 dBFS send splashes at DRIVE 1), test_antires / M6 Howl cells (Howl unchanged).

**Consequences to watch (owner, by ear):**
- On DAW-hot material (−6 dBFS peaks) at MIX 1, DRIVE above ~0.6 brings the wet into the output limiter (it catches peaks from −1.7 dBFS). On a mixer send (−18 dBFS) there is plenty of room. If the throw is too much, the heard share is one number (`kInputHeard`).
- SPLASH at DRIVE 0 on DAW-level material is gentler than before (the splash now grows with DRIVE): full on DAW-level hits from about noon.
- With the Loop no longer pushed, DRIVEN's noon colour is a little milder than ADR 0022's (level-matched null −16.8 dB vs −14.2, bar −20).

**Why:** the owner's decisions: DRIVE as the Wellspring's INPUT knob, driving harder should splash more and never shorten the tail, and a louder tank as DRIVE comes up.

**Amended 30 Sep 2026 (owner, "a more even spread of intensity", DRIVEN milder is wanted):** DRIVEN's voicing offset −6 / +16 → −5 / +13 dB and pickup push 24 → 21 dB; KICKED wet makeup 0.8 → 0.4 dB. Drive intensity (level-matched null vs DRIVE 0, 02_hits, SPLASH 0) at DRIVE 0.5 / 0.8: CLEAN −23.8 / −19.9, DRIVEN −19.1 / −13.5 (was −16.8 / −11.0), KICKED −11.2 / −6.8: DRIVEN about midway. Level curve unchanged in shape (SPLASH 0, DRIVE 1: CLEAN +6.6, DRIVEN +7.0, KICKED +7.5).
