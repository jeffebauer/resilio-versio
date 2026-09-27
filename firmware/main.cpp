// Firmware Host, M0 test build (SPEC §7 M0).
// - Audio: Core Tank passthrough (proves Core runs on the Versio).
// - Controls -> LEDs, so every control can be checked without a computer:
//     LED_0 R/G/B = K0 DECAY, K1 TONE, K2 BOING
//     LED_1 R/G/B = K3 SPLASH, K4 DRIVE, K5 WOBBLE
//     LED_2 R = K6 MIX, G = SW0 SPRINGS position, B = SW1 ATTITUDE position
//     LED_3 white while button held, red flash on each gate rising edge,
//           otherwise green when every knob reads <= 0.02 or >= 0.98
//           (checks 0 V / 5 V CV without a computer: rack power and USB
//           must not be connected at the same time)
// - Controls -> USB serial, 10 times a second, exact values (x1000).
// Boot pattern: red, green, blue, white sweep across the four LEDs.

#include "daisy_versio.h"
#include "dsp/Tank.h"

using namespace daisy;

namespace {

DaisyVersio hw;
rv::Tank    tank;

constexpr int kBlockSize = 48;

volatile int      gateEdges    = 0;
volatile int      buttonPresses = 0;
volatile uint32_t gateFlashUntil = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    hw.tap.Debounce(); // ProcessAllControls() only handles knobs

    static bool lastGate = false;
    const bool gate      = hw.Gate();
    if (gate && !lastGate) {
        ++gateEdges;
        gateFlashUntil = System::GetNow() + 60;
        tank.kick(0);
    }
    lastGate = gate;
    if (hw.tap.RisingEdge()) {
        ++buttonPresses;
        tank.kick(0);
    }

    tank.process(in[0], in[1], out[0], out[1], int(size));
}

int SwitchPosition(int sw)
{
    // Map libDaisy Switch3 positions to left / centre / right = 0 / 1 / 2.
    switch (hw.sw[sw].Read()) {
        case Switch3::POS_LEFT: return 0;
        case Switch3::POS_RIGHT: return 2;
        default: return 1;
    }
}

void BootPattern()
{
    const float colours[4][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
    for (int step = 0; step < 4; ++step) {
        for (int led = 0; led < 4; ++led) {
            const float* c = colours[(led + step) % 4];
            hw.SetLed(size_t(led), c[0], c[1], c[2]);
        }
        hw.UpdateLeds();
        System::Delay(150);
    }
}

int Milli(float v) { return int(v * 1000.0f + 0.5f); }

} // namespace

int main()
{
    hw.Init(true); // boost to 480 MHz
    hw.SetAudioBlockSize(kBlockSize);
    tank.prepare(hw.AudioSampleRate(), kBlockSize);

    BootPattern();

    hw.seed.StartLog(false); // don't block waiting for a serial monitor
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    uint32_t nextPrint = 0;
    while (true) {
        float k[DaisyVersio::KNOB_LAST];
        for (int i = 0; i < DaisyVersio::KNOB_LAST; ++i)
            k[i] = hw.GetKnobValue(i);
        const int springs  = SwitchPosition(0);
        const int attitude = SwitchPosition(1);

        hw.SetLed(DaisyVersio::LED_0, k[0], k[1], k[2]);
        hw.SetLed(DaisyVersio::LED_1, k[3], k[4], k[5]);
        hw.SetLed(DaisyVersio::LED_2, k[6], springs * 0.5f, attitude * 0.5f);
        if (hw.tap.Pressed())
            hw.SetLed(DaisyVersio::LED_3, 1, 1, 1);
        else if (System::GetNow() < gateFlashUntil)
            hw.SetLed(DaisyVersio::LED_3, 1, 0, 0);
        else {
            bool allAtExtremes = true;
            for (float v : k)
                allAtExtremes &= (v <= 0.02f || v >= 0.98f);
            hw.SetLed(DaisyVersio::LED_3, 0, allAtExtremes ? 1.0f : 0.0f, 0);
        }
        hw.UpdateLeds();

        const uint32_t now = System::GetNow();
        if (now >= nextPrint) {
            nextPrint = now + 100;
            hw.seed.PrintLine("K %4d %4d %4d %4d %4d %4d %4d | SW %d %d | BTN %d %d | GATE %d %d",
                              Milli(k[0]), Milli(k[1]), Milli(k[2]), Milli(k[3]), Milli(k[4]), Milli(k[5]), Milli(k[6]),
                              springs, attitude, int(hw.tap.Pressed()), buttonPresses, int(hw.Gate()), gateEdges);
        }
        System::Delay(1);
    }
}
