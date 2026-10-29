
#include "clipboard.h"

namespace Astral::Desktop::Clipboard {

namespace {

char content[MAX_TEXT]{};
uint32_t contentLength = 0;
uint32_t ownerId = 0;
bool available = false;

}

void initialize() {
    clear();
}

bool copy_text(
    uint32_t ownerAppId,
    const char* text
) {
    if (!text)
        return false;

    uint32_t len = 0;

    while (len < MAX_TEXT && text[len])
        ++len;

    if (len >= MAX_TEXT)
        return false;

    clear();

    for (uint32_t i = 0; i < len; ++i)
        content[i] = text[i];

    content[len] = '\0';

    contentLength = len;
    ownerId = ownerAppId;
    available = true;

    return true;
}

bool read_text(
    char* destination,
    uint32_t capacity
) {
    if (!available || !destination ||
        capacity <= contentLength)
        return false;

    for (uint32_t i = 0; i <= contentLength; ++i)
        destination[i] = content[i];

    return true;
}

void clear() {
    volatile char* data = content;

    for (uint32_t i = 0; i < MAX_TEXT; ++i)
        data[i] = '\0';

    contentLength = 0;
    ownerId = 0;
    available = false;
}

bool has_text() {
    return available;
}

uint32_t owner() {
    return ownerId;
}

uint32_t length() {
    return contentLength;
}

}
