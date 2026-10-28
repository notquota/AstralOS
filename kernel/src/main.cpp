
#include <stdint.h>

// ==================================================
// AstralOS v0.0.6-alpha
// Main Kernel Entry
// Architecture: x86-64
// Bootloader: Limine
// ==================================================

// ----------------------------------------------
// Boot information
// ----------------------------------------------

#include "boot/boot_info.h"

// ----------------------------------------------
// CPU / architecture
// ----------------------------------------------

#include "arch/x86_64/cpu.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/lowlevel.h"

// ----------------------------------------------
// Graphics
// ----------------------------------------------

#include "graphics/framebuffer.h"
#include "graphics/console.h"
#include "graphics/theme.h"
#include "graphics/loading_screen.h"

// ----------------------------------------------
// Logging
// ----------------------------------------------

#include "logging/serial.h"

// ----------------------------------------------
// Memory
// ----------------------------------------------

#include "memory/physical.h"
#include "memory/backend.h"

// ----------------------------------------------
// Hardware
// ----------------------------------------------

#include "hardware/pci.h"

// ----------------------------------------------
// Drivers
// ----------------------------------------------

#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/manager.h"

// ----------------------------------------------
// System configuration
// ----------------------------------------------

#include "config/settings.h"

// ----------------------------------------------
// AstralFS
// ----------------------------------------------

#include "filesystem/vfs.h"

// ----------------------------------------------
// Terminal
// ----------------------------------------------

#include "terminal/terminal.h"

// ----------------------------------------------
// System
// ----------------------------------------------

#include "system/panic.h"
#include "system/driver_runtime.h"

using namespace Astral;

// ==================================================
// AstralOS Boot State
// ==================================================

namespace {

struct BootState {
    bool cpuReady;
    bool framebufferReady;
    bool physicalMemoryReady;
    bool physicalMemoryTestReady;

    bool keyboardReady;
    bool mouseReady;

    bool configurationReady;
    bool filesystemReady;

    bool pciReady;
    bool driverRuntimeReady;

    bool memoryBackendReady;
    bool networkReady;
};

static BootState bootState{};

constexpr uint32_t STARTUP_TASKS = 9;

} // namespace

// ==================================================
// Early Kernel Halt
// ==================================================

[[noreturn]]
static void early_halt(const char* reason) {
    astral_cli();

    Serial::write("[FATAL] ");
    Serial::write(reason);
    Serial::write("\n");

    for (;;) {
        astral_hlt();
    }
}

// ==================================================
// Physical Memory Allocation Test
// ==================================================

static bool test_memory() {
    if (!Memory::initialized())
        return false;

    if (Memory::free_pages() < 2)
        return false;

    const uint64_t before =
        Memory::free_pages();

    const uint64_t page1 =
        Memory::allocate_page();

    const uint64_t page2 =
        Memory::allocate_page();

    if (page1 == 0 || page2 == 0)
        return false;

    if (page1 == page2)
        return false;

    if ((page1 % Memory::PAGE_SIZE) != 0)
        return false;

    if ((page2 % Memory::PAGE_SIZE) != 0)
        return false;

    if (Memory::free_pages() != before - 2)
        return false;

    Serial::write(
        "[MEMORY] Physical allocation test passed\n"
    );

    return true;
}

// ==================================================
// CPU Initialization
// ==================================================

static bool initialize_cpu() {
    CPU::initialize_idt();

    Serial::write(
        "[CPU] Exception IDT installed\n"
    );

    if (!CPU::supports_long_mode()) {
        Serial::write(
            "[CPU] Long mode unsupported\n"
        );

        return false;
    }

    bootState.cpuReady = true;

    return true;
}

// ==================================================
// Physical Memory Initialization
// ==================================================

static bool initialize_memory(BootInfo* bootInfo) {
    if (!bootInfo)
        return false;

    bootState.physicalMemoryReady =
        Memory::initialize(*bootInfo);

    if (!bootState.physicalMemoryReady)
        return false;

    Serial::write(
        "[MEMORY] Physical page allocator ready\n"
    );

    return true;
}

// ==================================================
// PS/2 Input Initialization
// ==================================================

