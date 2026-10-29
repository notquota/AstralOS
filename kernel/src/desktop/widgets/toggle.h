
#pragma once

#include <stdint.h>
#include "../windows/window.h"

namespace Astral::Desktop::Widgets::Toggle {

struct State {
    Windows::Rect bounds{};
    const char* label = nullptr;
    bool value = false;
    bool enabled = true;
    bool hovered = false;
};

void initialize(
    State& state,
    const Windows::Rect& bounds,
    const char* label,
    bool initialValue
);

void render(const State& state);

bool pointer_move(State& state, int32_t x, int32_t y);

// Returns true if the value changed.
bool click(State& state, int32_t x, int32_t y);

}
