
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Widgets {

using ButtonId = uint32_t;

struct Button {
    ButtonId id;

    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;

    const char* label;

    bool enabled;
    bool hovered;
    bool pressed;
};

bool contains(
    const Button& button,
    int32_t x,
    int32_t y
);

void update_hover(
    Button& button,
    int32_t x,
    int32_t y
);

bool mouse_down(
    Button& button,
    int32_t x,
    int32_t y
);

bool mouse_up(
    Button& button,
    int32_t x,
    int32_t y
);

// This widget uses the Desktop image canvas.
// Call during window rendering.
void render(const Button& button);

}
