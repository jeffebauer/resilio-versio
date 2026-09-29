# M0 hardware check

Test firmware: **`dist/resilio_versio_m0_test.bin`** (a saved copy; rebuild with `make -C firmware MODE=m0test` → `firmware/build/resilio_versio_m0test.bin`). It passes audio straight through and shows every control on the LEDs and over USB serial. Criteria: SPEC §7 M0.

**Never connect rack power and USB at the same time** (NE manual). So there are two sessions: USB-only (flash + serial) and rack-only (audio + CV + gate).

## Session 1: USB only (rack power unplugged)

1. Flash **`dist/resilio_versio_m0_test.bin`** with NE Firmware Swap → **Select Custom File**, as you normally would.
2. **Boot pattern:** after reboot the four LEDs sweep red/green/blue/white for about half a second.
3. Open the serial monitor in Terminal:
   ```bash
   screen /dev/tty.usbmodem* 115200
   ```
   (Quit: Ctrl-A, then K, then Y.) A line prints 10×/second:
   `K  500  500 ... | SW 1 1 | BTN 0 0 | GATE 0 0`
4. ~~Knobs over serial~~: **not possible on USB power.** The knob/CV circuit runs off the rack's ±12 V, so on USB alone every knob reads 1000 and doesn't move (seen 29 Sep 2026; expected, not a fault). The knobs are checked in Session 2 with the LEDs (steps 7–8).
5. Each switch: left / centre / right → `SW` shows 0 / 1 / 2. **Note which physical direction reads 0**, because libDaisy calls it "left/up".
6. Button: each press adds exactly 1 to the second `BTN` number (no double counts from fast or slow presses).

**Session 1 result (29 Sep 2026): pass.** Boots, USB serial streams. Both switches step 0 → 1 → 2 cleanly; **pointing left reads 0** (matches the firmware: left = 1 Spring / CLEAN). Button: ~37 presses, 37 counted, slow and fast, no doubles. Knobs: 1000 on USB (step 4).

## Session 2: rack power (USB unplugged)

LED key:

| LED | Shows |
|---|---|
| LED_0 | R = DECAY, G = TONE, B = TENSION (knob K2; called BOING when this test firmware was built) |
| LED_1 | R = SPLASH, G = DRIVE, B = WOBBLE |
| LED_2 | R = MIX, G = SPRINGS position, B = ATTITUDE position |
| LED_3 | White = button held. Red flash = gate. Green = every knob reads fully off or fully on (within 2%) |

7. **LEDs:** each knob changes its LED colour channel, smoothly from off (fully CCW) to full (fully CW); this is the knob check. Switches change LED_2's green/blue.
8. **CV at 0 V / 5 V:** set all knobs fully CCW → LED_3 green. Patch 5 V into one CV input → still green (it now reads fully on). Patch ~2.5 V → green goes off. Repeat for all 7 CV inputs.
9. **Gate:** clock into Gate in → LED_3 flashes red on every pulse.
10. **Passthrough vs cable** (Ableton, 48 kHz):
    - Play `test_audio/stimulus/04_skank.wav` and `test_audio/stimulus/09_pink_noise.wav` (20 s at −18 dBFS RMS; `python3 tools/make_stimulus.py` makes it) through the Versio, then through a plain patch cable. Record both.
    - Level within 0.5 dB per channel (I can measure the recordings for you: drop them in `test_audio/m0/`).
    - Gain up: no hum or hiss you can hear beyond the cable version.
    - Both channels work.
    - Only In L patched → sound on both outputs.

Tell me the results (or send the recordings) and I'll check the M0 boxes.
