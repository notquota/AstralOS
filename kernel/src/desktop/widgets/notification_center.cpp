
#include "notification_center.h"

#include "../graphics/image.h"
#include "../services/notifications.h"
#include "../taskbar/taskbar.h"

namespace Astral::Desktop::NotificationCenter {

namespace {

constexpr int32_t PANEL_W = 355;
constexpr int32_t PANEL_H = 390;
constexpr int32_t ROW_H = 78;

bool panelOpen = false;
uint32_t offset = 0;

bool inside(
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

int32_t panel_x(int32_t width) {
    return width - PANEL_W - 14;
}

int32_t panel_y(int32_t height) {
    return height - Taskbar::HEIGHT - PANEL_H - 10;
}

uint32_t max_offset() {
    const uint32_t total = Notifications::count();
    return total > 4 ? total - 4 : 0;
}

}

void initialize() {
    panelOpen = false;
    offset = 0;
}

bool visible() {
    return panelOpen;
}

void open() {
    panelOpen = true;
    offset = 0;
}

void close() {
    panelOpen = false;
}

void toggle() {
    panelOpen = !panelOpen;

    if (panelOpen)
        offset = 0;
}

void render(int32_t screenWidth, int32_t screenHeight) {
    if (!panelOpen)
        return;

    const int32_t x = panel_x(screenWidth);
    const int32_t y = panel_y(screenHeight);

    Image::rectangle(
        x + 5, y + 6,
        PANEL_W, PANEL_H,
        0x050A14
    );

    Image::rectangle(
        x, y,
        PANEL_W, PANEL_H,
        0x17243C
    );

    Image::rectangle(
        x, y,
        PANEL_W, 49,
        0x273854
    );

    Image::text(
        x + 17, y + 17,
        "Notifications",
        0xEDF2FF,
        2
    );

    Image::text(
        x + 18, y + PANEL_H - 24,
        "Click an item to dismiss it",
        0x9CABC5
    );

    const uint32_t total = Notifications::count();

    if (total == 0) {
        Image::text(
            x + 25, y + 105,
            "No notifications",
            0xAAB8CF
        );
        return;
    }

    if (offset > max_offset())
        offset = max_offset();

    for (uint32_t row = 0; row < 4; ++row) {
        const auto* item =
            Notifications::at(offset + row);

        if (!item)
            break;

        const int32_t rowY =
            y + 57 + static_cast<int32_t>(row) * ROW_H;

        Image::rectangle(
            x + 12, rowY,
            PANEL_W - 24,
            ROW_H - 7,
            item->read ? 0x1D2D46 : 0x283A5B
        );

        uint32_t levelColor = 0xAC9AFF;

        switch (item->level) {
            case Notifications::Level::Success:
                levelColor = 0x7BD5AB;
                break;
            case Notifications::Level::Warning:
                levelColor = 0xE7BD76;
                break;
            case Notifications::Level::Error:
                levelColor = 0xEE8B9B;
                break;
            default:
                break;
        }

        Image::rectangle(
            x + 12, rowY,
            3, ROW_H - 7,
            levelColor
        );

        Image::text(
            x + 24, rowY + 12,
            item->title,
            0xFFFFFF
        );

        Image::text(
            x + 24, rowY + 36,
            item->message,
            0xABBAD3
        );
    }
}

bool click(
    int32_t mx,
    int32_t my,
    int32_t screenWidth,
    int32_t screenHeight
) {
    if (!panelOpen)
        return false;

    const int32_t x = panel_x(screenWidth);
    const int32_t y = panel_y(screenHeight);

    if (!inside(mx, my, x, y, PANEL_W, PANEL_H)) {
        close();
        return false;
    }

    for (uint32_t row = 0; row < 4; ++row) {
        const int32_t rowY =
            y + 57 + static_cast<int32_t>(row) * ROW_H;

        if (!inside(
            mx, my,
            x + 12, rowY,
            PANEL_W - 24,
            ROW_H - 7
        ))
            continue;

        const auto* notification =
            Notifications::at(offset + row);

        if (!notification)
            return true;

        Notifications::dismiss(notification->id);

        if (offset > max_offset())
            offset = max_offset();

        return true;
    }

    return true;
}

void scroll(int32_t steps) {
    if (!panelOpen || steps == 0)
        return;

    const uint32_t maximum = max_offset();

    if (steps < 0) {
        const uint32_t amount =
            static_cast<uint32_t>(
                -static_cast<int64_t>(steps)
            );

        offset = amount > offset
            ? 0 : offset - amount;
    } else {
        const uint32_t amount =
            static_cast<uint32_t>(steps);

        if (amount >= maximum - offset)
            offset = maximum;
        else
            offset += amount;
    }
}

}
