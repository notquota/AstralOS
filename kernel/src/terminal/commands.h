
#pragma once

namespace Astral::Commands {

// AstralOS-specific command family.
void execute(
    const char* command,
    bool privileged
);

// Native shell command family.
void execute_native(
    const char* command,
    bool privileged
);

bool is_native(const char* command);

void show_help();

}
