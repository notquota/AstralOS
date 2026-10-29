
#include "launcher.h"
#include "../taskbar/taskbar.h"

namespace Astral::Desktop::Launcher {

namespace {

constexpr int32_t PANEL_W = 330;
constexpr int32_t PANEL_H = 365;
constexpr int32_t ROW_H = 49;
constexpr uint32_t MAX_ICONS = 32;
constexpr uint32_t VISIBLE_ROWS = 5;

bool isOpen = false;
uint32_t scrollOffset = 0;

const Image::Bitmap* appIcons[MAX_ICONS]{};

int32_t panel_y(int32_t screenHeight) {
    return screenHeight -
           Taskbar::HEIGHT -
           PANEL_H - 9;
}

bool contains(
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

uint32_t available_count() {
    uint32_t result = 0;

    for (uint32_t i = 0; i < AppRegistry::count(); ++i) {
        const auto* app = AppRegistry::at(i);
        if (app && app->available)
            ++result;
    }

    return result;
}

const AppRegistry::Application* available_at(
    uint32_t index
) {
    uint32_t seen = 0;

    for (uint32_t i = 0; i < AppRegistry::count(); ++i) {
        const auto* app = AppRegistry::at(i);
        if (!app || !app->available)
            continue;

        if (seen == index)
            return app;

        ++seen;
    }

    return nullptr;
}

}

void initialize() {
    isOpen = false;
    scrollOffset = 0;

    for (uint32_t i = 0; i < MAX_ICONS; ++i)
        appIcons[i] = nullptr;
}

bool visible() {
    return isOpen;
}

void open() {
    isOpen = true;
    scrollOffset = 0;
}

void close() {
    isOpen = false;
}

void toggle() {
    if (isOpen)
        close();
    else
        open();
}

void set_icon(
    AppRegistry::AppId appId,
    const Image::Bitmap* bitmap
) {
    if (appId == 0 || appId > MAX_ICONS)
        return;

    appIcons[appId - 1] = bitmap;
}

void render(
    int32_t screenWidth,
    int32_t screenHeight
) {
    if (!isOpen)
        return;

    (void)screenWidth;

    const int32_t x = 12;
    const int32_t y = panel_y(screenHeight);

    Image::rectangle(
        x + 4, y + 5,
        PANEL_W, PANEL_H,
        0x070D1A
    );

    Image::rectangle(
        x, y,
        PANEL_W, PANEL_H,
        0x172440
    );

    Image::rectangle(
        x, y,
        PANEL_W, 49,
        0x253756
    );

    Image::text(
        x + 20, y + 17,
        "Applications",
        0xEDF1FF,
        2
    );

    Image::text(
        x + 20, y + 67,
        "INSTALLED APPS",
        0xA2B4D0
    );

    const uint32_t total = available_count();

    if (total > VISIBLE_ROWS &&
        scrollOffset > total - VISIBLE_ROWS)
        scrollOffset = total - VISIBLE_ROWS;

    for (uint32_t row = 0;
         row < VISIBLE_ROWS;
         ++row) {
        const auto* app =
            available_at(scrollOffset + row);

        if (!app)
            break;

        const int32_t rowY =
            y + 91 + static_cast<int32_t>(row) * ROW_H;

        Image::rectangle(
            x + 10, rowY,
            PANEL_W - 20,
            ROW_H - 4,
            0x203250
        );

        const Image::Bitmap* bitmap = nullptr;

        if (app->id >= 1 && app->id <= MAX_ICONS)
            bitmap = appIcons[app->id - 1];

        if (bitmap && bitmap->rgba) {
            Image::draw_scaled(
                *bitmap,
                x + 18, rowY + 5,
                35, 35
            );
        } else {
            Image::text(
                x + 30, rowY + 17,
                "A", 0xAE9BFF
            );
        }

        Image::text(
            x + 70, rowY + 18,
            app->name, 0xEDF1FF
        );
    }

    if (total > VISIBLE_ROWS) {
        Image::text(
            x + 20, y + PANEL_H - 19,
            "Scroll for more apps",
            0x9FACCA
        );
    }
}

Result click(
    int32_t mouseX,
    int32_t mouseY,
    int32_t screenWidth,
    int32_t screenHeight
) {
    (void)screenWidth;

    if (!isOpen)
        return {};

    const int32_t x = 12;
    const int32_t y = panel_y(screenHeight);

    if (!contains(
        mouseX, mouseY,
        x, y,
        PANEL_W, PANEL_H
    )) {
        close();
        return {};
    }

    for (uint32_t row = 0;
         row < VISIBLE_ROWS;
         ++row) {
        const int32_t rowY =
            y + 91 + static_cast<int32_t>(row) * ROW_H;

        if (!contains(
            mouseX, mouseY,
            x + 10, rowY,
            PANEL_W - 20, ROW_H - 4
        ))
            continue;

        const auto* app =
            available_at(scrollOffset + row);

        if (!app)
            return {};

        close();
        return {true, app->id};
    }

    return {};
}

void scroll(int32_t steps) {
    if (!isOpen || steps == 0)
        return;

    const uint32_t total = available_count();

    const uint32_t maxOffset =
        total > VISIBLE_ROWS
            ? total - VISIBLE_ROWS
            : 0;

    if (steps < 0) {
        const uint32_t amount =
            static_cast<uint32_t>(
                -static_cast<int64_t>(steps)
            );

        scrollOffset =
            amount > scrollOffset
                ? 0
                : scrollOffset - amount;
    } else {
        const uint32_t amount =
            static_cast<uint32_t>(steps);

        if (amount >= maxOffset - scrollOffset)
            scrollOffset = maxOffset;
        else
            scrollOffset += amount;
    }
}

}
