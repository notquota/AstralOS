
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Notifications {

constexpr uint32_t MAX_NOTIFICATIONS = 32;

enum class Level : uint8_t {
    Information,
    Success,
    Warning,
    Error
};

struct Notification {
    uint32_t id;
    uint32_t appId;
    Level level;

    char title[64];
    char message[160];

    bool read;
    bool active;
};

void initialize();

uint32_t post(
    uint32_t appId,
    Level level,
    const char* title,
    const char* message
);

bool dismiss(uint32_t id);
bool mark_read(uint32_t id);

void clear_all();

uint32_t count();
uint32_t unread_count();

const Notification* at(uint32_t index);

}
