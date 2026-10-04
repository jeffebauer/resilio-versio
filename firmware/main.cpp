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
//                      gate -> ParamSpec -> Tank, LEDs as level meters (SPEC
//                      §3, ADR 0031). No serial logging (ADR 0011 flash-size
//                      watch item).
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
// it).
//
// M3 (29 Sep 2026): with the pool in AXI SRAM the worst case measured 83 %
// average / 100 % peak CPU: the delay reads jump around 120 KB behind a
// 16 KB D-cache. DTCM (zero-wait, no cache) was completely unused (the
// stack lives in AXI SRAM), so the pool now goes there (kDtcm below):
// 120,000 of its 131,072 bytes. .dtcmram_bss is NOLOAD (startup doesn't
// zero it), so PrepareTank() clears the pool first. The Tank object itself
// (~5.6 KB, has a constructor) stays in ordinary .bss.

#include "daisy_versio.h"
#if !defined(RV_MODE_M0TEST)
#include "dsp/Tank.h"
#include "params/ParamSpec.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

// Tightly-coupled data RAM (see "Memory placement" above).
#define RV_DTCM __attribute__((section(".dtcmram_bss")))

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
// The Tank is built at boot (placement new at the top of main), not as a
// plain global: its default member values made it ~7.9 KB of .data, stored
// in flash and copied to RAM at startup. Zeroed .bss storage costs no flash
// (ADR 0011's 128 KB budget). Never destroyed (main never returns).
alignas(rv::Tank) unsigned char gTankStorage[sizeof(rv::Tank)];
rv::Tank& tank = *reinterpret_cast<rv::Tank*>(gTankStorage);
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
#include "dsp/ProfileHook.h"

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

constexpr size_t kLineBufSize = 640; // CORNER + SPLIT (+ BENCH) lines, sent in one transmit

void TransmitLine(const char* buf, size_t len)
{
    hw.seed.usb_handle.TransmitInternal(
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buf)), len);
}

// ---- Tank delay pool (see file header for placement reasoning) ------------
// ~104 KB measured at 48 kHz (host/tests/test_tank, test_spring); sized with
// headroom in case per-Spring memory grows a little as Core work continues.
constexpr size_t kTankPoolFloats = 30000; // 120,000 bytes, in DTCM (RV_DTCM)
RV_DTCM float     kTankPool[kTankPoolFloats];
bool              gTankPrepared = false;

// ---- Echo mode's tape (SPRINGS 3, ADR 0041) --------------------------------
// 2 s at 48 kHz plus a margin (Tank::requiredTapeFloats): 388 KB, too big for
// DTCM, so ordinary .bss in AXI SRAM (zeroed at startup; the echo reads only
// from its own sequential write position, the cache copes). A Versio runs at
// 48 kHz; at another rate the Tank caps the longest echo to what fits.
constexpr size_t kEchoTapeFloats = rv::echo::tapeFloats(48000.0f);
float            kEchoTape[kEchoTapeFloats];

bool PrepareTank()
{
    const float  fs   = hw.AudioSampleRate();
    const size_t need = rv::Tank::requiredPoolFloats(fs);
    std::memset(kTankPool, 0, sizeof kTankPool); // NOLOAD section: not zeroed at startup
    tank.prepare(fs, kBlockSize, kTankPool, kTankPoolFloats, kEchoTape, kEchoTapeFloats);
    return need <= kTankPoolFloats; // Tank itself falls back to passthrough if this is false
}

// ---- Corner table (SPEC §5, §7 M3) -----------------------------------------
// Full grid: SPRINGS 1/2/3 x DECAY {0,1} x TENSION {0,1} x TONE {0.5,1} = 24
// corners, all at MIX 1 (fully wet, so the meter sees Tank cost, not dry
// mix), ATTITUDE KICKED and DRIVE max on every corner (see block comment
// above). The SPEC §5 worst case was "3 springs, KICKED, loosest TENSION (0),
// max DRIVE"; since echo mode (ADR 0041) no position runs three Springs, so
// the candidates are SPRINGS 2 at the loosest tank and SPRINGS 3 (2 Springs
// at the fixed tank + the tape echo, DECAY 1 = most feedback), both flagged
// "(worst?)" in the printed name.
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
                    // Loosest tank (most stages), 2 Springs: position 2, and position 3 (echo
                    // mode, ADR 0041: Springs A and B at the fixed tank plus the tape echo).
                    const bool worst = springsPos >= 1 && decay >= 1.0f && tension <= 0.0f;
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
                    if (worst) AppendStr(p, end, " (worst?)");
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

