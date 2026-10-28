#pragma once

#include <stdint.h>
#include "../boot/boot_info.h"

namespace Astral::Memory {

constexpr uint64_t PAGE_SIZE = 4096;

bool initialize(const BootInfo& info);

uint64_t allocate_page();

uint64_t total_pages();
uint64_t free_pages();

uint64_t total_memory_mib();
uint64_t free_memory_mib();

bool initialized();

}