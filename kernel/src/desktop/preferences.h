
#pragma once

#include <stdint.h>

namespace Astral::Desktop {

enum class BootMode : uint8_t {
    Terminal = 0,
    Desktop = 1
};

struct Preferences {
    bool enabled;
    BootMode bootMode;
};

using SavePreferences = bool (*)(
    const Preferences& preferences
);

void initialize_preferences(
    const Preferences& stored
);

void set_save_callback(SavePreferences callback);

const Preferences& preferences();

bool enable();
bool disable();

bool set_boot_mode(BootMode mode);

bool can_launch();

}
