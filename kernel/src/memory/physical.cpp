
#include "physical.h"

namespace Astral::Memory {

    

uint64_t total_memory_mib() {
    return total_pages() / 256;
}

uint64_t free_memory_mib() {
    return free_pages() / 256;
}



struct RegionState {
    uint64_t next;
    uint64_t end;
};

static RegionState regions[MAX_MEMORY_REGIONS]{};

static uint32_t regionCount = 0;
static uint32_t currentRegion = 0;

static uint64_t totalPages = 0;
static uint64_t freePages = 0;

static bool ready = false;

static bool align_up(
    uint64_t value,
    uint64_t& result
) {
    if (value > UINT64_MAX - (PAGE_SIZE - 1))
        return false;

    result =
        (value + PAGE_SIZE - 1) &
        ~(PAGE_SIZE - 1);

    return true;
}

bool initialize(const BootInfo& info) {
    ready = false;

    regionCount = 0;
    currentRegion = 0;
    totalPages = 0;
    freePages = 0;

    if (info.memoryRegionCount == 0 ||
        info.memoryRegionCount > MAX_MEMORY_REGIONS) {
        return false;
    }

    for (uint32_t i = 0;
         i < info.memoryRegionCount;
         ++i) {

        const MemoryRegion& region =
            info.memoryMap[i];

        if (region.type != MemoryType::Usable)
            continue;

        if (region.length == 0)
            continue;

        if (region.base > UINT64_MAX - region.length)
            return false;

        uint64_t start;

        if (!align_up(region.base, start))
            return false;

        uint64_t end =
            (region.base + region.length) &
            ~(PAGE_SIZE - 1);

        if (end <= start)
            continue;

        uint64_t pages =
            (end - start) / PAGE_SIZE;

        if (totalPages > UINT64_MAX - pages)
            return false;

        if (regionCount >= MAX_MEMORY_REGIONS)
            return false;

        regions[regionCount].next = start;
        regions[regionCount].end = end;

        ++regionCount;
        totalPages += pages;
    }

    if (totalPages == 0)
        return false;

    freePages = totalPages;
    ready = true;

    return true;
}

uint64_t allocate_page() {
    if (!ready || freePages == 0)
        return 0;

    while (currentRegion < regionCount) {
        RegionState& region =
            regions[currentRegion];

        if (region.next < region.end) {
            uint64_t address = region.next;

            region.next += PAGE_SIZE;
            --freePages;

            return address;
        }

        ++currentRegion;
    }

    return 0;
}

uint64_t total_pages() {
    return totalPages;
}

uint64_t free_pages() {
    return freePages;
}

bool initialized() {
    return ready;
}

}