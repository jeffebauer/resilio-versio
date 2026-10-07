---
title: "Install Resilio Versio: Versio firmware and macOS plugin"
description: "How to put Resilio Versio on a Noise Engineering Versio with Noise Engineering's Firmware Swap web app, how to go back to stock firmware, and how to install the AU/VST3 plugin on macOS, including the Gatekeeper step."
slug: "install"
order: 3
---

# Install

Two ways to play Resilio: as firmware on a Noise Engineering Versio, or as a plugin in a DAW on a Mac. They share one sound engine.

<!-- DOWNLOADS: the same two buttons as the overview, with version, date and size. -->

## What you need

**Firmware**
- A Noise Engineering Versio module.
- A computer with **Chrome**, for Noise Engineering's Firmware Swap web app.
- A micro-USB cable that carries data (not a charge-only cable).
- The Resilio firmware file (`.bin`) from the download above.

**Plugin**
- A Mac with **macOS 12 or newer**, Apple Silicon or Intel.
- A DAW that loads **Audio Unit** or **VST3** plugins. (Used day to day in Ableton Live.)
- No Windows or Linux version.

## On the Versio

Resilio replaces the module's firmware entirely. Nothing else on the module changes, and Noise Engineering's own firmware goes back on with the same app (below).

> **Never connect USB and Eurorack power at the same time.** Power off, take the module out of the rack and unplug its Eurorack power cable before you plug in USB.

1. **Power off** your case. Take the Versio out and **unplug the Eurorack power cable** from the module.
2. Plug a micro-USB cable into the **Daisy Seed on the back of the module**, and the other end into your computer. The module runs on USB power alone.
3. In **Chrome**, open Noise Engineering's Firmware Swap app: [noiseengineering.us/portal/firmware](https://noiseengineering.us/portal/firmware). If it asks which module, choose your Versio.
4. Choose **Select Custom File** and pick the Resilio `.bin` file you downloaded.
5. Click **Connect**, then **Change Firmware**. Wait until it reports that it's done.
6. **Unplug USB**, put the module back in the case, and reconnect the Eurorack power.
7. Power on. The four LEDs play a short colour sweep: Resilio has loaded. Then they meter the input and output.

Your Versio's printed labels won't match Resilio's controls: keep the [panel map](/manual#panel-map) handy.

### Going back to Noise Engineering's firmware

Same app, same steps, USB only with the rack power unplugged. At step 4, instead of a custom file, pick the Noise Engineering firmware you want from the app's list (for example your module's original one), then **Connect** → **Change Firmware**. Resilio doesn't install a bootloader or change anything else, so this fully restores the module.

### Updating Resilio

Download the new `.bin` and repeat the steps above. There are no settings to lose: the module's state is its knobs and switches.

## In a DAW (macOS)

The download is a zip with the plugin in both formats, a read-me, and the firmware.

1. **Unzip** it. You get `Resilio Versio.vst3` (VST3) and `Resilio Versio.component` (Audio Unit). Install whichever your DAW uses, or both.
2. In Finder, choose **Go → Go to Folder…** and paste:
   ```
   ~/Library/Audio/Plug-Ins/
   ```
   Drag `Resilio Versio.vst3` into the **VST3** folder and `Resilio Versio.component` into the **Components** folder. Create the folder if it's missing.
3. **Let macOS open it.** The plugin isn't notarised by Apple (it's a free download), so macOS blocks it at first. Open **Terminal** and paste these two lines, one at a time (each is a single line):
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Resilio Versio.vst3"
   ```
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Resilio Versio.component"
   ```
   These remove the "downloaded from the internet" flag from those two files only. Nothing else changes.
4. **Restart your DAW and rescan plugins.** In Ableton Live: Settings → Plug-Ins, turn on the VST3 and/or Audio Units system folders, then hold Option and click **Rescan**. It shows up as **Resilio Versio**.

Use it on a return track with BLEND fully right, or as an insert with BLEND to taste.

### Updating the plugin

Quit your DAW, replace the two files with the new ones, run the two Terminal lines again, and rescan. Saved sets keep their settings across versions.

If a set made with an older version still shows old control names (MIX, SPRINGS, DRIVEN, KICKED), that's the DAW's cached copy: rescan the plugin or load a fresh instance. The settings are the same.

### Uninstalling

Delete `Resilio Versio.vst3` from `~/Library/Audio/Plug-Ins/VST3/` and `Resilio Versio.component` from `~/Library/Audio/Plug-Ins/Components/`, then rescan.

## Troubleshooting

| What you see | Try |
|---|---|
| Firmware Swap doesn't find the module | Use Chrome. Try another micro-USB cable (many only charge). Check the rack power is unplugged and USB goes to the Daisy Seed on the back. |
| No colour sweep at power-up | Check the module is getting power, then flash the firmware again. |
| The knobs don't do what the printed labels say | Expected: Resilio has its own layout. See the [panel map](/manual#panel-map). |
| "Resilio Versio can't be opened" / the DAW doesn't list it | Run the two Terminal lines (step 3) and rescan. |
| Output LEDs red a lot | The output limiter is catching peaks. Lower DRIVE or DECAY (it's normal in a big howl). |
| Input LEDs red | Your source is near clipping at the jack. Turn it down. |
