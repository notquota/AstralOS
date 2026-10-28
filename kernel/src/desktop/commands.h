
#pragma once

namespace Astral::Desktop {

enum class CommandResult {
    NotDesktopCommand,
    Success,
    PermissionDenied,
    DesktopDisabled,
    StorageUnavailable,
    InvalidCommand
};

struct CommandAction {
    CommandResult result;
    bool launchDesktop;
};

// command is the portion after "ast desktop".
// Examples: "enable", "show", "show boot".
//
// administrator must come from the verified
// terminal/account permission system.
CommandAction execute_command(
    const char* command,
    bool administrator
);

const char* command_message(
    CommandResult result
);

}
