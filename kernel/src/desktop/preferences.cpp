
#include "preferences.h"

namespace Astral::Desktop {

static Preferences current = {
    false,
    BootMode::Terminal
};

static SavePreferences saveCallback = nullptr;

void initialize_preferences(
    const Preferences& stored
) {
    current = stored;

    if (current.bootMode != BootMode::Terminal &&
        current.bootMode != BootMode::Desktop)
        current.bootMode = BootMode::Terminal;

    if (!current.enabled)
        current.bootMode = BootMode::Terminal;
}

void set_save_callback(SavePreferences callback) {
    saveCallback = callback;
}

const Preferences& preferences() {
    return current;
}

bool enable() {
    // Enabling is immediately effective
    // for the current boot session.
    current.enabled = true;
    return true;
}

bool disable() {
    current.enabled = false;
    current.bootMode = BootMode::Terminal;
    return true;
}

bool set_boot_mode(BootMode mode) {
    if (mode != BootMode::Terminal &&
        mode != BootMode::Desktop)
        return false;

    if (mode == BootMode::Desktop &&
        !current.enabled)
        return false;

    // Never claim a boot setting is saved if
    // there is no persistent storage backend.
    if (!saveCallback)
        return false;

    Preferences proposed = current;
    proposed.bootMode = mode;

    if (!saveCallback(proposed))
        return false;

    current = proposed;
    return true;
}

bool can_launch() {
    return current.enabled;
}

}
