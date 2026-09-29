// Firmware Host (SPEC §6.4, §7 M0/M3). One file, three variants selected at
// build time by the Makefile's MODE (see firmware/Makefile and
// firmware/README.md):
//
//   RV_MODE_M0TEST  - SPEC §7 M0 test build. Dry passthrough, controls -> LEDs
//                      + serial. Same behaviour as the saved
//                      dist/resilio_versio_m0_test.bin; since M7 it copies
//                      input to output directly instead of running the Tank
//                      at MIX 0 (identical output, ~20 KB less flash).
//   RV_MODE_PROFILE - SPEC §7 M3 hardware profiling. Ignores knobs/switches
//                      (no rack power expected), cycles a fixed corner table,
//                      feeds the Tank a synthetic test signal, reports
//                      CpuLoadMeter stats per corner over serial.
//   RV_MODE_RELEASE - The real instrument (default). Knobs/switches/button/
//                      gate -> ParamSpec -> Tank, LEDs per SPEC §3. No serial
//                      logging (ADR 0011 flash-size watch item).
//
// Each variant's code lives in its own #if block below; they share nothing
// but BootPattern() (identical in all three) and the include list, so any
// one variant can be read top-to-bottom without cross-referencing another.
//
// Memory placement (applies to profile + release; m0test keeps its original
// prepare() call, see that block): the Tank needs Tank::requiredPoolFloats()
// floats of delay memory (~104 KB measured at 48 kHz by host/tests). A plain
// file-scope array with no section attribute lands in .bss, which the
// libDaisy linker script (libs/libDaisy/core/STM32H750IB_flash.lds) places
// in AXI SRAM (512 KB - 32 KB), not DTCM (only 128 KB total, and already
// carries the app's stack + HAL/USB buffers) and not SDRAM (64 MB, but an
// external bus - slower, and this pool is nowhere near large enough to need
// it). So: static pool, default placement, passed through the
// prepare(fs, block, pool, n) overload. If M3 profiling ever shows SRAM
// pressure from other buffers, the single line to change is the
// kTankPool declaration below: add DSY_SDRAM_BSS (from
// libs/libDaisy/src/dev/sdram.h) to move just this pool to SDRAM, nothing
// else in this file needs to change.

#include "daisy_versio.h"
#if !defined(RV_MODE_M0TEST)
#include "dsp/Tank.h"
#include "params/ParamSpec.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace daisy;

#if !defined(RV_MODE_M0TEST) && !defined(RV_MODE_PROFILE) && !defined(RV_MODE_RELEASE)
#define RV_MODE_RELEASE 1
#endif

#ifndef RV_PROFILE_BLOCK
#define RV_PROFILE_BLOCK 48
#endif

namespace {

DaisyVersio hw;
#if !defined(RV_MODE_M0TEST)
rv::Tank    tank;
#endif

// Boot pattern shared by all three variants (NE convention: a unique colour
// sequence on power-up confirms this firmware loaded, distinct from stock).
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

} // namespace

// =============================================================================
#if defined(RV_MODE_M0TEST)
// =============================================================================
// Firmware Host, M0 test build (SPEC §7 M0). Unchanged from the firmware the
// owner is currently running through the M0 hardware check
// (docs/m0-hardware-check.md): same behaviour, same LED/serial mapping.
// - Audio: dry passthrough, input copied straight to output. (Until M7 this
//   ran the Tank at MIX 0, which is bit-identical; dropping it saves ~20 KB
//   of flash for the diagnostic builds.)
// - Controls -> LEDs, so every control can be checked without a computer:
//     LED_0 R/G/B = K0 DECAY, K1 TONE, K2 TENSION
//     LED_1 R/G/B = K3 SPLASH, K4 DRIVE, K5 WOBBLE
//     LED_2 R = K6 MIX, G = SW0 SPRINGS position, B = SW1 ATTITUDE position
//     LED_3 white while button held, red flash on each gate rising edge,
//           otherwise green when every knob reads <= 0.02 or >= 0.98
//           (checks 0 V / 5 V CV without a computer: rack power and USB
//           must not be connected at the same time)
// - Controls -> USB serial, 10 times a second, exact values (x1000).