// Per-section cycle split (dsp/ProfileHook.h): the Tank marks each section's
// end; the time since the previous mark goes to that section. DWT cycle
// counter, 480 MHz.
constexpr int kNumSections = rv::prof::kNumSections;
uint32_t      gProfLast = 0;
uint64_t      gProfAcc[kNumSections]{};
uint64_t      gProfPrev[kNumSections]{};
uint32_t      gProfPeak[kNumSections]{}; // worst single block per section, this corner

void ProfMark(int section)
{
    const uint32_t now = DWT->CYCCNT;
    gProfAcc[section] += now - gProfLast;
    gProfLast = now;
}

void StartCycleCounter()
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55; // unlock (Cortex-M7)
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

struct Result {
    int      index;
    float    avg, max, min;
    size_t   memBytes;
    bool     prepared;
    uint64_t split[kNumSections];
    uint32_t peak[kNumSections];
    size_t   samples;
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
    gProfLast = DWT->CYCCNT;
    tank.process(bufL, bufR, out[0], out[1], int(size));
    for (int k = 0; k < kNumSections; ++k) {
        const uint32_t d = uint32_t(gProfAcc[k] - gProfPrev[k]);
        if (d > gProfPeak[k]) gProfPeak[k] = d;
        gProfPrev[k] = gProfAcc[k];
    }

    gLoadMeter.OnBlockEnd();

