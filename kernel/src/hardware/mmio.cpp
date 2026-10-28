
#include "mmio.h"

#include <stdint.h>

namespace Astral::MMIO {

static MapFunction currentMapper = nullptr;

void set_mapper(MapFunction mapper) {
    currentMapper = mapper;
}

bool available() {
    return currentMapper != nullptr;
}

volatile void* map(
    uint64_t physicalAddress,
    uint64_t length
) {
    if (!currentMapper ||
        !physicalAddress ||
        !length)
        return nullptr;

    if (physicalAddress > UINT64_MAX - length)
        return nullptr;

    return currentMapper(
        physicalAddress,
        length
    );
}

uint32_t read32(
    const volatile void* base,
    uint32_t offset
) {
    if (!base || (offset & 3u))
        return 0xFFFFFFFFu;

    auto bytes =
        reinterpret_cast<const volatile uint8_t*>(base);

    auto address =
        reinterpret_cast<const volatile uint32_t*>(
            bytes + offset
        );

    uint32_t result = *address;

    asm volatile("" ::: "memory");

    return result;
}

void write32(
    volatile void* base,
    uint32_t offset,
    uint32_t value
) {
    if (!base || (offset & 3u))
        return;

    auto bytes =
        reinterpret_cast<volatile uint8_t*>(base);

    auto address =
        reinterpret_cast<volatile uint32_t*>(
            bytes + offset
        );

    asm volatile("" ::: "memory");

    *address = value;

    asm volatile("" ::: "memory");
}

}
