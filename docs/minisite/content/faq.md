---
title: "FAQ: Resilio Versio"
description: "Short answers about Resilio Versio: which modules it runs on, whether flashing is safe, going back to stock firmware, the plugin on macOS, stereo, CV, cost and the source code."
slug: "faq"
order: 6
---

# FAQ

### What is it?
A dub spring reverb. It's free alternative firmware for the Noise Engineering Versio Eurorack module, and the same sound runs as an AU/VST3 plugin on macOS.

### Is it made by Noise Engineering?
No. Resilio Versio is an independent project, not affiliated with or endorsed by Noise Engineering. It runs on their open Versio platform and installs with their Firmware Swap web app, which accepts custom firmware.

### Which modules does it run on?
The Noise Engineering Versio. All Versio modules share one hardware platform with different panels and firmware, so your printed labels will be those of whichever Versio you have. The [panel map](/manual#panel-map) shows which control is which.

### Is flashing safe? Can I go back?
Yes. You install it with Noise Engineering's own Firmware Swap app, and the same app takes you back to any Noise Engineering firmware. Resilio doesn't install a bootloader or change anything else on the module. There's one rule: **never connect USB and Eurorack power at the same time.** [Install guide](/install)

### Is it free?
Yes. The firmware and plugin are free, and the source is open. My code is MIT licensed, and the plugin is under AGPLv3 because it's built on JUCE.

### Is there a Windows or Linux plugin?
No, it's macOS only (12 or newer, Apple Silicon or Intel), as Audio Unit and VST3.

### Why does macOS block the plugin?
It's free, and Apple hasn't notarised it. Two Terminal lines on the [install page](/install#in-a-daw-macos) let macOS open it.

### Does the plugin sound like the module?
They run the same sound engine with the same controls, so a setting in the plugin is a setting on the module. The plugin is my desktop test bench, and it's laid out like the module's panel.

### Is it stereo?
Stereo in and out. Your dry signal stays stereo. The springs hear left and right summed to mono, like a real tank, and come out wide. If you only patch In L, it feeds both sides.

### Can I control everything with CV?
All seven knobs have CV inputs (0–5 V, added to the knob). The gate input throws in TANK 1–2 and clocks the echo in TANK ECHO.

### Does it save presets?
No. The module's state is just its knobs and switches. The [starting points](/presets) are written out so you can dial them in by hand.

### Can it run away into feedback?
Only where you ask for it. The tail fades at any knob position except the very top of DECAY. In CLEAN and TAPE that's the hold, a held bed that won't get louder than what went in. In VALVE it's the howl, feedback you can ride, with an output limiter holding it back. Pull DECAY down and either one falls back into a normal tail.

### Is it a sample or an impulse response?
Neither. It's a physical-style model of springs, made of feedback loops that smear each echo by frequency (that's what makes the chirp), and I shaped it by ear against recordings of a real tank. There are no samples in it, and SPLASH doesn't add any to your hits.

### Can I read the code?
Yes, it's at [github.com/jeffebauer/resilio-versio](https://github.com/jeffebauer/resilio-versio). The repository includes the full specification and the reasons behind each design decision.

### How do I give feedback or report a bug?
On GitHub Issues, with a free GitHub account. I'd really like to hear from you, because Resilio gets better through the people playing it. There are two short forms.

- **[Suggest a sound](https://github.com/jeffebauer/resilio-versio/issues/new?template=sound_idea.yml)** if there's a sound, a move or a control you'd like, or one that could feel better. A reference helps (a track, a recording, another spring), and so does a note on how close Resilio gets today.
- **[Report a bug](https://github.com/jeffebauer/resilio-versio/issues/new?template=bug_report.yml)** if something clicks, glitches, crashes or doesn't do what its name says. Tell me whether it was the Versio or the plugin, which version, and the settings in panel names (BLEND, DECAY, TANK, ATTITUDE…). A short recording helps most.

Plain impressions are useful too, like how it sounds next to spring reverbs you know, or a knob that feels dead or too touchy. I read every issue, and I try out sound ideas and listen to them before anything changes. When a change comes from someone playing it, the [changelog](/changelog/) credits them.
