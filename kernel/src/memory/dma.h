
#pragma once

#include <stdint.h>

namespace Astral::DMA {

struct Buffer {
    void* virtualAddress;
    uint64_t physicalAddress;
    uint64_t size;
};

// Allocates physically contiguous, page-aligned,
// CPU-accessible memory.
//
// The backend must return cache-coherent memory
// suitable for DMA, with a stable physical address.
//
// All allocations in this initial interface
// live until shutdown.
using Allocator = bool (*)(
    uint64_t size,
    uint64_t alignment,
    Buffer& output
);

void set_allocator(Allocator allocator);

bool available();

bool allocate(
    uint64_t size,
    uint64_t alignment,
    Buffer& output
);

void zero(Buffer& buffer);

bool valid(
    const Buffer& buffer,
    uint64_t minimumSize,
    uint64_t alignment
);

}
