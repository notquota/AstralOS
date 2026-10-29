
#include "terminal.h"
#include "shell.h"

#include "../desktop/core/session_controller.h"

namespace Astral::Terminal {

// Everyone starts in User mode.
// HPT is earned, not handed out like free candy. (halloween reference btw)
static Mode activeMode = Mode::User;

void initialize() {
    activeMode = Mode::User;
}

Mode current_mode() {
    return activeMode;
}

void set_mode(Mode mode) {
    activeMode = mode;
}

bool is_privileged() {
    return activeMode == Mode::HPT;
}

[[noreturn]] void run() {
    initialize();

    // The session controller was initialized
    // during kernel startup in main.cpp.
    //
    // Do NOT initialize it again here.
    // Resetting sessions after login is how
    // you accidentally become your own security bug.

    // Shell::run() handles the actual terminal,
    // including account setup and login.
    Shell::run();
}

} // namespace Astral::Terminal
