
#include "renderer.h"

#include "wallpaper.h"
#include "icons.h"

#include "../windows/window_manager.h"
#include "../windows/decorations.h"

#include "../taskbar/taskbar.h"
#include "../launcher/launcher.h"

#include "../apps/files/file_manager.h"
#include "../apps/settings/settings_app.h"
#include "../apps/terminal/terminal_app.h"
#include "../apps/system_monitor/system_monitor.h"
#include "../apps/about/about_app.h"

namespace Astral::Desktop::Renderer {

namespace {

Configuration current{
    true,
    true,
    true
};

const Image::Bitmap* cursorBitmap = nullptr;

int32_t cursorX = 0;
int32_t cursorY = 0;

bool cursorDrawn = false;

void draw_desktop_icons() {
    // Desktop launch shortcuts are independent
    // from the application windows.
    constexpr uint32_t apps[] = {1, 2, 3, 4, 5};

    for (uint32_t i = 0; i < 5; ++i) {
        const int32_t x = 20;
        const int32_t y =
            26 + static_cast<int32_t>(i) * 99;

        Icons::draw_app(apps[i], x + 7, y, 52);

        const char* label = "Application";

        switch (apps[i]) {
            case 1: label = "Files"; break;
            case 2: label = "Settings"; break;
            case 3: label = "Terminal"; break;
            case 4: label = "Monitor"; break;
            case 5: label = "About"; break;
        }

        Image::text(
            x + 7, y + 62,
            label, 0xEDF1FF
        );
    }
}

void draw_app(const Windows::Window& window) {
    const Windows::Rect client =
        Decorations::client_bounds(window);

    if (client.width <= 0 || client.height <= 0)
        return;

    switch (window.appId) {
        case 1:
            Apps::FileManager::render(client);
            break;

        case 2:
            Apps::SettingsApp::render(client);
            break;

        case 3:
            Apps::TerminalApp::render(client);
            break;

        case 4:
            Apps::SystemMonitor::render(client);
            break;

        case 5:
            Apps::AboutApp::render(client);
            break;

        default:
            Image::rectangle(
                client.x, client.y,
                client.width, client.height,
                0x172440
            );

            Image::text(
                client.x + 20,
                client.y + 20,
                "Application renderer unavailable",
                0xAFBED8
            );
            break;
    }
}

void draw_windows() {
    // Insertion-sort a small fixed list by z-order.
    // Do not mutate the WindowManager's own array.
    const Windows::Window* sorted[Windows::MAX_WINDOWS]{};
    uint32_t total = 0;

    for (uint32_t i = 0; i < Windows::count(); ++i) {
        const Windows::Window* window =
            Windows::at(i);

        if (!window ||
            !window->visible ||
            window->state ==
                Windows::WindowState::Minimized)
            continue;

        uint32_t position = total;

        while (position > 0 &&
               sorted[position - 1]->zOrder >
               window->zOrder) {
            sorted[position] = sorted[position - 1];
            --position;
        }

        sorted[position] = window;
        ++total;
    }

    for (uint32_t i = 0; i < total; ++i) {
        Decorations::render(*sorted[i]);
        draw_app(*sorted[i]);
    }
}

void draw_cursor() {
    if (!cursorBitmap || !cursorBitmap->rgba)
        return;

    Image::draw_overlay(
        *cursorBitmap,
        cursorX, cursorY,
        24, 24
    );

    cursorDrawn = true;
}

}

void initialize() {
    current = {true, true, true};

    cursorBitmap = nullptr;
    cursorX = 0;
    cursorY = 0;
    cursorDrawn = false;
}

void set_configuration(const Configuration& config) {
    current = config;
}

const Configuration& configuration() {
    return current;
}

void set_cursor(const Image::Bitmap* bitmap) {
    cursorBitmap = bitmap;
    cursorDrawn = false;
}

void compose() {
    const uint32_t width = Image::width();
    const uint32_t height = Image::height();

    if (!width || !height)
        return;

    if (current.showWallpaper) {
        Wallpaper::render(width, height);
    } else {
        Image::clear(0x091225);
    }

    if (current.showDesktopIcons)
        draw_desktop_icons();

    draw_windows();

    if (current.showTaskbar) {
        Taskbar::render(
            static_cast<int32_t>(width),
            static_cast<int32_t>(height)
        );
    }

    Launcher::render(
        static_cast<int32_t>(width),
        static_cast<int32_t>(height)
    );
}

void present() {
    Image::present();
    cursorDrawn = false;
    draw_cursor();
}

void render() {
    compose();
    present();
}

void move_cursor(
    int32_t newX,
    int32_t newY
) {
    if (cursorDrawn) {
        // Restore only the old cursor footprint.
        Image::present_rect(
            cursorX, cursorY, 24, 24
        );
    }

    cursorX = newX;
    cursorY = newY;

    draw_cursor();
}

}
