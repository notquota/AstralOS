// W Aura ik fr
#include "panel.h"

#include "../graphics/image.h"

namespace Astral::Desktop::Widgets::Panel {

namespace {

struct Colors {
    uint32_t background;
    uint32_t border;
    uint32_t title;
    uint32_t subtitle;
    uint32_t header;
};

Colors colors_for(Style style) {
    switch (style) {
        case Style::Elevated:
            return {
                0x243450,
                0x415475,
                0xF4F5FF,
                0xAABAD4,
                0x2B3C5B
            };

        case Style::Subtle:
            return {
                0x17243A,
                0x2B3A55,
                0xE9EDFA,
                0x92A5C4,
                0x1D2B45
            };

        case Style::Transparent:
            return {
                0x10192D,
                0x31405B,
                0xEDF2FF,
                0xA6B5CD,
                0x10192D
            };

        case Style::Accent:
            return {
                0x302D54,
                0xA38BF7,
                0xF5EFFF,
                0xC7B8F4,
                0x403768
            };

        case Style::Danger:
            return {
                0x422939,
                0xCE718A,
                0xFFF0F4,
                0xE2A8B9,
                0x523044
            };

        default:
            return {
                0x1E2D48,
                0x364A69,
                0xEDF2FF,
                0xA5B6D0,
                0x263754
            };
    }
}

int32_t clamp_nonnegative(int32_t value) {
    return value < 0 ? 0 : value;
}

void rectangle(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t color
) {
    if (width <= 0 || height <= 0)
        return;

    Image::rectangle(
        x, y, width, height, color
    );
}

} // namespace

void initialize(
    State& panel,
    const Windows::Rect& bounds,
    Style style
) {
    panel = {};

    panel.bounds = bounds;
    panel.style = style;

    panel.padding = 16;
    panel.visible = true;
    panel.enabled = true;
    panel.showBorder = true;
}

void set_title(
    State& panel,
    const char* title,
    const char* subtitle
) {
    panel.title = title;
    panel.subtitle = subtitle;

    panel.headerHeight =
        title ? (subtitle ? 68 : 48) : 0;
}

void set_padding(
    State& panel,
    int32_t padding
) {
    panel.padding = clamp_nonnegative(padding);
}

void set_colors(
    State& panel,
    uint32_t background,
    uint32_t border
) {
    panel.backgroundOverride = background;
    panel.borderOverride = border;

    panel.customBackground = true;
    panel.customBorder = true;
}

Windows::Rect content_bounds(
    const State& panel
) {
    const Windows::Rect& b = panel.bounds;

    const int32_t padding =
        clamp_nonnegative(panel.padding);

    const int32_t header =
        clamp_nonnegative(panel.headerHeight);

    Windows::Rect result{
        b.x + padding,
        b.y + padding + header,
        b.width - padding * 2,
        b.height - padding * 2 - header
    };

    if (result.width < 0)
        result.width = 0;

    if (result.height < 0)
        result.height = 0;

    return result;
}

bool contains(
    const State& panel,
    int32_t x,
    int32_t y
) {
    if (!panel.visible)
        return false;

    return Windows::contains(
        panel.bounds, x, y
    );
}

bool pointer_move(
    State& panel,
    int32_t x,
    int32_t y
) {
    const bool next =
        panel.enabled &&
        panel.interactive &&
        contains(panel, x, y);

    const bool changed = panel.hovered != next;

    panel.hovered = next;
    return changed;
}

bool click(
    const State& panel,
    int32_t x,
    int32_t y
) {
    return panel.visible &&
           panel.enabled &&
           panel.interactive &&
           contains(panel, x, y);
}

void render(const State& panel) {
    if (!panel.visible)
        return;

    const Windows::Rect& b = panel.bounds;

    if (b.width <= 0 || b.height <= 0)
        return;

    const Colors theme = colors_for(panel.style);

    const uint32_t background =
        panel.customBackground
            ? panel.backgroundOverride
            : theme.background;

    const uint32_t border =
        panel.customBorder
            ? panel.borderOverride
            : theme.border;

    if (panel.showShadow) {
        rectangle(
            b.x + 5,
            b.y + 6,
            b.width,
            b.height,
            0x080F20
        );
    }

    // The Transparent style uses the underlying
    // canvas but may still draw a border/header.
    if (panel.style != Style::Transparent) {
        rectangle(
            b.x,
            b.y,
            b.width,
            b.height,
            background
        );
    }

    if (panel.showBorder) {
        // Four sides, avoiding negative dimensions.
        rectangle(
            b.x, b.y,
            b.width, 1,
            border
        );

        rectangle(
            b.x,
            b.y + b.height - 1,
            b.width, 1,
            border
        );

        rectangle(
            b.x, b.y,
            1, b.height,
            border
        );

        rectangle(
            b.x + b.width - 1,
            b.y,
            1, b.height,
            border
        );
    }

    if (panel.hovered && panel.interactive) {
        rectangle(
            b.x + 2,
            b.y + 2,
            b.width - 4,
            2,
            0xAB96FF
        );
    }

    if (panel.title && panel.headerHeight > 0) {
        const int32_t headerHeight =
            panel.headerHeight < b.height
                ? panel.headerHeight
                : b.height;

        if (panel.style != Style::Transparent) {
            rectangle(
                b.x + 1,
                b.y + 1,
                b.width - 2,
                headerHeight - 1,
                theme.header
            );
        }

        Image::text(
            b.x + panel.padding,
            b.y + 16,
            panel.title,
            theme.title,
            2
        );

        if (panel.subtitle) {
            Image::text(
                b.x + panel.padding,
                b.y + 43,
                panel.subtitle,
                theme.subtitle
            );
        }

        rectangle(
            b.x + 1,
            b.y + headerHeight,
            b.width - 2,
            1,
            border
        );
    }
}

}