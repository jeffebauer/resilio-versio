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
4. For each knob K0–K6: fully CCW → reads ≤ 20. Fully CW → reads ≥ 980. It moves smoothly in between.
5. Each switch: left / centre / right → `SW` shows 0 / 1 / 2. **Note which physical direction reads 0**, because libDaisy calls it "left/up".
6. Button: each press adds exactly 1 to the second `BTN` number (no double counts from fast or slow presses).

## Session 2: rack power (USB unplugged)

LED key:

| LED | Shows |
|---|---|
| LED_0 | R = DECAY, G = TONE, B = BOING |
| LED_1 | R = SPLASH, G = DRIVE, B = WOBBLE |
| LED_2 | R = MIX, G = SPRINGS position, B = ATTITUDE position |
| LED_3 | White = button held. Red flash = gate. Green = every knob reads fully off or fully on (within 2%) |

7. **LEDs:** each knob changes its LED colour channel. Switches change LED_2's green/blue.
8. **CV at 0 V / 5 V:** set all knobs fully CCW → LED_3 green. Patch 5 V into one CV input → still green (it now reads fully on). Patch ~2.5 V → green goes off. Repeat for all 7 CV inputs.
9. **Gate:** clock into Gate in → LED_3 flashes red on every pulse.
10. **Passthrough vs cable** (Ableton, 48 kHz):
    - Play `test_audio/stimulus/04_skank.wav` and pink noise through the Versio, then through a plain patch cable. Record both.
    - Level within 0.5 dB per channel (I can measure the recordings for you: drop them in `test_audio/m0/`).
    - Gain up: no hum or hiss you can hear beyond the cable version.
    - Both channels work.
    - Only In L patched → sound on both outputs.

Tell me the results (or send the recordings) and I'll check the M0 boxes.
