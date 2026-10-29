
#include "decorations.h"

namespace Astral::Desktop::Decorations {

namespace {

Icons currentIcons{};

enum class Control : uint8_t {
    Minimize,
    Maximize,
    Close
};

Windows::Rect control_bounds(
    const Windows::Window& window,
    Control control
) {
    const Windows::Rect& r = window.bounds;

    // Three equal slots at the far right.
    // The buttons are no longer microscopic.
    const int32_t right = r.x + r.width - 5;
    const int32_t top = r.y + 3;

    int32_t slot = 0;

    switch (control) {
        case Control::Close:
            slot = 0;
            break;

        case Control::Maximize:
            slot = 1;
            break;

        case Control::Minimize:
            slot = 2;
            break;
    }

    return {
        right - BUTTON_WIDTH * (slot + 1),
        top,
        BUTTON_WIDTH,
        BUTTON_HEIGHT
    };
}

void draw_control(
    const Windows::Rect& bounds,
    const Image::Bitmap* bitmap,
    uint32_t background,
    const char* fallback
) {
    Image::rectangle(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        background
    );

    // One slot = one enormous click target.
    // An icon is merely the employee inside it.
    if (bitmap && bitmap->rgba) {
        constexpr int32_t ICON_SIZE = 23;

        Image::draw_scaled(
            *bitmap,
            bounds.x + (bounds.width - ICON_SIZE) / 2,
            bounds.y + (bounds.height - ICON_SIZE) / 2,
            ICON_SIZE,
            ICON_SIZE
        );
    } else {
        Image::text(
            bounds.x + (bounds.width - 6) / 2,
            bounds.y + 15,
            fallback,
            0xEDF1FF
        );
    }
}

bool window_visible(const Windows::Window& window) {
    return window.visible &&
           window.state != Windows::WindowState::Minimized &&
           window.bounds.width > 0 &&
           window.bounds.height > 0;
}

} // namespace

void initialize() {
    currentIcons = {};
}

void set_icons(const Icons& icons) {
    currentIcons = icons;
}

Windows::Rect client_bounds(
    const Windows::Window& window
) {
    const Windows::Rect& r = window.bounds;

    Windows::Rect client{
        r.x + 1,
        r.y + TITLE_HEIGHT,
        r.width - 2,
        r.height - TITLE_HEIGHT - 1
    };

    if (client.width < 0)
        client.width = 0;

    if (client.height < 0)
        client.height = 0;

    return client;
}

void render(const Windows::Window& window) {
    if (!window_visible(window))
        return;

    const Windows::Rect& r = window.bounds;

    const uint32_t border =
        window.focused ? 0xA58BFF : 0x35445F;

    const uint32_t titleColor =
        window.focused ? 0x263957 : 0x1C2A44;

    // Shadow: proof the window has depth.
    // Unlike our collective sleep schedule.
    Image::rectangle(
        r.x + 5,
        r.y + 6,
        r.width,
        r.height,
        0x070B15
    );

    Image::rectangle(
        r.x,
        r.y,
        r.width,
        r.height,
        border
    );

    Image::rectangle(
        r.x + 1,
        r.y + 1,
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
            r.x + 1,
            r.y + 1,
            3,
            TITLE_HEIGHT - 1,
            0xA58BFF
        );
    }

    // Don't let a very long window title start
    // physically assaulting the close button.
    // Current font has no proper text clipping yet.
    const int32_t availableTitleWidth =
        r.width - 3 * BUTTON_WIDTH - 36;

    if (availableTitleWidth > 0) {
        uint32_t chars = 0;
        const uint32_t maxChars =
            static_cast<uint32_t>(
                availableTitleWidth / 12
            );

        while (window.title[chars] &&
               chars < maxChars &&
               chars < 63)
            ++chars;

        char title[64]{};

        for (uint32_t i = 0; i < chars; ++i)
            title[i] = window.title[i];

        title[chars] = '\0';

        Image::text(
            r.x + 17,
            r.y + 14,
            title,
            0xEDF1FF,
            2
        );
    }

    // Minimize, maximize, close.
    // Each one has a full 44x38 rectangle.
    draw_control(
        control_bounds(window, Control::Minimize),
        currentIcons.minimize,
        0x304565,
        "-"
    );

    if (window.resizable) {
        draw_control(
            control_bounds(window, Control::Maximize),
            window.state == Windows::WindowState::Maximized
                ? currentIcons.restore
                : currentIcons.maximize,
            0x304565,
            window.state == Windows::WindowState::Maximized
                ? "R" : "+"
        );
    }

    if (window.closable) {
        draw_control(
            control_bounds(window, Control::Close),
            currentIcons.close,
            0x663449,
            "X"
        );
    }

    if (window.resizable &&
        window.state == Windows::WindowState::Normal) {
        Image::rectangle(
            r.x + r.width - 11,
            r.y + r.height - 5,
            8,
            2,
            border
        );

        Image::rectangle(
            r.x + r.width - 6,
            r.y + r.height - 10,
            3,
            2,
            border
        );
    }
}

Hit hit_test(
    const Windows::Window& window,
    int32_t x,
    int32_t y
) {
    if (!window_visible(window))
        return Hit::None;

    const Windows::Rect& r = window.bounds;

    if (!Windows::contains(r, x, y))
        return Hit::None;

    // Control hitboxes get priority.
    // A resize handle must not kidnap the close button.
    if (window.closable &&
        Windows::contains(
            control_bounds(window, Control::Close),
            x, y
        ))
        return Hit::Close;

    if (window.resizable &&
        Windows::contains(
            control_bounds(window, Control::Maximize),
            x, y
        ))
        return Hit::Maximize;

    if (Windows::contains(
        control_bounds(window, Control::Minimize),
        x, y
    ))
        return Hit::Minimize;

    if (window.resizable &&
        window.state == Windows::WindowState::Normal &&
        Windows::contains(
            Windows::Rect{
                r.x + r.width - 15,
                r.y + r.height - 15,
                15,
                15
            },
            x, y
        ))
        return Hit::ResizeBottomRight;

    if (Windows::contains(
        Windows::Rect{
            r.x,
            r.y,
            r.width,
            TITLE_HEIGHT
        },
        x, y
    ))
        return Hit::TitleBar;

    return Hit::Client;
}

} // namespace Astral::Desktop::Decorations
