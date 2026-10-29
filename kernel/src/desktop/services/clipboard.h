
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Clipboard {

constexpr uint32_t MAX_TEXT = 4096;

void initialize();

bool copy_text(
    uint32_t ownerAppId,
    const char* text
);

bool read_text(
    char* destination,
    uint32_t capacity
);

void clear();

bool has_text();
uint32_t owner();
uint32_t length();

}
