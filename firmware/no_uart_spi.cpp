// Stub out libDaisy's global UART/SPI DMA-scheduler bookkeeping so the
// linker never has to pull in uart.o/spi.o (and the HAL modules they drag
// in behind them: stm32h7xx_hal_uart.o, _usart.o, _spi.o, ...) — flash
// budget work (SPEC/ADR 0011). See docs/adr/0011 and firmware/README.md.
//
// Why this is safe: libDaisy's System::Init() (src/sys/system.cpp) always
// calls dsy_uart_global_init() and dsy_spi_global_init() unconditionally,
// regardless of whether the app ever touches a UartHandler or SpiHandle.
// Those functions (libs/libDaisy/src/per/uart.cpp, spi.cpp) do nothing but
// reset a small static DMA-job-queue array — no register writes, no clock
// enables, no pin configuration. The Versio firmware (this repo) never
// constructs a UartHandler or SpiHandle directly in any of the three
// variants (SPEC hardware facts: no UART/SPI peripheral use on Versio), so
// that bookkeeping is dead weight. Neither libDaisy symbol is declared
// `weak`, so plain same-signature definitions here — linked ahead of
// libdaisy.a on the command line (see firmware/Makefile: CPP_SOURCES lists
// this file, and project object files always precede -ldaisy) — win symbol
// resolution and the archive member is never pulled in for that reference.
//
// SPI: no other code in any variant references anything else in spi.o, so
// this stub (plus the IRQ handlers below) is enough to drop it everywhere.
//
// UART: libDaisy's own logger.o (used by hw.seed.StartLog()/PrintLine())
// references UartHandler::Init() internally for its USB-vs-UART destination
// template, even when only the USB destination is used — that pulls uart.o
// back in regardless of this stub, and *conflicts* with it (duplicate
// dsy_uart_global_init/IRQ handler symbols) wherever both land in the same
// link. So the UART half of this stub excludes m0test: m0test is the only
// variant that still calls hw.seed.StartLog()/PrintLine() (SPEC §7 M0,
// unchanged on purpose — see main.cpp's m0test block header). release never
// logs at all (ADR 0011), and profile talks to USB CDC directly via
// hw.seed.usb_handle (main.cpp's RV_MODE_PROFILE block) instead of through
// Logger, specifically so it doesn't need logger.o/uart.o either — see the
// comment above that call for why StartLog() itself was dropped there.
//
// If a future milestone needs a real UartHandler or SpiHandle (e.g. MIDI
// over UART) in release or profile, delete the relevant half of this file
// first.
//
// USB itself is deliberately NOT stubbed here even though the Versio
// doesn't use raw peripheral USB either: profile and m0test both need USB
// CDC for serial logging, and disabling it would need touching libs/libDaisy
// (DaisySeed::Configure's own USB setup, not a simple global-bookkeeping
// call like these) — out of scope for a firmware/-only, no-libs-patch
// change. See building.md / README for the measured effect of this file.

extern "C" void dsy_spi_global_init() {}
extern "C" void SPI1_IRQHandler() {}
extern "C" void SPI2_IRQHandler() {}
extern "C" void SPI3_IRQHandler() {}
extern "C" void SPI4_IRQHandler() {}
extern "C" void SPI5_IRQHandler() {}
extern "C" void SPI6_IRQHandler() {}

#if !defined(RV_MODE_M0TEST)
extern "C" void dsy_uart_global_init() {}
extern "C" void USART1_IRQHandler() {}
extern "C" void USART2_IRQHandler() {}
extern "C" void USART3_IRQHandler() {}
extern "C" void UART4_IRQHandler() {}
extern "C" void UART5_IRQHandler() {}
extern "C" void USART6_IRQHandler() {}
extern "C" void UART7_IRQHandler() {}
extern "C" void UART8_IRQHandler() {}
extern "C" void LPUART1_IRQHandler() {}
#endif

// USB host (1 Oct 2026, flash): libDaisy's OTG_HS IRQ handlers
// (src/sys/system.cpp) reference the host handle hhcd_USB_OTG_HS (defined
// in usbh_conf.o) and HAL_HCD_IRQHandler, which drags in the whole USB host
// stack (~3 KB: usbh_core, usbh_ctlreq, usbh_ioreq, hal_hcd). The Versio
// never runs as a USB host, so the handle's Instance stays null and those
// calls never happen (each is guarded by `if (hhcd_USB_OTG_HS.Instance)`).
// A zeroed handle and an empty handler here keep the guard's meaning and
// let the linker leave the host stack out. The USB device path (serial in
// the profile build, firmware updates) is untouched.
#include "stm32h7xx_hal.h"
extern "C" {
HCD_HandleTypeDef hhcd_USB_OTG_HS;
void HAL_HCD_IRQHandler(HCD_HandleTypeDef*) {}
}

// printf / putchar (2 Oct 2026, flash): libDaisy is built with
// USBD_DEBUG_LEVEL 3 (src/usbd/usbd_conf.h), so the USB device core's
// USBD_UsrLog / USBD_ErrLog call printf and putchar, which pull newlib's
// whole stdio (vfprintf, the FILE machinery, malloc for its buffers: ~3 KB)
// into release and profile. Their output had nowhere to go: no variant
// retargets stdout (_write is newlib's always-failing stub, see the link
// warnings), so these do exactly what the real ones did here, minus the
// code. m0test keeps libc's: its Logger may reference other printf-family
// members from the same archive objects.
//
// exit (same day, flash): crt0 calls exit() if main() ever returns, and
// newlib's exit runs the stdio clean-up (__stdio_exit_handler), which pulls
// the FILE machinery, fflush, malloc and the read/write/lseek/close stubs
// behind it (~1.5 KB) into every build. main() never returns here (the
// firmware loops forever), so this exit is never called; if it were, it
// parks the core, as returning from main on bare metal would.
#if !defined(RV_MODE_M0TEST)
extern "C" int printf(const char*, ...) { return 0; }
extern "C" int putchar(int c) { return c; }
extern "C" void exit(int)
{
    for (;;) {}
}
#endif
