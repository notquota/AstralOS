
#pragma once

#include "boot/boot_info.h"
#include <stdint.h>

namespace Astral::Graphics {

bool initialize(const Framebuffer& framebuffer);

void pixel(
    uint32_t x,
    uint32_t y,
    uint32_t color
);

void rectangle(
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
);

void clear(uint32_t color);

uint32_t width();
uint32_t height();

}
