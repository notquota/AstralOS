#include "../boot/boot_info.h"
#include "graphics/framebuffer.h"
#include "graphics/console.h"
#include "graphics/theme.h"
#include "logging/log.h"
#include "logging/serial.h"
#include "system/panic.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/lowlevel.h"

using namespace Astral;

// Enable only when testing exception handling.
#define ASTRAL_TEST_EXCEPTION 0

[[noreturn]]
static void early_halt(const char* reason) {
    Serial::write("[FATAL] ");
    Serial::write(reason);
    Serial::write("\n");

    astral_cli();

    while (true) {
        astral_hlt();
    }
}

extern "C"
void kernel_main(BootInfo* bootInfo) {

    // 1. Disable interrupts during startup.
    astral_cli();

    // 2. Initialize serial debugging.
    // This is also initialized in limine_entry.cpp,
    // but calling it again is safe here.
    Serial::initialize();

    Serial::write(
        "[BOOT] AstralOS kernel started\n"
    );

    // 3. Initialize CPU exception handlers.
    CPU::initialize_idt();

    Serial::write(
        "[SUCCESS] IDT initialized\n"
    );

    // 4. Validate boot information.
    if (bootInfo == nullptr) {
        early_halt("NO BOOT INFORMATION");
    }

    // 5. Initialize graphics.
    if (!Graphics::initialize(
        bootInfo->framebuffer
    )) {
        early_halt("INVALID FRAMEBUFFER");
    }

    // 6. Initialize graphical console.
    Console::initialize();

    Console::write(
        "ASTRALOS V0.0.1-ALPHA\n",
        Theme::TEXT
    );

    Console::write(
        "X86-64 KERNEL\n\n",
        Theme::TEXT
    );

    // 7. Start kernel logging.
    Log::info("STARTING ASTRALOS");

    Log::success("IDT INITIALIZED");
    Log::success("SERIAL DEBUGGING READY");
    Log::success("FRAMEBUFFER READY");
    Log::success("KERNEL CONSOLE READY");
    Log::success("LOGGING SYSTEM READY");

    // 8. Check CPU capabilities.
    if (!CPU::supports_long_mode()) {
        kernel_panic("CPU NOT SUPPORTED");
    }

    Log::success("CPU CHECK PASSED");

    // 9. Test an invalid opcode exception.
#if ASTRAL_TEST_EXCEPTION
    Log::warning("TESTING CPU EXCEPTION");

    asm volatile("ud2");
#endif

    // 10. Report current development status.
    Log::warning(
        "MEMORY MANAGER NOT INITIALIZED"
    );

    Log::warning(
        "PROCESS SCHEDULER NOT INITIALIZED"
    );

    Log::warning(
        "HARDWARE INTERRUPTS DISABLED"
    );

    // 11. Complete startup.
    Log::success(
        "ASTRALOS INITIAL STARTUP COMPLETE"
    );

    Log::info(
        "ENTERING KERNEL HALT LOOP"
    );

    // 12. Halt CPU until reset or non-maskable event.
    astral_cli();

    while (true) {
        astral_hlt();
    }
}