
#include "taskbar.h"

#include "../taskbar/system_status.h"
#include "../windows/window_manager.h"

namespace Astral::Desktop::Taskbar {

namespace {

constexpr uint32_t BACKGROUND = 0x10192D;
constexpr uint32_t BORDER = 0x314365;
constexpr uint32_t TEXT = 0xEAF0FF;
constexpr uint32_t MUTED = 0x9CAEC9;
constexpr uint32_t ACCENT = 0x9B8AF7;

IconSet icons{};

struct Slot {
    uint32_t appId;
    Windows::WindowId windowId;
    bool pinned;
};

Slot slots[32]{};
uint32_t slotCount = 0;

const Image::Bitmap* app_icon(uint32_t appId) {
    if (appId == 0 || appId > 32)
        return nullptr;

    return icons.apps[appId - 1];
}

Windows::WindowId latest_window(uint32_t appId) {
    Windows::WindowId best = Windows::INVALID_WINDOW;
    uint32_t bestZ = 0;

    for (uint32_t i = 0; i < Windows::count(); ++i) {
        const Windows::Window* w = Windows::at(i);
        if (!w || w->appId != appId)
            continue;

        if (best == Windows::INVALID_WINDOW ||
            w->zOrder >= bestZ) {
            best = w->id;
            bestZ = w->zOrder;
        }
    }

    return best;
}

void rebuild_slots() {
    slotCount = 0;

    // First display pinned applications in registry order.
    for (uint32_t i = 0; i < AppRegistry::count(); ++i) {
        const auto* app = AppRegistry::at(i);
        if (!app || !app->pinned || !app->available)
            continue;

        if (slotCount >= 32)
            break;

        slots[slotCount++] = {
            app->id,
            latest_window(app->id),
            true
        };
    }

    // Then display running applications that aren't pinned.
    for (uint32_t i = 0; i < Windows::count(); ++i) {
        const Windows::Window* w = Windows::at(i);
        if (!w)
            continue;

        bool exists = false;

        for (uint32_t j = 0; j < slotCount; ++j) {
            if (slots[j].appId == w->appId) {
                exists = true;
                break;
            }
        }

        if (exists || slotCount >= 32)
            continue;

        slots[slotCount++] = {
            w->appId,
            latest_window(w->appId),
            false
        };
    }
}

bool in_rect(
    int32_t mx, int32_t my,
    int32_t x, int32_t y,
    int32_t w, int32_t h
) {
    return mx >= x && my >= y &&
           static_cast<int64_t>(mx) <
               static_cast<int64_t>(x) + w &&
           static_cast<int64_t>(my) <
               static_cast<int64_t>(y) + h;
}

void draw_icon(
    const Image::Bitmap* bitmap,
    int32_t x,
    int32_t y,
    uint32_t size,
    const char* fallback
) {
    if (bitmap && bitmap->rgba) {
        Image::draw_scaled(
            *bitmap, x, y, size, size
        );
    } else if (fallback) {
        Image::text(
            x + 8, y + 13,
            fallback, TEXT
        );
    }
}

}

void initialize() {
    icons = {};
    slotCount = 0;
}

void set_icons(const IconSet& value) {
    icons = value;
}

void render(
    int32_t screenWidth,
    int32_t screenHeight
) {
    const int32_t top = screenHeight - HEIGHT;

    rebuild_slots();
    SystemStatus::refresh();

    Image::rectangle(
        0, top, screenWidth, HEIGHT,
        BACKGROUND
    );

    Image::rectangle(
        0, top, screenWidth, 1,
        BORDER
    );

    // Launcher / AstralOS logo.
    Image::rectangle(
        12, top + 7, 46, 44,
        0x1C2C49
    );

    draw_icon(
        icons.logo,
        17, top + 10,
        36, "A"
    );

    // Running and pinned applications.
    const int32_t availableRight = screenWidth - 205;
    const int32_t maxVisible =
        availableRight > 76
            ? (availableRight - 76) / 51
            : 0;

    for (uint32_t i = 0;
         i < slotCount &&
         static_cast<int32_t>(i) < maxVisible;
         ++i) {
        const Slot& slot = slots[i];
        const int32_t x = 76 +
            static_cast<int32_t>(i) * 51;

        const bool focused =
            slot.windowId != Windows::INVALID_WINDOW &&
            slot.windowId == Windows::focused();

        if (focused) {
            Image::rectangle(
                x, top + 5, 45, 46,
                0x293E65
            );
        }

        draw_icon(
            app_icon(slot.appId),
            x + 6, top + 8,
            34, "App"
        );

        if (slot.windowId != Windows::INVALID_WINDOW) {
            Image::rectangle(
                x + 15,
                top + 52,
                16,
                2,
                focused ? ACCENT : MUTED
            );
        }
    }

    // Clock and battery are separate from their icons.
    Image::text(
        screenWidth - 183,
        top + 23,
        SystemStatus::time_text(),
        TEXT
    );

    draw_icon(
        icons.battery,
        screenWidth - 104,
        top + 14,
        25, nullptr
    );

    Image::text(
        screenWidth - 74,
        top + 23,
        SystemStatus::battery_text(),
        TEXT
    );
}

Action click(
    int32_t x,
    int32_t y,
    int32_t screenWidth,
    int32_t screenHeight
) {
    const int32_t top = screenHeight - HEIGHT;

    if (y < top || y >= screenHeight)
        return {};

    if (in_rect(x, y, 12, top + 7, 46, 44))
        return {ActionType::ToggleLauncher, 0};

    rebuild_slots();

    const int32_t availableRight = screenWidth - 205;
    const int32_t maxVisible =
        availableRight > 76
            ? (availableRight - 76) / 51
            : 0;

    for (uint32_t i = 0;
         i < slotCount &&
         static_cast<int32_t>(i) < maxVisible;
         ++i) {
        const int32_t buttonX =
            76 + static_cast<int32_t>(i) * 51;

        if (!in_rect(
            x, y, buttonX, top + 5,
            45, 48
        ))
            continue;

        const Slot& slot = slots[i];

        if (slot.windowId == Windows::INVALID_WINDOW)
            return {
                ActionType::LaunchApp,
                slot.appId
            };

        const Windows::Window* window =
            Windows::get(slot.windowId);

        if (window &&
            window->state == Windows::WindowState::Minimized)
            return {
                ActionType::RestoreWindow,
                slot.windowId
            };

        if (Windows::focused() == slot.windowId)
            return {
                ActionType::MinimizeWindow,
                slot.windowId
            };

        return {
            ActionType::FocusWindow,
            slot.windowId
        };
    }

    if (x >= screenWidth - 195)
        return {
            ActionType::OpenQuickSettings,
            0
        };

    return {};
}

}
