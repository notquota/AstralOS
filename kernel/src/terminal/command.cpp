
#include "commands.h"

#include "../graphics/console.h"
#include "../graphics/theme.h"
#include "../memory/physical.h"
#include "../filesystem/vfs.h"
#include "../security/account.h"
#include "../drivers/keyboard.h"

#include <stdint.h>

namespace Astral::Commands {

static void print(
    const char* s,
    uint32_t color = Theme::TEXT
) {
    Console::write(s, color);
}

static void line(
    const char* s,
    uint32_t color = Theme::TEXT
) {
    print(s, color);
    Console::newline();
}

static bool equal(const char* a, const char* b) {
    if (!a || !b) return false;

    while (*a && *b) {
        if (*a++ != *b++) return false;
    }

    return *a == 0 && *b == 0;
}

static bool prefix(
    const char* s,
    const char* p
) {
    if (!s || !p) return false;

    while (*p) {
        if (*s++ != *p++) return false;
    }

    return true;
}

static void number(uint64_t n) {
    char digits[32];
    uint32_t count = 0;

    do {
        digits[count++] = '0' + (n % 10);
        n /= 10;
    } while (n && count < 32);

    while (count) {
        char c[2] = {digits[--count], 0};
        print(c);
    }
}

static void result(VFS::Result r) {
    if (r != VFS::Result::OK) {
        line(VFS::error_string(r), Theme::ERROR);
    }
}

static const char* arg(
    const char* text,
    const char* command
) {
    while (*command && *text == *command) {
        ++text;
        ++command;
    }

    while (*text == ' ') ++text;
    return text;
}

static void cmd_pwd() {
    char path[VFS::MAX_PATH]{};
    VFS::working_directory(path, sizeof(path));
    line(path, Theme::DOWNLOAD);
}

static void cmd_ls(
    const char* path,
    bool privileged
) {
    if (!path || !*path)
        path = ".";

    VFS::Entry entries[VFS::MAX_NODES]{};
    uint32_t count = 0;

    VFS::Result r = VFS::list(
        path, entries, VFS::MAX_NODES,
        count, privileged
    );

    if (r != VFS::Result::OK) {
        result(r);
        return;
    }

    for (uint32_t i = 0; i < count; ++i) {
        print(
            entries[i].type == VFS::Type::Directory
                ? "[DIR]  " : "[FILE] ",
            entries[i].type == VFS::Type::Directory
                ? Theme::DOWNLOAD : Theme::TEXT
        );

        line(entries[i].name);
    }
}

static void cmd_cat(
    const char* path,
    bool privileged
) {
    if (!path || !*path) {
        line("Usage: cat PATH", Theme::WARNING);
        return;
    }

    char data[VFS::MAX_FILE_SIZE]{};

    VFS::Result r = VFS::read_file(
        path, data, sizeof(data), privileged
    );

    if (r == VFS::Result::OK)
        line(data);
    else
        result(r);
}

static void cmd_write(
    const char* arguments,
    bool privileged
) {
    // Syntax: write FILE TEXT
    if (!arguments || !*arguments) {
        line("Usage: write FILE TEXT", Theme::WARNING);
        return;
    }

    char path[VFS::MAX_PATH]{};
    uint32_t i = 0;

    while (arguments[i] &&
           arguments[i] != ' ' &&
           i + 1 < sizeof(path)) {
        path[i] = arguments[i];
        ++i;
    }

    if (arguments[i] != ' ') {
        line("Usage: write FILE TEXT", Theme::WARNING);
        return;
    }

    path[i] = 0;

    const char* text = arguments + i;
    while (*text == ' ') ++text;

    VFS::Result r = VFS::write_file(
        path, text, privileged
    );

    if (r == VFS::Result::OK)
        line("File saved in RAM", Theme::SUCCESS);
    else
        result(r);
}

static void cmd_memory() {
    if (!Memory::initialized()) {
        line("Memory unavailable", Theme::ERROR);
        return;
    }

    print("Total usable MiB: ");
    number(Memory::total_memory_mib());
    Console::newline();

    print("Free usable MiB:  ");
    number(Memory::free_memory_mib());
    Console::newline();
}

static void cmd_status() {
    line("ASTRALOS SYSTEM STATUS", Theme::DOWNLOAD);

    line(
        Memory::initialized()
            ? "Physical allocator: ready"
            : "Physical allocator: unavailable",
        Memory::initialized()
            ? Theme::SUCCESS : Theme::ERROR
    );

    line(
        VFS::initialized()
            ? "AstralFS: mounted in RAM"
            : "AstralFS: unavailable",
        VFS::initialized()
            ? Theme::SUCCESS : Theme::ERROR
    );

    line(
        Keyboard::initialized()
            ? "Keyboard: polling active"
            : "Keyboard: unavailable",
        Keyboard::initialized()
            ? Theme::SUCCESS : Theme::ERROR
    );

    line("Disk persistence: unavailable", Theme::WARNING);
}

void show_help() {
    line("ASTRALOS COMMANDS", Theme::DOWNLOAD);

    line("help               Show help");
    line("clear              Clear terminal");
    line("pwd                Current directory");
    line("ls [PATH]          List directory");
    line("cd PATH            Change directory");
    line("mkdir PATH         Create directory");
    line("touch PATH         Create file");
    line("cat PATH           Read text file");
    line("write FILE TEXT    Save text file");
    line("rm PATH            Remove file/empty dir");
    line("echo TEXT          Print text");
    line("whoami             Current identity");
    line("uname -a           OS information");
    line("free               Memory statistics");
    line("ast status         Kernel status");
    line("ast memory         Memory statistics");
    line("ast root-status    Root account details");
    line("ast user           Basic terminal");
    line("ast hpt            HPT terminal");
    line("sudo COMMAND       Temporary elevation");
}

static bool native_name(const char* s) {
    return equal(s, "help") ||
           equal(s, "clear") ||
           equal(s, "pwd") ||
           equal(s, "ls") ||
           equal(s, "cd") ||
           equal(s, "mkdir") ||
           equal(s, "touch") ||
           equal(s, "cat") ||
           equal(s, "write") ||
           equal(s, "rm") ||
           equal(s, "echo") ||
           equal(s, "whoami") ||
           equal(s, "uname") ||
           equal(s, "free");
}

bool is_native(const char* command) {
    if (!command || !*command)
        return false;

    char name[32]{};
    uint32_t i = 0;

    while (command[i] &&
           command[i] != ' ' &&
           i + 1 < sizeof(name)) {
        name[i] = command[i];
        ++i;
    }

    if (command[i] && command[i] != ' ')
        return false;

    name[i] = 0;
    return native_name(name);
}

static void dispatch(
    const char* command,
    bool privileged
) {
    if (equal(command, "help")) {
        show_help();
    }
    else if (equal(command, "clear")) {
        Console::initialize();
    }
    else if (equal(command, "pwd")) {
        cmd_pwd();
    }
    else if (equal(command, "ls") ||
             prefix(command, "ls ")) {
        cmd_ls(arg(command, "ls"), privileged);
    }
    else if (prefix(command, "cd ")) {
        result(VFS::change_directory(
            arg(command, "cd"), privileged
        ));
    }
    else if (equal(command, "cd")) {
        result(VFS::change_directory(
            privileged ? "/root" : "/home/user",
            privileged
        ));
    }
    else if (prefix(command, "mkdir ")) {
        result(VFS::make_directory(
            arg(command, "mkdir"), privileged
        ));
    }
    else if (prefix(command, "touch ")) {
        result(VFS::create_file(
            arg(command, "touch"), privileged
        ));
    }
    else if (prefix(command, "rm ")) {
        result(VFS::remove(
            arg(command, "rm"), privileged
        ));
    }
    else if (prefix(command, "cat ")) {
        cmd_cat(arg(command, "cat"), privileged);
    }
    else if (prefix(command, "write ")) {
        cmd_write(arg(command, "write"), privileged);
    }
    else if (equal(command, "echo")) {
        Console::newline();
    }
    else if (prefix(command, "echo ")) {
        line(command + 5);
    }
    else if (equal(command, "whoami")) {
        line(
            privileged ? "root" : Security::device_owner(),
            Theme::DOWNLOAD
        );
    }
    else if (equal(command, "uname") ||
             equal(command, "uname -a")) {
        line("AstralOS 0.0.4-alpha x86_64");
    }
    else if (equal(command, "free") ||
             equal(command, "memory")) {
        cmd_memory();
    }
    else if (equal(command, "status")) {
        cmd_status();
    }
    else if (equal(command, "version")) {
        line("AstralOS v0.0.4-alpha");
    }
    else if (equal(command, "root-status")) {
        if (!privileged) {
            line("Permission denied", Theme::ERROR);
            return;
        }

        line(
            Security::root_exists()
                ? "Root: configured"
                : "Root: missing",
            Theme::DOWNLOAD
        );

        print("Device owner: ");
        line(Security::device_owner());
    }
    else {
        line("Unknown command. Type help.", Theme::ERROR);
    }
}

void execute(
    const char* command,
    bool privileged
) {
    dispatch(command, privileged);
}

void execute_native(
    const char* command,
    bool privileged
) {
    if (!is_native(command)) {
        line("Unknown command", Theme::ERROR);
        return;
    }

    dispatch(command, privileged);
}

}
