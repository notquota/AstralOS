
#pragma once

namespace Astral::Terminal {

enum class Mode {
    User,
    HPT
};

void initialize();

Mode current_mode();

void set_mode(Mode mode);

bool is_privileged();

[[noreturn]] void run();

}
