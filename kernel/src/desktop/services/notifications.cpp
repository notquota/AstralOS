
#include "notifications.h"

namespace Astral::Desktop::Notifications {

namespace {

Notification entries[MAX_NOTIFICATIONS]{};
uint32_t nextId = 1;

void copy_text(
    char* output,
    uint32_t capacity,
    const char* input
) {
    if (!input) input = "";

    uint32_t i = 0;

    while (input[i] && i + 1 < capacity) {
        output[i] = input[i];
        ++i;
    }

    output[i] = '\0';
}

}

void initialize() {
    clear_all();
    nextId = 1;
}

uint32_t post(
    uint32_t appId,
    Level level,
    const char* title,
    const char* message
) {
    if (!nextId)
        return 0;

    uint32_t slot = MAX_NOTIFICATIONS;

    for (uint32_t i = 0; i < MAX_NOTIFICATIONS; ++i) {
        if (!entries[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == MAX_NOTIFICATIONS) {
        // Oldest insertion is discarded when full.
        for (uint32_t i = 1; i < MAX_NOTIFICATIONS; ++i)
            entries[i - 1] = entries[i];

        slot = MAX_NOTIFICATIONS - 1;
    }

    Notification item{};

    item.id = nextId++;
    item.appId = appId;
    item.level = level;
    item.active = true;
    item.read = false;

    copy_text(item.title, sizeof(item.title), title);
    copy_text(item.message, sizeof(item.message), message);

    entries[slot] = item;
    return item.id;
}

bool dismiss(uint32_t id) {
    for (auto& item : entries) {
        if (item.active && item.id == id) {
            item = {};
            return true;
        }
    }

    return false;
}

bool mark_read(uint32_t id) {
    for (auto& item : entries) {
        if (item.active && item.id == id) {
            item.read = true;
            return true;
        }
    }

    return false;
}

void clear_all() {
    for (auto& item : entries)
        item = {};
}

uint32_t count() {
    uint32_t result = 0;

    for (const auto& item : entries)
        if (item.active)
            ++result;

    return result;
}

uint32_t unread_count() {
    uint32_t result = 0;

    for (const auto& item : entries)
        if (item.active && !item.read)
            ++result;

    return result;
}

const Notification* at(uint32_t index) {
    uint32_t seen = 0;

    for (const auto& item : entries) {
        if (!item.active)
            continue;

        if (seen++ == index)
            return &item;
    }

    return nullptr;
}

}
