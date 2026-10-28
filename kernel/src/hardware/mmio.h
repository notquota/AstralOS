
#pragma once

#include <stdint.h>

namespace Astral::MMIO {

// Installed by the kernel virtual memory manager.
//
// The callback must map physical device memory
// into the kernel's virtual address space using
// appropriate uncached/device memory attributes.
//
// Returns nullptr if mapping fails.
using MapFunction = volatile void* (*)(
    uint64_t physicalAddress,
    uint64_t length
);

void set_mapper(MapFunction mapper);

bool available();

volatile void* map(
    uint64_t physicalAddress,
    uint64_t length
);

uint32_t read32(
    const volatile void* base,
    uint32_t offset
);

void write32(
    volatile void* base,
    uint32_t offset,
    uint32_t value
);

}
