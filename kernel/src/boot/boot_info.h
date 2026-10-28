#pragma once
#include <stdint.h>

namespace Astral {

enum class PixelFormat : uint32_t {
    RGBX = 0,
    BGRX = 1
};

struct Framebuffer {
    uint8_t* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    PixelFormat format;
};

enum class MemoryType : uint64_t {
    Usable = 0,
    Reserved = 1,
    ACPIReclaimable = 2,
    ACPINVS = 3,
    BadMemory = 4,
    BootloaderReclaimable = 5,
    ExecutableAndModules = 6,
    Framebuffer = 7,
    ReservedMapped = 8
};

struct MemoryRegion {
    uint64_t base;
    uint64_t length;
    MemoryType type;
};

constexpr uint32_t MAX_MEMORY_REGIONS = 128;

struct BootInfo {
    Framebuffer framebuffer;

    MemoryRegion memoryMap[MAX_MEMORY_REGIONS];
    uint32_t memoryRegionCount;
};

}