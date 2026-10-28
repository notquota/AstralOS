
#include "terminal.h"
#include "shell.h"

namespace Astral::Terminal {

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
    Shell::run();
}

}
