
#pragma once

#include <stdint.h>
#include "../windows/window.h"

namespace Astral::Desktop::Widgets::Panel {

enum class Style : uint8_t {
    Default,
    Elevated,
    Subtle,
    Transparent,
    Accent,
    Danger
};

struct State {
    Windows::Rect bounds{};

    const char* title = nullptr;
    const char* subtitle = nullptr;

    Style style = Style::Default;

    int32_t padding = 16;
    int32_t headerHeight = 0;

    bool visible = true;
    bool enabled = true;
    bool interactive = false;
    bool hovered = false;

    bool showBorder = true;
    bool showShadow = false;

    uint32_t backgroundOverride = 0;
    uint32_t borderOverride = 0;

    bool customBackground = false;
    bool customBorder = false;
};

void initialize(
    State& panel,
    const Windows::Rect& bounds,
    Style style = Style::Default
);

void set_title(
    State& panel,
    const char* title,
    const char* subtitle = nullptr
);

void set_padding(State& panel, int32_t padding);

void set_colors(
    State& panel,
    uint32_t background,
    uint32_t border
);

Windows::Rect content_bounds(const State& panel);

bool contains(
    const State& panel,
    int32_t x,
    int32_t y
);

bool pointer_move(
    State& panel,
    int32_t x,
    int32_t y
);

bool click(
    const State& panel,
    int32_t x,
    int32_t y
);

void render(const State& panel);

}