static bool initialize_input() {
    bootState.keyboardReady =
        Keyboard::initialize();

    if (!bootState.keyboardReady) {
        Serial::write(
            "[INPUT] Keyboard initialization failed\n"
        );

        return false;
    }

    Serial::write(
        "[INPUT] PS/2 keyboard initialized\n"
    );

    // Mouse initialization is optional.
    bootState.mouseReady =
        Mouse::initialize();

    if (bootState.mouseReady) {
        Serial::write(
            "[INPUT] PS/2 mouse initialized\n"
        );
    } else {
        Serial::write(
            "[WARNING] PS/2 mouse unavailable\n"
        );
    }

    return true;
}

// ==================================================
// PCI / Driver Initialization
// ==================================================

static bool initialize_hardware() {
    Serial::write(
        "[HARDWARE] Discovering PCI devices\n"
    );

    bootState.pciReady =
        PCI::initialized() || PCI::initialize();

    if (!bootState.pciReady) {
        Serial::write(
            "[WARNING] PCI initialization failed\n"
        );

        return false;
    }

    Serial::write(
        "[HARDWARE] PCI initialized\n"
    );

    // The memory backend must be initialized
    // with safe page-table mappings and a
    // reserved DMA region before starting
    // any bus-mastering driver.
    bootState.memoryBackendReady =
        MemoryBackend::initialized();

    if (!bootState.memoryBackendReady) {
        Serial::write(
            "[HARDWARE] DMA/MMIO backend not ready\n"
        );

        Serial::write(
            "[HARDWARE] Using detection-only mode\n"
        );
    }

    // Transaction ID zero disables DHCP
    // startup for the first safe integration.
    //
    // The driver manager can still detect
    // PCI devices and initialize supported
    // hardware probes when their prerequisites
    // are available.
    bootState.driverRuntimeReady =
        DriverRuntime::initialize(0);

    if (!bootState.driverRuntimeReady) {
        Serial::write(
            "[WARNING] Driver runtime failed\n"
        );

        return false;
    }

    const DriverRuntime::Status& runtime =
        DriverRuntime::status();

    bootState.networkReady =
        runtime.networkReady;

    Serial::write(
        "[HARDWARE] Driver discovery complete\n"
    );

    if (runtime.storageDetected) {
        Serial::write(
            "[HARDWARE] Storage controller detected\n"
        );
    }

    if (runtime.usbDetected) {
        Serial::write(
            "[HARDWARE] USB controller detected\n"
        );
    }

    if (runtime.audioDetected) {
        Serial::write(
            "[HARDWARE] Audio controller detected\n"
        );
    }

    if (runtime.wirelessDetected) {
        Serial::write(
            "[HARDWARE] Wireless candidate detected\n"
        );
    }

    if (runtime.networkReady) {
        Serial::write(
            "[NETWORK] Ethernet stack initialized\n"
        );
    } else {
        Serial::write(
            "[NETWORK] Network unavailable or disabled\n"
        );
    }

    return true;
}

// ==================================================
// AstralOS Main Kernel Entry
// ==================================================

