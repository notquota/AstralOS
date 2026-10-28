
#pragma once

#include <stdint.h>

#include "../hardware/mmio.h"

namespace Astral::MemoryBackend {

struct DmaRegion {
    void* virtualBase;
    uint64_t physicalBase;
    uint64_t size;
};

struct Configuration {
    // Maps physical device register memory into
    // an uncached, accessible virtual address.
    MMIO::MapFunction deviceMapper;

    // Dedicated, reserved, physically contiguous
    // DMA memory region.
    DmaRegion dmaRegion;
};

struct Statistics {
    uint64_t totalBytes;
    uint64_t allocatedBytes;
    uint64_t allocationCount;
    uint64_t failedAllocations;

    bool mmioReady;
    bool dmaReady;
};

// Requires a correctly configured virtual-memory
// system and a reserved DMA memory region.
bool initialize(const Configuration& configuration);

bool initialized();

const Statistics& statistics();

void reset();

}
