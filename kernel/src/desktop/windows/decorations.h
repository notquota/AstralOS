
#pragma once

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

constexpr int32_t TITLE_HEIGHT = 38;
constexpr int32_t BUTTON_SIZE = 26;

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
