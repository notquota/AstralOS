
#pragma once

#include "window.h"

namespace Astral::Desktop::Layout {

enum class Snap : uint8_t {
    None,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct Display {
    int32_t width;
    int32_t height;
    int32_t taskbarHeight;
};

void initialize(const Display& display);

Windows::Rect work_area();

Windows::Rect constrain(
    const Windows::Rect& bounds,
    int32_t minimumWidth = 240,
    int32_t minimumHeight = 160
);

Windows::Rect snapped(Snap position);

Snap detect_snap(
    int32_t mouseX,
    int32_t mouseY,
    int32_t threshold = 20
);

}
