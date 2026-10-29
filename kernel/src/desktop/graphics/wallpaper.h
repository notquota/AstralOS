
#pragma once

#include <stdint.h>
#include "image.h"

namespace Astral::Desktop::Wallpaper {

enum class Mode : uint8_t {
    Fill,
    Fit,
    Center,
    Tile
};

struct Configuration {
    Mode mode;
    uint32_t backgroundColor;
    bool enabled;
};

void initialize();

void set_bitmap(const Image::Bitmap* bitmap);
void set_configuration(const Configuration& config);

const Configuration& configuration();

void render(
    uint32_t screenWidth,
    uint32_t screenHeight
);

}