    gCornerElapsed += size;
    if (gCornerElapsed >= gCornerSamples && !gResultReady) {
        gPendingResult.index    = gCurrentCorner;
        gPendingResult.avg      = gLoadMeter.GetAvgCpuLoad();
        gPendingResult.max      = gLoadMeter.GetMaxCpuLoad();
        gPendingResult.min      = gLoadMeter.GetMinCpuLoad();
        gPendingResult.memBytes = tank.memoryBytes();
        gPendingResult.prepared = gTankPrepared;
        gPendingResult.samples  = gCornerElapsed;
        for (int k = 0; k < kNumSections; ++k) {
            gPendingResult.split[k] = gProfAcc[k];
            gPendingResult.peak[k]  = gProfPeak[k];
            gProfAcc[k]             = 0;
            gProfPrev[k]            = 0;
            gProfPeak[k]            = 0;
        }
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
    new (gTankStorage) rv::Tank(); // before anything touches it (see gTankStorage)
    // DaisyVersio::Init minus the parts this build never uses (flash, 4 Oct
    // 2026): the knob/CV ADC (it was set up but never started here), the two
    // switches, the button and the gate. Same Seed set-up (clocks at 480 MHz,
    // caches, SDRAM, audio codec and SAI) and the same four LEDs, so the
    // audio callback, and with it the CPU measurement, is unchanged.
    hw.seed.Configure();
    hw.seed.Init(true); // boost to 480 MHz
    {
        constexpr Pin kLedPins[DaisyVersio::LED_LAST][3] = {
            {seed::D10, seed::D3, seed::D4},
            {seed::D12, seed::D13, seed::D11},
            {seed::D25, seed::D26, seed::D14},
            {seed::D29, seed::D27, seed::D15},
        }; // libDaisy src/daisy_versio.cpp PIN_LEDn_R/G/B
        for (int i = 0; i < DaisyVersio::LED_LAST; ++i)
            hw.leds[i].Init(kLedPins[i][0], kLedPins[i][1], kLedPins[i][2], true);
    }
    hw.seed.SetAudioBlockSize(kBlockSize);

    gTankPrepared = PrepareTank();
    StartCycleCounter();
    rv::prof::markHook = ProfMark;
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

            // Where the time goes, each section as % of the whole budget
            // (10,000 cycles per sample at 480 MHz / 48 kHz): cycles / (samples x 10) = tenths of a %.
            static const char* const kSectionNames[kNumSections] = {
                "ctl", "drvIn", "splash", "tilt", "sprA", "sprB", "echo", "out"}; // echo mode: Spring C's slot times the tape (Tank.cpp)
            // Same buffer, one transmit: the USB CDC send is non-blocking and
            // drops a second call made while the first is still going out.
            AppendStr(p, end, "  SPLIT");
            for (int k = 0; k < kNumSections; ++k) {
                AppendStr(p, end, " ");
                AppendStr(p, end, kSectionNames[k]);
                // (float, not a 64-bit divide: __aeabi_uldivmod is ~850 B of flash)
                AppendFixed1(p, end, r.samples ? int(float(r.split[k]) / (float(r.samples) * 10.0f)) : 0, 5);
            }
            AppendStr(p, end, "  (% of budget)\r\n");
            // The worst single block per section (% of one block's budget): what
            // makes max jump above avg.
            AppendStr(p, end, "  PEAK ");
            for (int k = 0; k < kNumSections; ++k) {
                AppendStr(p, end, " ");
                AppendStr(p, end, kSectionNames[k]);
                AppendFixed1(p, end, int(r.peak[k] / (uint32_t(kBlockSize) * 10u)), 5);
            }
            AppendStr(p, end, "\r\n");
            if (r.index == 0) { // once per pass through the corners
                // Sanity check that the chip runs as configured (the
                // multiply-add micro-benchmarks of runs 5-12, m3_bench.cpp,
                // were dropped for flash on 1 Oct 2026: they had answered
                // their question).
                AppendStr(p, end, "BENCH clock ");
                AppendUInt(p, end, unsigned(SystemCoreClock / 1000000u));
                AppendStr(p, end, " MHz icache ");
                AppendStr(p, end, (SCB->CCR & SCB_CCR_IC_Msk) ? "on" : "OFF");
                AppendStr(p, end, " dcache ");
                AppendStr(p, end, (SCB->CCR & SCB_CCR_DC_Msk) ? "on" : "OFF");
                AppendStr(p, end, "\r\n");
            }
            TransmitLine(buf, size_t(p - buf));

            if (r.max > 0.70f) everExceeded = true; // SPEC §5 target: <= 70% peak worst case (ADR 0030 amendment)
        }

        // LED_0: progress through the corner table (brightness = index / count).
        const float v = float(gCurrentCorner) / float(kNumCorners - 1);
        hw.SetLed(DaisyVersio::LED_0, v, v, v);
        // LED_3: red once any corner has exceeded the SPEC §5 70% target.
        hw.SetLed(DaisyVersio::LED_3, everExceeded ? 1.0f : 0.0f, 0.0f, 0.0f);
        hw.UpdateLeds();
        System::Delay(20);
    }
}

// =============================================================================
#else // RV_MODE_RELEASE
// =============================================================================
// The real instrument (SPEC §3, §6.4). 7 knobs (+CV) -> ParamSpec Normalised
// values in panel order; SW0 -> SPRINGS, SW1 -> ATTITUDE; tap -> Kick on the
// rising edge, gate -> THROW in positions 1-2 (ADR 0039: the Springs' send open while high,
// from its first rising edge) and the echo clock in 3 (ADR 0041), both applied at the start of the block
// (offset 0) so they land within one block (1 ms at 48 frames, SPEC §7 M7). No USB logging
// (ADR 0011: flash-size watch item at M3, ~35 KB left for DSP code once the
// M0 test firmware's 94 KB baseline is accounted for).
//
// LEDs (SPEC §3, ADR 0031): level meters, the owner's Noise Engineering
// habit. Left pair In L / In R, right pair Out L / Out R; brightness follows
// level (dB scale), colour warms green -> amber, red = input near clip or
// output limiter pulling down. No mode colours, no Kick flash.

