
#include <stdint.h>

// ==================================================
// AstralOS v0.0.7-alpha
// Main Kernel Entry
// Architecture: x86-64
// Bootloader: Limine
// ==================================================

// Boot information
#include "boot/boot_info.h"

// CPU / architecture
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/lowlevel.h"

// Graphics
#include "graphics/framebuffer.h"
#include "graphics/console.h"
#include "graphics/theme.h"
#include "graphics/loading_screen.h"

// Logging
#include "logging/serial.h"

// Memory
#include "memory/physical.h"
#include "memory/backend.h"

// Hardware
#include "hardware/pci.h"

// Input drivers
#include "drivers/keyboard.h"
#include "drivers/mouse.h"

// Configuration
#include "config/settings.h"

// AstralFS
#include "filesystem/vfs.h"

// Terminal
#include "terminal/terminal.h"

// Desktop
#include "desktop/desktop.h"
#include "desktop/preferences.h"

// System
#include "system/panic.h"
#include "system/driver_runtime.h"

using namespace Astral;

// ==================================================
// Boot Configuration
// ==================================================

namespace {

// Development-only switch.
//
// TRUE:
//   Loading screen -> Desktop -> Terminal
//
// FALSE:
//   Loading screen -> Terminal
//
// This bypasses graphical-session authentication
// and must only be used for development in a VM.
constexpr bool DESKTOP_BOOT_TEST = false;

constexpr uint32_t STARTUP_TASKS = 9;

struct BootState {
    bool cpuReady = false;
    bool framebufferReady = false;

    bool physicalMemoryReady = false;
    bool memoryTestReady = false;

    bool keyboardReady = false;
    bool mouseReady = false;

    bool configurationReady = false;
    bool filesystemReady = false;

    bool pciReady = false;
    bool driverRuntimeReady = false;

    bool memoryBackendReady = false;
    bool networkReady = false;

    bool desktopReady = false;
};

static BootState bootState{};

}

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

    // Mouse is optional.
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
// PCI / Hardware Driver Discovery
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

    // Do not invent a DMA memory mapping.
    //
    // The paging / memory bootstrap must
    // install a valid backend separately.
    bootState.memoryBackendReady =
        MemoryBackend::initialized();

    if (!bootState.memoryBackendReady) {
        Serial::write(
            "[HARDWARE] MMIO/DMA backend unavailable\n"
        );

        Serial::write(
            "[HARDWARE] Detection-only boot mode\n"
        );
    }

    // DHCP remains disabled for this first
    // graphical Desktop boot test.
    bootState.driverRuntimeReady =
        DriverRuntime::initialize(0);

    if (!bootState.driverRuntimeReady) {
        Serial::write(
            "[WARNING] Driver runtime unavailable\n"
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
            "[NETWORK] Networking unavailable\n"
        );
    }

    return true;
}

// ==================================================
// Desktop Preferences
// ==================================================

static void initialize_desktop() {
    // Desktop is initially disabled.
    //
    // Once AstralFS supports persistent config,
    // load saved desktop preferences here.
    Desktop::initialize_preferences({
        false,
        Desktop::BootMode::Terminal
    });

    bootState.desktopReady = true;

    Serial::write(
        "[DESKTOP] Desktop subsystem configured\n"
    );
}

// ==================================================
// Desktop Development Boot Test
// ==================================================

static void launch_desktop_test() {
    if (!DESKTOP_BOOT_TEST)
        return;

    if (!bootState.desktopReady)
        return;

    if (Graphics::width() < 800 ||
        Graphics::height() < 600) {
        Serial::write(
            "[DESKTOP] Display resolution too small\n"
        );

        return;
    }

    Serial::write(
        "[DESKTOP] Starting development session\n"
    );

    // Enable for the current boot only.
    // This does NOT save a boot preference.
    Desktop::enable();

    // The session returns when the user
    // exits Desktop using Escape or launcher.
    Desktop::run();

    Serial::write(
        "[DESKTOP] Graphical session ended\n"
    );

    // Restore terminal graphics after the
    // Desktop has occupied the framebuffer.
    Console::initialize();
    Console::scroll_to_bottom();
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
        " AstralOS v0.0.7-alpha\n"
        " Architecture: x86-64\n"
        " Graphical Desktop development\n"
        "========================================\n"
    );

    if (!bootInfo) {
        early_halt(
            "Missing Limine boot information"
        );
    }

    // ------------------------------------------
    // Stage 1: CPU exceptions
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
    // Stage 3: Loading screen
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
    // Stage 4: CPU capability check
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
        "[BOOT] CPU verification passed\n"
    );

    // ------------------------------------------
    // Stage 5: Physical memory
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
    // Stage 6: Memory allocation test
    // ------------------------------------------

    bootState.memoryTestReady =
        test_memory();

    Loading::complete(
        "Physical memory verification",
        bootState.memoryTestReady
    );

    if (!bootState.memoryTestReady) {
        kernel_panic(
            "PHYSICAL MEMORY TEST FAILED"
        );
    }

    // ------------------------------------------
    // Stage 7: System configuration
    // ------------------------------------------

    Config::initialize();

    bootState.configurationReady = true;

    Loading::complete(
        "System configuration",
        true
    );

    Serial::write(
        "[CONFIG] System settings initialized\n"
    );

    // ------------------------------------------
    // Stage 8: Keyboard / mouse
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
    // Stage 11: Desktop subsystem
    // ------------------------------------------

    initialize_desktop();

    Serial::write(
        "[DESKTOP] Desktop initialized\n"
    );

    // ------------------------------------------
    // Stage 12: Finish loading
    // ------------------------------------------

    if (!Loading::finish()) {
        kernel_panic(
            "LOADING SCREEN TIMER FAILED"
        );
    }

    Serial::write(
        "[BOOT] Graphical loading completed\n"
    );

    // ------------------------------------------
    // Stage 13: Console
    // ------------------------------------------

    Console::initialize();

    Serial::write(
        "[CONSOLE] Console initialized\n"
    );

    // ------------------------------------------
    // Stage 14: Desktop or terminal
    // ------------------------------------------

    if (DESKTOP_BOOT_TEST) {
        launch_desktop_test();
    }

    // ------------------------------------------
    // Stage 15: Existing terminal
    // ------------------------------------------

    Serial::write(
        "[TERMINAL] Starting AstralOS terminal\n"
    );

    // Preserve:
    // - Root account creation
    // - Root authentication
    // - HPT / User terminal modes
    // - ast commands
    // - AstralFS
    // - System Settings

    Terminal::run();

    early_halt(
        "Terminal subsystem returned unexpectedly"
    );
}
