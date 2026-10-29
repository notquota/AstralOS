
#pragma once

#include <stdint.h>

namespace Astral::Desktop::SessionController {

enum class State : uint8_t {
    SignedOut,
    Active,
    DesktopLocked,
    MainLocked
};

void initialize();

// Trusted authenticated login integration only.
// Does not verify passwords or accept untrusted app requests.
bool start_authenticated(
    uint32_t userId,
    const char* username
);

bool lock_desktop();
bool lock_main();

// Verifies credentials through the existing
// Lock authentication backend.
bool unlock(const char* password);

// Terminates the graphical session.
void sign_out();

State state();
bool can_run_apps();
bool is_locked();

}
