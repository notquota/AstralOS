
#pragma once
#include <stdint.h>

namespace Astral::Desktop::Lock {

enum class Scope : uint8_t {
    None,
    Desktop,
    Main
};

struct Settings {
    bool desktopLockEnabled;
    bool mainLockEnabled;
    bool requirePasswordOnUnlock;
    uint32_t inactivitySeconds;
};

struct State {
    Scope scope;
    bool locked;
    uint32_t failedAttempts;
    uint64_t revision;
};

void initialize();

const Settings& settings();
const State& state();

void set_settings(const Settings& settings);

bool lock_desktop();
bool lock_main();

bool locked();
bool desktop_locked();
bool main_locked();

// Attempts to unlock using the actual account
// authentication implementation.
bool unlock(const char* password);

}
