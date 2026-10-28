
#include "dma.h"

#include <stdint.h>

namespace Astral::DMA {

static Allocator allocator = nullptr;

static bool power_of_two(uint64_t value) {
    return value != 0 &&
           (value & (value - 1)) == 0;
}

void set_allocator(Allocator callback) {
    allocator = callback;
}

bool available() {
    return allocator != nullptr;
}

bool valid(
    const Buffer& buffer,
    uint64_t minimumSize,
    uint64_t alignment
) {
    if (!buffer.virtualAddress ||
        !buffer.physicalAddress)
        return false;

    if (buffer.size < minimumSize)
        return false;

    if (!power_of_two(alignment))
        return false;

    uint64_t virtualAddress =
        reinterpret_cast<uint64_t>(
            buffer.virtualAddress
        );

    if ((virtualAddress & (alignment - 1)) != 0)
        return false;

    if ((buffer.physicalAddress &
         (alignment - 1)) != 0)
        return false;

    return true;
}

bool allocate(
    uint64_t size,
    uint64_t alignment,
    Buffer& output
) {
    output = {};

    if (!allocator || size == 0 ||
        !power_of_two(alignment))
        return false;

    Buffer candidate{};

    if (!allocator(size, alignment, candidate))
        return false;

    if (!valid(candidate, size, alignment))
        return false;

    // Verify physical range does not overflow.
    if (candidate.physicalAddress >
        UINT64_MAX - size)
        return false;

    output = candidate;
    return true;
}

void zero(Buffer& buffer) {
    if (!buffer.virtualAddress)
        return;

    volatile uint8_t* bytes =
        reinterpret_cast<volatile uint8_t*>(
            buffer.virtualAddress
        );

    for (uint64_t i = 0; i < buffer.size; ++i)
        bytes[i] = 0;

    asm volatile("" ::: "memory");
}

}
