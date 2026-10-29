
#include "layout.h"

namespace Astral::Desktop::Layout {

namespace {

Display display{800, 600, 58};

int32_t clamp(int32_t value, int32_t lo, int32_t hi) {
    if (hi < lo)
        hi = lo;

    if (value < lo)
        return lo;

    if (value > hi)
        return hi;

    return value;
}

}

void initialize(const Display& value) {
    display = value;

    if (display.width < 1)
        display.width = 1;

    if (display.height < 1)
        display.height = 1;

    display.taskbarHeight = clamp(
        display.taskbarHeight, 0, display.height
    );
}

Windows::Rect work_area() {
    return {
        0, 0,
        display.width,
        display.height - display.taskbarHeight
    };
}

Windows::Rect constrain(
    const Windows::Rect& bounds,
    int32_t minimumWidth,
    int32_t minimumHeight
) {
    const Windows::Rect area = work_area();

    Windows::Rect result = bounds;

    minimumWidth = clamp(
        minimumWidth, 1, area.width
    );
    minimumHeight = clamp(
        minimumHeight, 1, area.height
    );

    result.width = clamp(
        result.width, minimumWidth, area.width
    );
    result.height = clamp(
        result.height, minimumHeight, area.height
    );

    result.x = clamp(
        result.x, area.x,
        area.x + area.width - result.width
    );

    result.y = clamp(
        result.y, area.y,
        area.y + area.height - result.height
    );

    return result;
}

Windows::Rect snapped(Snap position) {
    const Windows::Rect r = work_area();

    const int32_t leftWidth = r.width / 2;
    const int32_t rightWidth = r.width - leftWidth;

    const int32_t topHeight = r.height / 2;
    const int32_t bottomHeight = r.height - topHeight;

    switch (position) {
        case Snap::Left:
            return {r.x, r.y, leftWidth, r.height};

        case Snap::Right:
            return {
                r.x + leftWidth, r.y,
                rightWidth, r.height
            };

        case Snap::TopLeft:
            return {
                r.x, r.y,
                leftWidth, topHeight
            };

        case Snap::TopRight:
            return {
                r.x + leftWidth, r.y,
                rightWidth, topHeight
            };

        case Snap::BottomLeft:
            return {
                r.x, r.y + topHeight,
                leftWidth, bottomHeight
            };

        case Snap::BottomRight:
            return {
                r.x + leftWidth,
                r.y + topHeight,
                rightWidth, bottomHeight
            };

        default:
            return r;
    }
}

Snap detect_snap(
    int32_t mouseX,
    int32_t mouseY,
    int32_t threshold
) {
    const Windows::Rect r = work_area();

    if (threshold < 1)
        threshold = 1;

    const bool left = mouseX <= r.x + threshold;
    const bool right =
        mouseX >= r.x + r.width - threshold;

    const bool top = mouseY <= r.y + threshold;
    const bool bottom =
        mouseY >= r.y + r.height - threshold;

    if (left && top) return Snap::TopLeft;
    if (right && top) return Snap::TopRight;
    if (left && bottom) return Snap::BottomLeft;
    if (right && bottom) return Snap::BottomRight;

    if (left) return Snap::Left;
    if (right) return Snap::Right;

    return Snap::None;
}

}
