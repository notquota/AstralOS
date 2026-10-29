
#pragma once
#include <stdint.h>

namespace Astral::Desktop::PreferencesStore {

constexpr uint32_t FORMAT_VERSION = 1;
constexpr uint32_t MAX_BYTES = 2048;

using WriteProvider = bool (*)(
    const char* path,
    const uint8_t* data,
    uint32_t size
);

using ReadProvider = bool (*)(
    const char* path,
    uint8_t* data,
    uint32_t capacity,
    uint32_t& bytesRead
);

void initialize();

void set_providers(
    ReadProvider reader,
    WriteProvider writer
);

bool save();
bool load();

bool persistence_available();

const char* path();

}
