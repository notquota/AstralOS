
#pragma once

#include <stdint.h>

namespace Astral::Desktop::NotificationCenter {

void initialize();

bool visible();
void open();
void close();
void toggle();

// Render over the desktop, below any lock screen.
void render(int32_t screenWidth, int32_t screenHeight);

// Returns true if a notification was clicked
// or the panel consumed the click.
bool click(
    int32_t x,
    int32_t y,
    int32_t screenWidth,
    int32_t screenHeight
);

void scroll(int32_t steps);

}
