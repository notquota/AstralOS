
#pragma once

#include <stdint.h>
#include "image.h"

namespace Astral::Desktop::Renderer {

struct Configuration {
    bool showDesktopIcons;
    bool showTaskbar;
    bool showWallpaper;
};

void initialize();

void set_configuration(const Configuration& config);
const Configuration& configuration();

// Rebuilds the entire software canvas.
void compose();

// Copies the canvas to the framebuffer.
void present();

// Recompose and present together.
void render();

// Repaint only the previous and new cursor areas.
// Cursor images are overlays, not part of the canvas.
void move_cursor(
    int32_t newX,
    int32_t newY
);

void set_cursor(const Image::Bitmap* bitmap);

}
