---
title: "FAQ: Resilio Versio"
description: "Short answers about Resilio Versio: which modules it runs on, whether flashing is safe, going back to stock firmware, the plugin on macOS, stereo, CV, cost and the source code."
slug: "faq"
order: 6
---

# FAQ

### What is it?
A dub spring reverb. Free alternative firmware for the Noise Engineering Versio Eurorack module, and the same sound as an AU/VST3 plugin for macOS.

### Is it made by Noise Engineering?
No. Resilio Versio is an independent project, not affiliated with or endorsed by Noise Engineering. It runs on their open Versio platform and installs with their Firmware Swap web app, which accepts custom firmware.

### Which modules does it run on?
The Noise Engineering Versio. All Versio modules are one hardware platform with different panels and firmware, so your printed labels will be those of whichever Versio you have: use the [panel map](/manual#panel-map). <!-- OWNER: which Versio module(s) has it been played on? Name it here, e.g. "Played on a <module>". -->

### Is flashing safe? Can I go back?
Yes. You install it with Noise Engineering's own Firmware Swap app, and you go back to any Noise Engineering firmware with the same app. Resilio doesn't install a bootloader or change anything else on the module. The one rule: **never connect USB and Eurorack power at the same time.** [Install guide](/install)

### Is it free?
Yes. <!-- OWNER: confirm, and the licence (see Credits). -->

### Is there a Windows or Linux plugin?
No, macOS only (12 or newer, Apple Silicon or Intel), as Audio Unit and VST3.

### Why does macOS block the plugin?
It's free and isn't notarised by Apple. Two Terminal lines on the [install page](/install#in-a-daw-macos) let macOS open it.

### Does the plugin sound like the module?
They run the same sound engine with the same controls, so a setting in the plugin is a setting on the module. The plugin is the project's desktop test bench, laid out like the module's panel.

### Is it stereo?
Stereo in and out. Your dry signal stays stereo; the springs hear left and right summed to mono, like a real tank, and come out wide. Patch only In L for mono: it feeds both sides.

### Can I control everything with CV?
All seven knobs have CV inputs (0–5 V, added to the knob). The gate input throws (TANK 1–2) or clocks the echo (TANK ECHO).

### Does it save presets?
No: the module's state is its knobs and switches. The [starting points](/presets) are written out so you can dial them in by hand.

### Can it run away into feedback?
Only where you ask for it. Every knob position is usable, and the tail always fades, except at the very top of DECAY: the hold in CLEAN and TAPE (a held bed, never louder than what went in) and the howl in VALVE (rideable feedback, held back by an output limiter). Pull DECAY back and both fall into a normal tail.

### Is it a sample or an impulse response?
Neither. It's a physical-style model of springs (feedback loops that smear each echo by frequency, which makes the chirp), shaped by ear against recordings of a real tank. Nothing is sampled, and SPLASH adds no samples to your hits.

### Can I read the code?
Yes: [github.com/jeffebauer/resilio-versio](https://github.com/jeffebauer/resilio-versio). It includes the full specification and every design decision with its reasons.

### How do I give feedback or report a bug?
On GitHub Issues, with a free GitHub account. Feedback is very welcome: Resilio is refined with the people playing it. Two short forms do the asking:

- **[Suggest a sound](https://github.com/jeffebauer/resilio-versio/issues/new?template=sound_idea.yml):** a sound, a move or a control you'd like, or one that could feel better. A reference (a track, a recording, another spring) helps, and how close Resilio gets today.
- **[Report a bug](https://github.com/jeffebauer/resilio-versio/issues/new?template=bug_report.yml):** something clicks, glitches, crashes or doesn't do what its name says. Say whether it was the Versio or the plugin, the version, and the settings in panel names (BLEND, DECAY, TANK, ATTITUDE…). A short recording helps most.

Plain impressions are useful too: what it sounds like next to spring reverbs you know, or a knob that feels dead or too touchy. Every issue is read, and sound ideas are tried and listened to before anything changes. Changes that come from the community are listed in the [changelog](/changelog/), credited to whoever suggested them.
