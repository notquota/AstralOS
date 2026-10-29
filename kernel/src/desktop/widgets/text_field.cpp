
#include "text_field.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Widgets::TextField {

void initialize(
    State& state,
    const Windows::Rect& bounds,
    bool password,
    uint32_t maxLength
) {
    state = {};
    state.bounds = bounds;
    state.password = password;
    state.maxLength =
        maxLength < CAPACITY ? maxLength : CAPACITY - 1;
    state.enabled = true;
}

void clear(State& state) {
    volatile char* p = state.content;

    for (uint32_t i = 0; i < CAPACITY; ++i)
        p[i] = '\0';

    state.length = 0;
    state.cursor = 0;
    state.changed = true;
}

bool set_text(State& state, const char* value) {
    if (!value)
        return false;

    uint32_t length = 0;

    while (value[length] &&
           length <= state.maxLength &&
           length < CAPACITY)
        ++length;

    if (length > state.maxLength ||
        length >= CAPACITY)
        return false;

    clear(state);

    for (uint32_t i = 0; i < length; ++i)
        state.content[i] = value[i];

    state.length = length;
    state.cursor = length;
    return true;
}

bool click(State& state, int32_t x, int32_t y) {
    const bool next =
        state.enabled &&
        Windows::contains(state.bounds, x, y);

    const bool changed = state.focused != next;
    state.focused = next;
    return changed;
}

bool key(State& state, char value) {
    if (!state.enabled || !state.focused)
        return false;

    if (value == '\b' || value == 127) {
        if (state.cursor == 0)
            return false;

        for (uint32_t i = state.cursor - 1;
             i < state.length;
             ++i) {
            state.content[i] = state.content[i + 1];
        }

        --state.length;
        --state.cursor;
        state.changed = true;
        return true;
    }

    if (value < 32 || value > 126)
        return false;

    if (state.length >= state.maxLength)
        return false;

    for (uint32_t i = state.length;
         i > state.cursor;
         --i) {
        state.content[i] = state.content[i - 1];
    }

    state.content[state.cursor] = value;
    ++state.length;
    ++state.cursor;

    state.content[state.length] = '\0';
    state.changed = true;

    return true;
}

void render(const State& state) {
    const auto& r = state.bounds;

    if (r.width <= 0 || r.height <= 0)
        return;

    Image::rectangle(
        r.x, r.y, r.width, r.height,
        state.focused ? 0xA28BEF : 0x3E4D68
    );

    Image::rectangle(
        r.x + 1, r.y + 1,
        r.width - 2, r.height - 2,
        0x17253E
    );

    const int32_t availableChars =
        (r.width - 24) / 6;

    if (availableChars <= 0)
        return;

    const uint32_t visible =
        static_cast<uint32_t>(availableChars);

    const uint32_t start =
        state.cursor >= visible
            ? state.cursor - visible + 1
            : 0;

    char display[CAPACITY]{};
    uint32_t count = 0;

    for (uint32_t i = start;
         i < state.length && count < visible;
         ++i) {
        display[count++] =
            state.password ? '*' : state.content[i];
    }

    display[count] = '\0';

    Image::text(
        r.x + 12,
        r.y + (r.height - 7) / 2,
        display,
        0xF1F4FF
    );

    if (state.focused) {
        uint32_t position =
            state.cursor > start
                ? state.cursor - start : 0;

        if (position > visible)
            position = visible;

        Image::rectangle(
            r.x + 12 +
                static_cast<int32_t>(position * 6),
            r.y + 7,
            1,
            r.height - 14,
            0xC6B8FF
        );
    }
}

const char* text(const State& state) {
    return state.content;
}

}
