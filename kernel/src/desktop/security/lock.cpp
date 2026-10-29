
#include "lock.h"

#include "../../security/account.h"

namespace Astral::Desktop::Lock {

static Settings currentSettings{
    true,
    true,
    true,
    300
};

static State currentState{
    Scope::None,
    false,
    0,
    0
};

void initialize() {
    currentSettings = {
        true,
        true,
        true,
        300
    };

    currentState = {
        Scope::None,
        false,
        0,
        currentState.revision + 1
    };
}

const Settings& settings() {
    return currentSettings;
}

const State& state() {
    return currentState;
}

void set_settings(const Settings& value) {
    currentSettings = value;

    // Keep password protection mandatory until
    // we implement alternative authentication.
    currentSettings.requirePasswordOnUnlock = true;

    if (currentSettings.inactivitySeconds > 86400)
        currentSettings.inactivitySeconds = 86400;
}

bool lock_desktop() {
    if (!currentSettings.desktopLockEnabled)
        return false;

    if (currentState.locked)
        return false;

    currentState.scope = Scope::Desktop;
    currentState.locked = true;
    currentState.failedAttempts = 0;
    ++currentState.revision;

    return true;
}

bool lock_main() {
    if (!currentSettings.mainLockEnabled)
        return false;

    currentState.scope = Scope::Main;
    currentState.locked = true;
    currentState.failedAttempts = 0;
    ++currentState.revision;

    return true;
}

bool locked() {
    return currentState.locked;
}

bool desktop_locked() {
    return locked() &&
           currentState.scope == Scope::Desktop;
}

bool main_locked() {
    return locked() &&
           currentState.scope == Scope::Main;
}

bool unlock(const char* password) {
    if (!currentState.locked)
        return true;

    if (!password)
        return false;

    // Temp: authenticate with
    // the existing root-account password.
    //
    // A normal users own credentials should
    // replace this once i add per-user accounts.
    if (!Security::authenticate_root(password)) {
        if (currentState.failedAttempts != UINT32_MAX)
            ++currentState.failedAttempts;

        return false;
    }

    currentState.locked = false;
    currentState.scope = Scope::None;
    currentState.failedAttempts = 0;
    ++currentState.revision;

    return true;
}

}
