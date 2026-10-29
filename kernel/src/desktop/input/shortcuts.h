
#pragma once

#include <stdint.h>

namespace Astral::Desktop::Shortcuts {

enum Modifier : uint32_t {
    Shift = 1u << 0,
    Ctrl = 1u << 1,
    Alt = 1u << 2,
    Super = 1u << 3
};

enum class Action : uint8_t {
    None,
    ToggleLauncher,
    LockDesktop,
    CloseWindow,
    MinimizeWindow,
    MaximizeWindow,
    SwitchWindow,
    OpenTerminal,
    OpenSettings,
    ShowDesktop
};

struct KeyEvent {
    uint32_t key;
    uint32_t modifiers;
    bool pressed;
};

Action resolve(const KeyEvent& event);

}
