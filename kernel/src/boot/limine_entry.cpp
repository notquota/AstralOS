#include <stdint.h>
#include <limine.h>

#include "boot_info.h"
#include "../arch/x86_64/lowlevel.h"
#include "../logging/serial.h"

using namespace Astral;

// Main AstralOS kernel entry point
extern "C" void kernel_main(BootInfo* info);

// C++ global constructors
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();

extern void (*__ctors_start[])();
extern void (*__ctors_end[])();

namespace {

// Limine boot protocol revision
__attribute__((used, section(".limine_requests")))
volatile uint64_t base_revision[] =
    LIMINE_BASE_REVISION(6);

// Framebuffer request
__attribute__((used, section(".limine_requests")))
volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

// Physical memory map request
__attribute__((used, section(".limine_requests")))
volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

// Limine request markers
__attribute__((used, section(".limine_requests_start")))
volatile uint64_t requests_start[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t requests_end[] =
    LIMINE_REQUESTS_END_MARKER;

// Halt processor
[[noreturn]]
void halt() {
    astral_cli();

    while (true) {
        astral_hlt();
    }
}

// Early boot logger
void boot_log(const char* message) {
    Serial::write("[BOOT] ");
    Serial::write(message);
    Serial::write("\n");
}

// Fatal boot error
[[noreturn]]
void boot_error(const char* message) {
    Serial::write("[ERROR] ");
    Serial::write(message);
    Serial::write("\n");

    halt();
}

// Initialize global C++ constructors
void initialize_constructors() {
    for (auto ctor = __init_array_start;
         ctor < __init_array_end;
         ++ctor) {
        (*ctor)();
    }

    for (auto ctor = __ctors_end;
         ctor > __ctors_start;) {
        (*--ctor)();
    }
}

// Determine framebuffer pixel format
bool detect_pixel_format(
    const limine_framebuffer* fb,
    PixelFormat& format
) {
    if (fb->red_mask_size != 8 ||
        fb->green_mask_size != 8 ||
        fb->blue_mask_size != 8) {
        return false;
    }

    if (fb->red_mask_shift == 0 &&
        fb->green_mask_shift == 8 &&
        fb->blue_mask_shift == 16) {
        format = PixelFormat::RGBX;
        return true;
    }

    if (fb->red_mask_shift == 16 &&
        fb->green_mask_shift == 8 &&
        fb->blue_mask_shift == 0) {
        format = PixelFormat::BGRX;
        return true;
    }

    return false;
}

// Validate Limine framebuffer
limine_framebuffer* get_framebuffer() {
    auto* response = framebuffer_request.response;

    if (!response ||
        response->framebuffer_count == 0 ||
        !response->framebuffers) {
        boot_error("No framebuffer available");
    }

    auto* fb = response->framebuffers[0];

    if (!fb || !fb->address) {
        boot_error("Invalid framebuffer address");
    }

    if (fb->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        fb->bpp != 32) {
        boot_error("Unsupported framebuffer format");
    }

    if (fb->width == 0 || fb->height == 0 ||
        fb->width > UINT32_MAX / 4 ||
        fb->height > UINT32_MAX ||
        fb->pitch > UINT32_MAX ||
        fb->pitch < fb->width * 4) {
        boot_error("Invalid framebuffer dimensions");
    }

    return fb;
}

// Validate physical memory map
limine_memmap_response* get_memory_map() {
    auto* response = memmap_request.response;

    if (!response ||
        !response->entries ||
        response->entry_count == 0) {
        boot_error("Memory map unavailable");
    }

    if (response->entry_count >
        MAX_MEMORY_REGIONS) {
        boot_error("Memory map exceeds capacity");
    }

    for (uint64_t i = 0;
         i < response->entry_count;
         ++i) {
        if (!response->entries[i]) {
            boot_error("Invalid memory region");
        }
    }

    return response;
}

// Populate AstralOS boot information
void prepare_boot_info(
    BootInfo& info,
    const limine_framebuffer* fb,
    const limine_memmap_response* memory
) {
    info.framebuffer.address =
        static_cast<uint8_t*>(fb->address);

    info.framebuffer.width =
        static_cast<uint32_t>(fb->width);

    info.framebuffer.height =
        static_cast<uint32_t>(fb->height);

    info.framebuffer.pitch =
        static_cast<uint32_t>(fb->pitch);

    PixelFormat format{};

    if (!detect_pixel_format(fb, format)) {
        boot_error("Unsupported pixel layout");
    }

    info.framebuffer.format = format;

    info.memoryRegionCount =
        static_cast<uint32_t>(memory->entry_count);

    for (uint32_t i = 0;
         i < info.memoryRegionCount;
         ++i) {
        const auto* entry = memory->entries[i];

        info.memoryMap[i].base = entry->base;
        info.memoryMap[i].length = entry->length;

        info.memoryMap[i].type =
            static_cast<MemoryType>(entry->type);
    }
}

} // namespace

// Limine entry point
extern "C" void kmain() {
    astral_cli();

    Serial::initialize();
    boot_log("AstralOS boot entry reached");

    // Verify supported boot protocol
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
        boot_error("Unsupported Limine revision");
    }

    boot_log("Limine protocol verified");

    // Prepare C++ runtime
    initialize_constructors();
    boot_log("C++ runtime initialized");

    // Retrieve display
    auto* framebuffer = get_framebuffer();
    boot_log("Framebuffer validated");

    // Retrieve physical memory information
    auto* memory = get_memory_map();
    boot_log("Physical memory map received");

    // Build kernel boot information
    static BootInfo info{};

    prepare_boot_info(info, framebuffer, memory);
    boot_log("Boot information prepared");

    // Transfer control to the AstralOS kernel
    boot_log("Starting AstralOS kernel");

    kernel_main(&info);

    // The kernel should never return
    boot_error("Kernel returned unexpectedly");
}