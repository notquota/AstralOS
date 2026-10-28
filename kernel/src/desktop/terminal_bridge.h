
#pragma once

namespace Astral::Desktop {

struct TerminalCommandResult {
    bool handled;
    bool succeeded;
    bool launchedDesktop;
};

// Pass the full command line after the terminal
// has completed authentication.
//
// administratorAuthorized must be determined
// by AstralOS's real account/privilege system.
// Never derive it from the presence of "sudo".
TerminalCommandResult handle_terminal_command(
    const char* commandLine,
    bool administratorAuthorized
);

}
