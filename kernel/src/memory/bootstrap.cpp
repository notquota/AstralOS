
#include "bootstrap.h"

#include "paging.h"
#include "backend.h"

#include "../hardware/mmio.h"

namespace Astral::MemoryBootstrap {

static constexpr uint64_t PAGE_SIZE = 4096;

static Status current{};
static bool ready = false;

static bool reserve_dma(
    Paging::AllocatePage allocator,
    uint32_t pages,
    MemoryBackend::DmaRegion& result
) {
    result = {};

    if (!allocator || pages == 0)
        return false;

    uint64_t firstPhysical = 0;
    uint64_t previousPhysical = 0;

    for (uint32_t i = 0; i < pages; ++i) {
        uint64_t physical = 0;

        if (!allocator(physical))
            return false;

        if (!physical ||
            (physical & (PAGE_SIZE - 1)))
            return false;

        if (i == 0) {
            firstPhysical = physical;
        } else {
            // DMA pool must occupy consecutive
            // physical page frames.
            if (previousPhysical >
                UINT64_MAX - PAGE_SIZE)
                return false;

            if (physical !=
                previousPhysical + PAGE_SIZE)
                return false;
        }

        previousPhysical = physical;
    }

    if (pages > UINT64_MAX / PAGE_SIZE)
        return false;

    const uint64_t bytes =
        static_cast<uint64_t>(pages) * PAGE_SIZE;

    void* virtualBase =
        Paging::physical_to_virtual(firstPhysical);

    if (!virtualBase)
        return false;

    result.virtualBase = virtualBase;
    result.physicalBase = firstPhysical;
    result.size = bytes;

    return true;
}

bool initialize(const Configuration& configuration) {
    if (ready)
        return true;

    current = {};

    if (!configuration.allocatePage ||
        configuration.dmaPages == 0)
        return false;

    if (!Paging::initialize(
        configuration.hhdmOffset,
        configuration.allocatePage
    ))
        return false;

    current.pagingReady = true;

    // Reserve the DMA pool after the paging
    // manager has been initialized.
    MemoryBackend::DmaRegion dmaRegion{};

    if (!reserve_dma(
        configuration.allocatePage,
        configuration.dmaPages,
        dmaRegion
    ))
        return false;

    current.dmaPhysicalBase =
        dmaRegion.physicalBase;

    current.dmaVirtualBase =
        reinterpret_cast<uint64_t>(
            dmaRegion.virtualBase
        );

    current.dmaBytes = dmaRegion.size;

    MemoryBackend::Configuration backend{};

    backend.deviceMapper =
        Paging::map_device;

    backend.dmaRegion =
        dmaRegion;

    if (!MemoryBackend::initialize(backend))
        return false;

    current.mmioReady =
        MMIO::available();

    current.dmaReady =
        MemoryBackend::statistics().dmaReady;

    ready =
        current.pagingReady &&
        current.mmioReady &&
        current.dmaReady;

    return ready;
}

bool initialized() {
    return ready;
}

const Status& status() {
    return current;
}

}
