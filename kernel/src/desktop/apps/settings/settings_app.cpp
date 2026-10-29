
#include "settings_app.h"
#include "lock_settings_page.h"

#include "../../graphics/image.h"
#include "../../widgets/setting_control.h"
#include "../../settings/registry.h"

namespace Astral::Desktop::Apps::SettingsApp {

namespace {

constexpr int32_t SIDEBAR_W = 186;
constexpr int32_t ROW_H = 29;
constexpr int32_t HEADER_H = 55;

State current{};

bool inside(
    int32_t x, int32_t y,
    int32_t left, int32_t top,
    int32_t width, int32_t height
) {
    return x >= left && y >= top &&
           static_cast<int64_t>(x) <
               static_cast<int64_t>(left) + width &&
           static_cast<int64_t>(y) <
               static_cast<int64_t>(top) + height;
}

uint32_t sidebar_rows(const Windows::Rect& bounds) {
    if (bounds.height <= 65)
        return 0;

    return static_cast<uint32_t>(
        (bounds.height - 65) / ROW_H
    );
}

uint32_t setting_rows(const Windows::Rect& bounds) {
    if (bounds.height <= 105)
        return 0;

    return static_cast<uint32_t>(
        (bounds.height - 105) /
        (Widgets::SettingControl::HEIGHT + 8)
    );
}

Windows::Rect content_rect(const Windows::Rect& bounds) {
    return {
        bounds.x + SIDEBAR_W,
        bounds.y,
        bounds.width - SIDEBAR_W,
        bounds.height
    };
}

void draw_sidebar(const Windows::Rect& bounds) {
    Image::rectangle(
        bounds.x, bounds.y,
        SIDEBAR_W, bounds.height,
        0x101B30
    );

    Image::text(
        bounds.x + 16,
        bounds.y + 18,
        "SETTINGS",
        0xBBAAFF, 2
    );

    const uint32_t visible = sidebar_rows(bounds);

    for (uint32_t row = 0; row < visible; ++row) {
        const uint32_t index =
            current.sidebarOffset + row;

        if (index >= Settings::CATEGORY_COUNT)
            break;

        const Category category =
            static_cast<Category>(index);

        const int32_t y =
            bounds.y + HEADER_H +
            static_cast<int32_t>(row) * ROW_H;

        const bool selected =
            category == current.category;

        if (selected) {
            Image::rectangle(
                bounds.x + 7, y - 3,
                SIDEBAR_W - 14, ROW_H - 3,
                0x3B416E
            );
        }

        Image::text(
            bounds.x + 16,
            y + 6,
            Settings::category_name(category),
            selected ? 0xFFFFFF : 0xB1BED6
        );
    }
}

void draw_settings(const Windows::Rect& bounds) {
    const Windows::Rect content = content_rect(bounds);

    if (content.width < 180)
        return;

    Image::text(
        content.x + 20,
        content.y + 19,
        Settings::category_name(current.category),
        0xEDF1FF, 2
    );

    Image::rectangle(
        content.x + 18,
        content.y + 50,
        content.width - 36,
        1,
        0x3A4C6B
    );

    const uint32_t total =
        Settings::category_count(current.category);

    const uint32_t visible =
        setting_rows(bounds);

    const int32_t controlW =
        content.width - 36;

    for (uint32_t row = 0; row < visible; ++row) {
        const uint32_t index =
            current.settingOffset + row;

        if (index >= total)
            break;

        const Settings::Item* item =
            Settings::category_item(
                current.category, index
            );

        if (!item)
            continue;

        const Windows::Rect control{
            content.x + 18,
            content.y + 67 +
                static_cast<int32_t>(row) *
                (Widgets::SettingControl::HEIGHT + 8),
            controlW,
            Widgets::SettingControl::HEIGHT
        };

        Widgets::SettingControl::render(
            *item, control, item->modified
        );
    }

    Image::text(
        content.x + 20,
        content.y + content.height - 21,
        "Values are saved in memory only",
        0x9EAFC8
    );
}

}

void initialize() {
    // Registry initialization belongs to the
    // Desktop session controller, not the app.
    // Opening Settings must not reset preferences.

    //all state is reset to default values on first launch.
    //This is done to ensure that the settings app starts with a clean state and does not carry over any previous configurations that may cause inconsistencies or unexpected behavior.
    //i wouldnt know tho.. actually i do know lmfaoo...

    current = {};
    current.category = Category::Appearance;
    current.sidebarOffset = 0;
    current.settingOffset = 0;
    current.initialized = true;

    LockSettingsPage::initialize();
}

void select(Category category) {
    if (static_cast<uint32_t>(category) >=
        Settings::CATEGORY_COUNT)
        return;

    current.category = category;
    current.settingOffset = 0;
}

void render(const Windows::Rect& bounds) {
    if (!current.initialized)
        initialize();

    if (bounds.width < 450 || bounds.height < 260)
        return;

    Image::rectangle(
        bounds.x, bounds.y,
        bounds.width, bounds.height,
        0x17243D
    );

    draw_sidebar(bounds);

    if (current.category == Category::Security) {
        const Windows::Rect content =
            content_rect(bounds);

        LockSettingsPage::render(content);
    } else {
        draw_settings(bounds);
    }
}

Action click(
    const Windows::Rect& bounds,
    int32_t mouseX,
    int32_t mouseY
) {
    if (!current.initialized)
        initialize();

    if (!inside(
        mouseX, mouseY,
        bounds.x, bounds.y,
        bounds.width, bounds.height
    ))
        return Action::None;

    if (mouseX < bounds.x + SIDEBAR_W) {
        const int32_t relative =
            mouseY - (bounds.y + HEADER_H);

        if (relative < 0)
            return Action::None;

        const uint32_t row =
            static_cast<uint32_t>(relative / ROW_H);

        if (row >= sidebar_rows(bounds))
            return Action::None;

        const uint32_t index =
            current.sidebarOffset + row;

        if (index >= Settings::CATEGORY_COUNT)
            return Action::None;

        select(static_cast<Category>(index));
        return Action::Redraw;
    }

    if (current.category == Category::Security) {
        const auto result =
            LockSettingsPage::click(
                content_rect(bounds),
                mouseX,
                mouseY
            );

        switch (result) {
            case LockSettingsPage::Action::Changed:
                return Action::Redraw;

            case LockSettingsPage::Action::LockDesktop:
                return Action::LockDesktop;

            case LockSettingsPage::Action::LockMain:
                return Action::LockMain;

            default:
                return Action::None;
        }
    }

    const Windows::Rect content =
        content_rect(bounds);

    const uint32_t visible = setting_rows(bounds);

    for (uint32_t row = 0; row < visible; ++row) {
        const uint32_t index =
            current.settingOffset + row;

        const Settings::Item* item =
            Settings::category_item(
                current.category, index
            );

        if (!item)
            break;

        const Windows::Rect control{
            content.x + 18,
            content.y + 67 +
                static_cast<int32_t>(row) *
                (Widgets::SettingControl::HEIGHT + 8),
            content.width - 36,
            Widgets::SettingControl::HEIGHT
        };

        if (Widgets::SettingControl::click(
            item->definition.id,
            control,
            mouseX,
            mouseY
        ))
            return Action::Redraw;
    }

    return Action::None;
}

void scroll(int32_t steps) {
    if (steps == 0)
        return;

    // Sidebar scroll for initial layout.
    // Input routing later distinguishes sidebar
    // from the main settings area.
    const uint32_t maxOffset =
        Settings::CATEGORY_COUNT - 1;

    if (steps < 0) {
        const uint32_t amount =
            static_cast<uint32_t>(
                -static_cast<int64_t>(steps)
            );

        current.sidebarOffset =
            amount > current.sidebarOffset
                ? 0
                : current.sidebarOffset - amount;
    } else {
        const uint32_t amount =
            static_cast<uint32_t>(steps);

        if (amount >= maxOffset - current.sidebarOffset)
            current.sidebarOffset = maxOffset;
        else
            current.sidebarOffset += amount;
    }
}

const State& state() {
    return current;
}

}