#include "LedMeter.h"
#include "PotEndStops.h"

namespace {

constexpr int kBlockSize = 48; // SPEC §5

// libDaisy quirk (SPEC §8.1): knobs are already flipped for us. Switches:
// the M0 hardware check (29 Sep 2026, docs/m0-hardware-check.md step 5)
// confirmed both read 0 (Switch3::POS_LEFT) pointing left on the panel, so
// left = 1 Spring / CLEAN as SPEC §3 wants and neither needs inverting.
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

// Output calibration from the M0 passthrough check (29 Sep 2026,
// docs/m0-hardware-check.md): with a plain in -> out copy, the Versio's
// analog path came out polarity-inverted and 1.17 dB hotter than a patch
// cable (flat 30 Hz-4 kHz, both channels). Undo both on the way out so MIX 0
// is indistinguishable from a cable (SPEC §7 M0). Both sides of MIX go
// through it, so the dry/wet balance is unchanged.
constexpr float kOutputTrim = -0.874f; // -(10^(-1.17/20))

// Panel pots P1..P7 (reading order: top to bottom, left to right, as in
// docs/panel/) -> libDaisy knob index. A hardware fact from the M0 LED check
// (29 Sep 2026): libDaisy's KNOB_0..KNOB_6 are NOT in reading order. Each
// pot's CV input is summed with it in hardware, so CV follows its pot.
constexpr int kPotKnob[DaisyVersio::KNOB_LAST] = {0, 4, 2, 1, 5, 3, 6};

// P1..P7 -> ParamSpec: the owner's panel layout (SPEC §3, ADR 0028).
constexpr rv::ParamId kPotParams[DaisyVersio::KNOB_LAST] = {
    rv::ParamId::Mix,     rv::ParamId::Decay,  rv::ParamId::Tone,  rv::ParamId::Splash,
    rv::ParamId::Tension, rv::ParamId::Wobble, rv::ParamId::Drive,
};

// ---- Tank delay pool (see file header for placement reasoning) ------------
constexpr size_t kTankPoolFloats = 30000; // 120,000 bytes; ~104 KB measured need + headroom
RV_DTCM float     kTankPool[kTankPoolFloats];
bool              gTankPrepared = false;

// ---- Echo mode's tape (SPRINGS 3, ADR 0041) --------------------------------
// 2 s at 48 kHz plus a margin (Tank::requiredTapeFloats): 388 KB, too big for
// DTCM, so ordinary .bss in AXI SRAM (zeroed at startup; the echo reads only
// from its own sequential write position, the cache copes). A Versio runs at
// 48 kHz; at another rate the Tank caps the longest echo to what fits.
constexpr size_t kEchoTapeFloats = rv::echo::tapeFloats(48000.0f);
float            kEchoTape[kEchoTapeFloats];

bool PrepareTank()
{
    const float  fs   = hw.AudioSampleRate();
    const size_t need = rv::Tank::requiredPoolFloats(fs);
    std::memset(kTankPool, 0, sizeof kTankPool); // NOLOAD section: not zeroed at startup
    tank.prepare(fs, kBlockSize, kTankPool, kTankPoolFloats, kEchoTape, kEchoTapeFloats);
    return need <= kTankPoolFloats;
}

// ---- LED level meters (SPEC §3 LEDs, ADR 0031; maths in LedMeter.h) -------
// The owner's NE habit: left pair = input, right pair = output.
enum Meter { kInL, kInR, kOutL, kOutR, kNumMeters };

// Meter -> libDaisy LED index. Wanted on the panel (docs/panel/ LED1..LED4,
// left to right): In L, In R, Out L, Out R. NOT YET CONFIRMED ON HARDWARE:
// the M0 check never recorded which LED_n sits where, so this assumes
// LED_0..LED_3 run left to right. If the meters light in the wrong places,
// reorder this one line.
constexpr size_t kMeterLed[kNumMeters] = {
    DaisyVersio::LED_0, // In L  -> panel LED1 (leftmost)
    DaisyVersio::LED_1, // In R  -> panel LED2
    DaisyVersio::LED_2, // Out L -> panel LED3
    DaisyVersio::LED_3, // Out R -> panel LED4 (rightmost)
};

// Written by the audio callback (peak since the main loop last looked), read
// and cleared by the main loop. Single-word float stores are atomic on the
// M7; a block landing between the loop's read and clear is lost, which a
// 1 ms LED can't show anyway.
volatile float gPeak[kNumMeters] = {};
volatile float gLimiterGain      = 1.0f; // lowest Tank::limiterGain() since last read
volatile bool  gThrowExited      = false; // KICK held: throw mode was on and is now off (LED blink)

// ---- LED PWM by timer + DMA (30 Sep 2026 fix, "LEDs flicker rather than dim")
// libDaisy's software PWM needs UpdateLeds() called at its sample rate
// (1 kHz by default, not settable through DaisyVersio), and the pins then
// only get ~8 uneven steps per 120 Hz period: dim values alias into sparse
// 1 ms flashes. Calling it faster from the main loop doesn't help: the audio
// callback runs in the SAI DMA interrupt (priority 0, nothing can pre-empt
// it) for ~60 % of every 1 ms block, freezing any CPU-driven PWM.
//
// So the CPU only writes a table per GPIO port (rvled::fillPwmWords(), one
// 32-bit BSRR word per PWM step, ~1 kHz from the main loop, only when a
// value changed), and DMA copies it to the port's BSRR register on every
// tick of TIM5: 512 steps per period at ~1 kHz, untouched by the CPU load.
// The 12 LED pins sit on 5 ports (A, B, C, D, G) -> 5 DMA streams, each
// paced by its own TIM5 request (update + the 4 compare channels, which only
// raise DMA requests; no timer pins are used). Streams: DMA2 5/6/7 (unused
// by libDaisy) and DMA2 0/1 (libDaisy's DAC streams; the Versio has no DAC).
// No DMA interrupts are enabled, and the audio's own DMA (SAI, ADC) is on
// DMA1, so the audio callback is untouched: its only new neighbour is the
// main loop's table writes. If a stream doesn't start moving, or stops later
// (a bus error clears its enable bit), the loop falls back to libDaisy's
// PWM: flickery, but lit.
//
// Tables live in SRAM1 (.sram1_bss), which libDaisy's MPU setup makes
// non-cacheable for its first 32 KB (sys/system.cpp), so the DMA reads what
// the CPU wrote without cache maintenance, and table writes don't evict the
// audio code's cached data.
constexpr float kPwmHz      = 1000.0f; // PWM periods per second
constexpr int   kNumLeds    = DaisyVersio::LED_LAST;
constexpr int   kMaxPorts   = 5;
constexpr int   kPinsPerLed = 3; // r, g, b

// DaisyVersio's LED pins (libs/libDaisy/src/daisy_versio.cpp, PIN_LEDn_R/G/B;
// not exported), per libDaisy LED index, in r, g, b order.
constexpr Pin kLedPin[kNumLeds][kPinsPerLed] = {
    {seed::D10, seed::D3, seed::D4},
    {seed::D12, seed::D13, seed::D11},
    {seed::D25, seed::D26, seed::D14},
    {seed::D29, seed::D27, seed::D15},
};

DMA_Stream_TypeDef* const kPwmStream[kMaxPorts] = {DMA2_Stream5, DMA2_Stream6, DMA2_Stream7, DMA2_Stream0,
                                                   DMA2_Stream1};
constexpr uint32_t kPwmRequest[kMaxPorts] = {DMA_REQUEST_TIM5_UP, DMA_REQUEST_TIM5_CH1, DMA_REQUEST_TIM5_CH2,
                                             DMA_REQUEST_TIM5_CH3, DMA_REQUEST_TIM5_CH4};

DMA_BUFFER_MEM_SECTION uint32_t gPwmWords[kMaxPorts][rvled::kPwmSteps];

class LedPwm {
public:
    // After hw.Init(); returns false (and leaves libDaisy's PWM in charge) if
    // anything is off.
    bool start()
    {
        // Group the 12 pins by port.
        for (int led = 0; led < kNumLeds; ++led) {
            for (int c = 0; c < kPinsPerLed; ++c) {
                const Pin pin = kLedPin[led][c];
                int       port = 0;
                while (port < numPorts_ && ports_[port].id != pin.port) ++port;
                if (port == numPorts_) {
                    if (numPorts_ == kMaxPorts) return false;
                    ports_[numPorts_++].id = pin.port;
                }
                Port& p = ports_[port];
                p.pins[p.numPins]   = rvled::pwmPin(pin.pin, true); // active low
                p.counts[p.numPins] = 0;
                slot_[led][c]       = {port, p.numPins++};
            }
        }
        // The tables must sit in the non-cacheable window (see above).
        const uintptr_t lo = uintptr_t(&gPwmWords[0][0]), hi = lo + sizeof gPwmWords;
        if (lo < 0x30000000u || hi > 0x30008000u) return false;
        for (int p = 0; p < numPorts_; ++p)
            rvled::fillPwmWords(gPwmWords[p], rvled::kPwmSteps, ports_[p].pins, ports_[p].counts, ports_[p].numPins);

        // TIM5 (free: libDaisy's System uses TIM2), one update per PWM step.
        // Its clock is 2x PCLK1 (240 MHz at 480 MHz boost; libDaisy's
        // TimerHandle::GetFreq() uses the same rule), so ~469 ticks a step.
        // The period goes in at Init, which loads it straight away.
        const float ticks = float(System::GetPClk1Freq()) * 2.0f / (kPwmHz * float(rvled::kPwmSteps));
        TimerHandle::Config tc;
        tc.periph = TimerHandle::Config::Peripheral::TIM_5;
        tc.dir    = TimerHandle::Config::CounterDir::UP;
        tc.period = std::max(uint32_t(8), uint32_t(ticks + 0.5f)) - 1;
        if (timer_.Init(tc) != TimerHandle::Result::OK) return false;
        TIM5->CCR1 = 1; // compare events at distinct points of each step,
        TIM5->CCR2 = 2; // each raising one stream's DMA request
        TIM5->CCR3 = 3;
        TIM5->CCR4 = 4;
        TIM5->DIER |= TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE | TIM_DIER_CC3DE | TIM_DIER_CC4DE;

        __HAL_RCC_DMA2_CLK_ENABLE();
        for (int p = 0; p < numPorts_; ++p) {
            DMA_HandleTypeDef& d = dma_[p];
            d.Instance                 = kPwmStream[p];
            d.Init.Request             = kPwmRequest[p];
            d.Init.Direction           = DMA_MEMORY_TO_PERIPH;
            d.Init.PeriphInc           = DMA_PINC_DISABLE;
            d.Init.MemInc              = DMA_MINC_ENABLE;
            d.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
            d.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
            d.Init.Mode                = DMA_CIRCULAR;
            d.Init.Priority            = DMA_PRIORITY_LOW;
            d.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
            // GPIOA..GPIOK sit 0x400 apart in the same order as GPIOPort.
            GPIO_TypeDef* gpio = reinterpret_cast<GPIO_TypeDef*>(GPIOA_BASE + 0x400u * uint32_t(ports_[p].id));
            if (HAL_DMA_Init(&d) != HAL_OK
                || HAL_DMA_Start(&d, uint32_t(gPwmWords[p]), uint32_t(&gpio->BSRR), rvled::kPwmSteps) != HAL_OK) {
                stop();
                return false;
            }
            ++numStarted_;
        }
        if (timer_.Start() != TimerHandle::Result::OK) {
            stop();
            return false;
        }
        // Every stream must be moving: ~100 us is ~50 steps, well short of
        // one full 512-step lap, so a live stream's counter has changed.
        uint32_t before[kMaxPorts];
        for (int p = 0; p < numPorts_; ++p) before[p] = kPwmStream[p]->NDTR;
        System::DelayUs(100);
        for (int p = 0; p < numPorts_; ++p) {
            if (kPwmStream[p]->NDTR == before[p]) {
                stop();
                return false;
            }
        }
        return true;
    }