namespace {

constexpr int kBlockSize = 48;

volatile int      gateEdges      = 0;
volatile int      buttonPresses  = 0;
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
    }
    lastGate = gate;
    if (hw.tap.RisingEdge()) {
        ++buttonPresses;
    }

    for (size_t i = 0; i < size; ++i) {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
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

int Milli(float v) { return int(v * 1000.0f + 0.5f); }

} // namespace

int main()
{
    hw.Init(true); // boost to 480 MHz
    hw.SetAudioBlockSize(kBlockSize);
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

// =============================================================================
#elif defined(RV_MODE_PROFILE)
// =============================================================================
// Hardware profiling build (SPEC §7 M3). Runs on USB power alone (no rack
// power, so no real audio in and knob/CV readings would be floating/garbage):
// knobs and switches are ignored entirely. Instead the firmware cycles
// through a fixed table of setting "corners" (SPRINGS x DECAY x TENSION x
// TONE, SPEC §5 worst-case combo included), feeding the Tank a synthetic
// click + low-level noise signal so the loops stay busy (denormal paths
// exercised, not just decaying to silence between corners). CpuLoadMeter
// (libDaisy, SPEC §5) reports per-corner average/max/min load over serial.
//
// ATTITUDE and DRIVE are fixed to KICKED / max for every corner even though
// Core does not yet act on them (Tank.h: "Stored but ignored until their
// milestone"). That keeps the corner table ready to mean something the day
// M5/M7 wire them up, instead of silently profiling only the CLEAN/no-drive
// case and needing a rewrite later.

#include "util/CpuLoadMeter.h"

#include <cstdint>
#include <cstring>

namespace {

using rv::ParamId;

constexpr int kBlockSize = RV_PROFILE_BLOCK;

// ---- Hand-rolled serial line formatting (flash-budget work, ADR 0011) -----
// Avoids printf/vsnprintf/std::snprintf entirely for this build: the
// nano-newlib printf family (iprintf/_vfiprintf_r/_printf_i/...) costs
// ~2.1 KB of flash once pulled in, all for one line printed every ~3 s here
// (not per-sample, no speed reason to keep it). As a bonus this also avoids
// a pre-existing correctness gap: this firmware links --specs=nano.specs
// without `-u _printf_float`, so the old "%5.1f"/"%.1f" format specifiers
// used here were never actually backed by float-printf support (only the
// integer path, iprintf, was linked) and would not have printed the decimal
// values correctly on hardware. These helpers use plain integer math
// instead, so what's printed is what's meant. Output goes straight to the
// same USB CDC transport Logger uses (hw.seed.usb_handle), so the serial
// monitor instructions in docs/building.md are unchanged.
void AppendStr(char*& p, const char* end, const char* s)
{
    while (*s && p < end) *p++ = *s++;
}

void AppendPadRight(char*& p, const char* end, const char* s, int width)
{
    int n = 0;
    while (*s && p < end) {
        *p++ = *s++;
        ++n;
    }
    while (n < width && p < end) {
        *p++ = ' ';
        ++n;
    }
}

void AppendUInt(char*& p, const char* end, unsigned v)
{
    char digits[10];
    int  n = 0;
    do {
        digits[n++] = char('0' + v % 10);
        v /= 10;
    } while (v && n < int(sizeof(digits)));
    while (n > 0 && p < end) *p++ = digits[--n];
}

void AppendInt(char*& p, const char* end, int v)
{
    if (v < 0 && p < end) *p++ = '-';
    AppendUInt(p, end, unsigned(v < 0 ? -v : v));
}

// Right-justifies "value.d" (one decimal digit) into `width` characters,
// e.g. AppendFixed1(p, end, 583, 5) -> " 58.3" (value is the number x10).
void AppendFixed1(char*& p, const char* end, int tenths, int width)
{
    if (tenths < 0) tenths = 0;
    char   digits[8];
    int    n     = 0;
    int    whole = tenths / 10;
    int    frac  = tenths % 10;
    digits[n++]  = char('0' + frac);
    digits[n++]  = '.';
    do {
        digits[n++] = char('0' + whole % 10);
        whole /= 10;
    } while (whole && n < int(sizeof(digits)));
    int pad = width - n;
    while (pad-- > 0 && p < end) *p++ = ' ';
    while (n > 0 && p < end) *p++ = digits[--n];
}

// Fraction (0..1) -> tenths of a percent, e.g. 0.583f -> 583 ("58.3").
int FracToPercentTenths(float frac)
{
    return int(frac * 1000.0f + 0.5f);
}

constexpr size_t kLineBufSize = 160;

void TransmitLine(const char* buf, size_t len)
{
    hw.seed.usb_handle.TransmitInternal(
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buf)), len);
}

