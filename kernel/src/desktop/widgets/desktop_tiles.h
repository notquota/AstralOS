#pragma once

#include <stdint.h>

namespace Astral::Desktop::Widgets::DesktopTiles {

// The icons have become cubes.
// Nobody knows who approved the geometry.

void initialize();

void render();

bool hover(int32_t x, int32_t y);

// Returns an app ID, or 0 if nothing
// launchable was clicked.
uint32_t click(int32_t x, int32_t y);

// Keeps desktop shortcuts behind open windows.
void set_screen_size(
    int32_t width,
    int32_t height
);

}