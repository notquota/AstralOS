
#pragma once

#include <stdint.h>
#include "../windows/window.h"

namespace Astral::Desktop::Widgets::TextField {

constexpr uint32_t CAPACITY = 256;

struct State {
    Windows::Rect bounds{};
    char content[CAPACITY]{};

    uint32_t length = 0;
    uint32_t cursor = 0;
    uint32_t maxLength = CAPACITY - 1;

    bool focused = false;
    bool enabled = true;
    bool password = false;
    bool changed = false;
};

void initialize(
    State& state,
    const Windows::Rect& bounds,
    bool password = false,
    uint32_t maxLength = CAPACITY - 1
);

void render(const State& state);

bool click(State& state, int32_t x, int32_t y);

bool key(State& state, char value);

bool set_text(State& state, const char* value);

void clear(State& state);

const char* text(const State& state);

}
