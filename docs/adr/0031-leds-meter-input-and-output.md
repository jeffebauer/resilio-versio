# 0031 — The LEDs meter input and output

**Status:** Accepted, 30 Sep 2026 (owner). Amends SPEC §3 (LEDs) and §7 M9.

**Context:** The owner plays the Versio with Noise Engineering's own firmwares, where the two LEDs on the left show the input level and the two on the right the output level. SPEC's plan used the four LEDs for four different things (input clip, tank energy, SPRINGS colour, ATTITUDE colour), so the same lights would mean something else here than on every other Versio firmware the owner uses. The switches already show their own position on the panel.

**Decision:** four level meters, one per channel. Panel LEDs left to right: In L, In R, Out L, Out R.
- Brightness follows the level on a dB scale (−48 dBFS and below off, 0 dBFS full), so quiet signals still glow.
- Colour warms green → amber as the level gets hot (−18 → −6 dBFS). Level alone never makes red.
- Red is a warning, held 0.5 s after the last trigger. Input LEDs: the input peaks at −1 dBFS or above (the ADC's full scale, the jack's analog clip point). Output LEDs: the Tank's output safety limiter pulls the wet down by 0.5 dB or more, e.g. a loud Howl. The limiter is stereo-linked, so both output LEDs go red together.
- Fast rise, ~0.3 s fall (Claude's default; the owner didn't ask for anything else).
- No mode colours, no Kick flash. The boot pattern stays, then metering.

Release firmware only (the m0test and profile builds keep their diagnostic LEDs). The audio callback only takes a per-block peak per channel and the lowest limiter gain; smoothing and colour run in the main loop (`firmware/LedMeter.h`, tested on desktop by `host/tests/test_led_meter.cpp`). The Core gains one read-only accessor, `Tank::limiterGain()`; the sound doesn't change (renders bit-identical).

**Hardware check:** the M0 check didn't record which libDaisy `LED_n` sits where on the panel. The firmware assumes `LED_0..LED_3` run left to right; if not, `kMeterLed` in `firmware/main.cpp` is the one line to reorder.

**Rejected:** SPEC's original plan (LED_0 input clip, LED_1 tank energy, LED_2 SPRINGS colour, LED_3 ATTITUDE colour): it breaks the owner's NE habit, and the mode colours repeat what the switches already show.
