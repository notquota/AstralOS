#include "cpu.h"

namespace Astral::CPU {

Registers cpuid(
    uint32_t leaf,
    uint32_t subleaf
) {
    Registers result{};

    asm volatile(
        "cpuid"
        : "=a"(result.eax),
          "=b"(result.ebx),
          "=c"(result.ecx),
          "=d"(result.edx)
        : "a"(leaf), "c"(subleaf)
    );

    return result;
}

bool supports_long_mode() {
    Registers max =
        cpuid(0x80000000);

    if (max.eax < 0x80000001)
        return false;

    Registers features =
        cpuid(0x80000001);

    return (features.edx & (1u << 29)) != 0;
}

}