
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Windows {

using WindowId = uint32_t;

constexpr WindowId INVALID_WINDOW = 0;
constexpr uint32_t MAX_WINDOWS = 32;

enum class WindowState : uint8_t {
    Normal,
    Minimized,
    Maximized
};

enum class WindowType : uint8_t {
    Application,
    Dialog,
    Utility
};

struct Rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

struct Window {
    WindowId id;
    uint32_t appId;

    char title[64];

    Rect bounds;
    Rect restoredBounds;

    WindowState state;
    WindowType type;

    bool visible;
    bool focused;
    bool closable;
    bool resizable;
    bool movable;
    bool modal;

    uint32_t zOrder;
};

bool contains(const Rect& rect, int32_t x, int32_t y);

}