    // Hand the pins back to libDaisy's PWM: timer and every stream off.
    void stop()
    {
        TIM5->DIER &= ~(TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE | TIM_DIER_CC3DE | TIM_DIER_CC4DE);
        timer_.Stop();
        for (int p = 0; p < numStarted_; ++p) HAL_DMA_Abort(&dma_[p]);
        numStarted_ = 0;
    }

    // False once any stream has stopped (the DMA clears EN on a bus error).
    bool running() const
    {
        for (int p = 0; p < numPorts_; ++p)
            if (!(kPwmStream[p]->CR & DMA_SxCR_EN)) return false;
        return true;
    }

    // Drive values 0..1 (rvled "drive": cubed here), rebuilding only the
    // tables whose counts changed.
    void set(int led, const rvled::Rgb& c)
    {
        const float v[kPinsPerLed] = {c.r, c.g, c.b};
        for (int k = 0; k < kPinsPerLed; ++k) {
            const Slot s = slot_[led][k];
            const int  n = rvled::pwmCount(v[k]);
            if (ports_[s.port].counts[s.pin] != n) {
                ports_[s.port].counts[s.pin] = n;
                ports_[s.port].dirty         = true;
            }
        }
    }

    void flush()
    {
        for (int p = 0; p < numPorts_; ++p) {
            if (!ports_[p].dirty) continue;
            rvled::fillPwmWords(gPwmWords[p], rvled::kPwmSteps, ports_[p].pins, ports_[p].counts, ports_[p].numPins);
            ports_[p].dirty = false;
        }
    }

private:
    struct Port {
        GPIOPort      id      = PORTX;
        int           numPins = 0;
        bool          dirty   = false;
        rvled::PwmPin pins[kNumLeds * kPinsPerLed];
        int           counts[kNumLeds * kPinsPerLed] = {};
    };
    struct Slot {
        int port = 0, pin = 0;
    };
    Port              ports_[kMaxPorts];
    int               numPorts_   = 0;
    int               numStarted_ = 0;
    Slot              slot_[kNumLeds][kPinsPerLed];
    TimerHandle       timer_;
    DMA_HandleTypeDef dma_[kMaxPorts] = {};
};

LedPwm gLedPwm;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    hw.tap.Debounce(); // ProcessAllControls() only handles knobs (SPEC §8.1)

