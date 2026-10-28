
#include "commands.h"
#include "preferences.h"

namespace Astral::Desktop {

static bool equal(
    const char* a,
    const char* b
) {
    if (!a || !b)
        return false;

    while (*a && *b) {
        if (*a != *b)
            return false;

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static CommandAction result(
    CommandResult code,
    bool launch = false
) {
    return {code, launch};
}

CommandAction execute_command(
    const char* command,
    bool administrator
) {
    if (!command)
        return result(CommandResult::InvalidCommand);

    if (equal(command, "enable")) {
        enable();
        return result(CommandResult::Success);
    }

    if (equal(command, "disable")) {
        disable();
        return result(CommandResult::Success);
    }

    if (equal(command, "status"))
        return result(CommandResult::Success);

    if (equal(command, "show")) {
        if (!can_launch())
            return result(CommandResult::DesktopDisabled);

        return result(CommandResult::Success, true);
    }

    if (equal(command, "show boot")) {
        if (!administrator)
            return result(CommandResult::PermissionDenied);

        if (!can_launch())
            return result(CommandResult::DesktopDisabled);

        if (!set_boot_mode(BootMode::Desktop))
            return result(CommandResult::StorageUnavailable);

        return result(CommandResult::Success);
    }

    if (equal(command, "show terminal")) {
        if (!administrator)
            return result(CommandResult::PermissionDenied);

        if (!set_boot_mode(BootMode::Terminal))
            return result(CommandResult::StorageUnavailable);

        return result(CommandResult::Success);
    }

    return result(CommandResult::InvalidCommand);
}

const char* command_message(CommandResult code) {
    switch (code) {
        case CommandResult::Success:
            return "Desktop command completed.";

        case CommandResult::PermissionDenied:
            return "Administrator permission required.";

        case CommandResult::DesktopDisabled:
            return "Desktop disabled. Run ast desktop enable.";

        case CommandResult::StorageUnavailable:
            return "Persistent boot settings unavailable.";

        case CommandResult::InvalidCommand:
            return "Invalid desktop command.";

        case CommandResult::NotDesktopCommand:
            return "Not a desktop command.";
    }

    return "Unknown desktop error.";
}

}
