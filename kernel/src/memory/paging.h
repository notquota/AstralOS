
#pragma once

#include <stdint.h>

namespace Astral::Paging {

constexpr uint64_t PAGE_SIZE = 4096;

// Must allocate one unused 4 KiB physical frame.
// The frame must be accessible through HHDM.
// The allocation must never be returned again.
using AllocatePage = bool (*)(uint64_t& physical);

// Uses the existing CR3 page tables.
// Requires a 4-level x86-64 paging environment.
bool initialize(
    uint64_t hhdmOffset,
    AllocatePage allocator
);

bool initialized();

uint64_t hhdm_offset();

// Converts an allocated, HHDM-mapped physical
// RAM address into an accessible virtual address.
void* physical_to_virtual(uint64_t physical);

// Map uncached PCI register memory.
// Returns nullptr on failure.
volatile void* map_device(
    uint64_t physical,
    uint64_t length
);

// Maps device memory into the dedicated
// virtual arena; no unmapping yet.
uint64_t mapped_device_bytes();

}
