
#include "terminal_bridge.h"

#include "commands.h"
#include "preferences.h"
#include "desktop.h"

#include "../graphics/console.h"
#include "../graphics/theme.h"

namespace Astral::Desktop {

static bool equal(
    const char* left,
    const char* right
) {
    if (!left || !right)
        return false;

    while (*left && *right) {
        if (*left != *right)
            return false;

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static bool starts_with(
    const char* text,
    const char* prefix
) {
    if (!text || !prefix)
        return false;

    while (*prefix) {
        if (*text != *prefix)
            return false;

        ++text;
        ++prefix;
    }

    return true;
}

static const char* skip_spaces(
    const char* text
) {
    while (*text == ' ' || *text == '\t')
        ++text;

    return text;
}

static void output(
    const char* message,
    bool success
) {
    Console::write(
        message,
        success ? Theme::SUCCESS : Theme::ERROR
    );

    Console::write("\n", Theme::TEXT);
}

static void show_status() {
    const Preferences& prefs = preferences();

    Console::write(
        "\nAstralOS Desktop\n",
        Theme::SUCCESS
    );

    Console::write(
        prefs.enabled
            ? "  Desktop: Enabled\n"
            : "  Desktop: Disabled\n",
        Theme::TEXT
    );

    Console::write(
        prefs.bootMode == BootMode::Desktop
            ? "  Boot interface: Desktop\n"
            : "  Boot interface: Terminal\n",
        Theme::TEXT
    );

    Console::write(
        "  Version: 0.0.7-alpha\n\n",
        Theme::TEXT
    );
}

TerminalCommandResult handle_terminal_command(
    const char* commandLine,
    bool administratorAuthorized
) {
    if (!commandLine)
        return {false, false, false};

    const char* command = skip_spaces(commandLine);

    // Accept the normal command and a sudo-prefixed
    // command, but do NOT grant privileges based
    // on text supplied by the user.
    if (starts_with(command, "sudo ")) {
        command = skip_spaces(command + 5);
    }

    static constexpr const char* prefix =
        "ast desktop";

    if (!starts_with(command, prefix))
        return {false, false, false};

    command += 11;

    // Reject commands like "ast desktopxyz".
    if (*command != '\0' &&
        *command != ' ' &&
        *command != '\t')
        return {false, false, false};

    command = skip_spaces(command);

    if (*command == '\0') {
        output(
            "Usage: ast desktop "
            "enable|disable|show|status",
            false
        );
        return {true, false, false};
    }

    if (equal(command, "status")) {
        show_status();
        return {true, true, false};
    }

    CommandAction action = execute_command(
        command,
        administratorAuthorized
    );

    if (action.result != CommandResult::Success) {
        output(
            command_message(action.result),
            false
        );

        return {true, false, false};
    }

    if (equal(command, "enable")) {
        output("Desktop enabled.", true);
    } else if (equal(command, "disable")) {
        output("Desktop disabled.", true);
    } else if (equal(command, "show boot")) {
        output(
            "Desktop boot preference saved.",
            true
        );
    } else if (equal(command, "show terminal")) {
        output(
            "Terminal boot preference saved.",
            true
        );
    }

    if (action.launchDesktop) {
        output(
            "Starting AstralOS Desktop...",
            true
        );

        Desktop::run();

        // Restore the existing console display
        // after leaving the graphical session.
        Console::scroll_to_bottom();

        output(
            "Returned to AstralOS Terminal.",
            true
        );

        return {true, true, true};
    }

    return {true, true, false};
}

}