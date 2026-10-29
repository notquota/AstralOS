
#pragma once

#include <stdint.h>
#include "../windows/window.h"

namespace Astral::Desktop::Widgets::Slider {

struct State {
    Windows::Rect bounds{};
    const char* label = nullptr;

    int32_t minimum = 0;
    int32_t maximum = 100;
    int32_t step = 1;
    int32_t value = 0;

    bool enabled = true;
    bool dragging = false;
};

bool initialize(
    State& state,
    const Windows::Rect& bounds,
    const char* label,
    int32_t minimum,
    int32_t maximum,
    int32_t step,
    int32_t value
);

void render(const State& state);

bool mouse_down(State& state, int32_t x, int32_t y);
bool mouse_move(State& state, int32_t x, int32_t y);
void mouse_up(State& state);

bool set(State& state, int32_t value);

}
