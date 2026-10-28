
#include "wizard.h"
#include "settings.h"

#include "../graphics/framebuffer.h"
#include "../graphics/font.h"
#include "../graphics/console.h"
#include "../graphics/theme.h"
#include "../graphics/cursor_pixels.h"

#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/manager.h"
#include "../drivers/network/wifi.h"

#include "../memory/physical.h"
#include "../filesystem/vfs.h"
#include "../security/account.h"

#include "../arch/x86_64/lowlevel.h"

#include <stdint.h>

namespace Astral::ConfigWizard {

using Config::Settings;
using Config::Accent;

static constexpr int32_t WIDTH = 760;
static constexpr int32_t HEIGHT = 490;
static constexpr int32_t SIDEBAR_WIDTH = 205;
static constexpr int32_t CONTENT_OFFSET = 224;

static int32_t originX = 0;
static int32_t originY = 0;

static int selectedCategory = 0;
static int selectedRow = 0;

static Settings working{};

static const char* categories[] = {
    "Appearance",
    "Input",
    "Language",
    "Date & Time",
    "Network",
    "Storage",
    "Accounts",
    "About"
};

static constexpr int CATEGORY_COUNT = 8;

struct Palette {
    uint32_t background;
    uint32_t panel;
    uint32_t sidebar;
    uint32_t item;
    uint32_t selected;
    uint32_t text;
    uint32_t muted;
};

static Palette palette() {
    if (working.darkMode) {
        return {
            0x080F24,
            0x111B34,
            0x0E1830,
            0x182542,
            0x26365C,
            0xE9D5FF,
            0x94A3B8
        };
    }

    return {
        0xE9EEF7,
        0xFFFFFF,
        0xDCE5F2,
        0xEFF3F9,
        0xC8D8F0,
        0x18243B,
        0x586982
    };
}

static uint32_t accent_color() {
    switch (working.accent) {
        case Accent::Blue: return 0x60A5FA;
        case Accent::Green: return 0x4ADE80;
        case Accent::Amber: return 0xFBBF24;
        default: return 0xA78BFA;
    }
}

static void rect(
    int32_t x,
    int32_t y,
    uint32_t w,
    uint32_t h,
    uint32_t color
) {
    if (x < 0 || y < 0)
        return;

    if (static_cast<uint64_t>(x) + w >
        Graphics::width())
        return;

    if (static_cast<uint64_t>(y) + h >
        Graphics::height())
        return;

    Graphics::rectangle(
        static_cast<uint32_t>(x),
        static_cast<uint32_t>(y),
        w, h, color
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
        if (x >= static_cast<int32_t>(
            Graphics::width()))
            return;

        const uint8_t* glyph =
            Graphics::glyph(*message++);

        for (uint32_t row = 0; row < 7; ++row) {
            for (uint32_t col = 0; col < 5; ++col) {
                if (!(glyph[row] & (1u << (4 - col))))
                    continue;

                const int32_t px =
                    x + static_cast<int32_t>(
                        col * scale
                    );

                const int32_t py =
                    y + static_cast<int32_t>(
                        row * scale
                    );

                rect(px, py, scale, scale, color);
            }
        }

        x += static_cast<int32_t>(6 * scale);
    }
}

static void number(
    int32_t x,
    int32_t y,
    uint32_t value,
    uint32_t color
) {
    char reversed[16];
    char buffer[16];

    uint32_t count = 0;

    do {
        reversed[count++] =
            static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value && count < 15);

    for (uint32_t i = 0; i < count; ++i)
        buffer[i] = reversed[count - i - 1];

    buffer[count] = '\0';

    text(x, y, buffer, color);
}

static void timezone_text(
    int32_t x,
    int32_t y,
    int32_t offset,
    uint32_t color
) {
    text(x, y, offset < 0 ? "UTC-" : "UTC+", color);

    uint32_t absolute = static_cast<uint32_t>(
        offset < 0 ? -offset : offset
    );

    const uint32_t hours = absolute / 60;
    const uint32_t minutes = absolute % 60;

    number(x + 30, y, hours, color);

    const int32_t minuteX =
        x + (hours >= 10 ? 48 : 42);

    text(minuteX, y, ":", color);

    char digits[3] = {
        static_cast<char>('0' + minutes / 10),
        static_cast<char>('0' + minutes % 10),
        '\0'
    };

    text(minuteX + 6, y, digits, color);
}

static int row_count() {
    switch (selectedCategory) {
        case 0: return 2;
        case 1: return 4;
        case 2: return 1;
        case 3: return 2;
        case 4: return 3;
        case 5: return 3;
        case 6: return 2;
        case 7: return 3;
        default: return 1;
    }
}

static const char* row_label(int row) {
    switch (selectedCategory) {
        case 0:
            return row == 0
                ? "Accent color"
                : "Interface theme";

        case 1:
            if (row == 0) return "Pointer speed";
            if (row == 1) return "Scroll sensitivity";
            if (row == 2) return "Pointer visibility";
            return "PS/2 scroll wheel";

        case 2:
            return "System language";

        case 3:
            return row == 0
                ? "Timezone offset"
                : "Clock format";

        case 4:
            if (row == 0) return "Network stack";
            if (row == 1) return "Ethernet";
            return "Wireless";

        case 5:
            if (row == 0) return "AstralFS";
            if (row == 1) return "SATA / AHCI";
            return "NVMe";

        case 6:
            return row == 0
                ? "Root account"
                : "Device owner";

        case 7:
            if (row == 0) return "OS version";
            if (row == 1) return "Architecture";
            return "Usable RAM";

        default:
            return "";
    }
}

static const char* row_value(int row) {
    switch (selectedCategory) {
        case 0:
            if (row == 0) {
                switch (working.accent) {
                    case Accent::Blue: return "Blue";
                    case Accent::Green: return "Green";
                    case Accent::Amber: return "Amber";
                    default: return "Violet";
                }
            }
            return working.darkMode ? "Dark" : "Light";

        case 1:
            if (row == 0) {
                if (working.pointerSpeed == 2)
                    return "Fast";
                if (working.pointerSpeed == 3)
                    return "Very fast";
                return "Normal";
            }

            if (row == 1) {
                switch (working.scrollLines) {
                    case 1: return "1 line";
                    case 3: return "3 lines";
                    case 6: return "6 lines";
                    case 9: return "9 lines";
                    default: return "Custom";
                }
            }

            if (row == 2)
                return working.showPointer
                    ? "Enabled" : "Disabled";

            return Mouse::wheel_supported()
                ? "Detected" : "Not detected";

        case 2:
            return "English";

        case 3:
            return row == 0
                ? "Change offset"
                : (working.use24HourClock
                    ? "24 hour" : "12 hour");

        case 4: {
            if (!Drivers::initialized())
                return "Not initialized";

            const auto& status = Drivers::status();

            if (row == 0)
                return status.networkReady
                    ? "Ready" : "Offline";

            if (row == 1) {
                if (status.ethernetReady)
                    return "Driver active";

                return status.ethernetDetected
                    ? "Device detected"
                    : "No adapter";
            }

            if (status.wifiDriverReady)
                return "Driver loaded";

            return status.wirelessControllerDetected
                ? "Driver required"
                : "No candidate";
        }

        case 5: {
            if (row == 0)
                return VFS::initialized()
                    ? "RAM filesystem" : "Offline";

            if (!Drivers::initialized())
                return "Not initialized";

            const auto& status = Drivers::status();

            if (row == 1) {
                if (status.ahciReady)
                    return "Read engine ready";

                return status.ahciDetected
                    ? "Controller detected"
                    : "Not detected";
            }

            if (status.nvmeReady)
                return "Read engine ready";

            return status.nvmeDetected
                ? "Controller detected"
                : "Not detected";
        }

        case 6:
            if (row == 0)
                return Security::root_exists()
                    ? "Configured" : "Not created";
            return Security::device_owner();

        case 7:
            if (row == 0) return "0.0.6-alpha";
            if (row == 1) return "x86_64";
            return "See below";
    }

    return "";
}

static bool editable(int row) {
    if (selectedCategory == 0)
        return true;

    if (selectedCategory == 1)
        return row <= 2;

    if (selectedCategory == 3)
        return true;

    return false;
}

static void activate_row(int row) {
    if (!editable(row))
        return;

    if (selectedCategory == 0) {
        if (row == 0) {
            switch (working.accent) {
                case Accent::Violet:
                    working.accent = Accent::Blue;
                    break;
                case Accent::Blue:
                    working.accent = Accent::Green;
                    break;
                case Accent::Green:
                    working.accent = Accent::Amber;
                    break;
                default:
                    working.accent = Accent::Violet;
                    break;
            }
        } else {
            working.darkMode = !working.darkMode;
        }
    }

    if (selectedCategory == 1) {
        if (row == 0)
            working.pointerSpeed =
                working.pointerSpeed % 3 + 1;

        if (row == 1) {
            if (working.scrollLines == 1)
                working.scrollLines = 3;
            else if (working.scrollLines == 3)
                working.scrollLines = 6;
            else if (working.scrollLines == 6)
                working.scrollLines = 9;
            else
                working.scrollLines = 1;
        }

        if (row == 2)
            working.showPointer = !working.showPointer;
    }

    if (selectedCategory == 3) {
        if (row == 0) {
            working.utcOffsetMinutes += 30;

            if (working.utcOffsetMinutes > 840)
                working.utcOffsetMinutes = -720;
        } else {
            working.use24HourClock =
                !working.use24HourClock;
        }
    }
}

static void draw_sidebar(const Palette& p) {
    rect(
        originX, originY,
        SIDEBAR_WIDTH, HEIGHT,
        p.sidebar
    );

    text(
        originX + 20,
        originY + 25,
        "AstralOS",
        p.text,
        3
    );

    text(
        originX + 20,
        originY + 58,
        "System Settings",
        p.muted
    );

    for (int i = 0; i < CATEGORY_COUNT; ++i) {
        const int32_t y = originY + 101 + i * 41;

        rect(
            originX + 10, y,
            SIDEBAR_WIDTH - 20, 35,
            selectedCategory == i
                ? p.selected : p.sidebar
        );

        if (selectedCategory == i) {
            rect(
                originX + 10, y,
                3, 35, accent_color()
            );
        }

        text(
            originX + 24, y + 13,
            categories[i],
            selectedCategory == i
                ? p.text : p.muted
        );
    }
}

static void draw_rows(const Palette& p) {
    const int32_t x = originX + CONTENT_OFFSET;
    const int32_t top = originY + 115;

    for (int i = 0; i < row_count(); ++i) {
        // Four rows must fit above the footer.
        const int32_t y = top + i * 68;

        rect(
            x, y, 512, 58,
            selectedRow == i
                ? p.selected : p.item
        );

        text(
            x + 18, y + 12,
            row_label(i),
            p.text,
            2
        );

        text(
            x + 18, y + 37,
            row_value(i),
            editable(i) ? accent_color() : p.muted
        );

        if (editable(i)) {
            text(
                x + 480, y + 22,
                ">",
                accent_color(),
                2
            );
        }
    }

    if (selectedCategory == 3) {
        timezone_text(
            x + 210, top + 37,
            working.utcOffsetMinutes,
            accent_color()
        );
    }

    const int32_t noteY = originY + 395;

    if (selectedCategory == 4) {
        text(
            x, noteY,
            "Driver status; connectivity may vary.",
            p.muted
        );
    } else if (selectedCategory == 5) {
        text(
            x, noteY,
            "AstralFS is currently RAM-only.",
            p.muted
        );
    } else if (selectedCategory == 7) {
        text(
            x, noteY,
            "Physical RAM (MiB):",
            p.muted
        );

        number(
            x + 140,
            noteY,
            static_cast<uint32_t>(
                Memory::total_memory_mib()
            ),
            accent_color()
        );
    } else if (selectedCategory == 3) {
        text(
            x, noteY,
            "Offset preference only; RTC unchanged.",
            p.muted
        );
    } else if (selectedCategory == 1) {
        text(
            x, noteY,
            "Wheel changes sidebar category.",
            p.muted
        );
    }
}

static void draw_cursor() {
    if (!Mouse::initialized())
        return;

    // Keep the pointer visible in Settings for
    // navigation, even if the saved preference
    // requests it hidden elsewhere.
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
    const Palette p = palette();

    Graphics::clear(p.background);

    rect(
        originX, originY,
        WIDTH, HEIGHT,
        p.panel
    );

    draw_sidebar(p);

    const int32_t x = originX + CONTENT_OFFSET;

    text(
        x, originY + 26,
        categories[selectedCategory],
        p.text, 3
    );

    text(
        x, originY + 67,
        "AstralOS configuration",
        p.muted
    );

    draw_rows(p);

    rect(
        originX + 472,
        originY + 430,
        105, 38,
        p.selected
    );

    text(
        originX + 497,
        originY + 445,
        "Cancel",
        p.text
    );

    rect(
        originX + 590,
        originY + 430,
        150, 38,
        accent_color()
    );

    text(
        originX + 617,
        originY + 445,
        "Save & Exit",
        0x080F24
    );

    text(
        originX + 221,
        originY + 412,
        "W/S page  A/D row  Enter edit",
        p.muted
    );

    draw_cursor();
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

static bool mouse_click(bool& closeWizard) {
    closeWizard = false;

    const int32_t mx = Mouse::x();
    const int32_t my = Mouse::y();

    for (int i = 0; i < CATEGORY_COUNT; ++i) {
        const int32_t y =
            originY + 101 + i * 41;

        if (inside(
            mx, my,
            originX + 10, y,
            SIDEBAR_WIDTH - 20, 35
        )) {
            selectedCategory = i;
            selectedRow = 0;
            return false;
        }
    }

    const int32_t x = originX + CONTENT_OFFSET;

    for (int i = 0; i < row_count(); ++i) {
        const int32_t y =
            originY + 115 + i * 68;

        if (inside(mx, my, x, y, 512, 58)) {
            selectedRow = i;
            activate_row(i);
            return false;
        }
    }

    if (inside(
        mx, my,
        originX + 472, originY + 430,
        105, 38
    )) {
        closeWizard = true;
        return false;
    }

    if (inside(
        mx, my,
        originX + 590, originY + 430,
        150, 38
    )) {
        closeWizard = true;
        return true;
    }

    return false;
}

void open() {
    if (Graphics::width() < 800 ||
        Graphics::height() < 600) {
        Console::write(
            "Settings require 800x600 or higher.\n",
            Theme::ERROR
        );
        return;
    }

    working = Config::get();

    selectedCategory = 0;
    selectedRow = 0;

    originX = static_cast<int32_t>(
        (Graphics::width() - WIDTH) / 2
    );

    originY = static_cast<int32_t>(
        (Graphics::height() - HEIGHT) / 2
    );

    Mouse::set_ui_capture(true);

    uint64_t lastRevision = Mouse::revision();
    bool save = false;

    render();

    while (true) {
        const char key = Keyboard::read_char();

        if (key == 27)
            break;

        bool redraw = false;

        if (key == 'w' || key == 'W') {
            selectedCategory =
                (selectedCategory + CATEGORY_COUNT - 1)
                % CATEGORY_COUNT;
            selectedRow = 0;
            redraw = true;
        }

        if (key == 's' || key == 'S') {
            selectedCategory =
                (selectedCategory + 1)
                % CATEGORY_COUNT;
            selectedRow = 0;
            redraw = true;
        }

        if (key == 'a' || key == 'A') {
            selectedRow =
                (selectedRow + row_count() - 1)
                % row_count();
            redraw = true;
        }

        if (key == 'd' || key == 'D') {
            selectedRow =
                (selectedRow + 1)
                % row_count();
            redraw = true;
        }

        if (key == '\n' || key == ' ') {
            activate_row(selectedRow);
            redraw = true;
        }

        // Scrolling now works while Settings
        // has exclusive control of the mouse.
        const int32_t scroll = Mouse::take_scroll();

        if (scroll != 0) {
            const int32_t direction =
                scroll > 0 ? -1 : 1;

            selectedCategory =
                (selectedCategory +
                 CATEGORY_COUNT + direction)
                % CATEGORY_COUNT;

            selectedRow = 0;
            redraw = true;
        }

        if (Mouse::take_left_click()) {
            bool closeWizard = false;

            const bool selectedSave =
                mouse_click(closeWizard);

            if (closeWizard) {
                save = selectedSave;
                break;
            }

            redraw = true;
        }

        if (Mouse::revision() != lastRevision) {
            lastRevision = Mouse::revision();
            redraw = true;
        }

        if (redraw)
            render();

        astral_pause();
    }

    if (save)
        Config::set(working);

    Mouse::set_ui_capture(false);

    Console::scroll_to_bottom();

    Console::write(
        save
            ? "Configuration applied (RAM only).\n"
            : "Configuration cancelled.\n",
        save ? Theme::SUCCESS : Theme::WARNING
    );
}

}
