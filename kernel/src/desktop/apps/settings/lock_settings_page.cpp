
#include "lock_settings_page.h"

#include "../../graphics/image.h"
#include "../../security/lock.h"

namespace Astral::Desktop::Apps::LockSettingsPage {

namespace {

Section activeSection = Section::Desktop;

bool inside(
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

void option(
    int32_t x, int32_t y,
    const char* name,
    const char* value,
    int32_t width
) {
    Image::rectangle(
        x, y, width, 43,
        0x21314D
    );

    Image::text(
        x + 13, y + 17,
        name, 0xE8EEFF
    );

    Image::text(
        x + width - 85,
        y + 17,
        value, 0xAB9AFF
    );
}

}

void initialize() {
    activeSection = Section::Desktop;
}

void set_section(Section section) {
    activeSection = section;
}

Section section() {
    return activeSection;
}

void render(const Windows::Rect& bounds) {
    const int32_t x = bounds.x + 18;
    const int32_t y = bounds.y + 15;
    const int32_t width = bounds.width - 36;

    if (width < 180 || bounds.height < 300)
        return;

    Image::text(
        x, y,
        "Security > Lock Screen",
        0xEDF1FF, 2
    );

    Image::rectangle(
        x, y + 36,
        110, 31,
        activeSection == Section::Desktop
            ? 0x554B83 : 0x263653
    );

    Image::rectangle(
        x + 117, y + 36,
        110, 31,
        activeSection == Section::Main
            ? 0x554B83 : 0x263653
    );

    Image::text(
        x + 18, y + 47,
        "Desktop", 0xFFFFFF
    );

    Image::text(
        x + 149, y + 47,
        "Main", 0xFFFFFF
    );

    const Lock::Settings settings = Lock::settings();

    const bool enabled =
        activeSection == Section::Desktop
            ? settings.desktopLockEnabled
            : settings.mainLockEnabled;

    option(
        x, y + 85,
        "Enable lock",
        enabled ? "ON" : "OFF",
        width
    );

    option(
        x, y + 137,
        "Require password",
        "ON",
        width
    );

    const char* timeout = "5 min";

    switch (settings.inactivitySeconds) {
        case 60: timeout = "1 min"; break;
        case 300: timeout = "5 min"; break;
        case 600: timeout = "10 min"; break;
        case 900: timeout = "15 min"; break;
        case 1800: timeout = "30 min"; break;
        default: timeout = "Custom"; break;
    }

    option(
        x, y + 189,
        "Inactivity timeout",
        timeout,
        width
    );

    Image::rectangle(
        x, y + 247,
        width, 35,
        0x4B447B
    );

    Image::text(
        x + 14, y + 259,
        activeSection == Section::Desktop
            ? "Lock Desktop Now"
            : "Lock Main Session Now",
        0xFFFFFF
    );
}

Action click(
    const Windows::Rect& bounds,
    int32_t mx,
    int32_t my
) {
    const int32_t x = bounds.x + 18;
    const int32_t y = bounds.y + 15;
    const int32_t width = bounds.width - 36;

    if (width < 180 || bounds.height < 300)
        return Action::None;

    if (inside(mx, my, x, y + 36, 110, 31)) {
        activeSection = Section::Desktop;
        return Action::Changed;
    }

    if (inside(mx, my, x + 117, y + 36, 110, 31)) {
        activeSection = Section::Main;
        return Action::Changed;
    }

    Lock::Settings settings = Lock::settings();

    if (inside(mx, my, x, y + 85, width, 43)) {
        if (activeSection == Section::Desktop)
            settings.desktopLockEnabled =
                !settings.desktopLockEnabled;
        else
            settings.mainLockEnabled =
                !settings.mainLockEnabled;

        Lock::set_settings(settings);
        return Action::Changed;
    }

    if (inside(mx, my, x, y + 137, width, 43)) {
        // Password verification remains mandatory
        // until alternative authentication exists.
        return Action::None;
    }

    if (inside(mx, my, x, y + 189, width, 43)) {
        constexpr uint32_t intervals[] = {
            60, 300, 600, 900, 1800
        };

        uint32_t index = 0;

        for (uint32_t i = 0; i < 5; ++i) {
            if (settings.inactivitySeconds == intervals[i]) {
                index = (i + 1) % 5;
                break;
            }
        }

        settings.inactivitySeconds = intervals[index];
        Lock::set_settings(settings);

        return Action::Changed;
    }

    if (inside(mx, my, x, y + 247, width, 35)) {
        return activeSection == Section::Desktop
            ? Action::LockDesktop
            : Action::LockMain;
    }

    return Action::None;
}

}
