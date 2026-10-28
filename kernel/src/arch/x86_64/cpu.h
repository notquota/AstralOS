#pragma once
#include <stdint.h>

namespace Astral::CPU {

struct Registers {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

Registers cpuid(
    uint32_t leaf,
    uint32_t subleaf = 0
);

bool supports_long_mode();

}