#include "framebuffer.h"

namespace Astral::Graphics {

static Framebuffer screen{};
static bool ready = false;

bool initialize(const Framebuffer& framebuffer) {
    ready = false;

    if (!framebuffer.address ||
        !framebuffer.width ||
        !framebuffer.height)
        return false;

    if (framebuffer.width > UINT32_MAX / 4)
        return false;

    if (framebuffer.pitch < framebuffer.width * 4)
        return false;

    if (framebuffer.format != PixelFormat::RGBX &&
        framebuffer.format != PixelFormat::BGRX)
        return false;

    screen = framebuffer;
    ready = true;
    return true;
}

void pixel(
    uint32_t x,
    uint32_t y,
    uint32_t color
) {
    if (!ready ||
        x >= screen.width ||
        y >= screen.height)
        return;

    uint8_t* address =
        screen.address +
        (uint64_t)y * screen.pitch +
        (uint64_t)x * 4;

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    if (screen.format == PixelFormat::RGBX) {
        address[0] = r;
        address[1] = g;
        address[2] = b;
    } else {
        address[0] = b;
        address[1] = g;
        address[2] = r;
    }

    address[3] = 0;
}

void rectangle(
    uint32_t x,
    uint32_t y,
    uint32_t w,
    uint32_t h,
    uint32_t color
) {
    if (!ready || x >= screen.width ||
        y >= screen.height)
        return;

    uint32_t endX =
        w > screen.width - x ?
        screen.width : x + w;

    uint32_t endY =
        h > screen.height - y ?
        screen.height : y + h;

    for (uint32_t py = y; py < endY; ++py) {
        for (uint32_t px = x; px < endX; ++px) {
            pixel(px, py, color);
        }
    }
}

void clear(uint32_t color) {
    rectangle(0, 0, screen.width,
              screen.height, color);
}

uint32_t width() {
    return ready ? screen.width : 0;
}

uint32_t height() {
    return ready ? screen.height : 0;
}

}