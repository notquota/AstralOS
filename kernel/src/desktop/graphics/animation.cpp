
#include "animation.h"

namespace Astral::Desktop::Animation {

namespace {

Item items[MAX_ANIMATIONS]{};
AnimationId nextId = 1;

uint32_t ease(
    Easing easing,
    uint32_t progress
) {
    // Fixed-point progress: 0 ... 1000.
    const uint64_t p = progress;

    switch (easing) {
        case Easing::Linear:
            return progress;

        case Easing::EaseIn:
            return static_cast<uint32_t>(
                p * p / 1000
            );

        case Easing::EaseOut:
            return static_cast<uint32_t>(
                2000 * p / 1000 -
                p * p / 1000
            );

        case Easing::EaseInOut:
            if (progress < 500)
                return static_cast<uint32_t>(
                    2 * p * p / 1000
                );

            return static_cast<uint32_t>(
                1000 -
                2 * (1000 - p) * (1000 - p) / 1000
            );
    }

    return progress;
}

}

void initialize() {
    for (auto& item : items)
        item = {};

    nextId = 1;
}

AnimationId start(
    uint32_t target,
    int32_t from,
    int32_t to,
    uint64_t nowMilliseconds,
    uint32_t durationMilliseconds,
    Easing easing
) {
    if (nextId == 0)
        return 0;

    for (auto& item : items) {
        if (item.active)
            continue;

        item = {};
        item.id = nextId++;
        item.target = target;
        item.from = from;
        item.to = to;
        item.value = from;
        item.startMilliseconds = nowMilliseconds;
        item.durationMilliseconds = durationMilliseconds;
        item.easing = easing;
        item.active = true;

        if (durationMilliseconds == 0) {
            item.value = to;
            item.active = false;
        }

        return item.id;
    }

    return 0;
}

void update(uint64_t nowMilliseconds) {
    for (auto& item : items) {
        if (!item.active)
            continue;

        const uint64_t elapsed =
            nowMilliseconds >= item.startMilliseconds
                ? nowMilliseconds - item.startMilliseconds
                : 0;

        if (elapsed >= item.durationMilliseconds) {
            item.value = item.to;
            item.active = false;
            continue;
        }

        const uint32_t progress =
            static_cast<uint32_t>(
                elapsed * 1000 /
                item.durationMilliseconds
            );

        const uint32_t smoothed =
            ease(item.easing, progress);

        const int64_t difference =
            static_cast<int64_t>(item.to) - item.from;

        item.value = static_cast<int32_t>(
            static_cast<int64_t>(item.from) +
            difference * smoothed / 1000
        );
    }
}

bool cancel(AnimationId id) {
    for (auto& item : items) {
        if (item.id == id) {
            item.active = false;
            return true;
        }
    }

    return false;
}

void cancel_all() {
    for (auto& item : items)
        item.active = false;
}

const Item* find(AnimationId id) {
    for (const auto& item : items)
        if (item.id == id)
            return &item;

    return nullptr;
}

bool running() {
    for (const auto& item : items)
        if (item.active)
            return true;

    return false;
}

uint32_t count() {
    uint32_t total = 0;

    for (const auto& item : items)
        if (item.active)
            ++total;

    return total;
}

}
