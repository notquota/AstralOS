
#pragma once

#include <stdint.h>

namespace Astral::Compositor {

using Allocator = void* (*)(
    uint64_t size,
    uint64_t alignment
);

struct Surface {
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stridePixels;
};

struct Rectangle {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

bool initialize(
    volatile void* framebuffer,
    uint32_t width,
    uint32_t height,
    uint32_t pitchBytes,
    Allocator allocator
);

bool initialized();

Surface backbuffer();

void clear(uint32_t color);

void pixel(
    int32_t x,
    int32_t y,
    uint32_t color
);

void fill(
    Rectangle rect,
    uint32_t color
);

void present();

void present_rect(Rectangle rect);

// Cursor pixels must use 0xRRGGBBAA.
void set_cursor(
    const uint32_t* pixels,
    uint32_t width,
    uint32_t height
);

void move_cursor(int32_t x, int32_t y);

void hide_cursor();

}