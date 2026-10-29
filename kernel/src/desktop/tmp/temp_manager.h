
#pragma once
#include <stdint.h>

namespace Astral::Desktop::TempManager {

constexpr uint32_t MAX_TEMP_FILES = 32;
constexpr uint32_t MAX_PATH_LENGTH = 128;

struct Entry {
    uint32_t id;
    char path[MAX_PATH_LENGTH];
    uint32_t ownerAppId;
    bool active;
};

void initialize();

uint32_t create(
    uint32_t ownerAppId,
    const char* extension
);

bool release(uint32_t id);
void release_owner(uint32_t ownerAppId);
void clear();

const Entry* find(uint32_t id);
uint32_t count();

}
