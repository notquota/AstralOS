
#include "slider.h"
#include "../graphics/image.h"

namespace Astral::Desktop::Widgets::Slider {

namespace {

int32_t clamp(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int32_t quantize(const State& state, int32_t value) {
    value = clamp(
        value,
        state.minimum,
        state.maximum
    );

    const int32_t step = state.step > 0
        ? state.step : 1;

    return state.minimum +
        ((value - state.minimum) / step) * step;
}

bool set_from_x(State& state, int32_t mouseX) {
    const int32_t trackX = state.bounds.x + 16;
    const int32_t trackW = state.bounds.width - 32;

    if (trackW <= 0)
        return false;

    const int64_t relative = clamp(
        mouseX - trackX, 0, trackW
    );

    const int64_t range =
        static_cast<int64_t>(state.maximum) -
        state.minimum;

    const int32_t value = static_cast<int32_t>(
        state.minimum + relative * range / trackW
    );

    return set(state, value);
}

}

bool initialize(
    State& state,
    const Windows::Rect& bounds,
    const char* label,
    int32_t minimum,
    int32_t maximum,
    int32_t step,
    int32_t value
) {
    if (minimum >= maximum || step <= 0)
        return false;

    state = {};
    state.bounds = bounds;
    state.label = label;
    state.minimum = minimum;
    state.maximum = maximum;
    state.step = step;
    state.enabled = true;
    state.value = quantize(state, value);

    return true;
}

bool set(State& state, int32_t value) {
    const int32_t next = quantize(state, value);

    if (next == state.value)
        return false;

    state.value = next;
    return true;
}

void render(const State& state) {
    const auto& r = state.bounds;

    if (r.width < 40 || r.height < 32)
        return;

    Image::rectangle(
        r.x, r.y, r.width, r.height,
        0x20314E
    );

    if (state.label) {
        Image::text(
            r.x + 14, r.y + 12,
            state.label, 0xEDF2FF
        );
    }

    const int32_t trackX = r.x + 16;
    const int32_t trackW = r.width - 32;
    const int32_t trackY = r.y + r.height - 18;

    Image::rectangle(
        trackX, trackY,
        trackW, 5,
        0x445371
    );

    const int64_t range =
        static_cast<int64_t>(state.maximum) -
        state.minimum;

    const int32_t fillW = range > 0
        ? static_cast<int32_t>(
            static_cast<int64_t>(
                state.value - state.minimum
            ) * trackW / range
        ) : 0;

    Image::rectangle(
        trackX, trackY,
        fillW, 5,
        state.enabled ? 0xA18AF4 : 0x6C7280
    );

    Image::rectangle(
        trackX + fillW - 4,
        trackY - 5,
        9, 15,
        0xF3F0FF
    );
}

bool mouse_down(State& state, int32_t x, int32_t y) {
    if (!state.enabled ||
        !Windows::contains(state.bounds, x, y))
        return false;

    state.dragging = true;
    return set_from_x(state, x);
}

bool mouse_move(State& state, int32_t x, int32_t y) {
    (void)y;

    if (!state.enabled || !state.dragging)
        return false;

    return set_from_x(state, x);
}

void mouse_up(State& state) {
    state.dragging = false;
}

}
