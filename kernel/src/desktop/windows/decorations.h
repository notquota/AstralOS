
#pragma once

#include <stdint.h>

#include "window.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Decorations {

enum class Hit : uint8_t {
    None,
    Client,
    TitleBar,
    Close,
    Minimize,
    Maximize,
    ResizeBottomRight
};

struct Icons {
    const Image::Bitmap* close = nullptr;
    const Image::Bitmap* minimize = nullptr;
    const Image::Bitmap* maximize = nullptr;
    const Image::Bitmap* restore = nullptr;
};

// We gave the title bar actual elbow room.
// The previous buttons were legally classified as crumbs.
constexpr int32_t TITLE_HEIGHT = 44;

constexpr int32_t BUTTON_WIDTH = 44;
constexpr int32_t BUTTON_HEIGHT = 38;

// Kept for any older code still using BUTTON_SIZE.
constexpr int32_t BUTTON_SIZE = BUTTON_HEIGHT;

void initialize();
void set_icons(const Icons& icons);

void render(const Windows::Window& window);

Hit hit_test(
    const Windows::Window& window,
    int32_t x,
    int32_t y
);

Windows::Rect client_bounds(
    const Windows::Window& window
);

}