    const int springsPos  = SwitchPosition(DaisyVersio::SW_0, kSpringsSwitchInverted);
    const int attitudePos = SwitchPosition(DaisyVersio::SW_1, kAttitudeSwitchInverted);

    // The gate (ThrowHold.h gateRole): in positions 1-2 it is THROW (ADR
    // 0039: the Tank gets every change at the block's start and keeps the
    // latch; unpatched it reads low, so the send stays open until the first
    // rising edge); in position 3 it is echo mode's clock (ADR 0041). Every
    // rising edge also feeds the clock in every position, so the tempo is
    // known before SPRINGS reaches 3. The button always kicks.
    static bool lastGate = false;
    const bool  gate     = hw.Gate();
    if (gate != lastGate) tank.gate(gate, 0);
    if (gate && !lastGate) tank.clock(0);
    lastGate = gate;
    if (hw.tap.RisingEdge()) tank.kick(0);
    // KICK held >= kThrowExitHoldSeconds: throw mode off (ADR 0039), once
    // per press; the Kick itself already fired on the press. Holding it
    // with throw mode off does nothing more (ADR 0013).
    static bool exitDone = false;
    if (!hw.tap.Pressed()) exitDone = false;
    else if (!exitDone && hw.tap.TimeHeldMs() >= 1000.0f * rv::throwhold::kThrowExitHoldSeconds) {
        exitDone = true;
        if (tank.exitThrowMode()) gThrowExited = true;
    }

