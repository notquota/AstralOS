
#pragma once

#include "window.h"

namespace Astral::Desktop::Windows {

void initialize(
    uint32_t screenWidth,
    uint32_t screenHeight,
    uint32_t taskbarHeight
);

WindowId create(
    uint32_t appId,
    const char* title,
    const Rect& bounds,
    WindowType type = WindowType::Application
);

bool close(WindowId id);
bool focus(WindowId id);

bool minimize(WindowId id);
bool maximize(WindowId id);
bool restore(WindowId id);

bool move(WindowId id, int32_t x, int32_t y);

bool resize(
    WindowId id,
    int32_t width,
    int32_t height
);

Window* get(WindowId id);
const Window* at(uint32_t index);

uint32_t count();

WindowId focused();
WindowId hit_test(int32_t x, int32_t y);

// Drag operations.
bool begin_drag(WindowId id, int32_t x, int32_t y);
bool drag_to(int32_t x, int32_t y);
void end_drag();
bool dragging();

}
