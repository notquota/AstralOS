
#pragma once

#include <stdint.h>
#include "events.h"

namespace Astral::Desktop::DesktopManager {

enum class Status : uint8_t {
    Stopped,
    Running,
    Locked
};

struct Configuration {
    uint32_t screenWidth;
    uint32_t screenHeight;
};

bool initialize(const Configuration& configuration);

// Requires an authenticated, unprivileged
// Desktop session to have been established.
bool start();

void stop();

Status status();

// Process one translated input event.
void dispatch(const Events::Event& event);

// Process all pending events from Events::push().
void pump();

// Update graphical output when needed.
void render();

// Requests a full redraw next time render() runs.
void invalidate();

bool launch_app(uint32_t appId);

}
