
#include "icons.h"

namespace Astral::Desktop::Icons {

namespace {

Entry entries[MAX_ICONS]{};
uint32_t entryCount = 0;

bool equal(const char* a, const char* b) {
    if (!a || !b)
        return false;

    while (*a && *b) {
        if (*a != *b)
            return false;
        ++a;
        ++b;
    }

    return *a == *b;
}

bool copy_name(char* out, const char* name) {
    if (!name || !name[0])
        return false;

    uint32_t i = 0;
    while (name[i] && i + 1 < MAX_NAME) {
        out[i] = name[i];
        ++i;
    }

    if (name[i])
        return false;

    out[i] = '\0';
    return true;
}

const char* app_name(uint32_t appId) {
    switch (appId) {
        case 1: return "apps_files";
        case 2: return "apps_settings";
        case 3: return "apps_terminal";
        case 4: return "apps_system_monitor";
        case 5: return "apps_about";
        case 6: return "apps_calculator";
        default: return nullptr;
    }
}

void fallback(
    int32_t x, int32_t y,
    uint32_t size
) {
    if (size == 0)
        return;

    Image::rectangle(
        x, y,
        static_cast<int32_t>(size),
        static_cast<int32_t>(size),
        0x354668
    );

    Image::rectangle(
        x + 3, y + 3,
        static_cast<int32_t>(size) - 6,
        static_cast<int32_t>(size) - 6,
        0x17243D
    );

    Image::text(
        x + static_cast<int32_t>(size / 2) - 3,
        y + static_cast<int32_t>(size / 2) - 3,
        "?",
        0xAD9FFF
    );
}

}

void initialize() {
    for (auto& entry : entries)
        entry = {};

    entryCount = 0;
}

bool register_icon(
    const char* name,
    const Image::Bitmap* bitmap
) {
    if (!bitmap || !bitmap->rgba ||
        !bitmap->width || !bitmap->height)
        return false;

    for (uint32_t i = 0; i < entryCount; ++i) {
        if (equal(entries[i].name, name)) {
            entries[i].bitmap = bitmap;
            return true;
        }
    }

    if (entryCount >= MAX_ICONS)
        return false;

    Entry& entry = entries[entryCount];

    if (!copy_name(entry.name, name))
        return false;

    entry.bitmap = bitmap;
    ++entryCount;
    return true;
}

const Image::Bitmap* find(const char* name) {
    for (uint32_t i = 0; i < entryCount; ++i) {
        if (equal(entries[i].name, name))
            return entries[i].bitmap;
    }

    return nullptr;
}

const Image::Bitmap* app(uint32_t appId) {
    const char* name = app_name(appId);
    return name ? find(name) : nullptr;
}

void draw(
    const char* name,
    int32_t x,
    int32_t y,
    uint32_t size
) {
    const Image::Bitmap* bitmap = find(name);

    if (bitmap) {
        Image::draw_scaled(
            *bitmap, x, y, size, size
        );
    } else {
        fallback(x, y, size);
    }
}

void draw_app(
    uint32_t appId,
    int32_t x,
    int32_t y,
    uint32_t size
) {
    const Image::Bitmap* bitmap = app(appId);

    if (bitmap)
        Image::draw_scaled(*bitmap, x, y, size, size);
    else
        fallback(x, y, size);
}

uint32_t count() {
    return entryCount;
}

}