extern "C"
void kernel_main(BootInfo* bootInfo) {

    // ------------------------------------------
    // Stage 0: Early kernel environment
    // ------------------------------------------

    astral_cli();

    Serial::write(
        "\n"
        "========================================\n"
        " AstralOS v0.0.6-alpha\n"
        " Architecture: x86-64\n"
        " Kernel startup initiated\n"
        "========================================\n"
    );

    if (!bootInfo) {
        early_halt(
            "Missing Limine boot information"
        );
    }

    // ------------------------------------------
    // Stage 1: CPU exception handling
    // ------------------------------------------

    CPU::initialize_idt();

    Serial::write(
        "[BOOT] CPU exception handlers installed\n"
    );

    // ------------------------------------------
    // Stage 2: Framebuffer
    // ------------------------------------------

    bootState.framebufferReady =
        Graphics::initialize(
            bootInfo->framebuffer
        );

    if (!bootState.framebufferReady) {
        early_halt(
            "Framebuffer initialization failed"
        );
    }

    Serial::write(
        "[BOOT] Framebuffer initialized\n"
    );

    // ------------------------------------------
    // Stage 3: Graphical loading screen
    // ------------------------------------------

    Loading::begin(STARTUP_TASKS);

    Loading::complete(
        "CPU exception handlers",
        true
    );

    Loading::complete(
        "Framebuffer graphics",
        true
    );

    // ------------------------------------------
    // Stage 4: CPU verification
    // ------------------------------------------

    bootState.cpuReady =
        CPU::supports_long_mode();

    Loading::complete(
        "CPU architecture verified",
        bootState.cpuReady
    );

    if (!bootState.cpuReady) {
        kernel_panic(
            "CPU CAPABILITY CHECK FAILED"
        );
    }

    Serial::write(
        "[BOOT] CPU capability verification passed\n"
    );

    // ------------------------------------------
    // Stage 5: Physical memory allocator
    // ------------------------------------------

    const bool memoryReady =
        initialize_memory(bootInfo);

    Loading::complete(
        "Physical memory initialized",
        memoryReady
    );

    if (!memoryReady) {
        kernel_panic(
            "PHYSICAL MEMORY INITIALIZATION FAILED"
        );
    }

    // ------------------------------------------
    // Stage 6: Physical allocation test
    // ------------------------------------------

    bootState.physicalMemoryTestReady =
        test_memory();

    Loading::complete(
        "Physical memory verification",
        bootState.physicalMemoryTestReady
    );

    if (!bootState.physicalMemoryTestReady) {
        kernel_panic(
            "PHYSICAL MEMORY TEST FAILED"
        );
    }

    // ------------------------------------------
    // Stage 7: System configuration
    // ------------------------------------------

    Config::initialize();

    bootState.configurationReady = true;

    Serial::write(
        "[CONFIG] System settings initialized\n"
    );

    Loading::complete(
        "System configuration",
        true
    );

    // ------------------------------------------
    // Stage 8: PS/2 input drivers
    // ------------------------------------------

    const bool inputReady =
        initialize_input();

    Loading::complete(
        "PS/2 input initialized",
        inputReady
    );

    if (!inputReady) {
        kernel_panic(
            "KEYBOARD INITIALIZATION FAILED"
        );
    }

    // ------------------------------------------
    // Stage 9: AstralFS
    // ------------------------------------------

    bootState.filesystemReady =
        VFS::initialize();

    Loading::complete(
        "AstralFS initialized",
        bootState.filesystemReady
    );

    if (!bootState.filesystemReady) {
        kernel_panic(
            "ASTRALFS INITIALIZATION FAILED"
        );
    }

    // ------------------------------------------
    // Stage 10: Hardware discovery
    // ------------------------------------------

    const bool hardwareReady =
        initialize_hardware();

    // PCI discovery is optional for the
    // terminal-focused first boot.
    Loading::complete(
        "Hardware driver discovery",
        hardwareReady
    );

    if (!hardwareReady) {
        Serial::write(
            "[WARNING] Hardware discovery incomplete\n"
        );
    }

    // ------------------------------------------
    // Stage 11: Finish loading screen
    // ------------------------------------------

    Serial::write(
        "[BOOT] Completing startup screen\n"
    );

    if (!Loading::finish()) {
        kernel_panic(
            "LOADING SCREEN TIMER FAILED"
        );
    }

    Serial::write(
        "[BOOT] Graphical loading completed\n"
    );

    // ------------------------------------------
    // Stage 12: Console
    // ------------------------------------------

    Console::initialize();

    Serial::write(
        "[CONSOLE] Console initialized\n"
    );

    // ------------------------------------------
    // Stage 13: Terminal
    // ------------------------------------------

    Serial::write(
        "[TERMINAL] Starting AstralOS terminal\n"
    );

    // Terminal::run() preserves:
    //
    // - First-boot root account creation
    // - Root authentication
    // - HPT / User terminal modes
    // - AstralOS native commands
    // - ast commands
    // - AstralFS navigation and file operations
    // - System Settings wizard entry
    //
    // Terminal::run() normally never returns.

    Terminal::run();

    // ------------------------------------------
    // Unexpected terminal exit
    // ------------------------------------------

    early_halt(
        "Terminal subsystem returned unexpectedly"
    );
}
