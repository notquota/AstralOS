
#include "session.h"

namespace Astral::Desktop::Session {

static State current{
    false,
    false,
    Permission::None,
    0
};

void initialize() {
    current = {
        false,
        false,
        Permission::None,
        current.revision + 1
    };
}

bool begin_authenticated_user() {
    if (current.active)
        return false;

    // This function must only be called by the
    // authenticated OS session launcher.
    // It does not authenticate credentials.
    current.active = true;
    current.authenticated = true;
    current.permission = Permission::User;
    ++current.revision;

    return true;
}

void end() {
    current.active = false;
    current.authenticated = false;
    current.permission = Permission::None;
    ++current.revision;
}

bool active() {
    return current.active;
}

bool authenticated() {
    return current.authenticated;
}

bool user_access_allowed() {
    return current.active &&
           current.authenticated &&
           current.permission == Permission::User;
}

const State& state() {
    return current;
}

}