// ---- Tank delay pool (see file header for placement reasoning) ------------
// ~104 KB measured at 48 kHz (host/tests/test_tank, test_spring); sized with
// headroom in case per-Spring memory grows a little as Core work continues.
constexpr size_t kTankPoolFloats = 30000; // 120,000 bytes, ~23% of AXI SRAM
float             kTankPool[kTankPoolFloats];
bool              gTankPrepared = false;

bool PrepareTank()
{
    const float  fs   = hw.AudioSampleRate();
    const size_t need = rv::Tank::requiredPoolFloats(fs);
    tank.prepare(fs, kBlockSize, kTankPool, kTankPoolFloats);
    return need <= kTankPoolFloats; // Tank itself falls back to passthrough if this is false
}

// ---- Corner table (SPEC §5, §7 M3) -----------------------------------------
// Full grid: SPRINGS 1/2/3 x DECAY {0,1} x TENSION {0,1} x TONE {0.5,1} = 24
// corners, all at MIX 1 (fully wet, so the meter sees Tank cost, not dry
// mix), ATTITUDE KICKED and DRIVE max on every corner (see block comment
// above). The SPEC §5 worst case ("3 springs, KICKED, max TENSION, max
// DRIVE") is already inside this grid once DRIVE matters; until then it is
// the springs=3/decay=1/tension=1 corners, flagged in the printed name.
struct Corner {
    int   springsPos; // 0/1/2 -> 1/2/3 Springs (Switch3 encoding)
    float decay, tension, tone;
    char  name[40];
};

constexpr int kNumCorners = 3 * 2 * 2 * 2;
Corner        gCorners[kNumCorners];

// decay/tension only ever take 0.0/1.0 and tone only 0.5/1.0 here (see the
// grid below), so corner names can use a fixed string table instead of
// float-formatting each one (keeps this build off printf/snprintf
// entirely - see the formatting helpers above).
const char* Decimal1Str(float v) // "0.0" / "0.5" / "1.0" only
{
    if (v <= 0.25f) return "0.0";
    if (v <= 0.75f) return "0.5";
    return "1.0";
}

void BuildCornerTable()
{
    const float decays[2] = {0.0f, 1.0f};
    const float tensions[2] = {0.0f, 1.0f};
    const float tones[2]  = {0.5f, 1.0f};
    int         idx       = 0;
    for (int springsPos = 0; springsPos < 3; ++springsPos) {
        for (float decay : decays) {
            for (float tension : tensions) {
                for (float tone : tones) {
                    Corner& c   = gCorners[idx++];
                    c.springsPos = springsPos;
                    c.decay      = decay;
                    c.tension      = tension;
                    c.tone       = tone;
                    const bool worst = springsPos == 2 && decay >= 1.0f && tension >= 1.0f;
                    char*      p     = c.name;
                    const char* end  = c.name + sizeof(c.name);
                    *p++ = 'S';
                    AppendInt(p, end, springsPos + 1);
                    AppendStr(p, end, " D");
                    AppendStr(p, end, Decimal1Str(decay));
                    AppendStr(p, end, " TN");
                    AppendStr(p, end, Decimal1Str(tension));
                    AppendStr(p, end, " TO");
                    AppendStr(p, end, Decimal1Str(tone));
                    if (worst) AppendStr(p, end, " (SPEC worst case)");
                    *p = '\0';
                }
            }
        }
    }
}

void ApplyCorner(int idx)
{
    const Corner& c = gCorners[idx];
    tank.setParam(ParamId::Springs, rv::switchToNormalised(c.springsPos));
    tank.setParam(ParamId::Decay, c.decay);
    tank.setParam(ParamId::Tension, c.tension);
    tank.setParam(ParamId::Tone, c.tone);
    tank.setParam(ParamId::Mix, 1.0f);
    tank.setParam(ParamId::Attitude, rv::switchToNormalised(2)); // KICKED
    tank.setParam(ParamId::Drive, 1.0f);                          // max (SPEC worst case)
}

