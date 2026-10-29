
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Events {

enum class Type : uint8_t {
    None,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    KeyDown,
    KeyUp,
    LaunchApp,
    CloseWindow,
    FocusWindow,
    LockScreen
};

struct Event {
    Type type;

    int32_t x;
    int32_t y;
    int32_t value;

    uint32_t target;
    uint32_t modifiers;
};

void initialize();
bool push(const Event& event);
bool pop(Event& event);

uint32_t pending();
uint32_t dropped();

}
