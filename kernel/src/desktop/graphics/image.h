
#pragma once

#include <stdint.h>

namespace Astral::Desktop::Image {

// A bitmap stored as 8-bit RGBA pixels.
struct Bitmap {
    uint32_t width;
    uint32_t height;
    const uint8_t* rgba;
};

// callback for reading pixels from an external framebuffer or graphics surface.
using PixelReader = uint32_t (*)(
    int32_t x,
    int32_t y
);

void set_pixel_reader(PixelReader reader);

// Initialize the software canvas.
bool begin(uint32_t width, uint32_t height);

// Canvas dimensions.
uint32_t width();
uint32_t height();

// Drawing primitives.
void clear(uint32_t color);

void rectangle(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t color
);

void text(
    int32_t x,
    int32_t y,
    const char* string,
    uint32_t color,
    uint32_t scale = 1
);

// Bitmap rendering.
void draw(
    const Bitmap& bitmap,
    int32_t x,
    int32_t y
);

void draw_scaled(
    const Bitmap& bitmap,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height
);

// Read a pixel from the software canvas.
uint32_t pixel_at(
    int32_t x,
    int32_t y
);

// Copy the complete software canvas to the framebuffer.
void present();

// Copy a rectangular portion of the canvas.
void present_rect(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height
);

// Draw an overlay without permanently changing canvas pixels.
void draw_overlay(
    const Bitmap& bitmap,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height
);

}