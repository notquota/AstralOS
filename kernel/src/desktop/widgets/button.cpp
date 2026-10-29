
#include "button.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Widgets {

bool contains(
    const Button& button,
    int32_t x,
    int32_t y
) {
    if (button.width <= 0 || button.height <= 0)
        return false;

    return x >= button.x &&
           y >= button.y &&
           static_cast<int64_t>(x) <
               static_cast<int64_t>(button.x) +
               button.width &&
           static_cast<int64_t>(y) <
               static_cast<int64_t>(button.y) +
               button.height;
}

void update_hover(
    Button& button,
    int32_t x,
    int32_t y
) {
    button.hovered =
        button.enabled && contains(button, x, y);
}

bool mouse_down(
    Button& button,
    int32_t x,
    int32_t y
) {
    button.pressed =
        button.enabled && contains(button, x, y);

    return button.pressed;
}

bool mouse_up(
    Button& button,
    int32_t x,
    int32_t y
) {
    const bool clicked =
        button.pressed &&
        button.enabled &&
        contains(button, x, y);

    button.pressed = false;
    return clicked;
}

void render(const Button& button) {
    uint32_t color = 0x293C60;

    if (!button.enabled)
        color = 0x19263C;
    else if (button.pressed)
        color = 0x435D8A;
    else if (button.hovered)
        color = 0x354B73;

    Image::rectangle(
        button.x,
        button.y,
        button.width,
        button.height,
        color
    );

    if (!button.label)
        return;

    // Current bitmap font: six pixels per glyph.
    uint32_t length = 0;

    while (button.label[length] && length < 80)
        ++length;

    const int32_t textWidth =
        static_cast<int32_t>(length * 6);

    const int32_t textX =
        button.x + (button.width - textWidth) / 2;

    const int32_t textY =
        button.y + (button.height - 7) / 2;

    Image::text(
        textX,
        textY,
        button.label,
        button.enabled ? 0xEDF1FF : 0x8190AB
    );
}

}