// ---- Synthetic test signal: click every 500 ms + low-level seeded noise ---
constexpr float kCornerSeconds = 3.0f;
size_t          gCornerSamples = 0; // set once fs is known, in main()
size_t          gClickPeriodSamples = 0;
size_t          gClickPhase         = 0;
uint32_t        gNoiseState         = 0x9E3779B9u; // fixed seed: reproducible profiling runs

float NextNoise()
{
    // xorshift32: cheap, deterministic, good enough to keep filter state off
    // zero (denormal paths exercised) without a real audio source.
    gNoiseState ^= gNoiseState << 13;
    gNoiseState ^= gNoiseState >> 17;
    gNoiseState ^= gNoiseState << 5;
    const float u = float(gNoiseState) * (1.0f / 4294967296.0f); // 0..1
    return (u - 0.5f) * 2.0f;                                    // -1..1
}

daisy::CpuLoadMeter gLoadMeter;
size_t              gCornerElapsed = 0;
int                 gCurrentCorner = 0;

struct Result {
    int    index;
    float  avg, max, min;
    size_t memBytes;
    bool   prepared;
};
volatile bool gResultReady = false;
Result        gPendingResult{};

void AudioCallback(AudioHandle::InputBuffer /*in*/, AudioHandle::OutputBuffer out, size_t size)
{
    gLoadMeter.OnBlockStart();

    float bufL[kBlockSize];
    float bufR[kBlockSize];
    for (size_t i = 0; i < size; ++i) {
        float x = 5.0e-4f * NextNoise(); // low-level noise: keeps loops busy between clicks
        if (gClickPhase == 0) x += 0.8f; // click
        if (++gClickPhase >= gClickPeriodSamples) gClickPhase = 0;
        bufL[i] = bufR[i] = x;
    }
    tank.process(bufL, bufR, out[0], out[1], int(size));

    gLoadMeter.OnBlockEnd();

    gCornerElapsed += size;
    if (gCornerElapsed >= gCornerSamples && !gResultReady) {
        gPendingResult.index    = gCurrentCorner;
        gPendingResult.avg      = gLoadMeter.GetAvgCpuLoad();
        gPendingResult.max      = gLoadMeter.GetMaxCpuLoad();
        gPendingResult.min      = gLoadMeter.GetMinCpuLoad();
        gPendingResult.memBytes = tank.memoryBytes();
        gPendingResult.prepared = gTankPrepared;
        gResultReady            = true;

        gCurrentCorner = (gCurrentCorner + 1) % kNumCorners;
        ApplyCorner(gCurrentCorner);
        gLoadMeter.Reset();
        gCornerElapsed = 0;
    }
}

} // namespace