    for (int p = 0; p < DaisyVersio::KNOB_LAST; ++p)
        tank.setParam(kPotParams[p], rvpot::endStops(hw.GetKnobValue(kPotKnob[p]))); // exact 0 / 1 at the stops

    tank.setParam(rv::ParamId::Springs, rv::switchToNormalised(springsPos));
    tank.setParam(rv::ParamId::Attitude, rv::switchToNormalised(attitudePos));

    // Meters: only a per-block abs peak per channel here; all smoothing and
    // colour maths runs in the main loop.
    float inL = 0.0f, inR = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        inL = std::max(inL, std::fabs(in[0][i]));
        inR = std::max(inR, std::fabs(in[1][i]));
    }

    tank.process(in[0], in[1], out[0], out[1], int(size));
    float outL = 0.0f, outR = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        out[0][i] *= kOutputTrim;
        out[1][i] *= kOutputTrim;
        outL = std::max(outL, std::fabs(out[0][i]));
        outR = std::max(outR, std::fabs(out[1][i]));
    }

    gPeak[kInL]  = std::max(float(gPeak[kInL]), inL);
    gPeak[kInR]  = std::max(float(gPeak[kInR]), inR);
    gPeak[kOutL] = std::max(float(gPeak[kOutL]), outL);
    gPeak[kOutR] = std::max(float(gPeak[kOutR]), outR);
    gLimiterGain = std::min(float(gLimiterGain), tank.limiterGain());
}

} // namespace

