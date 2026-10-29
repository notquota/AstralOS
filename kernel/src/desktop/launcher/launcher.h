
#pragma once

#include <stdint.h>
#include "app_registry.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Launcher {

struct Result {
    bool selected = false;
    AppRegistry::AppId appId = 0;
};

void initialize();

bool visible();
void open();
void close();
void toggle();

void set_icon(
    AppRegistry::AppId appId,
    const Image::Bitmap* bitmap
);

void render(
    int32_t screenWidth,
    int32_t screenHeight
);

Result click(
    int32_t x,
    int32_t y,
    int32_t screenWidth,
    int32_t screenHeight
);

// Negative values scroll toward the top.
void scroll(int32_t steps);

}