int main()
{
    hw.Init(true); // boost to 480 MHz
    hw.SetAudioBlockSize(kBlockSize);

    gTankPrepared = PrepareTank();
    BuildCornerTable();
    ApplyCorner(0);

    const float fs       = hw.AudioSampleRate();
    gCornerSamples        = size_t(kCornerSeconds * fs);
    gClickPeriodSamples   = size_t(0.5f * fs); // click every 500 ms
    gLoadMeter.Init(fs, kBlockSize);

    BootPattern();

    // hw.seed.StartLog(false) is deliberately NOT used here: besides
    // initialising the USB CDC port, libDaisy's StartLog() also prints a
    // "Daisy is online" banner via PrintLine() internally (logger.cpp),
    // which alone pulls the whole printf/vsnprintf chain back into this
    // build regardless of anything in this file. UsbHandle carries no
    // per-instance state (LoggerImpl<LOGGER_INTERNAL>::Init() static_asserts
    // exactly that, see libs/libDaisy/src/hid/logger_impl.h), so calling
    // Init() directly on hw.seed.usb_handle here is equivalent to what
    // StartLog() would have done, minus the banner text and its printf
    // dependency.
    hw.seed.usb_handle.Init(daisy::UsbHandle::FS_INTERNAL);
    hw.StartAudio(AudioCallback);

    bool everExceeded = false;
    while (true) {
        if (gResultReady) {
            const Result r = gPendingResult;
            gResultReady    = false;
            const Corner& c = gCorners[r.index];
            const int cyclesPerSample = int(r.avg * 10000.0f + 0.5f); // SPEC §5: 480 MHz / 48 kHz ~= 10,000 cycles/sample

            // Same fields/order as the old
            // "CORNER %-28s avg %5.1f%% max %5.1f%% min %5.1f%% | mem %6u B |
            //  prepared %s | block %d | fs %.0f Hz | ~%.0f cyc/sample" line,
            // built without printf (see AppendStr/AppendFixed1 above).
            char        buf[kLineBufSize];
            char*       p   = buf;
            const char* end = buf + sizeof(buf);
            AppendStr(p, end, "CORNER ");
            AppendPadRight(p, end, c.name, 28);
            AppendStr(p, end, " avg ");
            AppendFixed1(p, end, FracToPercentTenths(r.avg), 5);
            AppendStr(p, end, "% max ");
            AppendFixed1(p, end, FracToPercentTenths(r.max), 5);
            AppendStr(p, end, "% min ");
            AppendFixed1(p, end, FracToPercentTenths(r.min), 5);
            AppendStr(p, end, "% | mem ");
            AppendUInt(p, end, unsigned(r.memBytes));
            AppendStr(p, end, " B | prepared ");
            AppendStr(p, end, r.prepared ? "yes" : "NO (pool too small -> passthrough)");
            AppendStr(p, end, " | block ");
            AppendInt(p, end, kBlockSize);
            AppendStr(p, end, " | fs ");
            AppendInt(p, end, int(fs));
            AppendStr(p, end, " Hz | ~");
            AppendInt(p, end, cyclesPerSample);
            AppendStr(p, end, " cyc/sample\r\n");
            TransmitLine(buf, size_t(p - buf));

            if (r.max > 0.65f) everExceeded = true; // SPEC §5 target: <= 65% worst case
        }

        // LED_0: progress through the corner table (brightness = index / count).
        const float v = float(gCurrentCorner) / float(kNumCorners - 1);
        hw.SetLed(DaisyVersio::LED_0, v, v, v);
        // LED_3: red once any corner has exceeded the SPEC §5 65% target.
        hw.SetLed(DaisyVersio::LED_3, everExceeded ? 1.0f : 0.0f, 0.0f, 0.0f);
        hw.UpdateLeds();
        System::Delay(20);
    }
}

// =============================================================================
#else // RV_MODE_RELEASE
// =============================================================================
// The real instrument (SPEC §3, §6.4). 7 knobs (+CV) -> ParamSpec Normalised
// values in panel order; SW0 -> SPRINGS, SW1 -> ATTITUDE; tap + gate -> Kick
// on the rising edge, applied at the start of the block (offset 0) so it
// lands within one block (1 ms at 48 frames, SPEC §7 M7). No USB logging
// (ADR 0011: flash-size watch item at M3, ~35 KB left for DSP code once the
// M0 test firmware's 94 KB baseline is accounted for).

namespace {

constexpr int kBlockSize = 48; // SPEC §5

// libDaisy quirk (SPEC §8.1): knobs are already flipped for us. Whether
// "left" on the panel is Switch3::POS_LEFT or POS_RIGHT is exactly what the
// M0 hardware check (docs/m0-hardware-check.md step 5) tells us; until then
// these are single named switches to flip, nothing else in this file needs
// to change either way.
constexpr bool kSpringsSwitchInverted  = false;
constexpr bool kAttitudeSwitchInverted = false;

int SwitchPosition(int sw, bool invert)
{
    int pos;
    switch (hw.sw[sw].Read()) {
        case Switch3::POS_LEFT: pos = 0; break;
        case Switch3::POS_RIGHT: pos = 2; break;
        default: pos = 1;
    }
    return invert ? (2 - pos) : pos;
}

// K0..K6 -> ParamSpec, panel order (SPEC §3).
constexpr rv::ParamId kKnobParams[DaisyVersio::KNOB_LAST] = {
    rv::ParamId::Decay, rv::ParamId::Tone,   rv::ParamId::Tension,  rv::ParamId::Splash,
    rv::ParamId::Drive, rv::ParamId::Wobble, rv::ParamId::Mix,
};

// ---- Tank delay pool (see file header for placement reasoning) ------------
constexpr size_t kTankPoolFloats = 30000; // 120,000 bytes; ~104 KB measured need + headroom
float             kTankPool[kTankPoolFloats];
bool              gTankPrepared = false;

bool PrepareTank()
{
    const float  fs   = hw.AudioSampleRate();
    const size_t need = rv::Tank::requiredPoolFloats(fs);
    tank.prepare(fs, kBlockSize, kTankPool, kTankPoolFloats);
    return need <= kTankPoolFloats;
}

// Shared with the main loop for LED display; only ever written by the audio
// callback, only ever read by the (much slower) main loop, so plain floats
// are fine here (worst case the loop shows a half-updated value for 1 ms).
volatile float gInputLevel  = 0.0f; // smoothed input peak, for LED_0 clip
volatile float gWetEnergy   = 0.0f; // smoothed output RMS, for LED_1 (proxy for wet tank energy:
                                     // Tank has no public wet-only accessor, only the MIX-blended
                                     // output; a true wet-only meter is a Core follow-up, M9 polish)
volatile int   gSpringsPos  = 1;
volatile int   gAttitudePos = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    hw.tap.Debounce(); // ProcessAllControls() only handles knobs (SPEC §8.1)

