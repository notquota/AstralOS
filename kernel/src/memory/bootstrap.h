
#pragma once

#include <stdint.h>

#include "paging.h"

namespace Astral::MemoryBootstrap {

struct Configuration {
    // Offset supplied by Limine HHDM response.
    uint64_t hhdmOffset;

    // Must return a unique, reserved physical
    // 4 KiB frame on every successful call.
    Paging::AllocatePage allocatePage;

    // Number of contiguous pages requested
    // for the dedicated DMA region.
    uint32_t dmaPages;
};

struct Status {
    bool pagingReady;
    bool mmioReady;
    bool dmaReady;

    uint64_t dmaPhysicalBase;
    uint64_t dmaVirtualBase;
    uint64_t dmaBytes;
};

// Call only after the physical frame allocator
// has been initialized.
bool initialize(const Configuration& configuration);

bool initialized();

const Status& status();

}
