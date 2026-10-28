
#include "desktop.h"
#include "preferences.h"

#include "../graphics/framebuffer.h"
#include "../graphics/font.h"
#include "../graphics/cursor_pixels.h"

#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"

#include "../arch/x86_64/lowlevel.h"

#include <stdint.h>

namespace Astral::Desktop {

static constexpr uint32_t BG = 0x0B1120;
static constexpr uint32_t PANEL = 0x17243B;
static constexpr uint32_t TASKBAR = 0x101C30;
static constexpr uint32_t SELECTED = 0x263C5F;
static constexpr uint32_t TEXT = 0xEDF2FF;
static constexpr uint32_t MUTED = 0xA4B3CC;
static constexpr uint32_t ACCENT = 0xA78BFA;

static bool active = false;
static bool launcherOpen = false;
static bool windowOpen = false;

static int32_t windowX = 120;
static int32_t windowY = 110;

static uint32_t screenW = 0;
static uint32_t screenH = 0;

static void rect(
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
) {
    if (x < 0 || y < 0)
        return;

    if (static_cast<uint64_t>(x) + width > screenW ||
        static_cast<uint64_t>(y) + height > screenH)
        return;

    Graphics::rectangle(
        static_cast<uint32_t>(x),
        static_cast<uint32_t>(y),
        width,
        height,
        color
    );
}

static void text(
    int32_t x,
    int32_t y,
    const char* message,
    uint32_t color,
    uint32_t scale = 1
) {
    if (!message)
        return;

    while (*message) {
        const uint8_t* glyph =
            Graphics::glyph(*message++);

        for (uint32_t row = 0; row < 7; ++row) {
            for (uint32_t col = 0; col < 5; ++col) {
                if (!(glyph[row] & (1u << (4 - col))))
                    continue;

                rect(
                    x + static_cast<int32_t>(col * scale),
                    y + static_cast<int32_t>(row * scale),
                    scale, scale, color
                );
            }
        }

        x += static_cast<int32_t>(6 * scale);
    }
}

static bool inside(
    int32_t mx,
    int32_t my,
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h
) {
    return mx >= x && my >= y &&
           mx < x + w && my < y + h;
}

static void draw_wallpaper() {
    Graphics::clear(BG);

    rect(
        0, 0,
        screenW, 5,
        ACCENT
    );

    const int32_t titleX =
        static_cast<int32_t>(screenW / 2) - 108;

    const int32_t titleY =
        static_cast<int32_t>(screenH / 2) - 40;

    text(
        titleX,
        titleY,
        "ASTRAL",
        0x435A80,
        5
    );

    text(
        titleX + 16,
        titleY + 48,
        "AstralOS Desktop",
        MUTED
    );
}

static void draw_taskbar() {
    const int32_t y =
        static_cast<int32_t>(screenH) - 50;

    rect(0, y, screenW, 50, TASKBAR);

    rect(10, y + 8, 112, 34, SELECTED);

    rect(10, y + 8, 4, 34, ACCENT);

    text(
        26,
        y + 21,
        "AstralOS",
        TEXT
    );

    if (windowOpen) {
        rect(136, y + 8, 125, 34, SELECTED);
        text(
            147,
            y + 21,
            "Welcome",
            TEXT
        );
    }

    text(
        static_cast<int32_t>(screenW) - 133,
        y + 21,
        "v0.0.7-alpha",
        MUTED
    );
}

static void draw_window() {
    if (!windowOpen)
        return;

    rect(
        windowX + 5,
        windowY + 6,
        390, 240,
        0x080D18
    );

    rect(
        windowX,
        windowY,
        390, 240,
        PANEL
    );

    rect(
        windowX,
        windowY,
        390, 38,
        SELECTED
    );

    rect(
        windowX,
        windowY,
        4, 38,
        ACCENT
    );

    text(
        windowX + 16,
        windowY + 15,
        "Welcome to AstralOS",
        TEXT
    );

    rect(
        windowX + 351,
        windowY + 7,
        29, 24,
        0x7F344A
    );

    text(
        windowX + 362,
        windowY + 16,
        "X",
        TEXT
    );

    text(
        windowX + 24,
        windowY + 75,
        "AstralOS graphical shell",
        TEXT,
        2
    );

    text(
        windowX + 24,
        windowY + 114,
        "Desktop v0.0.7-alpha",
        ACCENT
    );

    text(
        windowX + 24,
        windowY + 139,
        "Kernel and terminal remain native.",
        MUTED
    );

    text(
        windowX + 24,
        windowY + 166,
        "Open the launcher for actions.",
        MUTED
    );
}

static void draw_launcher() {
    if (!launcherOpen)
        return;

    const int32_t y =
        static_cast<int32_t>(screenH) - 365;

    rect(
        10, y,
        265, 305,
        PANEL
    );

    rect(
        10, y,
        265, 44,
        SELECTED
    );

    text(
        27,
        y + 17,
        "AstralOS Launcher",
        TEXT,
        2
    );

    rect(22, y + 62, 241, 43, SELECTED);
    text(38, y + 79, "Welcome", TEXT);

    rect(22, y + 115, 241, 43, SELECTED);
    text(38, y + 132, "Return to Terminal", TEXT);

    text(
        28, y + 244,
        "More applications coming soon.",
        MUTED
    );
}

static void draw_cursor() {
    if (!Mouse::initialized())
        return;

    const int32_t px = Mouse::x();
    const int32_t py = Mouse::y();

    for (uint32_t y = 0;
         y < Assets::CURSOR_HEIGHT;
         ++y) {

        for (uint32_t x = 0;
             x < Assets::CURSOR_WIDTH;
             ++x) {

            const uint32_t pixel =
                Assets::CURSOR_RGBA[
                    y * Assets::CURSOR_WIDTH + x
                ];

            if ((pixel & 0xFF) < 128)
                continue;

            rect(
                px + static_cast<int32_t>(x),
                py + static_cast<int32_t>(y),
                1, 1,
                pixel >> 8
            );
        }
    }
}

static void render() {
    draw_wallpaper();
    draw_window();
    draw_launcher();
    draw_taskbar();
    draw_cursor();
}

static void handle_click() {
    const int32_t mx = Mouse::x();
    const int32_t my = Mouse::y();

    const int32_t barY =
        static_cast<int32_t>(screenH) - 50;

    if (inside(mx, my, 10, barY + 8, 112, 34)) {
        launcherOpen = !launcherOpen;
        return;
    }

    if (launcherOpen) {
        const int32_t menuY =
            static_cast<int32_t>(screenH) - 365;

        if (inside(
            mx, my,
            22, menuY + 62,
            241, 43
        )) {
            windowOpen = true;
            launcherOpen = false;
            return;
        }

        if (inside(
            mx, my,
            22, menuY + 115,
            241, 43
        )) {
            active = false;
            return;
        }

        launcherOpen = false;
    }

    if (windowOpen &&
        inside(
            mx, my,
            windowX + 351,
            windowY + 7,
            29, 24
        )) {
        windowOpen = false;
    }
}

bool running() {
    return active;
}

void run() {
    if (!can_launch())
        return;

    if (active)
        return;

    screenW = Graphics::width();
    screenH = Graphics::height();

    if (screenW < 800 || screenH < 600)
        return;

    active = true;
    launcherOpen = false;
    windowOpen = true;

    windowX =
        static_cast<int32_t>(screenW / 2) - 195;

    windowY =
        static_cast<int32_t>(screenH / 2) - 150;

    Mouse::set_ui_capture(true);

    uint64_t lastMouseRevision =
        Mouse::revision();

    render();

    while (active) {
        const char key = Keyboard::read_char();

        // Escape exits Desktop for now.
        if (key == 27) {
            active = false;
            break;
        }

        if (Mouse::take_left_click()) {
            handle_click();
            render();
        }

        if (Mouse::revision() != lastMouseRevision) {
            lastMouseRevision = Mouse::revision();
            render();
        }

        // Consume wheel events while the Desktop
        // has exclusive pointer control.
        Mouse::take_scroll();

        astral_pause();
    }

    Mouse::set_ui_capture(false);

    // Do not render terminal here. The existing
    // terminal subsystem should restore its own
    // console state when the session returns.
}

}
