
#include "toggle.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Widgets::Toggle {

void initialize(
    State& state,
    const Windows::Rect& bounds,
    const char* label,
    bool initialValue
) {
    state = {};
    state.bounds = bounds;
    state.label = label;
    state.value = initialValue;
    state.enabled = true;
}

void render(const State& state) {
    const auto& r = state.bounds;

    if (r.width <= 0 || r.height <= 0)
        return;

    Image::rectangle(
        r.x, r.y, r.width, r.height,
        state.hovered ? 0x293C5C : 0x20314E
    );

    if (state.label) {
        Image::text(
            r.x + 12,
            r.y + (r.height - 7) / 2,
            state.label,
            state.enabled ? 0xEDF2FF : 0x8292AD
        );
    }

    const int32_t switchX = r.x + r.width - 60;
    const int32_t switchY = r.y + (r.height - 24) / 2;

    Image::rectangle(
        switchX, switchY, 48, 24,
        !state.enabled
            ? 0x424A5A
            : state.value
                ? 0x806CE0
                : 0x475671
    );

    Image::rectangle(
        switchX + (state.value ? 27 : 3),
        switchY + 3,
        18, 18,
        state.enabled ? 0xFFFFFF : 0xA2A8B4
    );
}

bool pointer_move(State& state, int32_t x, int32_t y) {
    const bool hovered =
        state.enabled && Windows::contains(
            state.bounds, x, y
        );

    const bool changed = state.hovered != hovered;
    state.hovered = hovered;

    return changed;
}

bool click(State& state, int32_t x, int32_t y) {
    if (!state.enabled ||
        !Windows::contains(state.bounds, x, y))
        return false;

    state.value = !state.value;
    return true;
}

}
