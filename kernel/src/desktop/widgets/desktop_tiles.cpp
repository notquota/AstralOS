
#include "desktop_tiles.h"

#include "../graphics/image.h"
#include "../windows/window_manager.h"

#include "../../../assets/desktop/generated/desktop_assets.h"

namespace Astral::Desktop::Widgets::DesktopTiles {

namespace {

// Each cube knows its purpose.
// Most of them, anyway.

struct Tile {
    uint32_t appId;
    const char* name;
    const Image::Bitmap* icon;
    bool available;
};

constexpr int32_t TILE_WIDTH = 102;
constexpr int32_t TILE_HEIGHT = 112;
constexpr int32_t TILE_GAP = 14;
constexpr int32_t TILE_MARGIN_X = 24;
constexpr int32_t TILE_MARGIN_Y = 26;

constexpr int32_t ICON_SIZE = 62;

constexpr uint32_t BACKGROUND = 0x1D2A43;
constexpr uint32_t HOVER_COLOR = 0x344C73;
constexpr uint32_t BORDER_COLOR = 0x354765;
constexpr uint32_t HOVER_BORDER = 0xA28DFF;
constexpr uint32_t TEXT_COLOR = 0xEDF2FF;
constexpr uint32_t DISABLED_COLOR = 0x8C9AB2;

Tile tiles[] = {
    {
        1,
        "Files",
        &Assets::apps_files,
        true
    },
    {
        2,
        "Settings",
        &Assets::apps_settings,
        true
    },
    {
        3,
        "Terminal",
        &Assets::apps_terminal,
        true
    },
    {
        4,
        "Monitor",
        &Assets::apps_system_monitor,
        true
    },
    {
        5,
        "About",
        &Assets::apps_about,
        true
    },
    {
        6,
        "Browser",
        nullptr,
        true
    }
};

constexpr uint32_t TILE_COUNT =
    sizeof(tiles) / sizeof(tiles[0]);

int32_t screenWidth = 1024;
int32_t screenHeight = 768;

int32_t hoveredTile = -1;

struct Rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

Rect bounds_for(uint32_t index) {
    // Three columns.
    // Because two felt lonely.
    const int32_t column =
        static_cast<int32_t>(index % 3);

    const int32_t row =
        static_cast<int32_t>(index / 3);

    return {
        TILE_MARGIN_X +
            column * (TILE_WIDTH + TILE_GAP),

        TILE_MARGIN_Y +
            row * (TILE_HEIGHT + TILE_GAP),

        TILE_WIDTH,
        TILE_HEIGHT
    };
}

bool contains(
    const Rect& rect,
    int32_t x,
    int32_t y
) {
    return x >= rect.x &&
           y >= rect.y &&
           x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

void border(
    const Rect& r,
    uint32_t color
) {
    Image::rectangle(
        r.x, r.y,
        r.width, 2,
        color
    );

    Image::rectangle(
        r.x,
        r.y + r.height - 2,
        r.width, 2,
        color
    );

    Image::rectangle(
        r.x, r.y,
        2, r.height,
        color
    );

    Image::rectangle(
        r.x + r.width - 2,
        r.y,
        2, r.height,
        color
    );
}

bool blocked_by_window(
    int32_t x,
    int32_t y
) {
    // We do not click through actual windows.
    // The ghosts can wait.

    for (uint32_t i = 0;
         i < Windows::count();
         ++i) {

        const Windows::Window* window =
            Windows::at(i);

        if (!window || !window->visible)
            continue;

        if (window->state ==
            Windows::WindowState::Minimized)
            continue;

        if (Windows::contains(
            window->bounds, x, y
        ))
            return true;
    }

    return false;
}

int32_t hit_test(
    int32_t x,
    int32_t y
) {
    if (x < 0 || y < 0 ||
        x >= screenWidth ||
        y >= screenHeight)
        return -1;

    if (blocked_by_window(x, y))
        return -1;

    for (uint32_t i = 0;
         i < TILE_COUNT;
         ++i) {

        if (contains(bounds_for(i), x, y))
            return static_cast<int32_t>(i);
    }

    return -1;
}

void render_browser_placeholder(
    const Rect& r
) {
    const int32_t x =
        r.x + (r.width - ICON_SIZE) / 2;

    const int32_t y = r.y + 10;

    // Browser is still locked in the future.
    Image::rectangle(
        x, y,
        ICON_SIZE, ICON_SIZE,
        0x35445E
    );

    Image::rectangle(
        x + 5, y + 5,
        ICON_SIZE - 10, ICON_SIZE - 10,
        0x1B2943
    );

    Image::text(
        x + 19,
        y + 22,
        "WWW",
        0xB6C5DD
    );
}

void render_tile(uint32_t index) {
    const Tile& tile = tiles[index];
    const Rect r = bounds_for(index);

    const bool hovered =
        hoveredTile == static_cast<int32_t>(index);

    // An actual square with an actual boundary.
    // Revolutionary.
    Image::rectangle(
        r.x,
        r.y,
        r.width,
        r.height,
        hovered ? HOVER_COLOR : BACKGROUND
    );

    border(
        r,
        hovered ? HOVER_BORDER : BORDER_COLOR
    );

    // Very small fake depth effect.
    Image::rectangle(
        r.x + 4,
        r.y + 4,
        r.width - 8,
        2,
        hovered ? 0x7A8DB8 : 0x2A3A59
    );

    if (tile.icon && tile.icon->rgba) {
        Image::draw_scaled(
            *tile.icon,
            r.x + (r.width - ICON_SIZE) / 2,
            r.y + 10,
            ICON_SIZE,
            ICON_SIZE
        );
    } else {
        render_browser_placeholder(r);
    }

    // Our font is 6 pixels per character.
    // Centering is not a mystical operation.
    uint32_t length = 0;

    while (tile.name[length] &&
           length < 24)
        ++length;

    const int32_t textWidth =
        static_cast<int32_t>(length * 6);

    const int32_t textX =
        r.x + (r.width - textWidth) / 2;

    Image::text(
        textX,
        r.y + 85,
        tile.name,
        tile.available
            ? TEXT_COLOR
            : DISABLED_COLOR
    );

    if (!tile.available) {
        Image::text(
            r.x + 35,
            r.y + 99,
            "SOON",
            DISABLED_COLOR
        );
    }
}

} // namespace

void initialize() {
    hoveredTile = -1;
}

void set_screen_size(
    int32_t width,
    int32_t height
) {
    screenWidth = width;
    screenHeight = height;
}

void render() {
    for (uint32_t i = 0; i < TILE_COUNT; ++i)
        render_tile(i);
}

bool hover(int32_t x, int32_t y) {
    const int32_t next = hit_test(x, y);

    if (next == hoveredTile)
        return false;

    hoveredTile = next;
    return true;
}

uint32_t click(
    int32_t x,
    int32_t y
) {
    const int32_t index = hit_test(x, y);

    if (index < 0)
        return 0;

    const Tile& tile = tiles[index];

    if (!tile.available)
        return 0;

    return tile.appId;
}

} // namespace Astral::Desktop::Widgets::DesktopTiles
