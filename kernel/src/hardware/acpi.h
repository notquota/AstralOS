
#pragma once

#include <stdint.h>

namespace Astral::ACPI {

struct TableHeader {
    char signature[4];

    uint32_t length;
    uint8_t revision;
    uint8_t checksum;

    char oemId[6];
    char oemTableId[8];

    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} __attribute__((packed));

static_assert(sizeof(TableHeader) == 36);

// Maps physical firmware-table memory into
// a readable kernel virtual address.
//
// Unlike MMIO, ACPI tables are normally
// firmware memory, not device registers.
using MapPhysical = const void* (*)(
    uint64_t physicalAddress,
    uint64_t length
);

struct State {
    bool initialized;
    bool usingXsdt;
    uint8_t revision;

    uint64_t rootTablePhysical;
    uint32_t tableCount;
};

// rsdpVirtualAddress must already be readable.
// mapper must support physical table mappings.
bool initialize(
    const void* rsdpVirtualAddress,
    MapPhysical mapper
);

bool initialized();

const State& state();

// Find an ACPI table by its four-letter signature.
//
// Example:
//   find_table("APIC", table, length)
//
// The returned memory remains owned by the
// kernel memory mapper.
bool find_table(
    const char* signature,
    const TableHeader*& table,
    uint32_t& length
);

}
