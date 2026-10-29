
#include "temp_manager.h"

namespace Astral::Desktop::TempManager {

namespace {

Entry entries[MAX_TEMP_FILES]{};
uint32_t nextId = 1;

void append_number(char* out, uint32_t& pos, uint32_t n) {
    char reversed[12]{};
    uint32_t count = 0;

    do {
        reversed[count++] =
            static_cast<char>('0' + n % 10);
        n /= 10;
    } while (n && count < sizeof(reversed));

    while (count)
        out[pos++] = reversed[--count];
}

bool build_path(
    char* output,
    uint32_t id,
    const char* extension
) {
    constexpr char PREFIX[] = "/tmp/astral-";

    uint32_t pos = 0;

    for (uint32_t i = 0; PREFIX[i]; ++i)
        output[pos++] = PREFIX[i];

    append_number(output, pos, id);

    if (extension && extension[0]) {
        output[pos++] = '.';

        for (uint32_t i = 0; extension[i]; ++i) {
            const char c = extension[i];

            // Extension cannot contain path separators.
            if (c == '/' || c == '\\' ||
                c == '.' || c < 33 || c > 126)
                return false;

            if (pos + 1 >= MAX_PATH_LENGTH)
                return false;

            output[pos++] = c;
        }
    }

    output[pos] = '\0';
    return true;
}

}

void initialize() {
    clear();
    nextId = 1;
}

uint32_t create(
    uint32_t ownerAppId,
    const char* extension
) {
    if (nextId == 0)
        return 0;

    for (uint32_t i = 0; i < MAX_TEMP_FILES; ++i) {
        if (entries[i].active)
            continue;

        Entry entry{};
        entry.id = nextId;
        entry.ownerAppId = ownerAppId;

        if (!build_path(entry.path, nextId, extension))
            return 0;

        entry.active = true;
        entries[i] = entry;

        ++nextId;
        return entry.id;
    }

    return 0;
}

bool release(uint32_t id) {
    for (auto& entry : entries) {
        if (entry.active && entry.id == id) {
            entry = {};
            return true;
        }
    }
    return false;
}

void release_owner(uint32_t ownerAppId) {
    for (auto& entry : entries) {
        if (entry.active &&
            entry.ownerAppId == ownerAppId)
            entry = {};
    }
}

void clear() {
    for (auto& entry : entries)
        entry = {};
}

const Entry* find(uint32_t id) {
    for (const auto& entry : entries) {
        if (entry.active && entry.id == id)
            return &entry;
    }
    return nullptr;
}

uint32_t count() {
    uint32_t result = 0;

    for (const auto& entry : entries)
        if (entry.active)
            ++result;

    return result;
}

}
