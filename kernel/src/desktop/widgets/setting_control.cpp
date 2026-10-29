
#include "setting_control.h"

#include "../graphics/image.h"

namespace Astral::Desktop::Widgets::SettingControl {

namespace {

// Converting numbers manually because fck you std::to_string. I don't need your
// heap allocations..
void number_text(
    int32_t value,
    char* buffer
) {
    char reversed[16]{};
    uint32_t used = 0;

    int64_t n = value;
    const bool negative = n < 0;

    // INT_MIN exists solely to ruin somebody's Tuesday.
    // Using int64_t keeps us out of that particular circus.
    if (negative)
        n = -n;

    do {
        reversed[used++] =
            static_cast<char>('0' + n % 10);

        n /= 10;
    } while (n && used < 14);

    uint32_t position = 0;

    if (negative)
        buffer[position++] = '-';

    // Digits were collected backward.
    // Unfortunately, i insist on reading forward.
    while (used)
        buffer[position++] = reversed[--used];

    buffer[position] = '\0';
}

} // namespace

void render(
    const Settings::Item& item,
    const Windows::Rect& r,
    bool focused
) {
    // Negative-sized rectangles are not a feature.
    if (r.width <= 0 || r.height <= 0)
        return;

    Image::rectangle(
        r.x,
        r.y,
        r.width,
        r.height,
        focused ? 0x344568 : 0x22334F
    );

    Image::text(
        r.x + 13,
        r.y + 17,
        item.definition.name,
        0xEDF2FF
    );

    const int32_t right = r.x + r.width - 12;

    if (item.definition.type == Settings::Type::Toggle) {
        const int32_t x = right - 52;
        const int32_t y = r.y + 15;

        // Fancy switch. Same old boolean underneath.
        Image::rectangle(
            x,
            y,
            48,
            25,
            item.value ? 0x7A6BCD : 0x495671
        );

        // The little white rectangle that makes
        // this feel like an actual operating system.
        Image::rectangle(
            x + (item.value ? 27 : 3),
            y + 3,
            18,
            19,
            0xFFFFFF
        );

        return;
    }

    char number[24]{};

    const char* display =
        item.definition.type == Settings::Type::Choice
            ? Settings::choice_label(item)
            : nullptr;

    if (!display) {
        number_text(item.value, number);
        display = number;
    }

    // The mysterious control box where settings live
    // until the user starts pressing buttons.
    Image::rectangle(
        right - 136,
        r.y + 11,
        136,
        34,
        0x17263E
    );

    Image::text(
        right - 127,
        r.y + 24,
        "-",
        0xBEACFF
    );

    Image::text(
        right - 23,
        r.y + 24,
        "+",
        0xBEACFF
    );

    // Not mathematically centered, but neither
    // is my emotional relationship with UI alignment.
    Image::text(
        right - 105,
        r.y + 24,
        display,
        0xEDF2FF
    );
}

bool click(
    uint32_t settingId,
    const Windows::Rect& r,
    int32_t mx,
    int32_t my
) {
    // Windows::contains already exists.
    // We don't need its evil twin living in this file.
    if (!Windows::contains(r, mx, my))
        return false;

    const Settings::Item* item =
        Settings::find(settingId);

    if (!item)
        return false;

    if (item->definition.type == Settings::Type::Toggle) {
        // Flip the switch. Summon the consequences.
        return Settings::toggle(settingId);
    }

    const int32_t right = r.x + r.width - 12;

    const Windows::Rect minus{
        right - 136,
        r.y + 11,
        28,
        34
    };

    const Windows::Rect plus{
        right - 31,
        r.y + 11,
        31,
        34
    };

    if (Windows::contains(minus, mx, my)) {
        // Number goes down. Civilization continues.
        return Settings::increment(settingId, -1);
    }

    if (Windows::contains(plus, mx, my)) {
        // Number goes up. CPU remains unimpressed.
        return Settings::increment(settingId, 1);
    }

    // Clicking the void does absolutely nothing.
    // A rare moment of peace in ts kernels development.
    return false;
}

} // namespace Astral::Desktop::Widgets::SettingControl
