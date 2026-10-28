#pragma once
#include <stdint.h>

namespace Astral::CPU {

struct ExceptionFrame {
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

void initialize_idt();

}