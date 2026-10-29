
#include "decorations.h"

namespace Astral::Desktop::Decorations {

namespace {

Icons currentIcons{};

bool in_rect(
    int32_t px, int32_t py,
    int32_t x, int32_t y,
    int32_t w, int32_t h
) {
    return px >= x && py >= y &&
           static_cast<int64_t>(px) <
               static_cast<int64_t>(x) + w &&
           static_cast<int64_t>(py) <
               static_cast<int64_t>(y) + h;
}

void draw_control(
    const Image::Bitmap* icon,
    int32_t x,
    int32_t y,
    uint32_t background,
    const char* fallback
) {
    Image::rectangle(
        x, y,
        BUTTON_SIZE,
        BUTTON_SIZE,
        background
    );

    if (icon && icon->rgba) {
        Image::draw_scaled(
            *icon,
            x + 3, y + 3,
            20, 20
        );
    } else {
        Image::text(
            x + 9, y + 10,
            fallback,
            0xEDF1FF
        );
    }
}

}

void initialize() {
    currentIcons = {};
}

void set_icons(const Icons& icons) {
    currentIcons = icons;
}

Windows::Rect client_bounds(
    const Windows::Window& window
) {
    const auto& r = window.bounds;

    return {
        r.x + 1,
        r.y + TITLE_HEIGHT,
        r.width - 2,
        r.height - TITLE_HEIGHT - 1
    };
}

void render(const Windows::Window& window) {
    if (!window.visible ||
        window.state == Windows::WindowState::Minimized)
        return;

    const auto& r = window.bounds;

    if (r.width <= 0 || r.height <= 0)
        return;

    const uint32_t border =
        window.focused ? 0xA58BFF : 0x35445F;

    const uint32_t titleColor =
        window.focused ? 0x263957 : 0x1C2A44;

    // Shadow.
    Image::rectangle(
        r.x + 5,
        r.y + 6,
        r.width,
        r.height,
        0x070B15
    );

    // Window body.
    Image::rectangle(
        r.x, r.y,
        r.width, r.height,
        border
    );

    Image::rectangle(
        r.x + 1, r.y + 1,
        r.width - 2,
        r.height - 2,
        0x172440
    );

    Image::rectangle(
        r.x + 1,
        r.y + 1,
        r.width - 2,
        TITLE_HEIGHT - 1,
        titleColor
    );

    if (window.focused) {
        Image::rectangle(
            r.x + 1, r.y + 1,
            3, TITLE_HEIGHT - 1,
            0xA58BFF
        );
    }

    Image::text(
        r.x + 17,
        r.y + 15,
        window.title,
        0xEDF1FF,
        2
    );

    const int32_t buttonsY = r.y + 6;

    if (window.resizable) {
        draw_control(
            window.state == Windows::WindowState::Maximized
                ? currentIcons.restore
                : currentIcons.maximize,
            r.x + r.width - 69,
            buttonsY,
            0x304565,
            window.state == Windows::WindowState::Maximized
                ? "R" : "+"
        );
    }

    draw_control(
        currentIcons.minimize,
        r.x + r.width - 102,
        buttonsY,
        0x304565,
        "-"
    );

    if (window.closable) {
        draw_control(
            currentIcons.close,
            r.x + r.width - 36,
            buttonsY,
            0x663449,
            "x"
        );
    }

    if (window.resizable &&
        window.state == Windows::WindowState::Normal) {
        Image::rectangle(
            r.x + r.width - 11,
            r.y + r.height - 5,
            8, 2,
            border
        );

        Image::rectangle(
            r.x + r.width - 6,
            r.y + r.height - 10,
            3, 2,
            border
        );
    }
}

Hit hit_test(
    const Windows::Window& window,
    int32_t x,
    int32_t y
) {
    if (!window.visible ||
        window.state == Windows::WindowState::Minimized)
        return Hit::None;

    const auto& r = window.bounds;

    if (!Windows::contains(r, x, y))
        return Hit::None;

    if (window.resizable &&
        window.state == Windows::WindowState::Normal &&
        in_rect(
            x, y,
            r.x + r.width - 15,
            r.y + r.height - 15,
            15, 15
        ))
        return Hit::ResizeBottomRight;

    if (window.closable &&
        in_rect(
            x, y,
            r.x + r.width - 36,
            r.y + 6,
            BUTTON_SIZE, BUTTON_SIZE
        ))
        return Hit::Close;

    if (window.resizable &&
        in_rect(
            x, y,
            r.x + r.width - 69,
            r.y + 6,
            BUTTON_SIZE, BUTTON_SIZE
        ))
        return Hit::Maximize;

    if (in_rect(
        x, y,
        r.x + r.width - 102,
        r.y + 6,
        BUTTON_SIZE, BUTTON_SIZE
    ))
        return Hit::Minimize;

    if (in_rect(
        x, y,
        r.x, r.y,
        r.width, TITLE_HEIGHT
    ))
        return Hit::TitleBar;

    return Hit::Client;
}

}
