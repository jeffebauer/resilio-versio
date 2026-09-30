# Brief: the tank tames itself on held sounds (cloud session)

You are a cloud session working on Resilio Versio, dub spring reverb firmware. Read `CLAUDE.md` and `CONTEXT.md`, then the "Excitation trim" block in `core/params/DriveVoicing.h`, the Excitation trim and limiter code in `core/dsp/Tank.*`, and `firmware/LedMeter.h` (when the output LED goes red). The owner is a designer, new to DSP. Speak in plain language.

## Setup
- Branch `proto/sustain-trim` from `main`. Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_BUILD_PLUGIN=OFF && cmake --build build`. The plugin and firmware can't build here, and that's expected: say so.
- **Owned files:** `core/params/DriveVoicing.h` (a new "Sustain trim" block next to the Excitation trim), `core/dsp/Tank.*`, the tests you add or re-tune, a new ADR (next free number, Proposed), the backlog (`docs/m8-tuning-backlog.md`, new section "Sustain trim"), a stimulus generator + sweep JSON. Anything else: say why in the commit.
- Commit to `proto/sustain-trim` with explicit paths (never `git commit -a`), and push. Don't merge to `main`. Scratch output ≤ ~10 GB. No recordings are in the repo; don't look for any.

## What the owner found (1 Oct 2026, release `b3e5ac3` on the Versio)
A low-mid synth pad (C minor, slow swell, energy mostly 90–250 Hz) makes the output LEDs go red at mild settings: CLEAN, DRIVE 0, SPLASH 0, DECAY noon, TONE anywhere from fully left to ~3:30, 2 or 3 Springs, TENSION past 3 o'clock. Input LEDs just touch amber at the swell's peak. "It definitely sounds overdriven and sometimes a little harsh, as if I have the drive turned way up." Picked: **the tank tames itself on held sounds** (hits keep their punch; only pads and drones get trimmed), over "just turn the reverb down".

What Claude measured on the desktop with that pad (input peak −6 dBFS, MIX 1, DECAY 0.5, TONE 0.3): the wet signal before the limiter peaks **+1 to +4 dB over the input's peak** on 1, 2 and 3 Springs. The tank keeps accumulating a held sound, so it ends up louder than the source. The limiter aims at −1.7 dBFS and the LED goes red at ≥ 0.5 dB of gain reduction, only ~4–5 dB above an amber input. A tight tank (TENSION ≥ 0.9) adds a narrow bump at 120–180 Hz (up to +9 dB on 1 Spring). The existing Excitation trim fixes *which band* the input's energy sits in, not *how long* it's been held, so it can't help here.

## Build
1. **Synthetic stimulus** like the owner's pad (the real one never leaves the Mac): a C minor chord (C2 + C3/Eb3/G3), detuned saws low-passed around 600–800 Hz, 3 s swell, 6 s hold, 3 s release, peak −6 dBFS. Also a sustained bass drone (C2 sine + 2nd harmonic) and an organ-like held chord. Put the generator next to the existing stimulus tools.
2. **The Sustain trim:** follow how full the tank is compared with what's going in (e.g. a slow follower on the Springs' wet output vs the input follower). When a held sound has filled the tank past a target level, ease the **input to the Springs** down, never the wet. That way a ringing tail never pumps, and DECAY's tail length and the Howl are untouched: the Howl feeds itself, and must stay as loud as today at max DECAY. It should let go quickly once the sound stops being held, so the next hit arrives at full strength. Keep it cheap (a couple of one-pole followers, per control tick).
3. **Targets:**
   - The pad, drone and organ at a −6 dBFS peak input: limiter gain reduction **< 0.5 dB** (no red LED) at CLEAN, DRIVE 0, DECAY noon, SPLASH 0, every SPRINGS, TONE 0 / 0.5 / 0.9, TENSION 0.5 / 0.8 / 1, MIX 0.5 and 1. Report the worst case and the headroom.
   - Hits and skank (`02_hits`, `04_skank`, the Kick) come out within ~0.5 dB of `main` (peak and 0.6 s tail level). Say where they don't, and why.
   - No audible pumping: the pad's wet level shouldn't dip and swell as the chord is held (report the level wobble over the hold in dB, 200 ms windows).
   - Pads at DRIVEN/KICKED and at DECAY max: report what changes. The Howl still builds and stays loud.
4. **Also check the limiter itself:** the owner hears the limiting as drive-like harshness. Report how much of that comes from the soft clip between the knee (0.82) and the threshold (0.89) on sustained low material, and propose a cleaner behaviour if it's the soft clip.

## Deliver
- A sweep JSON for a listening page: A = `main`, B = the Sustain trim, on the synthetic pad, drone, `02_hits` and `04_skank` at the owner's settings (above) with TENSION 0.8, SPRINGS 2 and 3. Include the exact local commands to render it and build the page (`build/rv_render --sweep … --out-dir renders/proto_sustain_trim` and `python3 tools/review/make_review.py …`). Renders are made on the owner's Mac; Claude will add the owner's real pad there.
- Gates: the `ctest` log's summary line must read `100% tests passed` (never trust a piped exit code). Don't loosen a limit without saying so plainly. Desktop CPU (ns/sample) vs `main`.
- ADR (Proposed), backlog notes. Final message: what changed, in the owner's words, plus the local render commands.
