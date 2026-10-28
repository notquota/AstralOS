
#include "paging.h"

#include <stdint.h>

namespace Astral::Paging {

static constexpr uint64_t PRESENT = 1ull << 0;
static constexpr uint64_t WRITE = 1ull << 1;
static constexpr uint64_t PWT = 1ull << 3;
static constexpr uint64_t PCD = 1ull << 4;
static constexpr uint64_t HUGE = 1ull << 7;
static constexpr uint64_t NX = 1ull << 63;

static constexpr uint64_t ADDRESS_MASK =
    0x000FFFFFFFFFF000ull;

// Dedicated MMIO arena, 16 MiB.
static constexpr uint64_t MMIO_BASE =
    0xFFFFC00000000000ull;

static constexpr uint64_t MMIO_SIZE =
    16ull * 1024ull * 1024ull;

static bool ready = false;

static uint64_t directMapOffset = 0;
static uint64_t mmioUsed = 0;

static AllocatePage frameAllocator = nullptr;
static volatile uint64_t* pml4 = nullptr;

static uint64_t align_down(uint64_t value) {
    return value & ~(PAGE_SIZE - 1);
}

static bool align_up(
    uint64_t value,
    uint64_t& output
) {
    if (value > UINT64_MAX - (PAGE_SIZE - 1))
        return false;

    output =
        (value + PAGE_SIZE - 1) &
        ~(PAGE_SIZE - 1);

    return true;
}

static uint64_t read_cr3() {
    uint64_t value = 0;

    asm volatile(
        "mov %%cr3, %0"
        : "=r"(value)
    );

    return value & ADDRESS_MASK;
}

static void invalidate(uint64_t address) {
    asm volatile(
        "invlpg (%0)"
        :
        : "r"(address)
        : "memory"
    );
}

static void clear_page(void* address) {
    auto* bytes =
        reinterpret_cast<volatile uint8_t*>(address);

    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
        bytes[i] = 0;
}

void* physical_to_virtual(uint64_t physical) {
    if (!ready)
        return nullptr;

    if (physical > UINT64_MAX - directMapOffset)
        return nullptr;

    return reinterpret_cast<void*>(
        directMapOffset + physical
    );
}

static volatile uint64_t* table_at(uint64_t physical) {
    if (physical > UINT64_MAX - directMapOffset)
        return nullptr;

    return reinterpret_cast<volatile uint64_t*>(
        physical + directMapOffset
    );
}

static bool allocate_table(uint64_t& outputPhysical) {
    outputPhysical = 0;

    if (!frameAllocator)
        return false;

    uint64_t physical = 0;

    if (!frameAllocator(physical))
        return false;

    if (!physical ||
        (physical & (PAGE_SIZE - 1)))
        return false;

    volatile uint64_t* table = table_at(physical);

    if (!table)
        return false;

    clear_page(
        reinterpret_cast<void*>(
            physical + directMapOffset
        )
    );

    outputPhysical = physical;
    return true;
}

static bool descend(
    volatile uint64_t* table,
    uint16_t index,
    volatile uint64_t*& child
) {
    uint64_t entry = table[index];

    if (!(entry & PRESENT)) {
        uint64_t physical = 0;

        if (!allocate_table(physical))
            return false;

        table[index] = physical | PRESENT | WRITE;
        entry = table[index];
    }

    if (entry & HUGE)
        return false;

    if (!(entry & WRITE))
        return false;

    child = table_at(entry & ADDRESS_MASK);
    return child != nullptr;
}

static bool map_page(
    uint64_t virtualAddress,
    uint64_t physicalAddress
) {
    const uint16_t pml4Index =
        static_cast<uint16_t>(
            (virtualAddress >> 39) & 0x1FF
        );

    const uint16_t pdptIndex =
        static_cast<uint16_t>(
            (virtualAddress >> 30) & 0x1FF
        );

    const uint16_t pdIndex =
        static_cast<uint16_t>(
            (virtualAddress >> 21) & 0x1FF
        );

    const uint16_t ptIndex =
        static_cast<uint16_t>(
            (virtualAddress >> 12) & 0x1FF
        );

    volatile uint64_t* pdpt = nullptr;
    volatile uint64_t* pd = nullptr;
    volatile uint64_t* pt = nullptr;

    if (!descend(pml4, pml4Index, pdpt))
        return false;

    if (!descend(pdpt, pdptIndex, pd))
        return false;

    if (!descend(pd, pdIndex, pt))
        return false;

    if (pt[ptIndex] & PRESENT)
        return false;

    // Supervisor-only, writable, uncached,
    // non-executable MMIO register mapping.
    pt[ptIndex] =
        (physicalAddress & ADDRESS_MASK) |
        PRESENT | WRITE | PWT | PCD | NX;

    invalidate(virtualAddress);
    return true;
}

bool initialize(
    uint64_t hhdmOffset,
    AllocatePage allocator
) {
    if (ready || !allocator)
        return false;

    if ((hhdmOffset & (PAGE_SIZE - 1)) != 0)
        return false;

    uint64_t rootPhysical = read_cr3();

    if (!rootPhysical)
        return false;

    if (rootPhysical > UINT64_MAX - hhdmOffset)
        return false;

    auto* root = reinterpret_cast<
        volatile uint64_t*
    >(rootPhysical + hhdmOffset);

    // MMIO_BASE is in PML4 slot 384.
    constexpr uint16_t MMIO_SLOT = 384;

    // Never overwrite an existing kernel mapping.
    if (root[MMIO_SLOT] & PRESENT)
        return false;

    directMapOffset = hhdmOffset;
    frameAllocator = allocator;
    pml4 = root;

    mmioUsed = 0;
    ready = true;

    return true;
}

bool initialized() {
    return ready;
}

uint64_t hhdm_offset() {
    return directMapOffset;
}

volatile void* map_device(
    uint64_t physical,
    uint64_t length
) {
    if (!ready || !physical || !length)
        return nullptr;

    if (physical > UINT64_MAX - length)
        return nullptr;

    const uint64_t pagePhysical =
        align_down(physical);

    const uint64_t offset =
        physical - pagePhysical;

    if (length > UINT64_MAX - offset)
        return nullptr;

    uint64_t mappedLength = 0;

    if (!align_up(
        length + offset,
        mappedLength
    ))
        return nullptr;

    if (mappedLength > MMIO_SIZE - mmioUsed)
        return nullptr;

    const uint64_t virtualBase =
        MMIO_BASE + mmioUsed;

    // Establish page mappings before exposing
    // the virtual pointer to the driver.
    for (uint64_t i = 0;
         i < mappedLength;
         i += PAGE_SIZE) {

        if (!map_page(
            virtualBase + i,
            pagePhysical + i
        )) {
            // Mappings already created are left
            // allocated; do not reuse this range.
            mmioUsed += mappedLength;
            return nullptr;
        }
    }

    mmioUsed += mappedLength;

    return reinterpret_cast<volatile void*>(
        virtualBase + offset
    );
}

uint64_t mapped_device_bytes() {
    return mmioUsed;
}

}
