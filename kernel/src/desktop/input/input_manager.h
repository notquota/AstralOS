
#pragma once

#include <stdint.h>

#include "../core/events.h"
#include "../windows/window.h"
#include "shortcuts.h"

namespace Astral::Desktop::InputManager {

enum class ActionType : uint8_t {
    None,
    Redraw,
    LaunchApp,
    ToggleLauncher,
    LockDesktop,
    ShowDesktop,
    ForwardKey,
    ForwardMouse,
    ForwardScroll
};

struct Action {
    ActionType type = ActionType::None;

    // Application ID for LaunchApp.
    // Window ID for forwarded input.
    uint32_t target = 0;

    int32_t x = 0;
    int32_t y = 0;
    int32_t value = 0;

    uint32_t modifiers = 0;
};

// Called when the graphical session starts.
void initialize();

// Dispatch one event from the Desktop event queue.
// The caller handles application-specific actions.
Action dispatch(const Events::Event& event);

// Explicitly cancel an active mouse interaction,
// such as when the Desktop is locked.
void cancel_interaction();

// Prevent application/window input while locked.
// Only the Desktop security controller should
// change this value.
void set_locked(bool locked);
bool locked();

Windows::WindowId hovered_window();
Windows::WindowId captured_window();

}
