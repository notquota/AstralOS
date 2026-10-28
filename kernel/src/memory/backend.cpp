
#include "backend.h"
#include "dma.h"

#include "../hardware/mmio.h"

#include <stdint.h>

namespace Astral::MemoryBackend {

static bool ready = false;

static DmaRegion region{};
static uint64_t nextOffset = 0;

static Statistics stats{};

static bool is_power_of_two(uint64_t value) {
    return value != 0 &&
           (value & (value - 1)) == 0;
}

static bool align_up(
    uint64_t value,
    uint64_t alignment,
    uint64_t& output
) {
    if (!is_power_of_two(alignment))
        return false;

    const uint64_t mask = alignment - 1;

    if (value > UINT64_MAX - mask)
        return false;

    output = (value + mask) & ~mask;
    return true;
}

static bool allocate_dma(
    uint64_t size,
    uint64_t alignment,
    DMA::Buffer& output
) {
    output = {};

    if (!ready ||
        !region.virtualBase ||
        size == 0 ||
        !is_power_of_two(alignment)) {
        ++stats.failedAllocations;
        return false;
    }

    const uint64_t virtualBase =
        reinterpret_cast<uint64_t>(
            region.virtualBase
        );

    const uint64_t currentPhysical =
        region.physicalBase + nextOffset;

    uint64_t alignedPhysical = 0;

    if (!align_up(
        currentPhysical,
        alignment,
        alignedPhysical
    )) {
        ++stats.failedAllocations;
        return false;
    }

    const uint64_t padding =
        alignedPhysical - currentPhysical;

    if (padding > region.size - nextOffset) {
        ++stats.failedAllocations;
        return false;
    }

    const uint64_t offset =
        nextOffset + padding;

    if (size > region.size - offset) {
        ++stats.failedAllocations;
        return false;
    }

    const uint64_t virtualAddress =
        virtualBase + offset;

    if ((virtualAddress & (alignment - 1)) != 0) {
        ++stats.failedAllocations;
        return false;
    }

    output.virtualAddress =
        reinterpret_cast<void*>(virtualAddress);

    output.physicalAddress =
        region.physicalBase + offset;

    output.size = size;

    nextOffset = offset + size;

    stats.allocatedBytes = nextOffset;
    ++stats.allocationCount;

    return true;
}

bool initialize(const Configuration& configuration) {
    if (ready)
        return false;

    // Do not install callbacks until their
    // configuration has been checked.
    if (!configuration.deviceMapper)
        return false;

    const DmaRegion& supplied =
        configuration.dmaRegion;

    if (!supplied.virtualBase ||
        !supplied.physicalBase ||
        supplied.size < 4096)
        return false;

    const uint64_t virtualBase =
        reinterpret_cast<uint64_t>(
            supplied.virtualBase
        );

    // Require page-aligned DMA pool boundaries.
    if ((virtualBase & 0xFFF) != 0 ||
        (supplied.physicalBase & 0xFFF) != 0)
        return false;

    if (supplied.physicalBase >
        UINT64_MAX - supplied.size)
        return false;

    if (virtualBase >
        UINT64_MAX - supplied.size)
        return false;

    region = supplied;
    nextOffset = 0;

    stats = {};
    stats.totalBytes = supplied.size;

    ready = true;

    // Register the implementations that other
    // hardware drivers already expect.
    MMIO::set_mapper(
        configuration.deviceMapper
    );

    DMA::set_allocator(allocate_dma);

    stats.mmioReady = MMIO::available();
    stats.dmaReady = DMA::available();

    return stats.mmioReady && stats.dmaReady;
}

bool initialized() {
    return ready;
}

const Statistics& statistics() {
    return stats;
}

void reset() {
    // Never reset this allocator once hardware
    // has received DMA addresses. Resetting it
    // could reassign live descriptor memory.
    if (stats.allocationCount != 0)
        return;

    DMA::set_allocator(nullptr);
    MMIO::set_mapper(nullptr);

    region = {};
    nextOffset = 0;
    stats = {};
    ready = false;
}

}
