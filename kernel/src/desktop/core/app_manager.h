
#pragma once

#include <stdint.h>

#include "../windows/window.h"
#include "../core/events.h"
#include "../input/input_manager.h"

namespace Astral::Desktop::AppManager {

constexpr uint32_t MAX_INSTANCES = 32;

struct Instance {
    uint32_t appId;
    Windows::WindowId windowId;
    bool active;
};

void initialize();

// Creates a graphical window for an available app.
// An already-running singleton app is focused.
Windows::WindowId launch(uint32_t appId);

bool close(Windows::WindowId windowId);
void reconcile();

// Forward input to the window's application.
bool dispatch(
    const InputManager::Action& action,
    const Events::Event& event
);

uint32_t running_count();

const Instance* at(uint32_t index);

}
