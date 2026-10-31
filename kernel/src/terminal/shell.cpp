#include "shell.h"
#include "terminal.h"
#include "commands.h"
#include "../config/wizard.h"
#include "../graphics/console.h"
#include "../graphics/theme.h"
#include "../drivers/keyboard.h"
#include "../security/account.h"
#include "../filesystem/vfs.h"
#include "../desktop/desktop.h"
#include "../desktop/preferences.h"
#include "../desktop/commands.h"
#include "../desktop/core/session_controller.h"
#include <stdint.h>

namespace Astral::Shell {
namespace {

constexpr const char* VERSION = "0.0.8-alpha";
constexpr uint32_t INPUT_CAPACITY = 256;
constexpr uint32_t HISTORY_CAPACITY = 16;
constexpr int64_t INTEGER_MAX = INT64_MAX;
constexpr int64_t INTEGER_MIN = INT64_MIN;

char history[HISTORY_CAPACITY][INPUT_CAPACITY]{};
uint32_t history_start = 0;
uint32_t history_count = 0;

enum class ReadResult { Complete, Cancelled, TooLong };
enum class Builtin {
    Help, ShellHelp, Whoami, Owner, Pwd, Mode, History, Info,
    Calc, Banner, User, Hpt, Config, Desktop
};

struct Command {
    const char* name;
    const char* usage;
    const char* description;
    Builtin id;
    bool accepts_arguments;
};

constexpr Command builtins[] = {
    {"help", "help [command]", "Show all help or one shell command", Builtin::Help, true},
    {"shell-help", "shell-help", "List shell commands and editing keys", Builtin::ShellHelp, false},
    {"whoami", "whoami", "Show the terminal identity", Builtin::Whoami, false},
    {"owner", "owner", "Show the configured device owner", Builtin::Owner, false},
    {"pwd", "pwd", "Show the full working directory", Builtin::Pwd, false},
    {"mode", "mode", "Show terminal mode and command authority", Builtin::Mode, false},
    {"history", "history [clear]", "Show or erase this mode's command history", Builtin::History, true},
    {"shell-info", "shell-info", "Show shell limits and subsystem status", Builtin::Info, false},
    {"calc", "calc <integer> <+|-|*|/|%> <integer>", "Calculate with signed 64-bit integers", Builtin::Calc, true},
    {"banner", "banner", "Redraw the terminal heading", Builtin::Banner, false},
    {"user", "user", "Switch to basic terminal permissions", Builtin::User, false},
    {"hpt", "hpt", "Authenticate and enter HPT", Builtin::Hpt, false},
    {"config", "config", "Open configuration with root authority", Builtin::Config, false},
    {"desktop", "desktop [enable|disable|show [boot|terminal]|status]", "Manage or launch the desktop", Builtin::Desktop, true}
};

void print(const char* text, uint32_t color = Theme::TEXT) {
    if (text)
        Console::write(text, color);
}

void println(const char* text = "", uint32_t color = Theme::TEXT) {
    print(text, color);
    Console::newline();
}

bool space(char c) {
    return c == ' ' || c == '\t';
}

bool equals(const char* a, const char* b) {
    if (!a || !b)
        return false;
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

const char* skip_spaces(const char* text) {
    if (!text)
        return "";
    while (space(*text))
        ++text;
    return text;
}

bool take_word(const char*& text, const char* word) {
    const char* current = skip_spaces(text);
    while (*word) {
        if (*current != *word)
            return false;
        ++current;
        ++word;
    }
    if (*current && !space(*current))
        return false;
    text = skip_spaces(current);
    return true;
}

void erase_secret(char* buffer, uint32_t capacity) {
    if (!buffer)
        return;
    volatile char* data = buffer;
    for (uint32_t i = 0; i < capacity; ++i)
        data[i] = '\0';
}

uint64_t magnitude(int64_t value) {
    return value < 0 ? static_cast<uint64_t>(-(value + 1)) + 1
                     : static_cast<uint64_t>(value);
}

void print_number(uint64_t value) {
    char buffer[21]{};
    uint32_t position = sizeof(buffer) - 1;
    do {
        buffer[--position] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value);
    print(buffer + position);
}

const char* owner_name() {
    const char* owner = Security::device_owner();
    return owner && *owner ? owner : "user";
}

bool current_path(char* path, uint32_t capacity) {
    if (!path || capacity < 2)
        return false;
    path[0] = '\0';
    if (!VFS::initialized())
        return false;
    VFS::working_directory(path, capacity);
    path[capacity - 1] = '\0';
    return path[0] != '\0';
}

ReadResult read_line(char* buffer, uint32_t capacity, bool hidden = false) {
    if (!buffer || capacity == 0)
        return ReadResult::TooLong;

    uint32_t count = 0;
    bool overflow = false;
    erase_secret(buffer, capacity);

    while (true) {
        unsigned char c = static_cast<unsigned char>(Keyboard::wait_char());
        if (c == '\r' || c == '\n') {
            Console::newline();
            if (overflow) {
                erase_secret(buffer, capacity);
                println("Input too long. Line discarded; use Ctrl+U to start over.", Theme::ERROR);
                return ReadResult::TooLong;
            }
            return ReadResult::Complete;
        }
        if (c == 3) {
            erase_secret(buffer, capacity);
            if (!hidden)
                print("^C");
            Console::newline();
            return ReadResult::Cancelled;
        }
        if (c == 21) {
            while (count) {
                buffer[--count] = '\0';
                if (!hidden)
                    Console::backspace();
            }
            overflow = false;
            continue;
        }
        if (c == 23) {
            while (count && space(buffer[count - 1])) {
                buffer[--count] = '\0';
                if (!hidden)
                    Console::backspace();
            }
            while (count && !space(buffer[count - 1])) {
                buffer[--count] = '\0';
                if (!hidden)
                    Console::backspace();
            }
            continue;
        }
        if (c == '\b' || c == 127) {
            if (count) {
                buffer[--count] = '\0';
                if (!hidden)
                    Console::backspace();
            }
            continue;
        }
        if (c == '\t' && !hidden)
            c = ' ';
        if (c < 32 || c > 126)
            continue;
        if (overflow || count + 1 >= capacity) {
            overflow = true;
            continue;
        }

        buffer[count++] = static_cast<char>(c);
        buffer[count] = '\0';
        if (!hidden) {
            char output[2] = {static_cast<char>(c), '\0'};
            print(output);
        }
    }
}

bool authenticate() {
    if (!Security::root_exists()) {
        println("Root account is not configured.", Theme::ERROR);
        return false;
    }
    char password[64]{};
    print("Root password: ", Theme::WARNING);
    const ReadResult result = read_line(password, sizeof(password), true);
    const bool valid = result == ReadResult::Complete &&
                       Security::authenticate_root(password);
    erase_secret(password, sizeof(password));
    if (result == ReadResult::Complete && !valid)
        println("Authentication failed.", Theme::ERROR);
    return valid;
}

void clear_history() {
    for (uint32_t i = 0; i < HISTORY_CAPACITY; ++i)
        erase_secret(history[i], INPUT_CAPACITY);
    history_start = 0;
    history_count = 0;
}

void remember(const char* input) {
    if (!input || !*input)
        return;
    if (history_count) {
        const uint32_t last = (history_start + history_count - 1) % HISTORY_CAPACITY;
        if (equals(history[last], input))
            return;
    }
    uint32_t slot = (history_start + history_count) % HISTORY_CAPACITY;
    if (history_count == HISTORY_CAPACITY) {
        slot = history_start;
        history_start = (history_start + 1) % HISTORY_CAPACITY;
    } else {
        ++history_count;
    }
    erase_secret(history[slot], INPUT_CAPACITY);
    for (uint32_t i = 0; i + 1 < INPUT_CAPACITY && input[i]; ++i)
        history[slot][i] = input[i];
}

void show_history() {
    if (!history_count) {
        println("History is empty.");
        return;
    }
    for (uint32_t i = 0; i < history_count; ++i) {
        print_number(i + 1);
        print("  ");
        println(history[(history_start + i) % HISTORY_CAPACITY]);
    }
}

void account_setup() {
    Terminal::set_mode(Terminal::Mode::User);
    clear_history();
    Console::initialize();
    println("WELCOME TO ASTRALOS", Theme::DOWNLOAD);
    println();

    if (!Security::root_exists()) {
        println("CREATE ROOT ACCOUNT", Theme::DOWNLOAD);
        println("Development account: RAM only");
        println("Use a temporary password (8+ characters)");
        println();

        char owner[32]{};
        char password[64]{};
        char confirmation[64]{};

        while (true) {
            erase_secret(owner, sizeof(owner));
            erase_secret(password, sizeof(password));
            erase_secret(confirmation, sizeof(confirmation));

            print("Owner: ", Theme::DOWNLOAD);
            if (read_line(owner, sizeof(owner)) != ReadResult::Complete)
                continue;
            const char* name = skip_spaces(owner);
            uint32_t owner_length = 0;
            while (owner[owner_length])
                ++owner_length;
            while (owner_length && space(owner[owner_length - 1]))
                owner[--owner_length] = '\0';
            if (!*name) {
                println("Owner cannot be empty.", Theme::ERROR);
                continue;
            }

            print("Password: ", Theme::WARNING);
            if (read_line(password, sizeof(password), true) != ReadResult::Complete)
                continue;
            print("Confirm: ", Theme::WARNING);
            if (read_line(confirmation, sizeof(confirmation), true) != ReadResult::Complete) {
                erase_secret(password, sizeof(password));
                continue;
            }

            uint32_t length = 0;
            while (password[length])
                ++length;
            const bool created = length >= 8 && equals(password, confirmation) &&
                                 Security::create_root(name, password);
            erase_secret(password, sizeof(password));
            erase_secret(confirmation, sizeof(confirmation));
            erase_secret(owner, sizeof(owner));
            if (created) {
                println("Root account created.", Theme::SUCCESS);
                break;
            }
            println("Invalid details, short password, or passwords do not match.", Theme::ERROR);
        }
    }

    println();
    println("ROOT LOGIN", Theme::DOWNLOAD);
    while (!authenticate())
        println("Please try again.", Theme::WARNING);

    if (!Desktop::SessionController::start_authenticated(0, "root"))
        println("Warning: graphical session unavailable.", Theme::WARNING);
    Terminal::set_mode(Terminal::Mode::HPT);
}

void heading() {
    Console::initialize();
    print("AstralOS v");
    println(VERSION);
    println(Terminal::is_privileged() ? "HPT Terminal" : "User Terminal", Theme::DOWNLOAD);
    println(Terminal::is_privileged() ? "High Permissions Terminal" : "Basic Permissions Terminal");
    println();
    println("Type help or ast help for commands; shell-help for editing keys.");
    println("Desktop: ast desktop enable / show / status");
    println();
}

void switch_user() {
    clear_history();
    Terminal::set_mode(Terminal::Mode::User);
    if (VFS::initialized()) {
        VFS::change_directory("/", false);
        VFS::change_directory("/home/user", false);
    }
    heading();
    println("Basic terminal permissions enabled.", Theme::SUCCESS);
}

void switch_hpt(bool authorized) {
    if (Terminal::is_privileged()) {
        println("Already in HPT Terminal.", Theme::SUCCESS);
        return;
    }
    if (!authorized && !authenticate())
        return;
    clear_history();
    Terminal::set_mode(Terminal::Mode::HPT);
    if (VFS::initialized())
        VFS::change_directory("/root", true);
    heading();
    println("HPT access granted.", Theme::SUCCESS);
}

void desktop_status() {
    const Desktop::Preferences& prefs = Desktop::preferences();
    println("ASTRALOS DESKTOP", Theme::DOWNLOAD);
    println(prefs.enabled ? "Status: Enabled" : "Status: Disabled",
            prefs.enabled ? Theme::SUCCESS : Theme::WARNING);
    println(prefs.bootMode == Desktop::BootMode::Desktop
                ? "Boot interface: Desktop" : "Boot interface: Terminal");
    println(Desktop::SessionController::can_run_apps()
                ? "Application session: Available" : "Application session: Unavailable");
    print("Version: ");
    println(VERSION);
}

void desktop_command(const char* argument, bool privileged) {
    const char* rest = skip_spaces(argument);
    const char* normalized = nullptr;
    if (!*rest || equals(rest, "status")) {
        desktop_status();
        return;
    }
    if (equals(rest, "enable"))
        normalized = "enable";
    else if (equals(rest, "disable"))
        normalized = "disable";
    else if (take_word(rest, "show")) {
        if (!*rest)
            normalized = "show";
        else if (equals(rest, "boot"))
            normalized = "show boot";
        else if (equals(rest, "terminal"))
            normalized = "show terminal";
    }
    if (!normalized)
        normalized = argument;

    const Desktop::CommandAction action = Desktop::execute_command(normalized, privileged);
    if (action.result != Desktop::CommandResult::Success) {
        println(Desktop::command_message(action.result), Theme::ERROR);
        return;
    }
    if (action.launchDesktop) {
        if (!Desktop::SessionController::can_run_apps()) {
            println("Desktop session unavailable or locked.", Theme::ERROR);
            return;
        }
        println("Starting AstralOS Desktop...", Theme::SUCCESS);
        Desktop::run();
        heading();
        println("Returned to AstralOS Terminal.", Theme::SUCCESS);
        return;
    }
    if (equals(normalized, "enable"))
        println("Desktop enabled.", Theme::SUCCESS);
    else if (equals(normalized, "disable"))
        println("Desktop disabled.", Theme::SUCCESS);
    else if (equals(normalized, "show boot"))
        println("Desktop boot preference saved.", Theme::SUCCESS);
    else if (equals(normalized, "show terminal"))
        println("Terminal boot preference saved.", Theme::SUCCESS);
}

const Command* find_builtin(const char* input, const char*& arguments) {
    for (const Command& command : builtins) {
        const char* rest = input;
        if (take_word(rest, command.name)) {
            arguments = rest;
            return &command;
        }
    }
    return nullptr;
}

void command_help(const Command& command) {
    print("  ");
    println(command.usage, Theme::DOWNLOAD);
    print("    ");
    println(command.description);
}

void shell_help() {
    println("SHELL COMMANDS", Theme::DOWNLOAD);
    for (const Command& command : builtins)
        command_help(command);
    println("Shell commands accept an optional ast prefix.");
    println("sudo ast <command> authenticates for that command only; hpt changes mode.");
    println("History is RAM-only and is erased when terminal mode changes.");
    println("Password prompts are never recorded in history.");
    println("Backspace: erase character; Tab: space.");
    println("Ctrl+C: cancel input; Ctrl+U: erase line; Ctrl+W: erase word.");
    println("Control shortcuts require the keyboard driver to emit control characters.");
}

void show_mode(bool privileged) {
    println(Terminal::is_privileged() ? "Terminal mode: HPT" : "Terminal mode: User");
    println(privileged ? "Command authority: Root" : "Command authority: Basic");
}

void shell_info(bool privileged) {
    print("AstralOS shell ");
    println(VERSION);
    show_mode(privileged);
    println(Security::root_exists() ? "Root account: Configured" : "Root account: Missing");
    println(VFS::initialized() ? "VFS: Initialized" : "VFS: Unavailable");
    println(Desktop::SessionController::can_run_apps()
                ? "Desktop session: Available" : "Desktop session: Unavailable");
    print("Maximum input characters: ");
    print_number(INPUT_CAPACITY - 1);
    println();
    print("History entries: ");
    print_number(history_count);
    print(" / ");
    print_number(HISTORY_CAPACITY);
    println();
}

bool parse_integer(const char*& text, int64_t& value) {
    const char* p = skip_spaces(text);
    const bool negative = *p == '-';
    if (*p == '-' || *p == '+')
        ++p;
    if (*p < '0' || *p > '9')
        return false;
    const uint64_t limit = static_cast<uint64_t>(INTEGER_MAX) + (negative ? 1u : 0u);
    uint64_t result = 0;
    while (*p >= '0' && *p <= '9') {
        const uint64_t digit = static_cast<uint64_t>(*p - '0');
        if (result > (limit - digit) / 10)
            return false;
        result = result * 10 + digit;
        ++p;
    }
    if (*p && !space(*p))
        return false;
    if (negative && result == static_cast<uint64_t>(INTEGER_MAX) + 1)
        value = INTEGER_MIN;
    else
        value = negative ? -static_cast<int64_t>(result) : static_cast<int64_t>(result);
    text = skip_spaces(p);
    return true;
}

void calculate(const char* arguments) {
    int64_t left = 0;
    int64_t right = 0;
    const char* rest = arguments;
    if (!parse_integer(rest, left) || !*rest) {
        println("Usage: calc <integer> <+|-|*|/|%> <integer>", Theme::WARNING);
        return;
    }
    const char operation = *rest++;
    if (!space(*rest) || !parse_integer(rest, right) || *rest) {
        println("Use two signed 64-bit decimal integers separated by an operator and spaces.", Theme::ERROR);
        return;
    }

    int64_t result = 0;
    bool overflow = false;
    switch (operation) {
        case '+':
            overflow = (right > 0 && left > INTEGER_MAX - right) ||
                       (right < 0 && left < INTEGER_MIN - right);
            if (!overflow)
                result = left + right;
            break;
        case '-':
            overflow = (right < 0 && left > INTEGER_MAX + right) ||
                       (right > 0 && left < INTEGER_MIN + right);
            if (!overflow)
                result = left - right;
            break;
        case '*': {
            const bool negative = (left < 0) != (right < 0);
            const uint64_t a = magnitude(left);
            const uint64_t b = magnitude(right);
            const uint64_t limit = static_cast<uint64_t>(INTEGER_MAX) + (negative ? 1u : 0u);
            overflow = b && a > limit / b;
            if (!overflow) {
                const uint64_t product = a * b;
                if (negative && product == static_cast<uint64_t>(INTEGER_MAX) + 1)
                    result = INTEGER_MIN;
                else
                    result = negative ? -static_cast<int64_t>(product) : static_cast<int64_t>(product);
            }
            break;
        }
        case '/':
        case '%':
            if (!right) {
                println("Division by zero.", Theme::ERROR);
                return;
            }
            if (left == INTEGER_MIN && right == -1) {
                overflow = operation == '/';
                result = 0;
            } else {
                result = operation == '/' ? left / right : left % right;
            }
            break;
        default:
            println("Unknown operator. Use +, -, *, /, or %.", Theme::ERROR);
            return;
    }
    if (overflow) {
        println("Integer overflow.", Theme::ERROR);
        return;
    }
    if (result < 0)
        print("-");
    print_number(magnitude(result));
    println();
}

void execute_builtin(const Command& command, const char* arguments, bool privileged) {
    switch (command.id) {
        case Builtin::Help:
            if (*arguments) {
                const char* extra = nullptr;
                const Command* found = find_builtin(arguments, extra);
                if (found && !*extra)
                    command_help(*found);
                else
                    println("No shell help for that name. Use help for module commands.", Theme::WARNING);
            } else {
                Commands::show_help();
                println();
                shell_help();
            }
            break;
        case Builtin::ShellHelp:
            shell_help();
            break;
        case Builtin::Whoami:
            println(Terminal::is_privileged() ? "root" : owner_name());
            break;
        case Builtin::Owner:
            println(owner_name());
            break;
        case Builtin::Pwd: {
            char path[VFS::MAX_PATH]{};
            if (current_path(path, sizeof(path)))
                println(path);
            else
                println("Working directory unavailable.", Theme::WARNING);
            break;
        }
        case Builtin::Mode:
            show_mode(privileged);
            break;
        case Builtin::History:
            if (!*arguments)
                show_history();
            else if (equals(arguments, "clear")) {
                clear_history();
                println("History cleared.", Theme::SUCCESS);
            } else
                command_help(command);
            break;
        case Builtin::Info:
            shell_info(privileged);
            break;
        case Builtin::Calc:
            calculate(arguments);
            break;
        case Builtin::Banner:
            heading();
            break;
        case Builtin::User:
            switch_user();
            break;
        case Builtin::Hpt:
            switch_hpt(privileged);
            break;
        case Builtin::Config: {
            if (!privileged) {
                println("Root authority required. Use sudo ast config or enter HPT.", Theme::ERROR);
                break;
            }
            const bool was_hpt = Terminal::is_privileged();
            if (!was_hpt)
                Terminal::set_mode(Terminal::Mode::HPT);
            ConfigWizard::open();
            Terminal::set_mode(was_hpt ? Terminal::Mode::HPT : Terminal::Mode::User);
            break;
        }
        case Builtin::Desktop:
            desktop_command(arguments, privileged);
            break;
    }
}

void process(const char* input) {
    input = skip_spaces(input);
    if (!*input)
        return;

    char line[INPUT_CAPACITY]{};
    uint32_t length = 0;
    while (input[length] && length + 1 < INPUT_CAPACITY) {
        line[length] = input[length];
        ++length;
    }
    if (input[length]) {
        erase_secret(line, sizeof(line));
        println("Command too long. Nothing executed.", Theme::ERROR);
        return;
    }
    while (length && space(line[length - 1]))
        line[--length] = '\0';

    const char* command_text = line;
    const bool sudo_requested = take_word(command_text, "sudo");
    const bool astral_command = take_word(command_text, "ast");
    if (!*command_text) {
        if (sudo_requested)
            println("Usage: sudo [ast] <command>", Theme::WARNING);
        else if (astral_command) {
            Commands::show_help();
            shell_help();
        }
        erase_secret(line, sizeof(line));
        return;
    }

    const char* arguments = nullptr;
    const Command* builtin = find_builtin(command_text, arguments);
    if (builtin && !builtin->accepts_arguments && *arguments) {
        command_help(*builtin);
        erase_secret(line, sizeof(line));
        return;
    }
    if (!builtin && !astral_command && !Commands::is_native(command_text)) {
        println("Unknown command. Type help or use ast <command>.", Theme::ERROR);
        erase_secret(line, sizeof(line));
        return;
    }

    bool privileged = Terminal::is_privileged();
    const bool dropping_privileges = builtin && builtin->id == Builtin::User;
    if (sudo_requested && !privileged && !dropping_privileges) {
        if (!authenticate()) {
            erase_secret(line, sizeof(line));
            return;
        }
        privileged = true;
    }

    if (!builtin || (builtin->id != Builtin::History && builtin->id != Builtin::Hpt &&
                     builtin->id != Builtin::User))
        remember(line);

    if (builtin)
        execute_builtin(*builtin, arguments, privileged);
    else if (astral_command)
        Commands::execute(command_text, privileged);
    else
        Commands::execute_native(command_text, privileged);
    erase_secret(line, sizeof(line));
}

void prompt() {
    char path[VFS::MAX_PATH]{};
    const bool has_path = current_path(path, sizeof(path));
    print(Terminal::is_privileged() ? "root" : owner_name(), Theme::DOWNLOAD);
    print("@astral:", Theme::DOWNLOAD);
    print(has_path ? path : "?");
    print(Terminal::is_privileged() ? " # " : " $ ", Theme::SUCCESS);
}

}

[[noreturn]] void run() {
    account_setup();
    if (VFS::initialized())
        VFS::change_directory("/root", true);
    heading();
    char input[INPUT_CAPACITY]{};
    while (true) {
        prompt();
        if (read_line(input, sizeof(input)) == ReadResult::Complete)
            process(input);
        erase_secret(input, sizeof(input));
    }
}

}