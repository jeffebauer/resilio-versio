# 0032 — SPLASH comes from the hit: the Clang and the Bite

**Status:** Accepted, 30 Sep 2026 (owner picks, SPLASH round 4). Supersedes the noise-burst Clatter on hits (M8 rounds 1–2); changes ADR 0025 (see below); the Kick keeps its crash (ADR 0016). Numbers: `core/params/SplashVoicing.h` "Hit envelope e"; measurements: `docs/m8-tuning-backlog.md` "SPLASH/DRIVE build".

**Context:** The owner heard the noise-burst Clatter as "a static impulse that doesn't change in relation to the input", a layer on top of the reverb. A real tank splashes because the hit itself hits the springs harder. SPLASH round 4 (`proto/splash-round4`, page `renders/splash_round4/`) offered two ways of doing that; the owner picked, all at the "clear" strength: hits CLEAN **C2**, DRIVEN **T2**, KICKED **T2**; the skank **C2** in every ATTITUDE. "In most cases I like C2 on longer sounds like the skank, and T2 on shorter impulses."

**Decision:** The Splash listens to the input after the INPUT gain (DRIVE, ADR 0033) and before any saturation. From it, a per-sample hit envelope e = SPLASH × sudden × loud (round 4's detector: how suddenly the input rises, times how loud it is against a floor that sits after the INPUT gain, and against the programme level in a groove). Two things follow a loud hit, and nothing is added:
- **The Clang** (every ATTITUDE): the hit's own highs (above 2 kHz) are fed harder into the springs while e is up (round 4's C, C2 = ×5 at e = 1). Brightness, chirped and coloured by the tank, dying with the tail.
- **The Bite** (DRIVEN and KICKED, blended by the ATTITUDE Morph): a short, cracking hit is pushed harder into the input transducer and tape, and half of the push (in dB) is taken back after (round 4's T, T2 = ×(1 + 4e)). Grit on the hit, and the hit reaches the springs harder, so its tail comes back louder (DriveIn's automatic makeup gives back what the saturators squashed).
- **"Short"** is how much of the hit is crack rather than notes: the share of its high-passed peak envelope above 2 kHz. Measured on the owner's stimuli, a skank stab decays *faster* than the snare and has the same onset crest, so neither decay nor crest can tell them apart; what differs is what the hit is made of (snare 0.62–0.72, rim 0.53–0.58, a click 0.96, skank 0.21–0.27). Drum hits bite; chord stabs get the Clang and next to no Bite.
- **DRIVE's top half:** past noon DRIVE saturates the input and pushes the pickups, and both squash the drips with everything else. So above noon the Clang and the Bite grow with DRIVE, normalised to exactly C2 / T2 at DRIVE 0.8 (where the owner picked them): ×0.64 up to noon, ×1.28 at DRIVE 1. With that, DRIVE never reduces the splash (test_m7_tank).
- **Kept:** the Jolt (pitch lurch) on every hit, stepping up with ATTITUDE; the Kick's crash (thud + Clatter burst + forced Jolt, ADR 0016); SPLASH 0 = nothing but the small Jolt floor (DRIVEN, KICKED).
- **Removed:** the Clatter on hits, its level λ, and its rattle impacts on hits (the rattle stays on the Kick).

**Relation to ADR 0025 (CLEAN gets a gentler splash):** CLEAN keeps a real splash on hard hits and nothing at SPLASH 0. It now gets the same Clang as DRIVEN and KICKED (the owner picked C2 for CLEAN too); it stays the gentle voice because it has no Bite and a Jolt a third of DRIVEN's. The ATTITUDEs step up in the Bite and the Jolt, not in the Clang.

**Relation to ADR 0016 (Kick = thud + crash):** unchanged. The Clatter is now the Kick's alone.

**Testable:** test_splash (the envelope: none on sustained sound, rises with level, ghost < 25 %; the Bite on snare and rim, < 5 % on a chord stab, none in CLEAN), test_m7_tank (clear splash per ATTITUDE, ghosts in a groove barely trigger at any DRIVE, DRIVE never reduces the splash, a −24 dBFS send splashes at DRIVE 1), M6 grid (no Ringing without the burst: the tight-tank bend fade, AntiRes.h).

**Consequences to watch (owner, by ear):**
- At the default SPLASH (0.3) drum hits in DRIVEN / KICKED come back a couple of dB louder than at SPLASH 0 (the Bite hits the tank harder); the MIX and ATTITUDE loudness checks are made at SPLASH 0 and report the default as INFO.
- With DRIVE high, isolated quiet hits come up to full splash too (a tank with its INPUT cranked); each clangs with its own, quieter highs. In a groove, ghost notes between backbeats stay quiet at any DRIVE.
- Drum hits in DRIVEN / KICKED get the Clang and the Bite (round 4's TC). The owner picked T2 alone for them at DRIVE 0.8; the Bite replacing the Clang was tried and dropped because below DRIVE ~0.5 the Bite alone gave KICKED rimshots almost no splash (+1.3 dB).

**Why:** the owner's picks, and a model of a real tank: harder hits, not an added sound.
