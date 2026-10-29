
#pragma once

#include <stdint.h>

namespace Astral::Desktop::Animation {

constexpr uint32_t MAX_ANIMATIONS = 32;

enum class Easing : uint8_t {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

using AnimationId = uint32_t;

struct Item {
    AnimationId id;
    uint32_t target;

    int32_t from;
    int32_t to;
    int32_t value;

    uint64_t startMilliseconds;
    uint32_t durationMilliseconds;

    Easing easing;
    bool active;
};

void initialize();

AnimationId start(
    uint32_t target,
    int32_t from,
    int32_t to,
    uint64_t nowMilliseconds,
    uint32_t durationMilliseconds,
    Easing easing = Easing::EaseOut
);

void update(uint64_t nowMilliseconds);

bool cancel(AnimationId id);
void cancel_all();

const Item* find(AnimationId id);

bool running();
uint32_t count();

}
