
#include "shell.h"
#include "terminal.h"
#include "commands.h"


#include "../config/wizard.h"

#include "../graphics/console.h"
#include "../graphics/theme.h"

#include "../drivers/keyboard.h"
#include "../security/account.h"
#include "../filesystem/vfs.h"

#include <stdint.h>

namespace Astral::Shell {

// ================================================
// Console utilities
// ================================================

static void print(
    const char* text,
    uint32_t color = Theme::TEXT
) {
    Console::write(text, color);
}

static void println(
    const char* text,
    uint32_t color = Theme::TEXT
) {
    print(text, color);
    Console::newline();
}

// ================================================
// String utilities
// ================================================

static bool equals(
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

static bool starts(
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

static const char* skip_spaces(const char* text) {
    while (*text == ' ')
        ++text;

    return text;
}

// ================================================
// Keyboard input
// ================================================

static void read_line(
    char* buffer,
    uint32_t capacity,
    bool hidden = false
) {
    if (!buffer || capacity == 0)
        return;

    uint32_t count = 0;
    buffer[0] = '\0';

    while (true) {
        char c = Keyboard::wait_char();

        if (c == '\r')
            c = '\n';

        if (c == '\n') {
            buffer[count] = '\0';
            Console::newline();
            return;
        }

        if (c == '\b') {
            if (count > 0) {
                --count;
                buffer[count] = '\0';

                if (!hidden)
                    Console::backspace();
            }

            continue;
        }

        if (c < 32 || c > 126)
            continue;

        if (count + 1 >= capacity)
            continue;

        buffer[count++] = c;
        buffer[count] = '\0';

        if (!hidden) {
            char output[2] = {c, '\0'};
            print(output);
        }
    }
}

// ================================================
// Password handling
// ================================================

static void erase_secret(
    char* buffer,
    uint32_t capacity
) {
    volatile char* data = buffer;

    for (uint32_t i = 0; i < capacity; ++i)
        data[i] = '\0';
}

static bool authenticate() {
    char password[64]{};

    print("Root password: ", Theme::WARNING);

    read_line(
        password,
        sizeof(password),
        true
    );

    bool valid =
        Security::authenticate_root(password);

    erase_secret(password, sizeof(password));

    if (!valid) {
        println(
            "Authentication failed",
            Theme::ERROR
        );
    }

    return valid;
}

// ================================================
// First-run account setup
// ================================================

static void account_setup() {
    Console::initialize();

    println(
        "WELCOME TO ASTRALOS",
        Theme::DOWNLOAD
    );

    Console::newline();

    if (!Security::root_exists()) {
        println(
            "CREATE ROOT ACCOUNT",
            Theme::DOWNLOAD
        );

        println(
            "Development account: RAM only"
        );

        println(
            "Use a temporary password (8+ characters)"
        );

        Console::newline();

        char owner[32]{};
        char password[64]{};
        char confirmation[64]{};

        while (true) {
            print("Owner: ", Theme::DOWNLOAD);
            read_line(owner, sizeof(owner));

            print("Password: ", Theme::WARNING);
            read_line(
                password,
                sizeof(password),
                true
            );

            print("Confirm: ", Theme::WARNING);
            read_line(
                confirmation,
                sizeof(confirmation),
                true
            );

            const bool matches =
                equals(password, confirmation);

            const bool created =
                matches &&
                Security::create_root(
                    owner,
                    password
                );

            erase_secret(password, sizeof(password));
            erase_secret(
                confirmation,
                sizeof(confirmation)
            );

            if (created) {
                println(
                    "Root account created",
                    Theme::SUCCESS
                );
                break;
            }

            println(
                "Invalid details or passwords do not match",
                Theme::ERROR
            );
        }
    }

    Console::newline();
    println("ROOT LOGIN", Theme::DOWNLOAD);

    while (!authenticate()) {
        println("Please try again", Theme::WARNING);
    }

    Terminal::set_mode(Terminal::Mode::HPT);
}

// ================================================
// Terminal display
// ================================================

static void heading() {
    Console::initialize();

    println(
        "AstralOS v0.0.5-alpha",
        Theme::TEXT
    );

    if (Terminal::is_privileged()) {
        println(
            "HPT Terminal",
            Theme::DOWNLOAD
        );

        println(
            "High Permissions Terminal",
            Theme::TEXT
        );
    } else {
        println(
            "User Terminal",
            Theme::DOWNLOAD
        );

        println(
            "Basic Permissions Terminal",
            Theme::TEXT
        );
    }

    Console::newline();

    println(
        "Type help or ast help for commands."
    );

    Console::newline();
}

// ================================================
// Terminal mode switching
// ================================================

static void switch_user() {
    if (VFS::initialized()) {
        VFS::change_directory(
            "/home/user",
            true
        );
    }

    Terminal::set_mode(Terminal::Mode::User);

    heading();

    println(
        "Basic permissions enabled",
        Theme::SUCCESS
    );
}

static void switch_hpt() {
    if (Terminal::is_privileged()) {
        println(
            "Already in HPT Terminal",
            Theme::SUCCESS
        );
        return;
    }

    if (!authenticate())
        return;

    Terminal::set_mode(Terminal::Mode::HPT);

    if (VFS::initialized()) {
        VFS::change_directory(
            "/root",
            true
        );
    }

    heading();

    println(
        "HPT access granted",
        Theme::SUCCESS
    );
}

// ================================================
// Command processing
// ================================================

static void process(const char* input) {
    if (!input)
        return;

    input = skip_spaces(input);

    if (!*input)
        return;

    bool sudoRequested = false;
    bool astralCommand = false;

    // Optional sudo prefix.
    if (starts(input, "sudo ")) {
        sudoRequested = true;
        input = skip_spaces(input + 5);
    }

    // Check for AstralOS command prefix.
    if (equals(input, "ast")) {
        Commands::show_help();
        return;
    }

    if (starts(input, "ast ")) {
        astralCommand = true;
        input = skip_spaces(input + 4);
    }

    if (!*input)
        return;

    // --------------------------------------------
    // Terminal mode transitions
    // --------------------------------------------

    if (astralCommand && equals(input, "user")) {
        switch_user();
        return;
    }

    if (astralCommand && equals(input, "hpt")) {
        switch_hpt();
        return;
    }

    // --------------------------------------------
    // Graphical configuration wizard
    // --------------------------------------------

    if (equals(input, "config")) {
        // Configuration is currently user-accessible.
        // Future system-wide actions can require HPT.
        Astral::ConfigWizard::open();
        return;
    }

    // --------------------------------------------
    // Native command validation
    // --------------------------------------------

    if (!astralCommand &&
        !Commands::is_native(input)) {
        println(
            "Unknown command. Type help.",
            Theme::ERROR
        );
        return;
    }

    // --------------------------------------------
    // Privilege handling
    // --------------------------------------------

    bool privileged =
        Terminal::is_privileged();

    // sudo elevates one command after authentication.
    if (sudoRequested && !privileged) {
        if (!authenticate())
            return;

        privileged = true;
    }

    // --------------------------------------------
    // Command dispatch
    // --------------------------------------------

    if (astralCommand) {
        Commands::execute(
            input,
            privileged
        );
    } else {
        Commands::execute_native(
            input,
            privileged
        );
    }
}

// ================================================
// Dynamic filesystem prompt
// ================================================

static void prompt() {
    char path[VFS::MAX_PATH]{};

    if (VFS::initialized()) {
        VFS::working_directory(
            path,
            sizeof(path)
        );
    } else {
        path[0] = '/';
        path[1] = '\0';
    }

    // Username / privilege mode.
    if (Terminal::is_privileged()) {
        print("root", Theme::DOWNLOAD);
    } else {
        print(
            Security::device_owner(),
            Theme::DOWNLOAD
        );
    }

    print("@astral", Theme::DOWNLOAD);

    // Display the final component of the path.
    const char* folder = path;

    for (const char* p = path; *p; ++p) {
        if (*p == '/' && *(p + 1) != '\0')
            folder = p + 1;
    }

    print("/", Theme::TEXT);

    if (!(path[0] == '/' && path[1] == '\0'))
        print(folder, Theme::TEXT);

    // Root mode uses #; user mode uses $.
    if (Terminal::is_privileged()) {
        print(" # ", Theme::SUCCESS);
    } else {
        print(" $ ", Theme::SUCCESS);
    }
}

// ================================================
// Main shell
// ================================================

[[noreturn]]
void run() {
    account_setup();

    // Start the HPT session in /root.
    if (VFS::initialized()) {
        VFS::change_directory(
            "/root",
            true
        );
    }

    heading();

    char input[128]{};

    while (true) {
        prompt();

        read_line(
            input,
            sizeof(input)
        );

        if (input[0] != '\0') {
            process(input);
        }
    }
}

}