int main()
{
    new (gTankStorage) rv::Tank(); // before anything touches it (see gTankStorage)
    hw.Init(true); // boost to 480 MHz
    hw.SetAudioBlockSize(kBlockSize);
    gTankPrepared = PrepareTank();

    BootPattern(); // then metering

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // Our own PWM from here on (see LedPwm); libDaisy's is only the fallback.
    bool ledDma = gLedPwm.start();

    rvled::LevelMeter meters[kNumMeters];
    uint32_t          lastUs = System::GetUs();
    while (true) {
        const uint32_t nowUs = System::GetUs();
        const float    dt    = float(nowUs - lastUs) * 1.0e-6f;
        lastUs               = nowUs;

        float peak[kNumMeters];
        for (int m = 0; m < kNumMeters; ++m) {
            peak[m]  = gPeak[m];
            gPeak[m] = 0.0f;
        }
        const float limiterGain = gLimiterGain;
        gLimiterGain            = 1.0f;

        // Red: input near the ADC's full scale (the jack's clip point);
        // output while the Tank's safety limiter pulls the wet down. The
        // limiter is stereo-linked, so both output LEDs go red together.
        const bool limiting = rvled::limiterReducing(limiterGain);
        meters[kInL].update(peak[kInL], rvled::inputNearClip(peak[kInL]), dt);
        meters[kInR].update(peak[kInR], rvled::inputNearClip(peak[kInR]), dt);
        meters[kOutL].update(peak[kOutL], limiting, dt);
        meters[kOutR].update(peak[kOutR], limiting, dt);

        // The meters (and so dt) still update about once a millisecond; the
        // PWM runs on its own (DMA) and needs no call per step.
        if (ledDma && !gLedPwm.running()) {
            gLedPwm.stop();
            ledDma = false;
        }
        // Throw mode off (ADR 0039): all four white for a moment, the one
        // exception to the meters-only LEDs (ADR 0031).
        static uint32_t blinkUntilUs = 0;
        if (gThrowExited) {
            gThrowExited = false;
            blinkUntilUs = nowUs + uint32_t(1.0e6f * rv::throwhold::kThrowExitBlinkSeconds);
        }
        const bool blink = int32_t(blinkUntilUs - nowUs) > 0;
        for (int m = 0; m < kNumMeters; ++m) {
            const rvled::Rgb c = blink ? rvled::Rgb{1.0f, 1.0f, 1.0f} : meters[m].colour();
            if (ledDma) gLedPwm.set(int(kMeterLed[m]), c);
            else hw.SetLed(kMeterLed[m], c.r, c.g, c.b);
        }
        if (ledDma) gLedPwm.flush();
        else hw.UpdateLeds();
        System::Delay(1);
    }
}

#endif
