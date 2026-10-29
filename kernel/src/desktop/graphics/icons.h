
#pragma once

#include <stdint.h>
#include "image.h"

namespace Astral::Desktop::Icons {

constexpr uint32_t MAX_ICONS = 128;
constexpr uint32_t MAX_NAME = 64;

struct Entry {
    char name[MAX_NAME];
    const Image::Bitmap* bitmap;
};

void initialize();

bool register_icon(
    const char* name,
    const Image::Bitmap* bitmap
);

const Image::Bitmap* find(const char* name);
const Image::Bitmap* app(uint32_t appId);

void draw(
    const char* name,
    int32_t x,
    int32_t y,
    uint32_t size
);

void draw_app(
    uint32_t appId,
    int32_t x,
    int32_t y,
    uint32_t size
);

uint32_t count();

}