    static bool lastGate = false;
    const bool  gate     = hw.Gate();
    if (gate && !lastGate) tank.kick(0);
    lastGate = gate;
    if (hw.tap.RisingEdge()) tank.kick(0);

    for (int i = 0; i < DaisyVersio::KNOB_LAST; ++i)
        tank.setParam(kKnobParams[i], hw.GetKnobValue(i));

    gSpringsPos  = SwitchPosition(DaisyVersio::SW_0, kSpringsSwitchInverted);
    gAttitudePos = SwitchPosition(DaisyVersio::SW_1, kAttitudeSwitchInverted);
    tank.setParam(rv::ParamId::Springs, rv::switchToNormalised(gSpringsPos));
    tank.setParam(rv::ParamId::Attitude, rv::switchToNormalised(gAttitudePos));

    float peak = 0.0f;
    for (size_t i = 0; i < size; ++i) peak = std::max(peak, std::fabs(0.5f * (in[0][i] + in[1][i])));
    gInputLevel = 0.9f * gInputLevel + 0.1f * peak; // simple one-pole for a readable LED, not a meter

    tank.process(in[0], in[1], out[0], out[1], int(size));

    float sumSq = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        const float m = 0.5f * (out[0][i] + out[1][i]);
        sumSq += m * m;
    }
    const float rms = std::sqrt(sumSq / float(size));
    gWetEnergy       = 0.9f * gWetEnergy + 0.1f * rms;
}

void SpringsColour(int pos, float& r, float& g, float& b)
{
    // Placeholder palette (tuned by ear at M9): green/blue/magenta for
    // 1/2/3 Springs, sparse -> classic -> dense.
    switch (pos) {
        case 0: r = 0; g = 1; b = 0; break;
        case 1: r = 0; g = 0; b = 1; break;
        default: r = 1; g = 0; b = 1;
    }
}

void AttitudeColour(int pos, float& r, float& g, float& b)
{
    // Placeholder palette (tuned by ear at M9): green -> amber -> red,
    // clean -> driven -> kicked (cool to hot).
    switch (pos) {
        case 0: r = 0; g = 1; b = 0; break;
        case 1: r = 1; g = 0.5f; b = 0; break;
        default: r = 1; g = 0; b = 0;
    }
}

} // namespace

int main()
{
    hw.Init(true); // boost to 480 MHz
    hw.SetAudioBlockSize(kBlockSize);
    gTankPrepared = PrepareTank();

    BootPattern();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true) {
        const float clip = std::min(gInputLevel / 0.95f, 1.0f);
        hw.SetLed(DaisyVersio::LED_0, clip, 1.0f - clip, 0.0f); // green -> red on clip

        const float e = std::min(gWetEnergy * 4.0f, 1.0f); // scale tuned by ear at M9
        hw.SetLed(DaisyVersio::LED_1, e, e, e);

        float r, g, b;
        SpringsColour(gSpringsPos, r, g, b);
        hw.SetLed(DaisyVersio::LED_2, r, g, b);
        AttitudeColour(gAttitudePos, r, g, b);
        hw.SetLed(DaisyVersio::LED_3, r, g, b);

        hw.UpdateLeds();
        System::Delay(1);
    }
}

#endif
